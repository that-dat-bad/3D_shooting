#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include "../base/Math/MyMath.h"
#include "../System/DirectXCommon.h"
#include "../System/BlendMode.h"

using namespace MyMath;

class Object3dCommon;
class Model;
class Camera;

class Object3d
{
public:
	
public:
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};
	struct DirectionalLight {
		Vector4 color;
		Vector3 direction;
		float intensity;
	};
	// LOD Level Structure
	struct LODLevel {
		Model* model = nullptr;
		float minScreenSize = 0.0f; // 画面高さに対する最小投影比率
	};

public: // メンバ関数
	// 初期化
	void Initialize(Object3dCommon* object3dCommon);
	// 更新
	void Update();
	// 親の行列等を合成したワールド行列を直接指定して更新
	void UpdateWithWorldMatrix(const Matrix4x4& worldMatrix);
	// 描画
	void Draw();
	// スケルトンのデバッグ描画
	void DebugDrawSkeleton(const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

	// セッター
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetModel(Model* model) { model_ = model; }
	void SetModel(const std::string& filePath);
	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetBlendMode(BlendMode mode) { blendMode_ = mode; }
	void SetEnvTextureOverride(uint32_t textureIndex) { envTextureOverride_ = textureIndex; }
	void SetMaterialEmissive(uint32_t materialIndex, const Vector3& color, float intensity);
	void SetMaterialEnableLighting(uint32_t materialIndex, bool enable);
	void SetMaterialColor(uint32_t materialIndex, const Vector4& color);

	// LOD (Level of Detail) API
	void SetLODEnabled(bool enabled) { isLODEnabled_ = enabled; }
	bool IsLODEnabled() const { return isLODEnabled_; }
	void AddLODLevel(Model* model, float minScreenSize);
	void ClearLODLevels();
	void SetCullScreenSize(float minScreenSize) { cullScreenSize_ = minScreenSize; }
	void SetBoundingRadius(float radius) { boundingRadius_ = radius; }
	void SetTargetNodeName(const std::string& name) {
		targetNodeName_ = name;
		targetNodeNames_.clear();
		if (!name.empty()) {
			targetNodeNames_.push_back(name);
		}
	}
	void SetTargetNodeNames(const std::vector<std::string>& names) {
		targetNodeNames_ = names;
		targetNodeName_ = names.empty() ? "" : names[0];
	}
	void AddTargetNodeName(const std::string& name) {
		targetNodeNames_.push_back(name);
		if (targetNodeName_.empty()) {
			targetNodeName_ = name;
		}
	}
	const std::vector<std::string>& GetTargetNodeNames() const { return targetNodeNames_; }
	const std::string& GetTargetNodeName() const { return targetNodeName_; }

	// カスタムパイプラインステート（特殊シェーダー描画用）
	void SetCustomPipelineState(ID3D12PipelineState* pso) { customPipelineState_ = pso; }
	ID3D12PipelineState* GetCustomPipelineState() const { return customPipelineState_.Get(); }

	// ゲッター
	Vector3 GetScale() const { return transform_.scale; }
	Vector3 GetRotate() const { return transform_.rotate; }
	Vector3 GetTranslate() const { return transform_.translate; }
	Model* GetModel() const { return model_; }
	float GetScreenSize() const { return currentScreenSize_; }
	int GetActiveLODIndex() const { return activeLODIndex_; }

private: 
	// LOD切り替え計算
	void UpdateLOD();

private: 
	Object3dCommon* object3dCommon_ = nullptr;
	Model* model_ = nullptr;
	Camera* camera_ = nullptr;

	// トランスフォーム
	Transform transform_;

	//--座標変換行列--//
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_ = nullptr;
	TransformationMatrix* transformationMatrixData_ = nullptr;

	BlendMode blendMode_ = BlendMode::kNormal;
	
	uint32_t envTextureOverride_ = 0;

	//--LOD--//
	bool isLODEnabled_ = false;
	std::vector<LODLevel> lodLevels_;
	float cullScreenSize_ = 0.005f; // 画面の0.5%未満は描画スキップ（カリング）
	float boundingRadius_ = 1.0f;  // オブジェクト境界半径
	float currentScreenSize_ = 1.0f;
	int activeLODIndex_ = 0;
	std::string targetNodeName_ = "";
	std::vector<std::string> targetNodeNames_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> customPipelineState_ = nullptr;
};