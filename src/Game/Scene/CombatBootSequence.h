#pragma once
#include <memory>
#include <vector>
#include <string>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Sprite/Sprite.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Audio/AudioManager.h"

using namespace MyMath;

/// @brief AC6（アーマード・コア6）スタイルの「メインシステム、戦闘モード起動...」演出シーケンス管理クラス
class CombatBootSequence {
public:
	CombatBootSequence() = default;
	~CombatBootSequence();

	/// @brief 初期化
	/// @param spriteCommon SpriteCommonポインタ
	void Initialize(SpriteCommon* spriteCommon);

	/// @brief 演出開始（音声再生とタイマーリセット）
	void Start();

	/// @brief 毎フレーム更新
	/// @param dt 経過時間
	void Update(float dt);

	/// @brief 背景の黒フェード描画（3Dシーンの直後に描画）
	void DrawBackgroundFade();

	/// @brief AC6風ブートUI描画（最前面に描画）
	void DrawBootUI();

	/// @brief 演出が完了したか
	bool IsFinished() const { return timer_ >= kDuration; }

	/// @brief 演出の総時間（秒）
	static constexpr float GetDuration() { return kDuration; }

	/// @brief 現在の演出タイマー取得
	float GetTimer() const { return timer_; }

	/// @brief 黒フェードの現在のアルファ値（0.0f〜1.0f）
	float GetFadeAlpha() const { return fadeAlpha_; }

	/// @brief 演出終了処理
	void Finalize();

private:
	// --- 2D描画ヘルパー ---
	void DrawLine(const Vector2& p1, const Vector2& p2, float thickness, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);
	void DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& anchor = { 0.0f, 0.0f }, BlendMode blendMode = BlendMode::kNormal);
	void DrawTacticalBracket(const Vector2& center, const Vector2& size, float bracketLen, float thickness, const Vector4& color);
	void DrawCircleRing(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// スプライト群
	std::unique_ptr<Sprite> fadeSprite_;    ///< 全画面黒フェード用
	std::unique_ptr<Sprite> rectSprite_;    ///< ライン・矩形描画用
	std::unique_ptr<Sprite> ringSprite_;    ///< 円形リング描画用

	// サウンドデータ
	SoundData voiceData_{};
	bool voicePlayed_ = false;

	// タイマー管理
	float timer_ = 0.0f;
	static constexpr float kDuration = 4.80f; ///< 演出の総時間（秒: 暗転タメ＋タイピング＋シームレス移行）
	float fadeAlpha_ = 1.0f;
};
