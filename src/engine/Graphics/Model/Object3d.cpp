#include "Object3d.h"
#include "Object3dCommon.h"
#include "../System/TextureManager.h" 
#include <map>
#include <fstream>
#include <sstream>
#include <cassert>
#include "Model.h"
#include "ModelManager.h"
#include "../Camera/Camera.h"

#include <cmath>
#include <algorithm>

void Object3d::Initialize(Object3dCommon* object3dCommon) {
	// メンバ変数に保存
	object3dCommon_ = object3dCommon;


	DirectXCommon* dxCommon = object3dCommon_->GetDirectXCommon();

	// 4. 座標変換行列の初期化

	transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	transformationMatrixData_->WVP = Identity4x4();
	transformationMatrixData_->World = Identity4x4();
	transformationMatrixData_->WorldInverseTranspose = Identity4x4();

	// 7. Transform初期値設定

	transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	camera_ = object3dCommon_->GetDefaultCamera();
}

void Object3d::Update() {
	// 1. World行列を作る
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);


	// 2. Camera行列を使ってWVPを作る
	Matrix4x4 worldViewProjectionMatrix;

	if (camera_) {
		// カメラがセットされていれば、そのカメラの ViewProjection 行列をもらう
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
	} else {
		// カメラが無い場合はとりあえずWorld行列だけ（または単位行列など）
		worldViewProjectionMatrix = worldMatrix;
	}

	// 3. 定数バッファに転送
	transformationMatrixData_->WVP = worldViewProjectionMatrix;
	transformationMatrixData_->World = worldMatrix;
	transformationMatrixData_->WorldInverseTranspose = Transpose(Inverse(worldMatrix));

	// 4. LOD情報の更新
	UpdateLOD();
}

void Object3d::UpdateWithWorldMatrix(const Matrix4x4& worldMatrix) {
	// 1. Camera行列を使ってWVPを作る
	Matrix4x4 worldViewProjectionMatrix;

	if (camera_) {
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
	} else {
		worldViewProjectionMatrix = worldMatrix;
	}

	// 2. 定数バッファに転送
	transformationMatrixData_->WVP = worldViewProjectionMatrix;
	transformationMatrixData_->World = worldMatrix;
	transformationMatrixData_->WorldInverseTranspose = Transpose(Inverse(worldMatrix));

	// 3. LOD情報の更新
	UpdateLOD();
}

void Object3d::Draw() {
	// LODカリング等でモデルが無い場合は描画処理自体をスキップ
	if (!model_) {
		return;
	}

	// コマンドリストの取得
	ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDirectXCommon()->GetCommandList();

	object3dCommon_->SetBlendMode(blendMode_);

	// 座標変換行列CBufferの設定 (RootParameter Index: 1)
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());

	// 平行光源CBufferの設定 (RootParameter Index: 3)
	commandList->SetGraphicsRootConstantBufferView(3, object3dCommon_->GetDirectionalLightResource()->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(4, object3dCommon_->GetLightingSettingsResource()->GetGPUVirtualAddress());
	
	commandList->SetGraphicsRootConstantBufferView(5, object3dCommon_->GetPointLightResource()->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(6, object3dCommon_->GetSpotLightResource()->GetGPUVirtualAddress());

	// 環境マップテクスチャの設定 (RootParameter Index: 8)
	uint32_t useEnvTexIndex = (envTextureOverride_ != 0) ? envTextureOverride_ : object3dCommon_->GetDefaultEnvTextureIndex();
	if (useEnvTexIndex != 0) {
		commandList->SetGraphicsRootDescriptorTable(8, TextureManager::GetInstance()->GetSrvHandleGPU(useEnvTexIndex));
	} else {
		// 0だとバインドできずD3D12のエラーになる可能性があるため、ダミーやuvCheckerなどの安全なテクスチャをバインドする。
		// ここではuvChecker（インデックス未指定時のTextureManagerのデフォルトなどにフォールバック）をダミーとして利用。
		uint32_t dummyIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/uvChecker.png");
		if (dummyIndex != 0) {
			commandList->SetGraphicsRootDescriptorTable(8, TextureManager::GetInstance()->GetSrvHandleGPU(dummyIndex));
		}
	}

	if (!targetNodeName_.empty()) {
		model_->DrawNode(targetNodeName_);
	} else {
		model_->Draw();
	}
}

void Object3d::SetModel(const std::string& filePath) {
	model_ = ModelManager::GetInstance()->FindModel(filePath);
}

void Object3d::DebugDrawSkeleton(const Vector4& color) {
	if (model_ && camera_) {
		Matrix4x4 wMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
		model_->DebugDrawSkeleton(wMatrix, camera_, color);
	}
}

void Object3d::AddLODLevel(Model* model, float minScreenSize) {
	if (!model) return;
	lodLevels_.push_back({ model, minScreenSize });
	// minScreenSize の降順（スクリーンサイズが大きい順＝高詳細順）でソート
	std::sort(lodLevels_.begin(), lodLevels_.end(), [](const LODLevel& a, const LODLevel& b) {
		return a.minScreenSize > b.minScreenSize;
	});
	isLODEnabled_ = true;
}

void Object3d::ClearLODLevels() {
	lodLevels_.clear();
	isLODEnabled_ = false;
	activeLODIndex_ = 0;
}

void Object3d::UpdateLOD() {
	if (!isLODEnabled_ || lodLevels_.empty() || !camera_) {
		return;
	}

	// 1. カメラとオブジェクトの位置取得
	Vector3 camPos = camera_->GetTranslate();
	Vector3 objPos = transform_.translate;

	// 2. 距離計算
	Vector3 diff = { objPos.x - camPos.x, objPos.y - camPos.y, objPos.z - camPos.z };
	float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

	if (distance <= 0.0001f) {
		currentScreenSize_ = 1e9f;
		activeLODIndex_ = 0;
		model_ = lodLevels_[0].model;
		return;
	}

	// 3. 最大スケールの適用
	float maxScale = (std::max)({ std::abs(transform_.scale.x), std::abs(transform_.scale.y), std::abs(transform_.scale.z) });
	float worldRadius = boundingRadius_ * maxScale;

	// 4. スクリーン高さ比の計算
	float fovY = camera_->GetFovY();
	float tanHalfFov = std::tan(fovY * 0.5f);
	if (tanHalfFov <= 0.0001f) tanHalfFov = 0.0001f;

	currentScreenSize_ = worldRadius / (distance * tanHalfFov);

	// 5. カリング判定
	if (cullScreenSize_ > 0.0f && currentScreenSize_ < cullScreenSize_) {
		activeLODIndex_ = -1;
		model_ = nullptr; // 描画を行わない
		return;
	}

	// 6. LOD選択 (minScreenSize の高い順から判定)
	activeLODIndex_ = (int)lodLevels_.size() - 1; // デフォルトは最低詳細
	for (size_t i = 0; i < lodLevels_.size(); ++i) {
		if (currentScreenSize_ >= lodLevels_[i].minScreenSize) {
			activeLODIndex_ = (int)i;
			break;
		}
	}

	model_ = lodLevels_[activeLODIndex_].model;
}
