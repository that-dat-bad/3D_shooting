#include "DirectXCommon.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#include <cassert>
#include"../base/StringUtility.h"
#include"../base/Logger.h"
#include <format>
#include<thread>
#include "../../../external/DirectXTex/DirectXTex.h"
#include "../../../external/DirectXTex/d3dx12.h"
#include "SrvManager.h"
#include "PostProcess/PostEffect.h"
#include <filesystem>
#include <fstream>

using namespace logger;
using namespace StringUtility;

std::unique_ptr<DirectXCommon> DirectXCommon::instance_ = nullptr;

DirectXCommon* DirectXCommon::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique<DirectXCommon>();
	}
	return instance_.get();
}

void DirectXCommon::Finalize() {
	if (instance_) {
		instance_->WaitForGPU();
		if (instance_->fenceEvent_) {
			CloseHandle(instance_->fenceEvent_);
			instance_->fenceEvent_ = nullptr;
		}
		instance_.reset();
	}
}

void DirectXCommon::WaitForGPU() {
	fenceValue_++;
	commandQueue_->Signal(fence_.Get(), fenceValue_);
	if (fence_->GetCompletedValue() < fenceValue_) {
		fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}
}

void DirectXCommon::Initialize(WinApp* winApp) {

	InitializeFixFPS();

	assert(winApp);
	//メンバ変数にセット
	winApp_ = winApp;

	//デバイスの生成
	CreateDevice();
	//コマンド関連の生成
	CreateCommand();
	//スワップチェインの生成
	CreateSwapChain();
	//深度バッファの生成
	CreateDepthStencilBuffer();
	//デスクリプタヒープの生成
	CreateDescriptorHeaps();
	//レンダーターゲットビューの生成
	CreateRenderTargetView();
	//深度バッファの生成
	CreateDepthStencilView();
	//フェンスの生成
	CreateFence();
	//ビューポートの設定
	SetViewport();
	//シザー矩形の設定
	SetScissorRect();
	InitializeDxcCompiler();
	
	CreateRenderTargetTextures();
}

void DirectXCommon::PreDraw()
{
	// 描画開始前に、溜まっているテクスチャアップロードを一括で実行・完了させる
	FlushTextureUploads();

	HRESULT hr;

	// 現在のフレームのアロケーターがGPUで使い終わるのを待つ
	// （ダブルバッファなので、2フレーム前のGPU完了を待つことになる）
	if (fence_->GetCompletedValue() < frameFenceValues_[currentFrameIndex_]) {
		fence_->SetEventOnCompletion(frameFenceValues_[currentFrameIndex_], fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

	hr = commandAllocators_[currentFrameIndex_]->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList_->Reset(commandAllocators_[currentFrameIndex_].Get(), nullptr);
	assert(SUCCEEDED(hr));

	D3D12_RESOURCE_BARRIER barrierRT = {};
	barrierRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierRT.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrierRT.Transition.pResource = renderTextures_[0].Get();
	barrierRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrierRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList_->ResourceBarrier(1, &barrierRT);

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetDSVHandle();
	commandList_->OMSetRenderTargets(1, &renderTextureRtvHandles_[0], false, &dsvHandle);

	float clearColor[] = { 0.02f, 0.02f, 0.03f, 1.0f };
	commandList_->ClearRenderTargetView(renderTextureRtvHandles_[0], clearColor, 0, nullptr);
	commandList_->ClearDepthStencilView(GetDSVHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);

}

void DirectXCommon::PostDraw()
{
	UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER barrierRTtoSRV = {};
	barrierRTtoSRV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierRTtoSRV.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrierRTtoSRV.Transition.pResource = renderTextures_[0].Get();
	barrierRTtoSRV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierRTtoSRV.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrierRTtoSRV.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	D3D12_RESOURCE_BARRIER barrierBackToRT = {};
	barrierBackToRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierBackToRT.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrierBackToRT.Transition.pResource = swapChainResources_[backBufferIndex].Get();
	barrierBackToRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierBackToRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrierBackToRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	D3D12_RESOURCE_BARRIER barriers[] = { barrierRTtoSRV, barrierBackToRT };
	commandList_->ResourceBarrier(_countof(barriers), barriers);

	commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex], false, nullptr);

	// ポストエフェクトのフルスクリーン描画
	PostEffect::GetInstance()->Draw(renderTextures_[0].Get(), renderTextureSrvIndex_);

	// バックバッファは RENDER_TARGET のまま — ImGui がここに描画できる
}

void DirectXCommon::EndFrame()
{
	UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER barrierBackToPresent = {};
	barrierBackToPresent.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierBackToPresent.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrierBackToPresent.Transition.pResource = swapChainResources_[backBufferIndex].Get();
	barrierBackToPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierBackToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrierBackToPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList_->ResourceBarrier(1, &barrierBackToPresent);

	HRESULT hr = commandList_->Close();
	assert(SUCCEEDED(hr));
	ID3D12CommandList* commandLists[] = { commandList_.Get() };
	commandQueue_->ExecuteCommandLists(1, commandLists);
	swapChain_->Present(1, 0);

	// 既存のフェンス処理
	fenceValue_++;
	frameFenceValues_[currentFrameIndex_] = fenceValue_;
	commandQueue_->Signal(fence_.Get(), fenceValue_);

	currentFrameIndex_ = (currentFrameIndex_ + 1) % kSwapChainBufferCount_;

	UpdateFixFPS();
}

void DirectXCommon::CreateDevice()
{
	HRESULT hr;

	//デバッグレイヤー
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	dxgiFactory_ = nullptr;
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
	assert(SUCCEEDED(hr));

	// アダプタの選別
	useAdapter_ = nullptr;
	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter_)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter_->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Log(ConvertString(std::format(L"Use Adapater:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter_ = nullptr;
	}
	assert(useAdapter_ != nullptr);

	device_ = nullptr;
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter_.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
		if (SUCCEEDED(hr)) {
			Log(std::format("FeatureLevel: {}\n", featureLevelStrings[i]));
			break;
		}
	}
	assert(device_ != nullptr);
	Log("Complete create D3D12Device!!!\n");
}

void DirectXCommon::CreateCommand()
{
	HRESULT hr;

	// フレーム数分のコマンドアロケーターを生成
	for (uint32_t i = 0; i < kSwapChainBufferCount_; ++i) {
		hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators_[i]));
		assert(SUCCEEDED(hr));
	}

	// コマンドリストの生成（最初のアロケーターで作成）
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators_[0].Get(), nullptr, IID_PPV_ARGS(&commandList_));
	assert(SUCCEEDED(hr));

	// コマンドキューの生成
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
	assert(SUCCEEDED(hr));
	commandList_->Close();

	// テクスチャバッチアップロード用のコマンドアロケーター/リストを生成
	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAllocator_));
	assert(SUCCEEDED(hr));
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAllocator_.Get(), nullptr, IID_PPV_ARGS(&uploadCommandList_));
	assert(SUCCEEDED(hr));
	uploadCommandList_->Close();
	uploadRecording_ = false;

}

void DirectXCommon::CreateSwapChain()
{
	// スワップチェーンの生成の設定
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = winApp_->kClientWidth;
	swapChainDesc.Height = winApp_->kClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain;
	HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), winApp_->GetHwnd(), &swapChainDesc, nullptr, nullptr, &tempSwapChain);
	assert(SUCCEEDED(hr));
	hr = tempSwapChain.As(&swapChain_);
	assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateDepthStencilBuffer()
{
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = winApp_->kClientWidth;
	resourceDesc.Height = winApp_->kClientHeight;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&depthStencilResource_)
	);
	assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateDepthStencilView()
{

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, GetDSVHandle());

	dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
	device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, GetReadOnlyDSVHandle());
}

void DirectXCommon::CreateDescriptorHeaps()
{
	//デスクリプタサイズを取得
	rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	dsvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);



	//ディスクリプタヒープの生成
	rtvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kRtvHeapDescriptorNum_, false);

	dsvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, kDsvHeapDescriptorNum_, false);
}

void DirectXCommon::CreateRenderTargetView()
{
	HRESULT hr;
	rtvHandles_;
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvFormat_ = rtvDesc.Format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	for (UINT i = 0; i < swapChainResources_.size(); i++) {
		hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
		assert(SUCCEEDED(hr));
		rtvHandles_[i] = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV), i);
		device_->CreateRenderTargetView(swapChainResources_[i].Get(), &rtvDesc, rtvHandles_[i]);
	}
}


void DirectXCommon::CreateFence()
{
	HRESULT hr;
	// フェンスの生成
	fence_ = nullptr;
	hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	assert(SUCCEEDED(hr));
	fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent_ != nullptr);
}

void DirectXCommon::SetViewport()
{

	viewport_.Width = winApp_->kClientWidth; viewport_.Height = winApp_->kClientHeight;
	viewport_.TopLeftX = 0; viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f; viewport_.MaxDepth = 1.0f;
}

void DirectXCommon::SetScissorRect()
{
	scissorRect_.left = 0; scissorRect_.right = winApp_->kClientWidth;
	scissorRect_.top = 0; scissorRect_.bottom = winApp_->kClientHeight;
}

void DirectXCommon::InitializeDxcCompiler()
{
	HRESULT hr;

	dxcUtils_ = nullptr;
	dxcCompiler_ = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr));
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr));

}

void DirectXCommon::InitializeFixFPS()
{
	reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::UpdateFixFPS()
{
	//1/60秒ぴったり
	const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));

	//1/60秒よりわずかに短い時間
	const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

	//現在時間を取得
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	//前回記録からの経過時間を取得する
	std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

	//1//60(よりわずかに短い時間)経っていない場合
	if (elapsed<kMinCheckTime)
	{
		//1/60経過するまで微小なスリープを繰り返す
		while(std::chrono::steady_clock::now()-reference_<kMinTime)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
	reference_ = std::chrono::steady_clock::now();
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateUAVBufferResource(size_t sizeInBytes) {
	// 頂点バッファやインデックスバッファのインスタンス生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	// リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // Defaultヒープ（GPU上）

	// リソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // バッファ
	resourceDesc.Width = sizeInBytes;                         // リソースのサイズ
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// バッファリソースを作る
	HRESULT hr = device_->CreateCommittedResource(
		&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	if (FAILED(hr)) { return nullptr; }

	return resource;
}

void DirectXCommon::UAVBarrier(ID3D12Resource* pResource) {
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.UAV.pResource = pResource;
	commandList_->ResourceBarrier(1, &barrier);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}


Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filePath, const wchar_t* profile) {
	std::filesystem::path hlslPath = filePath;
	std::filesystem::path csoPath = hlslPath.parent_path() / "compiled";
	if (!std::filesystem::exists(csoPath)) {
		std::filesystem::create_directories(csoPath);
	}
	csoPath /= hlslPath.filename();
	csoPath.replace_extension(".cso");

	bool needsCompile = true;
	if (std::filesystem::exists(csoPath)) {
		auto hlslTime = std::filesystem::last_write_time(hlslPath);
		auto csoTime = std::filesystem::last_write_time(csoPath);
		if (csoTime >= hlslTime) {
			needsCompile = false;
		}
	}

	if (!needsCompile) {
		Log(ConvertString(std::format(L"Load Cached Shader, Path :{}\n", csoPath.wstring())));
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderBlob = nullptr;
		HRESULT hr = dxcUtils_->LoadFile(csoPath.c_str(), nullptr, &shaderBlob);
		if (SUCCEEDED(hr)) {
			return shaderBlob;
		}
	}

	Log(ConvertString(std::format(L"Begin CompileShader, Path :{}, profile : {}\n", filePath, profile)));
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
	HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));
	DxcBuffer shaderSourceBuffer{};
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E", L"main",
		L"-T", profile,
		L"-Zi",L"-Qembed_debug",
		L"-Od",
		L"-Zpr",
	};
	Microsoft::WRL::ComPtr<IDxcResult> shaderResult = nullptr;
	hr = dxcCompiler_->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler_.Get(),
		IID_PPV_ARGS(&shaderResult)
	);
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(shaderError->GetStringPointer());
		assert(false);
	}

	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));

	std::ofstream out(csoPath, std::ios::binary);
	if (out.is_open()) {
		out.write(reinterpret_cast<const char*>(shaderBlob->GetBufferPointer()), shaderBlob->GetBufferSize());
	}

	Log(ConvertString(std::format(L"Compile Succeeded, Path :{}, profile : {}\n", filePath, profile)));
	return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* bufferResource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&bufferResource)
	);
	assert(SUCCEEDED(hr));
	return bufferResource;
}


Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}


Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::TexMetadata metadata = mipImages.GetMetadata();
	const DirectX::Image* images = mipImages.GetImages();
	subresources.resize(metadata.mipLevels * metadata.arraySize);

	for (size_t i = 0; i < subresources.size(); ++i) {
		subresources[i].pData = images[i].pixels;
		subresources[i].RowPitch = (LONG_PTR)images[i].rowPitch;
		subresources[i].SlicePitch = (LONG_PTR)images[i].slicePitch;
	}

	// 必要なサイズを計算
	UINT64 intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));

	// 中間リソースの作成 (アップロード用バッファ)
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(intermediateSize);

	// アップロード用コマンドリストがまだ開かれていなければ開く
	if (!uploadRecording_) {
		HRESULT hr;
		hr = uploadAllocator_->Reset();
		assert(SUCCEEDED(hr));
		hr = uploadCommandList_->Reset(uploadAllocator_.Get(), nullptr);
		assert(SUCCEEDED(hr));
		uploadRecording_ = true;
	}

	// データ転送命令を積む（実行はまだしない）
	UpdateSubresources(uploadCommandList_.Get(), texture, intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

	// バリアの設定（コピー先 -> 読み取り専用）
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter =  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	uploadCommandList_->ResourceBarrier(1, &barrier);

	// 中間リソースを転送完了まで保持（リストに追加）
	pendingIntermediateResources_.push_back(intermediateResource);

	// 中間リソースを返す（呼び出し元でも保持）
	return intermediateResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateRenderTextureResource(ID3D12Device* device, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor)
{
	// リソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;  // RTV として使用可能

	// ヒープ設定（GPU専用メモリ）
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// クリアカラーの最適値を設定
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = format;
	clearValue.Color[0] = clearColor.x;
	clearValue.Color[1] = clearColor.y;
	clearValue.Color[2] = clearColor.z;
	clearValue.Color[3] = clearColor.w;

	// リソースの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,  // 初期状態は SRV として読み取り可能
		&clearValue,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}


void DirectXCommon::FlushTextureUploads()
{
	// アップロードコマンドが記録されていなければ何もしない
	if (!uploadRecording_) {
		return;
	}

	// コマンドリストを閉じる
	HRESULT hr = uploadCommandList_->Close();
	assert(SUCCEEDED(hr));

	// コマンドリストを実行
	ID3D12CommandList* commandLists[] = { uploadCommandList_.Get() };
	commandQueue_->ExecuteCommandLists(1, commandLists);

	WaitForGPU();

	// 中間リソースを解放
	pendingIntermediateResources_.clear();

	// 記録フラグをリセット
	uploadRecording_ = false;
}

//レンダーテクスチャを生成して RTV を作成
void DirectXCommon::CreateRenderTargetTextures()
{
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = rtvFormat_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    Vector4 defaultClear = { 0.02f, 0.02f, 0.03f, 1.0f };
    uint32_t descriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (uint32_t i = 0; i < kRenderTextureCount_; ++i) {
        // リソース作成（初期状態は SRV 相当）
        renderTextures_[i] = CreateRenderTextureResource(device_.Get(), winApp_->kClientWidth, winApp_->kClientHeight, rtvFormat_, defaultClear);

        uint32_t descriptorIndex = kSwapChainBufferCount_ + i;
        renderTextureRtvHandles_[i] = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), descriptorSize, descriptorIndex);

        device_->CreateRenderTargetView(renderTextures_[i].Get(), &rtvDesc, renderTextureRtvHandles_[i]);
    }
}

void DirectXCommon::SetupRenderTextureSRV(SrvManager* srvManager) {
	renderTextureSrvIndex_ = srvManager->Allocate();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = rtvFormat_;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	device_->CreateShaderResourceView(
		renderTextures_[0].Get(),
		&srvDesc,
		srvManager->GetCPUDescriptorHandle(renderTextureSrvIndex_)
	);

	renderTextureSrvIndex1_ = srvManager->Allocate();
	device_->CreateShaderResourceView(
		renderTextures_[1].Get(),
		&srvDesc,
		srvManager->GetCPUDescriptorHandle(renderTextureSrvIndex1_)
	);
}

void DirectXCommon::SetupDepthTextureSRV(SrvManager* srvManager) {
	// 深度バッファのSRVを登録
	depthSrvIndex_ = srvManager->Allocate();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	device_->CreateShaderResourceView(
		depthStencilResource_.Get(),
		&srvDesc,
		srvManager->GetCPUDescriptorHandle(depthSrvIndex_)
	);
}
