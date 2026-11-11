#pragma once
#include <cstdint>
#include <wrl/client.h>
#include <d3d12.h>
class DirectXCommon;

/// <summary>
/// SRV(シェーダーリソースビュー)やUAVのデスクリプタヒープを管理するクラス
/// </summary>
class SrvManager {
public:
	/// <summary>最大SRV数(最大テクスチャ枚数)</summary>
	static const uint32_t kMaxSRVCount_ = 2048;

	/// <summary>初期化処理</summary>
	void Initialize(DirectXCommon* dxCommon);
	void Finalize();
	void PreDraw();

	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);
	void SetComputeRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

	/// <summary>デスクリプタヒープの空きインデックスを1つ割り当てる</summary>
	uint32_t Allocate();

	//テクスチャ枚数上限チェック
	bool CanAllocate() const;

	uint32_t descriptorSize_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
	void CreateSRVforTextureCube(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
	void CreateUAVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
private:
	DirectXCommon* dxCommon_ = nullptr;
	//次に使用するSRVインデックス
	uint32_t useIndex_ = 0;
};

