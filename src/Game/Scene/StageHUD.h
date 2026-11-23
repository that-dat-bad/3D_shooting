#pragma once
#include <memory>
#include <string>
#include <vector>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Sprite/Sprite.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"

using namespace MyMath;

namespace StageHUDTimerDefaults {
	inline constexpr float kDefaultElapsedSeconds = 0.0f;
	inline constexpr int kSecondsPerMinute = 60;
}

// 統一ターゲットマーカーデータ
struct StageHUDTargetMarker {
	Vector3 worldPos{};
	Vector3 velocity{};
	std::string label;
	Vector4 color{ 1.0f, 0.28f, 0.26f, 0.98f };
	bool showLeadSight = false;
};

// システムテレメトリログ表示データ
struct StageHUDSysLogEntry {
	std::string category = "SYS";
	std::string timestampText;
	std::string message;
	float ageSeconds = 0.0f;
};

// 下部操作・進行ガイドパネルデータ
struct StageHUDGuideData {
	bool visible = false;
	std::string title;
	std::string sub;
	bool showProgressBar = false;
	float progressRatio = 0.0f;
};

// ターゲット部位構造テレメトリデータ
struct StageHUDStructuralTelemetry {
	bool visible = false;
	float portWingRatio = 1.0f;
	float stbdWingRatio = 1.0f;
	float engineRatio = 1.0f;
	int brokenPartIndex = 0; // 0: None, 1: PortWing, 2: StbdWing, 3: EngineCore
};

// HUD表示用データ
struct StageHUDData {
	float remainingTime = 300.0f;
	float elapsedTime = StageHUDTimerDefaults::kDefaultElapsedSeconds;
	bool isElapsedTimerMode = false;
	int currentAmmo = 500;
	int maxAmmo = 500;
	bool isFiring = false;

	Vector3 aircraftPosition{};
	Vector3 aircraftVelocity{};
	Vector3 aircraftForward{};
	Vector3 mouseAimTargetDirection{};
	bool mouseAimEnabled = true;
	bool showReticles = true;

	Matrix4x4 viewProjectionMatrix{};
	float screenWidth = 1280.0f;
	float screenHeight = 720.0f;

	std::vector<StageHUDTargetMarker> targetMarkers;
	std::vector<StageHUDSysLogEntry> systemLogs;
	StageHUDGuideData guide{};
	StageHUDStructuralTelemetry structuralTelemetry{};
	bool showPullUpAlert = false;
	bool showHighAltCaution = false;
	bool showRwrAlert = false;
	float hitMarkerTimer = 0.0f;
	float fadeOutAlpha = 0.0f;
	bool isClearDemoMode = false;
	float clearCountdownSec = -1.0f;
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

	// 最前面フェードアウトオーバーレイ描画
	void DrawFadeOverlay();

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

	// 偏差射撃予測位置計算
	static Vector3 ComputeLeadInterceptPoint(
		const Vector3& shooterPos,
		const Vector3& shooterVel,
		const Vector3& targetPos,
		const Vector3& targetVel,
		float muzzleSpeed = 500.0f,
		float muzzleForwardOffset = 8.0f,
		float maxLeadTime = 2.80f);

	// 画面外クランプ時のシェブロン矢印頂点＆内側距離テキスト座標を計算
	static void ComputeOffscreenChevronGeometry(
		const Vector2& screenPt,
		float screenW,
		float screenH,
		float boxHalf,
		float hudScale,
		Vector2& outTip,
		Vector2& outWingA,
		Vector2& outWingB,
		Vector2& outTextPos);

private:
	float deployTimer_ = 0.0f;
	bool isWarningMode_ = false;
	bool isShuttingDown_ = false;
	float shutdownTimer_ = 0.0f;
	// --- 2D描画ヘルパー ---
	Sprite* AcquireRectSprite();
	Sprite* AcquireRingSprite();
	Sprite* AcquireDotSprite();
	void DrawLine(const Vector2& p1, const Vector2& p2, float thickness, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);
	void DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& anchor = { 0.0f, 0.0f }, BlendMode blendMode = BlendMode::kNormal);
	void DrawBorderedPanel(const Vector2& pos, const Vector2& size, const Vector4& bgColor, const Vector4& borderColor, float borderWidth = 1.5f);
	void DrawTargetBrackets(const Vector2& center, float halfSize, float cornerLen, float thickness, const Vector4& color);
	void DrawRing(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);
	void DrawDot(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);

	// 3Dワールド座標から2Dスクリーン座標への投影
	bool ProjectToScreen(const Vector3& worldPos, const Matrix4x4& vpMat, float screenW, float screenH, Vector2& outScreen) const;

	// --- 各HUD要素の描画 ---
	void DrawTimer();
	void DrawBailoutGuide();
	void DrawClearDemoCountdown();
	void DrawTargetMarkers();
	void DrawSystemLogs();
	void DrawGuidePanel();
	void DrawTargetTelemetry();
	void DrawAlerts();
	void DrawReticles();
	void DrawLeadGuidanceLine(const Vector2& noseScreen, const Vector2& aimScreen, float alpha);
	void DrawMouseAimReticle(const Vector2& aimScreen, float reticleScale, const Vector4& baseAimCol, const Vector4& baseTickCol);
	void DrawBoresightReticle(const Vector2& noseScreen, float reticleScale, float alpha);

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// DX12 安全複数描画用スプライトプール
	std::vector<std::unique_ptr<Sprite>> rectSpritePool_;
	size_t rectSpritePoolIndex_ = 0;
	std::vector<std::unique_ptr<Sprite>> ringSpritePool_;
	size_t ringSpritePoolIndex_ = 0;
	std::vector<std::unique_ptr<Sprite>> dotSpritePool_;
	size_t dotSpritePoolIndex_ = 0;

	// キャッシュデータ
	StageHUDData hudData_{};
	float animTimer_ = 0.0f;
	float firePunchTimer_ = 0.0f;
};
