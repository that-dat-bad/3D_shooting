#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class DirectXCommon;
class SrvManager;

/// <summary>
/// 3Dモデルの共通設定やリソースを管理するクラス
/// </summary>
class ModelCommon
{
public:
	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="dxCommon">DirectX共通クラスのポインタ</param>
	/// <param name="srvManager">SrvManagerのポインタ</param>
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/// <summary>
	/// DirectX共通クラスのポインタを取得
	/// </summary>
	/// <returns>DirectX共通クラスポインタ</returns>
	DirectXCommon* GetDirectXCommon() { return dxCommon_; }
	SrvManager* GetSrvManager() { return srvManager_; }

	ID3D12RootSignature* GetSkinningRootSignature() const { return skinningRootSignature_.Get(); }
	ID3D12PipelineState* GetSkinningPipelineState() const { return skinningPipelineState_.Get(); }

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningPipelineState_;

	void CreateSkinningPipeline();
};


