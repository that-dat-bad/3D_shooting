#pragma once
#include <string>
#include <algorithm>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/base/Math/Vector2.h"
#include "../../engine/base/Math/Vector4.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/UI/UIText.h"

// フライト計器情報UIの定数定義
namespace InfoUIDefaults {
	// 基準解像度とスケーリング制限
	inline constexpr float kReferenceScreenWidth = 1280.0f;
	inline constexpr float kReferenceScreenHeight = 720.0f;
	inline constexpr float kMinResolutionScale = 0.70f;
	inline constexpr float kMaxResolutionScale = 1.60f;

	// 左上配置レイアウト定数
	inline constexpr float kMarginLeft = 32.0f;          // 画面左端からのマージン
	inline constexpr float kMarginTop = 32.0f;           // 画面上端からのマージン
	inline constexpr float kFontSize = 18.5f;            // 計器フォントサイズ (px)
	inline constexpr float kLineSpacing = 24.0f;         // 通常行間 (px)
	inline constexpr float kSectionGap = 10.0f;          // 機関砲行の前の区切りギャップ (px)
	inline constexpr float kValueColumnOffset = 104.0f;  // ラベル列から数値列へのオフセット (px)

	// 会話UIと完全同一の2層ソフトドロップシャドウ定数
	inline constexpr Vector2 kInnerShadowOffset = { 1.6f, 2.0f };
	inline constexpr Vector2 kOuterShadowOffset = { 3.0f, 3.6f };
	inline constexpr MyMath::Vector4 kInnerShadowColor = { 0.01f, 0.02f, 0.05f, 0.92f };
	inline constexpr MyMath::Vector4 kOuterShadowColor = { 0.01f, 0.02f, 0.05f, 0.68f };

	// テキストカラー定数
	inline constexpr MyMath::Vector4 kTextColor = { 0.97f, 0.98f, 1.00f, 1.00f };        // WT準拠 ホワイト
	inline constexpr MyMath::Vector4 kWepColor = { 1.00f, 0.45f, 0.20f, 1.00f };          // WEP時のオレンジ
	inline constexpr MyMath::Vector4 kLowAmmoColor = { 1.00f, 0.35f, 0.35f, 1.00f };      // 弾数警告時のレッド

	inline constexpr const char* kFontName = "HackGen";

	// 単位換算定数
	inline constexpr float kMpsToKmh = 3.6f;             // m/s -> km/h 変換係数
	inline constexpr float kThrottlePercentScale = 100.0f;
	inline constexpr float kLowAmmoThresholdRatio = 0.15f; // 残弾15%以下で警告色
}

// InfoUI更新用データ構造体
struct FlightInfoUIData {
	float throttle = 1.0f;           // スロットル値
	float speed = 0.0f;              // 飛行速度 (m/s)
	int currentAmmo = 0;             // 現在の残弾数
	int maxAmmo = 500;               // 最大装填数
	int currentMissiles = 0;         // 現在のミサイル残弾数
	int maxMissiles = 4;             // ミサイル最大搭載数
	bool isMissileSelected = false;  // ミサイル選択中フラグ
	int missileSeekerState = 0;      // シーカー状態 (0:Off..4:Locked)
	float absoluteAltitude = 0.0f;   // 絶対高度
	float relativeAltitude = 0.0f;   // 相対高度
	float screenWidth = 1280.0f;     // 画面解像度幅
	float screenHeight = 720.0f;    // 画面解像度高
	float alpha = 1.0f;              // UI全体の不透明度
};

// War Thunder風 戦闘フライト計器情報UI
class InfoUI {
public:
	InfoUI() = default;
	~InfoUI() = default;

	// 初期化処理
	// spriteCommon: SpriteCommonポインタ
	void Initialize(SpriteCommon* spriteCommon);

	// 計器データの更新
	// dt: 経過時間（秒）
	// data: フライトデータ
	void Update(float dt, const FlightInfoUIData& data);

	// 2Dテキスト描画処理
	void Draw();

	// 表示・非表示の切り替え
	void SetVisible(bool visible);
	bool IsVisible() const { return isVisible_; }

private:
	// 各UIText要素に共通の会話UI風2層シャドウとアンカーを設定するヘルパー
	void SetupTextElement(UIText& textElem, const std::string& initialText);

private:
	FlightInfoUIData data_{};
	bool isVisible_ = true;
	SpriteCommon* spriteCommon_ = nullptr;

	// --- 既存の文字UIクラスインスタンス群 ---
	// 1. エンジン出力
	UIText labelThrottle_;
	UIText valThrottle_;

	// 2. 速度
	UIText labelSpeed_;
	UIText valSpeed_;

	// 3. 絶対高度
	UIText labelAlt_;
	UIText valAlt_;

	// 4. 相対高度
	UIText labelRalt_;
	UIText valRalt_;

	// 5. 機関砲弾数
	UIText labelAmmo_;
	UIText valAmmo_;

	// 6. ミサイル残弾数
	UIText labelMissile_;
	UIText valMissile_;
};
