#pragma once
#include <string>
#include <algorithm>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/base/Math/Vector2.h"
#include "../../engine/base/Math/Vector4.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/UI/UIText.h"

/// @brief War Thunder風 フライト計器情報UIの全定数定義（マジックナンバー排除）
namespace InfoUIDefaults {
	// 基準解像度とスケーリング制限
	inline constexpr float kReferenceScreenWidth = 1280.0f;
	inline constexpr float kReferenceScreenHeight = 720.0f;
	inline constexpr float kMinResolutionScale = 0.70f;
	inline constexpr float kMaxResolutionScale = 1.60f;

	// 左上配置レイアウト定数
	inline constexpr float kMarginLeft = 32.0f;          ///< 画面左端からのマージン
	inline constexpr float kMarginTop = 32.0f;           ///< 画面上端からのマージン
	inline constexpr float kFontSize = 16.0f;            ///< 計器フォントサイズ (px)
	inline constexpr float kLineSpacing = 21.0f;         ///< 通常行間 (px)
	inline constexpr float kSectionGap = 10.0f;          ///< 機関砲行の前の区切りギャップ (px)
	inline constexpr float kValueColumnOffset = 88.0f;   ///< ラベル列から数値列へのオフセット (px)

	// 会話UI（RadioCommunicator）と完全同一の2層ソフトドロップシャドウ定数
	inline constexpr Vector2 kInnerShadowOffset = { 1.2f, 1.5f };
	inline constexpr Vector2 kOuterShadowOffset = { 2.2f, 2.8f };
	inline constexpr MyMath::Vector4 kInnerShadowColor = { 0.02f, 0.04f, 0.08f, 0.62f };
	inline constexpr MyMath::Vector4 kOuterShadowColor = { 0.02f, 0.04f, 0.08f, 0.34f };

	// テキストカラー定数
	inline constexpr MyMath::Vector4 kTextColor = { 0.97f, 0.98f, 1.00f, 1.00f };        ///< WT準拠 ホワイト
	inline constexpr MyMath::Vector4 kWepColor = { 1.00f, 0.45f, 0.20f, 1.00f };          ///< WEP時（>100%）のオレンジ
	inline constexpr MyMath::Vector4 kLowAmmoColor = { 1.00f, 0.35f, 0.35f, 1.00f };      ///< 弾数警告時のレッド

	inline constexpr const char* kFontName = "HackGen";

	// 単位換算定数
	inline constexpr float kMpsToKmh = 3.6f;             ///< m/s -> km/h 変換係数
	inline constexpr float kThrottlePercentScale = 100.0f;
	inline constexpr float kLowAmmoThresholdRatio = 0.15f; ///< 残弾15%以下で警告色
}

/// @brief InfoUI更新用データ構造体
struct FlightInfoUIData {
	float throttle = 1.0f;           ///< スロットル値（0.0f〜1.0f、ブースト時は > 1.0f）
	float speed = 0.0f;              ///< 飛行速度 (m/s)
	int currentAmmo = 0;             ///< 現在の残弾数
	int maxAmmo = 500;               ///< 最大装填数
	float absoluteAltitude = 0.0f;   ///< 絶対高度（海抜高度 MSL, m）
	float relativeAltitude = 0.0f;   ///< 相対高度（対地高度 AGL / RALT, m）
	float screenWidth = 1280.0f;     ///< 画面解像度幅
	float screenHeight = 720.0f;    ///< 画面解像度高
	float alpha = 1.0f;              ///< UI全体の不透明度
};

/// @brief War Thunder風 戦闘フライト計器情報UI（既存UITextクラスを流用した独立コンポーネント）
class InfoUI {
public:
	InfoUI() = default;
	~InfoUI() = default;

	/// @brief 初期化処理（各UITextのフォント・装飾設定）
	/// @param spriteCommon SpriteCommonポインタ
	void Initialize(SpriteCommon* spriteCommon);

	/// @brief 計器データの更新
	/// @param dt 経過時間（秒）
	/// @param data フライトデータ
	void Update(float dt, const FlightInfoUIData& data);

	/// @brief 2Dテキスト描画処理（全UITextの描画）
	void Draw();

	/// @brief 表示・非表示の切り替え
	void SetVisible(bool visible);
	bool IsVisible() const { return isVisible_; }

private:
	/// @brief 各UIText要素に共通の会話UI風2層シャドウとアンカーを設定するヘルパー
	void SetupTextElement(UIText& textElem, const std::string& initialText);

private:
	FlightInfoUIData data_{};
	bool isVisible_ = true;
	SpriteCommon* spriteCommon_ = nullptr;

	// --- 既存の文字UIクラス（UIText）インスタンス群 ---
	// 1. エンジン出力 (THR)
	UIText labelThrottle_;
	UIText valThrottle_;

	// 2. 速度 (SPD)
	UIText labelSpeed_;
	UIText valSpeed_;

	// 3. 絶対高度 (ALT)
	UIText labelAlt_;
	UIText valAlt_;

	// 4. 相対高度 (RALT)
	UIText labelRalt_;
	UIText valRalt_;

	// 5. 機関砲弾数 (AMMO)
	UIText labelAmmo_;
	UIText valAmmo_;
};
