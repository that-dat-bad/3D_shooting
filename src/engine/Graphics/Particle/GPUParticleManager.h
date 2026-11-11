#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <memory>

#include "../base/Math/MyMath.h"
#include "../System/DirectXCommon.h"
#include "../System/SrvManager.h"
#include "../System/BlendMode.h"

/// <summary>
/// GPU駆動型パーティクルシステム管理クラス (Compute Shader利用)
/// </summary>
class GPUParticleManager {
public:
	struct ParticleCS {
		Vector3 translate;
		float padding1;
		Vector3 scale;
		float lifeTime;
		Vector3 velocity;
		float currentTime;
		Vector4 color;
	};

	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static GPUParticleManager* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	GPUParticleManager() = default;

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Finalize();

	void Update();
	void Draw(const Matrix4x4& viewProjection, const Matrix4x4& billboardMatrix);

	void SetTexture(uint32_t srvIndex) { textureSrvIndex_ = srvIndex; }
	
	void SetEmitParams(const Vector3& position, uint32_t count, float radius, float frequency);
	
	// 強制的に発生させるフラグ（毎フレーム呼び出すと発生し続ける）
	void Emit();

	void DrawImGui();

public:
	~GPUParticleManager() = default;

private:
	GPUParticleManager(const GPUParticleManager&) = delete;
	GPUParticleManager& operator=(const GPUParticleManager&) = delete;

	static std::unique_ptr<GPUParticleManager> instance_;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> initPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePipelineState_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> graphicsRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature_;

	static const uint32_t kMaxParticles = 1024;
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

	Microsoft::WRL::ComPtr<ID3D12Resource> emitterSphereBuffer_[2];
	struct EmitterSphere {
		Vector3 translate;
		float radius;
		uint32_t count;
		float frequency;
		float frequencyTime;
		uint32_t emit;
	};
	EmitterSphere* emitterSphereData_[2] = { nullptr, nullptr };
	uint32_t currentBufferIndex_ = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> perFrameBuffer_;
	struct PerFrame {
		float time;
		float deltaTime;
	};
	PerFrame* perFrameData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> perViewBuffer_;
	struct PerView {
		Matrix4x4 viewProjection;
		Matrix4x4 billboardMatrix;
	};
	PerView* perViewData_ = nullptr;

	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	uint32_t textureSrvIndex_ = 0;
	float time_ = 0.0f;

	void CreateComputePipeline();
	void CreateGraphicsPipeline();
	void CreateCommandSignature();
	void CreateBuffers();
	void CreateModel();
	void InitializeParticles();

	bool manualEmitRequested_ = false;

	Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer_;
	int32_t debugFreeListIndex_ = 0;
	uint32_t debugInstanceCount_ = 0;

	Vector3 guiEmitTranslate_ = { 0.0f, 0.0f, 0.0f };
	int guiEmitCount_ = 1024;
	float guiEmitRadius_ = 5.0f;
	float guiEmitFrequency_ = 0.0f;
};
