#include "Skybox.h"
#include "SkyboxCommon.h"
#include "../System/DirectXCommon.h"
#include "../System/SrvManager.h"
#include "../Camera/Camera.h"

void Skybox::Initialize(SkyboxCommon* skyboxCommon) {
	skyboxCommon_ = skyboxCommon;
	srvManager_ = skyboxCommon_->GetSrvManager();

	DirectXCommon* dxCommon = skyboxCommon_->GetDirectXCommon();

	// マテリアルリソースの作成
	materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = 0;
	materialData_->shininess = 0.0f;
	materialData_->uvTransform = Identity4x4();

	// 変換行列リソースの作成
	transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
	transformationMatrixData_->WVP = Identity4x4();
	transformationMatrixData_->World = Identity4x4();
	transformationMatrixData_->WorldInverseTranspose = Identity4x4();

	// トランスフォーム初期値
	transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	// 箱の頂点データ作成
	CreateBox();
}

void Skybox::CreateBox() {
	DirectXCommon* dxCommon = skyboxCommon_->GetDirectXCommon();

	// --- 頂点バッファの作成 ---
	vertexResource_ = dxCommon->CreateBufferResource(sizeof(SkyboxVertex) * kVertexCount);

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(SkyboxVertex) * kVertexCount;
	vertexBufferView_.StrideInBytes = sizeof(SkyboxVertex);

	SkyboxVertex* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	// 原点を中心として、幅2m、高さ2mの箱を作る (x,y,zそれぞれ -1~1)

	// 右面 (x = +1) : 描画インデックスは[0,1,2][2,1,3]で内側に向く
	vertexData[0].position = { 1.0f,  1.0f,  1.0f, 1.0f };
	vertexData[1].position = { 1.0f,  1.0f, -1.0f, 1.0f };
	vertexData[2].position = { 1.0f, -1.0f,  1.0f, 1.0f };
	vertexData[3].position = { 1.0f, -1.0f, -1.0f, 1.0f };

	// 左面 (x = -1) : 描画インデックスは[4,5,6][6,5,7]
	vertexData[4].position = { -1.0f,  1.0f, -1.0f, 1.0f };
	vertexData[5].position = { -1.0f,  1.0f,  1.0f, 1.0f };
	vertexData[6].position = { -1.0f, -1.0f, -1.0f, 1.0f };
	vertexData[7].position = { -1.0f, -1.0f,  1.0f, 1.0f };

	// 前面 (z = +1) : 描画インデックスは[8,9,10][10,9,11]
	vertexData[8].position  = { -1.0f,  1.0f, 1.0f, 1.0f };
	vertexData[9].position  = {  1.0f,  1.0f, 1.0f, 1.0f };
	vertexData[10].position = { -1.0f, -1.0f, 1.0f, 1.0f };
	vertexData[11].position = {  1.0f, -1.0f, 1.0f, 1.0f };

	// 後面 (z = -1) : 描画インデックスは[12,13,14][14,13,15]
	vertexData[12].position = {  1.0f,  1.0f, -1.0f, 1.0f };
	vertexData[13].position = { -1.0f,  1.0f, -1.0f, 1.0f };
	vertexData[14].position = {  1.0f, -1.0f, -1.0f, 1.0f };
	vertexData[15].position = { -1.0f, -1.0f, -1.0f, 1.0f };

	// 上面 (y = +1) : 描画インデックスは[16,17,18][18,17,19]
	vertexData[16].position = { -1.0f, 1.0f, -1.0f, 1.0f };
	vertexData[17].position = {  1.0f, 1.0f, -1.0f, 1.0f };
	vertexData[18].position = { -1.0f, 1.0f,  1.0f, 1.0f };
	vertexData[19].position = {  1.0f, 1.0f,  1.0f, 1.0f };

	// 下面 (y = -1) : 描画インデックスは[20,21,22][22,21,23]
	vertexData[20].position = { -1.0f, -1.0f,  1.0f, 1.0f };
	vertexData[21].position = {  1.0f, -1.0f,  1.0f, 1.0f };
	vertexData[22].position = { -1.0f, -1.0f, -1.0f, 1.0f };
	vertexData[23].position = {  1.0f, -1.0f, -1.0f, 1.0f };

	// --- インデックスバッファの作成 ---
	indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * kIndexCount);

	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexData = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));

	// 各面のインデックス (内側に向くように)
	// 右面
	indexData[0] = 0;  indexData[1] = 1;  indexData[2] = 2;
	indexData[3] = 2;  indexData[4] = 1;  indexData[5] = 3;
	// 左面
	indexData[6] = 4;  indexData[7] = 5;  indexData[8] = 6;
	indexData[9] = 6;  indexData[10] = 5; indexData[11] = 7;
	// 前面
	indexData[12] = 8;  indexData[13] = 9;  indexData[14] = 10;
	indexData[15] = 10; indexData[16] = 9;  indexData[17] = 11;
	// 後面
	indexData[18] = 12; indexData[19] = 13; indexData[20] = 14;
	indexData[21] = 14; indexData[22] = 13; indexData[23] = 15;
	// 上面
	indexData[24] = 16; indexData[25] = 17; indexData[26] = 18;
	indexData[27] = 18; indexData[28] = 17; indexData[29] = 19;
	// 下面
	indexData[30] = 20; indexData[31] = 21; indexData[32] = 22;
	indexData[33] = 22; indexData[34] = 21; indexData[35] = 23;
}

void Skybox::Update() {
	if (!camera_) { return; }

	transform_.translate = camera_->GetTranslate();

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

	const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);

	transformationMatrixData_->WVP = worldViewProjectionMatrix;
	transformationMatrixData_->World = worldMatrix;
	transformationMatrixData_->WorldInverseTranspose = Transpose(Inverse(worldMatrix));
}

void Skybox::Draw() {
	ID3D12GraphicsCommandList* commandList = skyboxCommon_->GetDirectXCommon()->GetCommandList();

	// マテリアル (RootParameter 0: PS b0)
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	// 変換行列 (RootParameter 1: VS b1)
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	// テクスチャ (RootParameter 2: PS t0)
	srvManager_->SetGraphicsRootDescriptorTable(2, textureIndex_);

	// 頂点バッファ・インデックスバッファをセット
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);

	// 描画
	commandList->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0);
}

void Skybox::SetColor(const Vector4& color) {
	if (materialData_) {
		materialData_->color = color;
	}
}
