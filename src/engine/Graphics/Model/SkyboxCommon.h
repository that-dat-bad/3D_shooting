#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <array>
#include "../System/BlendMode.h"
#include <memory>

class DirectXCommon;
class SrvManager;

/// <summary>
/// スカイボックス描画の共通設定管理クラス
/// </summary>
class SkyboxCommon
{
public:
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static SkyboxCommon* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	SkyboxCommon() = default;

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Finalize();

	// 共通描画設定
	void SetupCommonState();

	DirectXCommon* GetDirectXCommon() { return dxCommon_; }
	SrvManager* GetSrvManager() { return srvManager_; }

	~SkyboxCommon() = default;
private:
	SkyboxCommon(const SkyboxCommon&) = delete;
	SkyboxCommon& operator=(const SkyboxCommon&) = delete;

	static std::unique_ptr<SkyboxCommon> instance_;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// ルートシグネチャの作成
	void CreateRootSignature(DirectXCommon* dxCommon);

	// グラフィックパイプラインの生成
	void CreateGraphicsPipeline(DirectXCommon* dxCommon);
};
