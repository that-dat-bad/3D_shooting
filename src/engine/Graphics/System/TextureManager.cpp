#include "TextureManager.h"
#include "DirectXCommon.h"
#include <cassert>
#include <filesystem> 
#include "SrvManager.h"
#include <memory>

std::unique_ptr<TextureManager> TextureManager::instance_ = nullptr;


std::wstring ConvertString(const std::string& str) {
	if (str.empty()) { return std::wstring(); }
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

TextureManager* TextureManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique<TextureManager>();
	}
	return instance_.get();
}

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	textureDatas_.reserve(SrvManager::kMaxSRVCount_);
	metadataByIndex_.fill(nullptr);
}

void TextureManager::Finalize() {
	textureDatas_.clear();
	instance_.reset();
}

void TextureManager::LoadTexture(const std::string& filePath) {
	HRESULT hr;


	if (textureDatas_.contains(filePath)) {
		return;
	}

	assert(srvManager_->CanAllocate());

	DirectX::ScratchImage image;
	std::wstring wFilePath = ConvertString(filePath);

	DirectX::TexMetadata fileMetadata;
	hr = DirectX::GetMetadataFromDDSFile(wFilePath.c_str(), DirectX::DDS_FLAGS_NONE, fileMetadata);

	if (wFilePath.ends_with(L".dds")) {//ddsで終わっていたらddsとする(要改修)
		hr = DirectX::LoadFromDDSFile(wFilePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
	} else {
		hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	}

	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipImages;
	const DirectX::TexMetadata& metadata = image.GetMetadata();
	// 1x1テクスチャなど小さすぎる場合はミップマップ生成をスキップ
	if (DirectX::IsCompressed(metadata.format) || (metadata.width <= 1 && metadata.height <= 1)) {
		mipImages = std::move(image);
	} else {
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), metadata, DirectX::TEX_FILTER_SRGB, 0, mipImages);
		assert(SUCCEEDED(hr));
	}


	// --- テクスチャデータを追加 ---
	// 追加した要素の参照を取得する
	TextureData& textureData = textureDatas_[filePath];


	// ---  テクスチャデータ書き込み ---
	textureData.filePath = filePath;
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(dxCommon_->GetDevice(), textureData.metadata);

	// ---テクスチャデータ転送 ---
	textureData.intermediateResource = dxCommon_->UploadTextureData(textureData.resource.Get(), mipImages);

	//-- アップロードとバリアを完了
	dxCommon_->FlushTextureUploads();


	// ---  デスクリプタハンドルの計算 ---
	textureData.srvIndex = srvManager_->Allocate();

	//ハンドルを取得して保存
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	if (textureData.metadata.IsCubemap()) {
		srvManager_->CreateSRVforTextureCube(
			textureData.srvIndex,
			textureData.resource.Get(),
			textureData.metadata.format,
			UINT(textureData.metadata.mipLevels)
		);
	} else {
		srvManager_->CreateSRVforTexture2D(
			textureData.srvIndex,
			textureData.resource.Get(),
			textureData.metadata.format,
			UINT(textureData.metadata.mipLevels)
		);
	}
}

void TextureManager::LoadTextureFromMemory(const std::string& textureName, const void* data, size_t size) {
	HRESULT hr;

	// 既に読み込み済みならスキップ
	if (textureDatas_.contains(textureName)) {
		return;
	}

	assert(srvManager_->CanAllocate());

	DirectX::ScratchImage image;

	
	hr = DirectX::LoadFromWICMemory(data, size, DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipImages;
	const DirectX::TexMetadata& memMetadata = image.GetMetadata();
	// 1x1テクスチャなど小さすぎる場合はミップマップ生成をスキップ
	if (DirectX::IsCompressed(memMetadata.format) || (memMetadata.width <= 1 && memMetadata.height <= 1)) {
		mipImages = std::move(image);
	} else {
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), memMetadata, DirectX::TEX_FILTER_SRGB, 0, mipImages);
		assert(SUCCEEDED(hr));
	}

	// --- テクスチャデータを追加 ---
	TextureData& textureData = textureDatas_[textureName];

	// --- テクスチャデータ書き込み ---
	textureData.filePath = textureName; // filePathという変数名ですが、ここでは識別子として使います
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(dxCommon_->GetDevice(), textureData.metadata);

	// --- テクスチャデータ転送 ---
	textureData.intermediateResource = dxCommon_->UploadTextureData(textureData.resource.Get(), mipImages);

	// --- デスクリプタハンドルの計算 ---
	textureData.srvIndex = srvManager_->Allocate();

	// ハンドルを取得して保存
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	srvManager_->CreateSRVforTexture2D(
		textureData.srvIndex,
		textureData.resource.Get(),
		textureData.metadata.format,
		UINT(textureData.metadata.mipLevels)
	);
}

void TextureManager::LoadTextureFromRawPixels(const std::string& textureName, uint32_t width, uint32_t height, DXGI_FORMAT format, const void* pixels) {
	// 既に読み込み済みならスキップ
	if (textureDatas_.contains(textureName)) {
		return;
	}

	assert(srvManager_->CanAllocate());

	DirectX::Image img;
	img.width = width;
	img.height = height;
	img.format = format;
	img.rowPitch = (width * DirectX::BitsPerPixel(format)) / 8;
	img.slicePitch = img.rowPitch * height;
	img.pixels = (uint8_t*)pixels;

	DirectX::ScratchImage scratchImage;
	HRESULT hr = scratchImage.InitializeFromImage(img);
	assert(SUCCEEDED(hr));

	// --- テクスチャデータを追加 ---
	TextureData& textureData = textureDatas_[textureName];

	// --- テクスチャデータ書き込み ---
	textureData.filePath = textureName;
	textureData.metadata = scratchImage.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(dxCommon_->GetDevice(), textureData.metadata);

	// --- テクスチャデータ転送 ---
	textureData.intermediateResource = dxCommon_->UploadTextureData(textureData.resource.Get(), scratchImage);

	// --- デスクリプタハンドルの計算 ---
	textureData.srvIndex = srvManager_->Allocate();
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	srvManager_->CreateSRVforTexture2D(
		textureData.srvIndex,
		textureData.resource.Get(),
		textureData.metadata.format,
		1 // mipLevels = 1 for raw pixels
	);
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureIndex) {
	return srvManager_->GetGPUDescriptorHandle(textureIndex);
}

const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath) {
	if (textureDatas_.contains(filePath)) {
		return textureDatas_[filePath].metadata;
	}

	assert(false && "Texture not found.");
	static DirectX::TexMetadata dummy{};
	return dummy;
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath) {
	if (textureDatas_.contains(filePath)) {
		return textureDatas_[filePath].srvIndex;
	}

	assert(false && "Texture not found.");
	return 0;
}

const DirectX::TexMetadata& TextureManager::GetMetaData(uint32_t textureIndex) {
	if (textureIndex < metadataByIndex_.size() && metadataByIndex_[textureIndex]) {
		return *metadataByIndex_[textureIndex];
	}

	for (const auto& [key, data] : textureDatas_) {
		if (data.srvIndex == textureIndex) {
			if (textureIndex < metadataByIndex_.size()) {
				metadataByIndex_[textureIndex] = &data.metadata;
			}
			return data.metadata;
		}
	}

	static DirectX::TexMetadata dummy{};
	return dummy;
}

void TextureManager::CreateRenderTexture(const std::string& name, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor) {
	// 既に作成済みならスキップ
	if (textureDatas_.contains(name)) {
		return;
	}

	assert(srvManager_->CanAllocate());
	assert(rtvNextIndex_ < DirectXCommon::kRtvHeapDescriptorNum_ && "RTV heap is full");

	// レンダーテクスチャリソースの作成
	TextureData& textureData = textureDatas_[name];
	textureData.filePath = name;
	textureData.isRenderTexture = true;
	textureData.resource = dxCommon_->CreateRenderTextureResource(dxCommon_->GetDevice(), width, height, format, clearColor);

	// メタデータを手動設定
	textureData.metadata.width = width;
	textureData.metadata.height = height;
	textureData.metadata.format = format;
	textureData.metadata.mipLevels = 1;
	textureData.metadata.arraySize = 1;
	textureData.metadata.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

	// --- RTV の作成 ---
	uint32_t rtvDescriptorSize = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = DirectXCommon::GetCPUDescriptorHandle(
		dxCommon_->GetRTVDescriptorHeap(), rtvDescriptorSize, rtvNextIndex_);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	dxCommon_->GetDevice()->CreateRenderTargetView(textureData.resource.Get(), &rtvDesc, rtvHandle);

	textureData.rtvHandle = rtvHandle;
	rtvNextIndex_++;

	// --- SRV の作成 ---
	textureData.srvIndex = srvManager_->Allocate();
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);
	srvManager_->CreateSRVforTexture2D(
		textureData.srvIndex,
		textureData.resource.Get(),
		format,
		1
	);
}

D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::GetRtvHandle(const std::string& name) {
	assert(textureDatas_.contains(name) && "Render texture not found.");
	assert(textureDatas_[name].isRenderTexture && "Not a render texture.");
	return textureDatas_[name].rtvHandle;
}

ID3D12Resource* TextureManager::GetResource(const std::string& name) {
	assert(textureDatas_.contains(name) && "Texture not found.");
	return textureDatas_[name].resource.Get();
}
