#pragma once
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>

#include "../base/Math/MyMath.h"
#include "../System/BlendMode.h"
#include "../System/DirectXCommon.h"
#include "../System/SrvManager.h"
#include "GPUParticleTypes.h"
#include "ParticleManager.h"

/// <summary>
/// GPU駆動型パーティクルシステム管理クラス（65,536粒子 Compute + 2バケット ExecuteIndirect）
/// </summary>
class GPUParticleManager {
public:
	static constexpr uint32_t kMaxParticles = GPUParticleConstants::kMaxParticles;
	static constexpr uint32_t kMaxEmitRequests = GPUParticleConstants::kMaxEmitRequests;
	static constexpr uint32_t kMaxWakeFields = GPUParticleConstants::kMaxWakeFields;
	static constexpr uint32_t kInitThreadGroupSize = GPUParticleConstants::kInitThreadGroupSize;
	static constexpr uint32_t kEmitThreadGroupSize = GPUParticleConstants::kEmitThreadGroupSize;
	static constexpr uint32_t kUpdateThreadGroupSize = GPUParticleConstants::kUpdateThreadGroupSize;
	static constexpr size_t kCbAlignment = GPUParticleConstants::kPerViewCbAlignment;
	static constexpr size_t kPerFrameCbAlignment = GPUParticleConstants::kPerFrameCbAlignment;
	static constexpr uint32_t kParticleFlagFadeOut = GPUParticleFlags::kFadeOut;

	using ParticleCS = ::ParticleCS;
	using GpuEmitRequest = ::GpuEmitRequest;

	static GPUParticleManager* GetInstance();

	GPUParticleManager() = default;
	~GPUParticleManager() = default;

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Finalize();

	void Update();
	void Draw(const Matrix4x4& viewProjection, const Matrix4x4& billboardMatrix);

	bool IsInitialized() const { return isInitialized_; }

	void SetDeltaTime(float dt) { deltaTime_ = dt; }
	void SetBlendMode(BlendMode mode) { blendMode_ = mode; }
	BlendMode GetBlendMode() const { return blendMode_; }

	void SetTexture(uint32_t srvIndex) {
		textureSrvIndex_ = srvIndex;
		hasCustomTexture_ = true;
	}

	// カメラクリップ距離（ソフトパーティクル深度線形化用）
	void SetCameraClipParams(float nearClip, float farClip) {
		cameraNearClip_ = nearClip;
		cameraFarClip_ = farClip;
	}

	// グローバル基準床高さ（バウンド用フォールバック）
	void SetDefaultFloorY(float floorY) { defaultFloorY_ = floorY; }
	float GetDefaultFloorY() const { return defaultFloorY_; }

	// 航空機ウェイク場の登録（フレーム単位で反映）
	void SetWakeField(uint32_t slot, const GpuWakeField& field);
	void ClearWakeFields();

	// カメラ追従環境パーティクル空間の設定
	void SetAmbientVolume(const GpuAmbientVolume& volume) { ambientVolume_ = volume; }
	const GpuAmbientVolume& GetAmbientVolume() const { return ambientVolume_; }

	// 高機能GPUパーティクル発生（黒体放射・カールノイズ・ストレッチ・バウンド・ウェイク対応）
	void EnqueueRichEmit(const GpuParticleEmitDesc& desc);

	// 既存 .effect / 複数エミッタ互換API
	void EnqueueEmit(const Vector3& position, const ParticleParameters& params, uint32_t count);

	// 既存単一エミッタ互換API（DebugScene等向け）
	void SetEmitParams(const Vector3& position, uint32_t count, float radius, float frequency);
	void Emit();

	void ClearAllParticles();

	void SetReadbackDebugEnabled(bool enabled) { enableReadbackDebug_ = enabled; }
	bool IsReadbackDebugEnabled() const { return enableReadbackDebug_; }
	uint32_t GetDebugAliveCount() const { return debugInstanceCount_; }
	uint32_t GetDebugAlphaAliveCount() const { return debugAlphaInstanceCount_; }
	uint32_t GetDebugAdditiveAliveCount() const { return debugAdditiveInstanceCount_; }
	int32_t GetDebugFreeCount() const { return debugFreeListIndex_ + 1; }
	uint32_t GetMaxParticles() const { return kMaxParticles; }
	uint32_t GetLastFrameEmitCount() const { return lastFrameEmitCount_; }
	uint32_t GetLastFrameDroppedEmitRequests() const { return lastFrameDroppedEmitRequests_; }

	void DrawImGui();

private:
	GPUParticleManager(const GPUParticleManager&) = delete;
	GPUParticleManager& operator=(const GPUParticleManager&) = delete;

	static std::unique_ptr<GPUParticleManager> instance_;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	bool isInitialized_ = false;
	bool needsInitializeParticles_ = true;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> initPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> resetArgsPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePipelineState_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> graphicsRootSignature_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, static_cast<size_t>(BlendMode::kCountOf)> graphicsPipelineStates_;
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature_;
	BlendMode blendMode_ = BlendMode::kAdd;

	Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> aliveListAlphaBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> aliveListAddBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indirectArgsBuffer_;

	uint32_t particleSrvIndex_ = 0;
	uint32_t aliveListAlphaSrvIndex_ = 0;
	uint32_t aliveListAddSrvIndex_ = 0;

	uint32_t particleUavIndex_ = 0;
	uint32_t freeListIndexUavIndex_ = 0;
	uint32_t freeListUavIndex_ = 0;
	uint32_t aliveListAlphaUavIndex_ = 0;
	uint32_t aliveListAddUavIndex_ = 0;
	uint32_t indirectArgsUavIndex_ = 0;

	static constexpr uint32_t kFrameCount = DirectXCommon::kSwapChainBufferCount_;

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kFrameCount> emitRequestBuffer_;
	std::array<GpuEmitRequest*, kFrameCount> emitRequestMapped_{ nullptr, nullptr };
	std::array<uint32_t, kFrameCount> emitRequestSrvIndex_{ 0, 0 };

	struct PerFrame {
		float time;
		float deltaTime;
		uint32_t requestCount;
		uint32_t totalEmitCount;
		GpuAmbientVolume ambient;
		GpuWakeField wakeFields[kMaxWakeFields];
		float defaultFloorY;
		uint32_t activeWakeCount;
		float globalCurlTimeScale;
		float pad0;
	};
	static_assert(sizeof(PerFrame) == 320, "PerFrame size must match HLSL (320 bytes)");
	static_assert(sizeof(PerFrame) <= kPerFrameCbAlignment, "PerFrame must fit inside CB stride");
	Microsoft::WRL::ComPtr<ID3D12Resource> perFrameBuffer_;
	uint8_t* perFrameMapped_ = nullptr;

	struct PerView {
		Matrix4x4 viewProjection;
		Matrix4x4 billboardMatrix;
		Vector3 cameraPosition;
		float nearClip;
		float farClip;
		float time;
		uint32_t hasDepthTexture;
		float pad0;
	};
	static_assert(sizeof(PerView) == 160, "PerView size must match HLSL (160 bytes)");
	static_assert(sizeof(PerView) <= kCbAlignment, "PerView must fit inside CB stride");
	Microsoft::WRL::ComPtr<ID3D12Resource> perViewBuffer_;
	uint8_t* perViewMapped_ = nullptr;

	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	uint32_t textureSrvIndex_ = 0;
	uint32_t nullDepthSrvIndex_ = 0;
	bool hasCustomTexture_ = false;
	float time_ = 0.0f;
	float deltaTime_ = 1.0f / 60.0f;
	float cameraNearClip_ = 0.1f;
	float cameraFarClip_ = 100000.0f;
	float defaultFloorY_ = 0.0f;

	std::array<GpuWakeField, kMaxWakeFields> wakeFields_{};
	GpuAmbientVolume ambientVolume_{};

	std::vector<GpuEmitRequest> pendingEmitRequests_;
	uint32_t seedCounter_ = 1u;
	uint32_t lastFrameEmitCount_ = 0u;
	uint32_t pendingDroppedEmitRequests_ = 0u;
	uint32_t lastFrameDroppedEmitRequests_ = 0u;

	// 既存API互換用
	Vector3 legacyTranslate_ = { 0.0f, 0.0f, 0.0f };
	uint32_t legacyCount_ = 100u;
	float legacyRadius_ = 1.0f;
	float legacyFrequency_ = 0.0f;
	float legacyFrequencyTime_ = 0.0f;
	bool manualEmitRequested_ = false;

	void CreateComputePipeline();
	void CreateGraphicsPipeline();
	void CreateCommandSignature();
	void CreateBuffers();
	void CreateModel();
	void InitializeParticles();

	Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer_;
	bool enableReadbackDebug_ = false;
	int32_t debugFreeListIndex_ = static_cast<int32_t>(kMaxParticles - 1u);
	uint32_t debugInstanceCount_ = 0u;
	uint32_t debugAlphaInstanceCount_ = 0u;
	uint32_t debugAdditiveInstanceCount_ = 0u;

	Vector3 guiEmitTranslate_ = { 0.0f, 0.0f, 0.0f };
	int guiEmitCount_ = 1024;
	float guiEmitRadius_ = 5.0f;
	float guiEmitFrequency_ = 0.0f;
};
