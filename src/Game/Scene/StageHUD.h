#pragma once
#include <memory>
#include <string>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Sprite/Sprite.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"

using namespace MyMath;

namespace StageHUDTimerDefaults {
	inline constexpr float kDefaultElapsedSeconds = 0.0f;
	inline constexpr int kSecondsPerMinute = 60;
}

// HUD表示用データ
struct StageHUDData {
	float remainingTime = 300.0f;
	float elapsedTime = StageHUDTimerDefaults::kDefaultElapsedSeconds;
	bool isElapsedTimerMode = false;
	int currentAmmo = 500;
	int maxAmmo = 500;
	bool isFiring = false;

	Vector3 aircraftPosition{};
	Vector3 aircraftForward{};
	Vector3 mouseAimTargetDirection{};
	bool mouseAimEnabled = true;

	Matrix4x4 viewProjectionMatrix{};
	float screenWidth = 1280.0f;
	float screenHeight = 720.0f;
};

// 戦闘ステージ用HUD
class StageHUD {
public:
	StageHUD() = default;
	~StageHUD() = default;

	// 初期化
	// spriteCommon: SpriteCommon
	void Initialize(SpriteCommon* spriteCommon);

	// 更新
	// dt: 経過時間
	// data: HUDデータ
	void Update(float dt, const StageHUDData& data);

	// 描画
	void Draw();

	// HUD展開リセット
	void ResetDeploy() { deployTimer_ = 0.0f; }

	// 警告カラー設定
	void SetWarningColorMode(bool warn) { isWarningMode_ = warn; }
	bool IsWarningColorMode() const { return isWarningMode_; }

	// シャットダウン開始
	void StartShutdown() { isShuttingDown_ = true; shutdownTimer_ = 0.0f; }
	void ResetShutdown() { isShuttingDown_ = false; shutdownTimer_ = 0.0f; }
	bool IsShutdownComplete() const { return isShuttingDown_ && shutdownTimer_ >= 1.2f; }

	// スクリーン座標投影
	static void ProjectToScreenClamped(
		const Vector3& worldPos,
		const Matrix4x4& vpMat,
		float screenW,
		float screenH,
		float marginPx,
		Vector2& outScreen,
		bool& outIsClamped);

private:
	float deployTimer_ = 0.0f;
	bool isWarningMode_ = false;
	bool isShuttingDown_ = false;
	float shutdownTimer_ = 0.0f;
	// --- 2D描画ヘルパー ---
	void DrawLine(const Vector2& p1, const Vector2& p2, float thickness, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);
	void DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& anchor = { 0.0f, 0.0f }, BlendMode blendMode = BlendMode::kNormal);
	void DrawBorderedPanel(const Vector2& pos, const Vector2& size, const Vector4& bgColor, const Vector4& borderColor, float borderWidth = 1.5f);
	void DrawRing(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);
	void DrawDot(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);

	// 3Dワールド座標から2Dスクリーン座標への投影
	bool ProjectToScreen(const Vector3& worldPos, const Matrix4x4& vpMat, float screenW, float screenH, Vector2& outScreen) const;

	// --- 各HUD要素の描画 ---
	void DrawTimer();
	void DrawBailoutGuide();
	void DrawReticles();
	void DrawLeadGuidanceLine(const Vector2& noseScreen, const Vector2& aimScreen, float alpha);
	void DrawMouseAimReticle(const Vector2& aimScreen, float reticleScale, const Vector4& baseAimCol, const Vector4& baseTickCol);
	void DrawBoresightReticle(const Vector2& noseScreen, float reticleScale, float alpha);

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// 再利用スプライト
	std::unique_ptr<Sprite> rectSprite_;
	std::unique_ptr<Sprite> ringSprite_;
	std::unique_ptr<Sprite> dotSprite_;

	// キャッシュデータ
	StageHUDData hudData_{};
	float animTimer_ = 0.0f;
	float firePunchTimer_ = 0.0f;
};
