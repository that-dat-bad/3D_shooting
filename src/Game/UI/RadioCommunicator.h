#pragma once
#include <deque>
#include <string>
#include <vector>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/base/Math/Vector2.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/UI/TypewriterEffect.h"

// 無線通信の発信元属性
enum class RadioFaction {
	Ally,   // 味方機・僚機
	Awacs,  // 管制・AWACS・司令部
	Enemy,  // 敵機
	System  // システム警告
};

inline const char* RadioFactionToString(RadioFaction faction) {
	switch (faction) {
	case RadioFaction::Ally:   return "Ally";
	case RadioFaction::Awacs:  return "Awacs";
	case RadioFaction::Enemy:  return "Enemy";
	case RadioFaction::System: return "System";
	default:                   return "Ally";
	}
}

inline RadioFaction StringToRadioFaction(const std::string& text) {
	if (text == "Awacs")  return RadioFaction::Awacs;
	if (text == "Enemy")  return RadioFaction::Enemy;
	if (text == "System") return RadioFaction::System;
	return RadioFaction::Ally;
}

// 無線字幕の定数
namespace RadioDefaults {
	// --- 基本インデックス・文字数定数 ---
	inline constexpr size_t kInitialIndex = 0;
	inline constexpr size_t kIndexStep = 1;
	inline constexpr size_t kEnclosingPairCharCount = 2;
	inline constexpr size_t kDoubleBracketCharCount = 2;
	inline constexpr size_t kDoubleBracketPairMinLength = 4;

	// --- 基本正規化比率・補間係数 ---
	inline constexpr float kZero = 0.0f;
	inline constexpr float kHalf = 0.5f;
	inline constexpr float kUnit = 1.0f;
	inline constexpr float kEpsilon = 0.0001f;
	inline constexpr float kSmoothStepFactorA = 3.0f;
	inline constexpr float kSmoothStepFactorB = 2.0f;

	// --- 基準解像度とスケーリング制限 ---
	inline constexpr float kReferenceScreenWidth = 1280.0f;
	inline constexpr float kReferenceScreenHeight = 720.0f;
	inline constexpr float kMinResolutionScale = 0.65f;
	inline constexpr float kMaxResolutionScale = 1.60f;

	// --- フローティング字幕レイアウト定数（ウィンドウ枠なし・画面上部中央） ---
	inline constexpr float kCenterAnchorRatioX = 0.5f;
	inline constexpr float kTopAnchorRatioY = 0.145f;
	inline constexpr float kSpeakerFontSize = 18.0f;      // 話者名フォントサイズ
	inline constexpr float kDialogueFontSize = 20.5f;     // セリフフォントサイズ
	inline constexpr float kSpeakerToDialogueGap = 6.0f;  // 間隔
	inline constexpr float kDialogueLineGap = 5.0f;       // 行間

	// --- 雲・青空背景でもくっきり読める2層ソフトドロップシャドウ定数 ---
	inline constexpr float kInnerShadowOffsetX = 1.2f;
	inline constexpr float kInnerShadowOffsetY = 1.5f;
	inline constexpr float kOuterShadowOffsetX = 2.2f;
	inline constexpr float kOuterShadowOffsetY = 2.8f;
	inline constexpr MyMath::Vector4 kInnerShadowColor = { 0.02f, 0.04f, 0.08f, 0.62f };
	inline constexpr MyMath::Vector4 kOuterShadowColor = { 0.02f, 0.04f, 0.08f, 0.34f };

	// --- タイミング・フェード定数 ---
	inline constexpr float kDefaultHoldSeconds = 3.00f;
	inline constexpr float kMinHoldSeconds = 0.50f;
	inline constexpr float kOpenDurationSeconds = 0.18f;
	inline constexpr float kCloseDurationSeconds = 0.24f;
	inline constexpr float kInterruptCloseSeconds = 0.08f;
	inline constexpr float kDefaultCharIntervalSeconds = 0.030f;
	inline constexpr bool kDefaultEnableTypewriter = false; // AC7準拠: デフォルトはウィンドウ無し一括フェード表示

	// --- 文字列フォーマット・フォント定数 ---
	inline constexpr char kSingleOpenAngleChar = '<';
	inline constexpr char kSingleCloseAngleChar = '>';
	inline constexpr char kDoubleQuoteChar = '"';
	inline constexpr char kSpaceChar = ' ';
	inline constexpr char kNewlineChar = '\n';
	inline constexpr char kCarriageReturnChar = '\r';

	inline constexpr const char* kDialogueOpenDelimiter = "<< ";
	inline constexpr const char* kDialogueCloseDelimiter = " >>";
	inline constexpr const char* kDoubleOpenAngle = "<<";
	inline constexpr const char* kDoubleCloseAngle = ">>";
	inline constexpr const char* kDefaultSpeakerName = "UNKNOWN";
	inline constexpr const char* kEmptyCursor = "";
	inline constexpr const char* kFontName = "HackGen";

	// --- AC7準拠 カラーパレット定数（話者名と << >> が同色、本文がホワイト） ---
	inline constexpr MyMath::Vector4 kDialogueBodyColor = { 0.97f, 0.98f, 1.00f, 1.00f };

	// 味方 (Ally): AC7 "Joker 2 Coster" 準拠のペールミントグリーン
	inline constexpr MyMath::Vector4 kAllySpeakerColor = { 0.62f, 0.90f, 0.65f, 1.00f };

	// 管制 (Awacs): AC7 管制官準拠のペールスカイシアン
	inline constexpr MyMath::Vector4 kAwacsSpeakerColor = { 0.68f, 0.93f, 1.00f, 1.00f };

	// 敵 (Enemy): AC7 敵エース準拠のソフトコーラルレッド
	inline constexpr MyMath::Vector4 kEnemySpeakerColor = { 1.00f, 0.56f, 0.52f, 1.00f };

	// システム (System): 戦術通知用のペールアンバー
	inline constexpr MyMath::Vector4 kSystemSpeakerColor = { 1.00f, 0.90f, 0.58f, 1.00f };
}

// 無線会話データ
struct RadioMessage {
	std::string speakerName;
	std::string dialogueText;
	RadioFaction faction = RadioFaction::Ally;
	float holdDuration = RadioDefaults::kDefaultHoldSeconds;
	float charInterval = RadioDefaults::kDefaultCharIntervalSeconds;
	bool interrupt = false;
};

// 無線会話UIのレイアウト設定
struct RadioLayoutConfig {
	float topAnchorRatioY = RadioDefaults::kTopAnchorRatioY;
	float speakerFontSize = RadioDefaults::kSpeakerFontSize;
	float dialogueFontSize = RadioDefaults::kDialogueFontSize;
	float speakerToDialogueGap = RadioDefaults::kSpeakerToDialogueGap;
	float dialogueLineGap = RadioDefaults::kDialogueLineGap;
};

// 無線会話UIのタイミング設定
struct RadioTimingConfig {
	float defaultHoldSeconds = RadioDefaults::kDefaultHoldSeconds;
	float openDurationSeconds = RadioDefaults::kOpenDurationSeconds;
	float closeDurationSeconds = RadioDefaults::kCloseDurationSeconds;
	float interruptCloseSeconds = RadioDefaults::kInterruptCloseSeconds;
	float defaultCharIntervalSeconds = RadioDefaults::kDefaultCharIntervalSeconds;
	bool enableTypewriter = RadioDefaults::kDefaultEnableTypewriter;
};

// 無線字幕管理クラス
class RadioCommunicator {
public:
	RadioCommunicator() = default;
	~RadioCommunicator() = default;

	// 初期化
	void Initialize(SpriteCommon* spriteCommon = nullptr);

	// 更新
	void Update(float deltaTime);

	// 描画
	void Draw(float screenWidth, float screenHeight);

	// メッセージ追加
	void Enqueue(
		const std::string& speakerName,
		const std::string& dialogueText,
		RadioFaction faction = RadioFaction::Ally,
		float holdDuration = RadioDefaults::kDefaultHoldSeconds);

	// 構造体指定でメッセージを追加
	void Enqueue(const RadioMessage& message);

	// 割り込みメッセージの追加
	void Interrupt(
		const std::string& speakerName,
		const std::string& dialogueText,
		RadioFaction faction = RadioFaction::Awacs,
		float holdDuration = RadioDefaults::kDefaultHoldSeconds);

	// スキップ
	void SkipCurrent();

	// キューのクリア
	void Clear();

	// 通信中判定
	bool IsActive() const;

	size_t GetQueuedCount() const { return messageQueue_.size(); }

	void SetLayoutConfig(const RadioLayoutConfig& config) { layoutConfig_ = config; }
	const RadioLayoutConfig& GetLayoutConfig() const { return layoutConfig_; }

	void SetTimingConfig(const RadioTimingConfig& config) { timingConfig_ = config; }
	const RadioTimingConfig& GetTimingConfig() const { return timingConfig_; }

	// 話者名の整形
	static std::string SanitizeSpeakerName(const std::string& rawSpeaker);

	// セリフ本文の整形
	static std::string SanitizeDialogueBody(const std::string& rawDialogue);

private:
	enum class Phase {
		Idle,
		Opening,
		Typing,
		Hold,
		Closing
	};

	void StartNextMessage();
	static MyMath::Vector4 ResolveSpeakerColor(RadioFaction faction);
	static float ComputeSmoothStep(float progress);
	static std::vector<std::string> SplitLines(const std::string& text);
	static std::string TrimSpaces(const std::string& text);

	static void PrintFloatingText(
		const std::string& fontName,
		const std::string& text,
		float x,
		float y,
		float fontSize,
		float scale,
		const MyMath::Vector4& textColor,
		const Vector2& anchorPoint);

private:
	SpriteCommon* spriteCommon_ = nullptr;

	RadioLayoutConfig layoutConfig_{};
	RadioTimingConfig timingConfig_{};

	std::deque<RadioMessage> messageQueue_;
	RadioMessage currentMessage_{};
	std::string cleanSpeakerName_;
	std::string cleanDialogueBody_;
	std::vector<std::string> dialogueLines_;

	TypewriterEffect typewriter_;
	Phase phase_ = Phase::Idle;
	float phaseTimer_ = RadioDefaults::kZero;
	float activeCloseDuration_ = RadioDefaults::kCloseDurationSeconds;
};
