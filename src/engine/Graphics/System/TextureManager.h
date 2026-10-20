#pragma once
#include <string>
#include <array>
#include "../../../external/DirectXTex/DirectXTex.h"
#include <wrl/client.h>
#include <d3d12.h>
#include<unordered_map>
#include "../base/Math/MyMath.h"
using namespace MyMath;


class SrvManager;
class DirectXCommon;

#include <memory> 

/// <summary>
/// テクスチャ管理クラス
/// </summary>
class TextureManager
{
public:
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static TextureManager* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	TextureManager() = default;

	/// <summary>初期化処理</summary>
	void Initialize(DirectXCommon* dxCommon,SrvManager* srvManager);

	/// <summary>終了処理</summary>
	void Finalize();

	/// <summary>
	/// テクスチャ読み込み
	/// </summary>
	/// <param name="filePath">ファイルパス</param>
	void LoadTexture(const std::string& filePath);

	/// <summary>メモリからテクスチャ読み込み</summary>
	void LoadTextureFromMemory(const std::string& textureName, const void* data, size_t size);

	/// <summary>生ピクセルデータからテクスチャ読み込み</summary>
	void LoadTextureFromRawPixels(const std::string& textureName, uint32_t width, uint32_t height, DXGI_FORMAT format, const void* pixels);

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex);

	// メタデータを取得
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

	uint32_t GetTextureIndexByFilePath(const std::string& filePath);

	const DirectX::TexMetadata& GetMetaData(uint32_t textureIndex);

	// レンダーテクスチャを作成（RTV + SRV を両方登録）
	void CreateRenderTexture(const std::string& name, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

	// レンダーテクスチャの RTV ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(const std::string& name);

	// リソースを取得（レンダーテクスチャのバリア遷移などに使用）
	ID3D12Resource* GetResource(const std::string& name);

	~TextureManager() = default;

private:

	static std::unique_ptr<TextureManager> instance_;
	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	struct TextureData
	{
		std::string filePath;
		DirectX::TexMetadata metadata{};
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
		uint32_t srvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU{};
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU{};
		// レンダーテクスチャ用
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
		bool isRenderTexture = false;
	};
	std::unordered_map<std::string,TextureData> textureDatas_;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// レンダーテクスチャ用 RTV インデックス（スワップチェーン2枚の次から）
	uint32_t rtvNextIndex_ = 2;

	// O(1) 高速アクセスメタデータキャッシュ（512枚対応、線形探索を完全根絶）
	std::array<const DirectX::TexMetadata*, 512> metadataByIndex_{};
};
