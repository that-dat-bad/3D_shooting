#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include"../../engine/base/WinApp.h"

class SrvManager;
#include <array>
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

#include "../../../external/DirectXTex/DirectXTex.h"
#include<string>
#include<chrono>

#include <memory>
#include <vector>
#include "../base/Math/MyMath.h"
using namespace MyMath;

/// <summary>
/// DirectX12基盤管理クラス
/// </summary>
class DirectXCommon
{
public:
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static DirectXCommon* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	DirectXCommon() = default;

	static const uint32_t kRtvHeapDescriptorNum_ = 5; // ダブルバッファ(2) + レンダーテクスチャ用(3)
	static const uint32_t kDsvHeapDescriptorNum_ = 2; // 深度バッファ用(R/W と ReadOnly)
	static const uint32_t kSwapChainBufferCount_ = 2;


	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="winApp">Windowsアプリケーションへのポインタ</param>
	void Initialize(WinApp* winApp);
	
	/// <summary>終了処理</summary>
	void Finalize();

	/// <summary>全てのGPU処理の完了を待つ</summary>
	void WaitForGPU();

	/// <summary>描画前処理</summary>
	void PreDraw();

	/// <summary>描画後処理（ポストエフェクト適用、バックバッファをRTのまま残す）</summary>
	void PostDraw();

	/// <summary>フレーム終了処理（バックバッファをPRESENTに戻してsubmit）</summary>
	void EndFrame();

	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const wchar_t* profile);




	//--ゲッター--
	ID3D12Device* GetDevice() { return device_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() { return commandList_.Get(); }
	ID3D12CommandQueue* GetCommandQueue() { return commandQueue_.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() { return commandAllocators_[currentFrameIndex_].Get(); }
	IDXGISwapChain4* GetSwapChain() { return swapChain_.Get(); }
	ID3D12Resource* GetCurrentBackBuffer() {
		return swapChainResources_[swapChain_->GetCurrentBackBufferIndex()].Get();
	}
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTVHandle() {
		return rtvHandles_[swapChain_->GetCurrentBackBufferIndex()];
	}
	ID3D12DescriptorHeap* GetRTVDescriptorHeap() { return rtvDescriptorHeap_.Get(); }
	ID3D12DescriptorHeap* GetDSVDescriptorHeap() { return dsvDescriptorHeap_.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() { return GetCPUDescriptorHandle(dsvDescriptorHeap_.Get(), dsvDescriptorSize_, 0); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetReadOnlyDSVHandle() { return GetCPUDescriptorHandle(dsvDescriptorHeap_.Get(), dsvDescriptorSize_, 1); }
	ID3D12Resource* GetDepthStencilBuffer() { return depthStencilResource_.Get(); }
	IDxcUtils* GetDxcUtils() { return dxcUtils_.Get(); }
	IDxcCompiler3* GetDxcCompiler() { return dxcCompiler_.Get(); }
	IDxcIncludeHandler* GetIncludeHandler() { return includeHandler_.Get(); }
	DXGI_FORMAT GetRTVFormat() { return rtvFormat_; }
	UINT64 GetFenceValue() { return fenceValue_; }
	HANDLE GetFenceEvent() { return fenceEvent_; }
	ID3D12Fence* GetFence() { return fence_.Get(); }
	D3D12_RECT GetScissorRect() { return scissorRect_; }
	D3D12_VIEWPORT GetViewport() { return viewport_; }
	ID3D12Resource* GetRenderTexture(uint32_t index = 0) { return renderTextures_[index].Get(); }
	uint32_t GetRenderTextureSrvIndex(uint32_t index = 0) const { return index == 0 ? renderTextureSrvIndex_ : renderTextureSrvIndex1_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTextureRTVHandle(uint32_t index = 0) const { return renderTextureRtvHandles_[index]; }

	/// renderTextures_[0] の SRV を SrvManager に登録する
	void SetupRenderTextureSRV(SrvManager* srvManager);

	/// depthStencilResource_ の SRV を SrvManager に登録する
	void SetupDepthTextureSRV(SrvManager* srvManager);
	uint32_t GetDepthSrvIndex() const { return depthSrvIndex_; }

	//セッター
	void IncrementFenceValue() { fenceValue_++; }





	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateUAVBufferResource(size_t sizeInBytes);
	void UAVBarrier(ID3D12Resource* pResource);
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(ID3D12Device* device, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);
	// バッチアップロード: 溜めたテクスチャ転送コマンドを一括実行し、GPU完了を1回だけ待つ
	void FlushTextureUploads();

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	~DirectXCommon() = default;

private:
	DirectXCommon(const DirectXCommon&) = delete;
	DirectXCommon& operator=(const DirectXCommon&) = delete;
	static std::unique_ptr<DirectXCommon> instance_;


	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> device_ = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
	// フレームバッファリング: フレーム数分のコマンドアロケーター
	std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, kSwapChainBufferCount_> commandAllocators_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;

	// デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;


	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

	// デスクリプタサイズ
	uint32_t rtvDescriptorSize_;
	uint32_t dsvDescriptorSize_;

	D3D12_RECT scissorRect_{};

	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;


	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_;
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 2> rtvHandles_;

	static const uint32_t kRenderTextureCount_ = kRtvHeapDescriptorNum_ - kSwapChainBufferCount_;
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kRenderTextureCount_> renderTextures_;
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kRenderTextureCount_> renderTextureRtvHandles_;
	uint32_t renderTextureSrvIndex_ = 0; // renderTextures_[0] の SRV インデックス
	uint32_t renderTextureSrvIndex1_ = 0; // renderTextures_[1] の SRV インデックス
	uint32_t depthSrvIndex_ = 0; // 深度バッファのSRVインデックス

	DXGI_FORMAT rtvFormat_;

	HANDLE fenceEvent_ = nullptr;
	UINT64 fenceValue_ = 0;

	// フレームバッファリング: フレームごとのフェンス値と現在のフレームインデックス
	std::array<UINT64, kSwapChainBufferCount_> frameFenceValues_ = {};
	uint32_t currentFrameIndex_ = 0;

	D3D12_VIEWPORT viewport_{};


	//記録時間(FPS固定用	)
	std::chrono::steady_clock::time_point reference_;

	WinApp* winApp_ = nullptr;

	// テクスチャバッチアップロード用
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> uploadAllocator_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> uploadCommandList_;
	bool uploadRecording_ = false; // アップロードコマンドを記録中かどうか
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> pendingIntermediateResources_; // 転送完了まで保持

	//デバイスの初期化
	void CreateDevice();

	//コマンド関連の初期化
	void CreateCommand();

	//スワップチェーンの初期化
	void CreateSwapChain();

	//深度バッファの生成
	void CreateDepthStencilBuffer();

	//深度ステンシルビューの生成
	void CreateDepthStencilView();

	//各種デスクリプタヒープの生成
	void CreateDescriptorHeaps();

	//レンダーターゲットビューの初期化
	void CreateRenderTargetView();

	//レンダーテクスチャの生成
	void CreateRenderTargetTextures();

	//フェンスの作成
	void CreateFence();

	//ビューポート矩形の初期化
	void SetViewport();

	//シザー矩形の初期化
	void SetScissorRect();

	void InitializeDxcCompiler();

	void InitializeFixFPS();

	void UpdateFixFPS();
};
