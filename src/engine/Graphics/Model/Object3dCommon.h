#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <array>
#include "../System/BlendMode.h"
#include "../../engine/base/Math/MyMath.h"
#include <memory> 

class DirectXCommon;
class Camera;
using namespace MyMath;

struct DirectionalLight {
	Vector4 color;
	Vector3 direction;
	float intensity;
};

struct PointLight {
	Vector4 color;
	Vector3 position;
	float intensity;
	float radius;
	float decay;
	float padding[2];
};

struct SpotLight {
	Vector4 color;
	Vector3 position;
	float intensity;
	Vector3 direction;
	float distance;
	float decay;
	float cosAngle;
	float cosFalloffStart;
	float padding;
};

struct LightingSettings {
	int32_t shadingModel;
	int32_t specularModel;
	int32_t lightType;
	float padding;
	Vector3 cameraPosition;
	float emissiveIntensityScale = 1.0f; ///< メッシュ自発光の全体強度スケール
};

/// <summary>
/// 3Dオブジェクト描画の共通設定管理クラス
/// </summary>
class Object3dCommon
{
public:
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static Object3dCommon* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	Object3dCommon() = default;

	void Initialize(DirectXCommon* dxCommon);
	void Finalize();

	//共通描画設定
	void SetupCommonState();

	// ブレンドモード設定
	void SetBlendMode(BlendMode mode);

	// ライティング設定
	void SetShadingModel(int32_t model) { lightingSettingsData->shadingModel = model; }
	void SetSpecularModel(int32_t model) { lightingSettingsData->specularModel = model; }
	void SetLightType(int32_t type) { lightingSettingsData->lightType = type; }
	void SetCameraPosition(const Vector3& position) { lightingSettingsData->cameraPosition = position; }
	void SetEmissiveIntensityScale(float scale) { lightingSettingsData->emissiveIntensityScale = scale; }
	float GetEmissiveIntensityScale() const { return lightingSettingsData->emissiveIntensityScale; }

	DirectXCommon* GetDirectXCommon() { return dxCommon_; }

	//アクセッサ
	//セッター
	void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }

	//ゲッター
	Camera* GetDefaultCamera() const { return defaultCamera_; }
	ID3D12Resource* GetDirectionalLightResource() { return directionalLightResource_.Get(); }
	ID3D12Resource* GetPointLightResource() { return pointLightResource_.Get(); }
	ID3D12Resource* GetSpotLightResource() { return spotLightResource_.Get(); }
	ID3D12Resource* GetLightingSettingsResource() { return lightingSettingsResource_.Get(); }
	DirectionalLight* GetDirectionalLightData() { return directionalLightData; }
	PointLight* GetPointLightData() { return pointLightData; }
	SpotLight* GetSpotLightData() { return spotLightData; }
	
	void SetDefaultEnvTextureIndex(uint32_t index) { defaultEnvTextureIndex_ = index; }
	uint32_t GetDefaultEnvTextureIndex() const { return defaultEnvTextureIndex_; }
	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

	~Object3dCommon() = default;
private:
	Object3dCommon(const Object3dCommon&) = delete;
	Object3dCommon& operator=(const Object3dCommon&) = delete;

	static std::unique_ptr<Object3dCommon> instance;

	DirectXCommon* dxCommon_ = nullptr;
	Camera* defaultCamera_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, static_cast<size_t>(BlendMode::kCountOf)> graphicsPipelineStates_;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;

	Microsoft::WRL::ComPtr<ID3D12Resource> lightingSettingsResource_;

	DirectionalLight* directionalLightData = nullptr;
	PointLight* pointLightData = nullptr;
	SpotLight* spotLightData = nullptr;
	LightingSettings* lightingSettingsData = nullptr;
	
	uint32_t defaultEnvTextureIndex_ = 0;

	//ルートシグネチャの作成
	void CreateRootSignature(DirectXCommon* dxCommon);

	//グラフィックパイプラインの生成
	void CreateGraphicsPipeline(DirectXCommon* dxCommon);
};
