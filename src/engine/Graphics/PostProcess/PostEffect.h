#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

class DirectXCommon;
class SrvManager;

/// ポストエフェクトの種類
enum class PostEffectType : uint32_t {
	kNone = 0,      // エフェクトなし
	kColorTint,     // カラーティント
	kVignette,      // ビネット
	kBoxFilter,     // ボックスフィルタ
	kGaussBlur,     // ガウスブラー
	kKawaseBlur,    // 川瀬式ブラー
	kRadialBlur,    // ラジアルブラー
	kDissolve,      // ディゾルブ
	kRandom,        // ランダム
	kScanLine,      // 走査線
	kLightAmp,      // 光量増幅 (Light Amp)
	kLensDistortion,// レンズ歪み (Lens Distortion)
	kChromaticAberration, // 色収差 (Chromatic Aberration)
	kBloom,         // ブルーム (Bloom)
	kColorIsolation,// カラーアイソレーション (Color Isolation / パートカラー)
	kLuminanceOutline, // 輝度ベースアウトライン (Luminance Outline)
	kDepthOutline,     // 深度ベースアウトライン (Depth Outline)
	kCountOfPostEffects, // エフェクトの種類
};

struct PostEffectParams {
	int32_t kernelSize;
	float intensity;
	float dirX;
	float dirY;
	float time;
	float colorR;
	float colorG;
	float colorB;
};

struct ActivePostEffect {
	PostEffectType type = PostEffectType::kNone;
	int32_t kernelSize = 3;
	float intensity = 1.0f;
	float dirX = 0.0f;
	float dirY = 0.0f;
	float dissolveThreshold = 0.5f;
	float dissolveEdgeWidth = 0.05f;
	int dissolveMaskIndex = 0;
	float colorR = 1.0f;
	float colorG = 1.0f;
	float colorB = 1.0f;
};

/// <summary>
/// フルスクリーンポストエフェクトを管理するシングルトンクラス
/// </summary>
class PostEffect {
public:
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static PostEffect* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	PostEffect() = default;

	/// <summary>
	/// 初期化（PSO 生成、SRV 登録）
	/// </summary>
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// レンダーテクスチャから swap chain への fullscreen 描画を実行する
	/// PostDraw 内で CopyResource の代わりに呼ぶ
	/// </summary>
	void Draw(ID3D12Resource* renderTextureResource, uint32_t renderTextureSrvIndex);

	/// 現在のエフェクトを設定
	void SetEffectType(PostEffectType type) { currentEffect_ = type; }

	/// 現在のエフェクトを取得
	PostEffectType GetEffectType() const { return currentEffect_; }

	void SetKernelSize(int32_t size) { kernelSize_ = size; }
	int32_t GetKernelSize() const { return kernelSize_; }

	void SetIntensity(float intensity) { intensity_ = intensity; }
	float GetIntensity() const { return intensity_; }

	void SetDirX(float dirX) { dirX_ = dirX; }
	float GetDirX() const { return dirX_; }

	void SetDirY(float dirY) { dirY_ = dirY; }
	float GetDirY() const { return dirY_; }

	void SetDissolveThreshold(float t) { dissolveThreshold_ = t; }
	float GetDissolveThreshold() const { return dissolveThreshold_; }

	void SetDissolveEdgeWidth(float w) { dissolveEdgeWidth_ = w; }
	float GetDissolveEdgeWidth() const { return dissolveEdgeWidth_; }

	void SetDissolveMaskIndex(int idx) { dissolveMaskIndex_ = idx; }
	int GetDissolveMaskIndex() const { return dissolveMaskIndex_; }
	int GetDissolveMaskCount() const { return static_cast<int>(dissolveMaskSrvIndices_.size()); }

	void SetColorR(float r) { colorR_ = r; }
	float GetColorR() const { return colorR_; }

	void SetColorG(float g) { colorG_ = g; }
	float GetColorG() const { return colorG_; }

	void SetColorB(float b) { colorB_ = b; }
	float GetColorB() const { return colorB_; }

	// 複数ポストエフェクトの制御用API
	std::vector<ActivePostEffect>& GetActiveEffects() { return activeEffects_; }
	const std::vector<ActivePostEffect>& GetActiveEffects() const { return activeEffects_; }
	void AddActiveEffect(const ActivePostEffect& effect) { activeEffects_.push_back(effect); }
	void ClearActiveEffects() { activeEffects_.clear(); }

	// プリセット保存・読込用API
	void ApplyBuiltInPreset(const std::string& presetName);
	bool SavePreset(const std::string& presetName);
	bool LoadPreset(const std::string& presetName);

	~PostEffect() = default;

private:
	PostEffect(const PostEffect&) = delete;
	PostEffect& operator=(const PostEffect&) = delete;

	static std::unique_ptr<PostEffect> instance_;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// ルートシグネチャ（全エフェクト共通）
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// エフェクトごとの PSO
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates_[static_cast<size_t>(PostEffectType::kCountOfPostEffects)];

	// 現在のエフェクト種類
	PostEffectType currentEffect_ = PostEffectType::kNone;

	std::vector<ActivePostEffect> activeEffects_;

	Microsoft::WRL::ComPtr<ID3D12Resource> postEffectParamsBuffer_;
	PostEffectParams* mappedPostEffectParams_ = nullptr;
	int32_t kernelSize_ = 3;
	float intensity_ = 1.0f;
	float dirX_ = 0.0f;
	float dirY_ = 0.0f;
	float colorR_ = 1.0f;
	float colorG_ = 1.0f;
	float colorB_ = 1.0f;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> dissolveRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> dissolvePSO_;
	std::vector<uint32_t> dissolveMaskSrvIndices_;
	float dissolveThreshold_ = 0.5f;
	float dissolveEdgeWidth_ = 0.05f;
	int dissolveMaskIndex_ = 0;

	float time_ = 0.0f;

	void DrawSinglePass(ID3D12GraphicsCommandList* commandList, const ActivePostEffect& effect, uint32_t srcIndex, uint32_t dstIndex, bool isOutputToBackBuffer, uint32_t passIndex, float customDirX = -999999.0f, float customDirY = -999999.0f, float customIntensity = -1.0f);

	void CreateRootSignature();
	void CreateDissolveRootSignature();
	void CreateGraphicsPipelines();
	void LoadDissolveMasks();
};
