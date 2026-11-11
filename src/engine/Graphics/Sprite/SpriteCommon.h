#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include "../System/BlendMode.h"
class DirectXCommon;

#include <memory> 

/// <summary>
/// スプライト描画の共通設定管理クラス
/// </summary>
class SpriteCommon
{
public:
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static SpriteCommon* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	SpriteCommon() = default;

    void Initialize(DirectXCommon* dxCommon);
    void Finalize();

	//共通描画設定
	void SetupCommonState();

	// ブレンドモード設定
	void SetBlendMode(BlendMode mode);

	DirectXCommon* GetDirectXCommon() { return dxCommon_; }

private:
	static std::unique_ptr<SpriteCommon> instance;
	SpriteCommon(const SpriteCommon&) = delete;
	SpriteCommon& operator=(const SpriteCommon&) = delete;
	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, static_cast<size_t>(BlendMode::kCountOf)> graphicsPipelineStates_;
	
	//ルートシグネチャの作成
	void CreateRootSignature(DirectXCommon* dxCommon);

	//グラフィックパイプラインの生成
	void CreateGraphicsPipeline(DirectXCommon* dxCommon);
};

