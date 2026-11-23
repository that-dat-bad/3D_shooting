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
#include "ParticleManager.h"

/// <summary>
/// GPU駆動型パーティクルシステム管理クラス
/// </summary>
class GPUParticleManager {
public:
	static constexpr uint32_t kMaxParticles = 65536u;
	static constexpr uint32_t kMaxEmitRequests = 64u;
	static constexpr uint32_t kInitThreadGroupSize = 256u;
	static constexpr uint32_t kEmitThreadGroupSize = 64u;
	static constexpr uint32_t kUpdateThreadGroupSize = 256u;
	static constexpr size_t kCbAlignment = 256u;
	static constexpr uint32_t kParticleFlagFadeOut = 1u;

	struct ParticleCS {
		Vector3 translate;
		float rotation;
		Vector3 scale;
		float lifeTime;
		Vector3 velocity;
		float currentTime;
		Vector3 acceleration;
		float rotationSpeed;
		Vector4 color;
		float startScale;
		float endScale;
		float scaleEasing;
		float startAlpha;
		float spawnY;
		float maxRiseHeight;
		uint32_t flags;
		uint32_t padding;
	};
	static_assert(sizeof(ParticleCS) == 112, "ParticleCS size must match HLSL (112 bytes)");

	struct GpuEmitRequest {
		Vector3 origin;
		float randomPositionRange;
		Vector3 minVelocity;
		float minLifeTime;
		Vector3 maxVelocity;
		float maxLifeTime;
		Vector3 acceleration;
		float maxRiseHeight;
		Vector4 minColor;
		Vector4 maxColor;
		float minScale;
		float maxScale;
		float endScale;
		float scaleEasing;
		float minRotation;
		float maxRotation;
		float minRotationSpeed;
		float maxRotationSpeed;
		uint32_t startOffset;
		uint32_t count;
		uint32_t flags;
		uint32_t seedSalt;
	};
	static_assert(sizeof(GpuEmitRequest) == 144, "GpuEmitRequest size must match HLSL (144 bytes)");

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

	// 複数エミッタ対応キュー発生
	void EnqueueEmit(const Vector3& position, const ParticleParameters& params, uint32_t count);

	// 既存単一エミッタ互換API（DebugScene等向け）
	void SetEmitParams(const Vector3& position, uint32_t count, float radius, float frequency);
	void Emit();

	void ClearAllParticles();

	void SetReadbackDebugEnabled(bool enabled) { enableReadbackDebug_ = enabled; }
	bool IsReadbackDebugEnabled() const { return enableReadbackDebug_; }
	uint32_t GetDebugAliveCount() const { return debugInstanceCount_; }
	uint32_t GetMaxParticles() const { return kMaxParticles; }
	uint32_t GetLastFrameEmitCount() const { return lastFrameEmitCount_; }

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
	Microsoft::WRL::ComPtr<ID3D12Resource> aliveListBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indirectArgsBuffer_;

	uint32_t particleSrvIndex_ = 0;
	uint32_t aliveListSrvIndex_ = 0;

	uint32_t particleUavIndex_ = 0;
	uint32_t freeListIndexUavIndex_ = 0;
	uint32_t freeListUavIndex_ = 0;
	uint32_t aliveListUavIndex_ = 0;
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
	};
	static_assert(sizeof(PerFrame) == 16, "PerFrame size must match HLSL (16 bytes)");
	Microsoft::WRL::ComPtr<ID3D12Resource> perFrameBuffer_;
	uint8_t* perFrameMapped_ = nullptr;

	struct PerView {
		Matrix4x4 viewProjection;
		Matrix4x4 billboardMatrix;
	};
	static_assert(sizeof(PerView) == 128, "PerView size must match HLSL (128 bytes)");
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
	bool hasCustomTexture_ = false;
	float time_ = 0.0f;
	float deltaTime_ = 1.0f / 60.0f;

	std::vector<GpuEmitRequest> pendingEmitRequests_;
	uint32_t seedCounter_ = 1u;
	uint32_t lastFrameEmitCount_ = 0u;

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

	Vector3 guiEmitTranslate_ = { 0.0f, 0.0f, 0.0f };
	int guiEmitCount_ = 1024;
	float guiEmitRadius_ = 5.0f;
	float guiEmitFrequency_ = 0.0f;
};
