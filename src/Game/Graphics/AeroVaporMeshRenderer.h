#pragma once

#include <array>
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <wrl/client.h>

#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Camera/Camera.h"
#include "../../engine/Graphics/System/DirectXCommon.h"
#include "../../engine/Graphics/System/SrvManager.h"

class Model;

/// <summary>
/// 専用3D多層シェル＋空力形状マスク＆フローノイズによるリアル空力ベイパー描画マネージャ
/// （主翼前縁・ストレーキ凝縮シート / 遷音速ベイパーコーン兼用）
/// </summary>
class AeroVaporMeshRenderer {
public:
	struct AeroFlowParams {
		float aoaRad = 0.0f;
		float sideslipRad = 0.0f;
		float rollAsymmetry = 0.0f;
		float airspeedScale = 1.0f;
	};

	struct WingPlanformSummary {
		bool valid = false;
		float halfWingspan = 4.50f;
		float rootX = 0.95f;
		float tipZ = -4.78f;
		float tipY = 0.09f;
		std::array<MyMath::Vector3, 5> rightTrailingStations{};
	};

	struct VaporTuningParams {
		float flowSpeed = 3.2f;
		float leadingEdgeSharpness = 0.075f;
		float trailingErosion = 0.72f;
		float densityScale = 0.78f;
		float domeThickness = 1.0f;
		float fresnelPower = 0.75f;
		float softDistance = 0.30f;
		float wingSpanExpand = 1.05f;
		float vortexCoreStrength = 0.85f;
		float streamlineTwist = 1.0f;
		MyMath::Vector3 shadowTint = { 0.72f, 0.80f, 0.91f };
		bool forcePreview = false;
		float previewWingIntensity = 0.85f;
		float previewConeIntensity = 0.0f;
	};

	static AeroVaporMeshRenderer* GetInstance();

	AeroVaporMeshRenderer() = default;
	~AeroVaporMeshRenderer() = default;

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Finalize();
	void Reset();

	bool IsInitialized() const { return isInitialized_; }

	/// <summary>
	/// ロード済み機体モデルの主翼メッシュから前縁・後縁・翼上面を自動抽出して3Dベイパーシェルを再構築
	/// </summary>
	bool RebuildWingMeshFromModel(
		const Model* model,
		const MyMath::Vector3& baseRotation = { 0.0f, 3.14159265358979323846f, 0.0f });

	const WingPlanformSummary& GetWingPlanform() const { return planformSummary_; }

	/// <summary>
	/// 主翼上面・ストレーキの3D多層凝縮ベイパーシェルを描画
	/// </summary>
	void DrawWingVapor(
		const MyMath::Matrix4x4& aircraftWorldMatrix,
		float intensity,
		float timeSec,
		Camera* camera,
		const AeroFlowParams& flowParams = {});

	/// <summary>
	/// 遷音速プラントル＝グロワート ベイパーコーンを描画
	/// </summary>
	void DrawTransonicCone(const MyMath::Matrix4x4& aircraftWorldMatrix, float intensity, float timeSec, Camera* camera);

	VaporTuningParams& TuningParams() { return tuning_; }
	const VaporTuningParams& GetTuningParams() const { return tuning_; }

	void DrawDebugGui();

private:
	AeroVaporMeshRenderer(const AeroVaporMeshRenderer&) = delete;
	AeroVaporMeshRenderer& operator=(const AeroVaporMeshRenderer&) = delete;

	struct VertexData {
		MyMath::Vector4 position;
		Vector2 texcoord;
		MyMath::Vector3 normal;
		MyMath::Vector4 extra; // x: layerFactor [0..1], y: domeHeight (m), z: sideSign (-1/+1), w: chordNorm [0..1]
	};

	struct VaporTransformCB {
		MyMath::Matrix4x4 WVP;
		MyMath::Matrix4x4 World;
		MyMath::Matrix4x4 WorldInverseTranspose;
	};
	static_assert(sizeof(VaporTransformCB) == 192, "VaporTransformCB must match HLSL (192 bytes)");

	struct VaporParamsCB {
		float time;
		float intensity;
		float flowSpeed;
		float softDistance;

		float leadingEdgeSharpness;
		float trailingErosion;
		float densityScale;
		float fresnelPower;

		MyMath::Vector3 sunDirection;
		float sunIntensity;

		MyMath::Vector3 sunColor;
		float domeThickness;

		MyMath::Vector3 shadowTint;
		uint32_t mode;

		MyMath::Vector3 cameraPosition;
		float nearClip;

		float farClip;
		uint32_t hasDepthTexture;
		float wingSpanExpand;
		float opacityBoost;

		MyMath::Vector4 baseColor;

		float aoaRad;
		float sideslipRad;
		float rollAsymmetry;
		float airspeedScale;

		float vortexCoreStrength;
		float streamlineTwist;
		float pad0;
		float pad1;
	};
	static_assert(sizeof(VaporParamsCB) == 160, "VaporParamsCB must match HLSL (160 bytes)");

	struct MeshBuffers {
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vbView{};
		D3D12_INDEX_BUFFER_VIEW ibView{};
		uint32_t indexCount = 0;
	};

	static constexpr uint32_t kMaxDrawCount = 64u;
	static constexpr size_t kCbAlignment = 256u;
	static constexpr uint32_t kFrameCount = DirectXCommon::kSwapChainBufferCount_;

	void CreatePipeline();
	void CreateConstantBuffers();
	void CreateWingVaporMesh();
	void CreateTransonicConeMesh();
	void EnsureNoiseTextureLoaded();

	void DrawMesh(
		const MeshBuffers& mesh,
		const MyMath::Matrix4x4& worldMatrix,
		float intensity,
		float timeSec,
		uint32_t mode,
		float opacityBoost,
		const MyMath::Vector4& baseColor,
		const AeroFlowParams& flowParams,
		Camera* camera);

	static std::unique_ptr<AeroVaporMeshRenderer> instance_;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	bool isInitialized_ = false;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

	MeshBuffers wingMesh_{};
	MeshBuffers coneMesh_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> transformBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> paramsBuffer_;
	uint8_t* transformMapped_ = nullptr;
	uint8_t* paramsMapped_ = nullptr;
	uint32_t currentDrawCount_ = 0;

	uint32_t noiseTextureIndex_ = 0;
	bool noiseTextureLoaded_ = false;
	uint32_t nullDepthSrvIndex_ = 0;

	VaporTuningParams tuning_{};
	WingPlanformSummary planformSummary_{};
	const Model* currentWingModel_ = nullptr;
	MyMath::Vector3 currentBaseRotation_{};
};
