#define NOMINMAX
#include "StageHUD.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace {
	constexpr const char* kWhiteTexPath = "assets/textures/white1x1.png";
	constexpr const char* kRingTexPath = "assets/textures/circle2.png";
	constexpr const char* kDotTexPath = "assets/textures/circle.png";
	constexpr size_t kInitialRectPoolCount = 384;
	constexpr size_t kInitialRingPoolCount = 16;
	constexpr size_t kInitialDotPoolCount = 16;

	void PrintShadowedMarkerText(
		const std::string& text,
		float x,
		float y,
		float fontSize,
		const Vector4& color,
		const Vector2& anchor = { 0.0f, 0.0f })
	{
		TextRenderer* tr = TextRenderer::GetInstance();
		if (!tr || text.empty()) {
			return;
		}
		constexpr float kShadowOffset = 1.8f;
		constexpr float kShadowAlpha = 0.90f;
		const Vector4 shadowCol = { 0.0f, 0.0f, 0.0f, color.w * kShadowAlpha };
		tr->Print("HackGen", text, x + kShadowOffset, y + kShadowOffset, fontSize, shadowCol, anchor);
		tr->Print("HackGen", text, x, y, fontSize, color, anchor);
	}
}

void StageHUD::Initialize(SpriteCommon* spriteCommon) {
	spriteCommon_ = spriteCommon;

	// テクスチャ読み込み確認
	TextureManager::GetInstance()->LoadTexture(kWhiteTexPath);
	TextureManager::GetInstance()->LoadTexture(kRingTexPath);
	TextureManager::GetInstance()->LoadTexture(kDotTexPath);

	rectSpritePool_.clear();
	rectSpritePool_.reserve(kInitialRectPoolCount);
	for (size_t i = 0; i < kInitialRectPoolCount; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_, kWhiteTexPath);
		rectSpritePool_.push_back(std::move(sp));
	}
	rectSpritePoolIndex_ = 0;

	ringSpritePool_.clear();
	ringSpritePool_.reserve(kInitialRingPoolCount);
	for (size_t i = 0; i < kInitialRingPoolCount; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_, kRingTexPath);
		ringSpritePool_.push_back(std::move(sp));
	}
	ringSpritePoolIndex_ = 0;

	dotSpritePool_.clear();
	dotSpritePool_.reserve(kInitialDotPoolCount);
	for (size_t i = 0; i < kInitialDotPoolCount; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_, kDotTexPath);
		dotSpritePool_.push_back(std::move(sp));
	}
	dotSpritePoolIndex_ = 0;

	animTimer_ = 0.0f;
	firePunchTimer_ = 0.0f;
}

void StageHUD::Update(float dt, const StageHUDData& data) {
	hudData_ = data;
	animTimer_ += dt;
	deployTimer_ += dt;
	if (isShuttingDown_) {
		shutdownTimer_ += dt;
	}

	if (hudData_.isFiring) {
		firePunchTimer_ = 0.08f;
	} else if (firePunchTimer_ > 0.0f) {
		firePunchTimer_ -= dt;
		if (firePunchTimer_ < 0.0f) { firePunchTimer_ = 0.0f; }
	}
}

void StageHUD::Draw() {
	if (!spriteCommon_) return;

	rectSpritePoolIndex_ = 0;
	ringSpritePoolIndex_ = 0;
	dotSpritePoolIndex_ = 0;

	DrawTimer();
	DrawBailoutGuide();
	DrawClearDemoCountdown();
	DrawTargetMarkers();
	DrawSystemLogs();
	DrawGuidePanel();
	DrawTargetTelemetry();
	DrawAlerts();
	DrawReticles();
	DrawMissileSeeker();
}

void StageHUD::DrawFadeOverlay() {
	if (!spriteCommon_ || hudData_.fadeOutAlpha <= 0.0f) {
		return;
	}
	TextRenderer::GetInstance()->Flush();
	const float alpha = std::clamp(hudData_.fadeOutAlpha, 0.0f, 1.0f);
	DrawRect({ 0.0f, 0.0f }, { hudData_.screenWidth, hudData_.screenHeight }, { 0.0f, 0.0f, 0.0f, alpha });
}

// ================================================================
// 2D 描画ヘルパー
// ================================================================

Sprite* StageHUD::AcquireRectSprite() {
	if (!spriteCommon_) {
		return nullptr;
	}
	if (rectSpritePoolIndex_ >= rectSpritePool_.size()) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_, kWhiteTexPath);
		rectSpritePool_.push_back(std::move(sp));
	}
	return rectSpritePool_[rectSpritePoolIndex_++].get();
}

Sprite* StageHUD::AcquireRingSprite() {
	if (!spriteCommon_) {
		return nullptr;
	}
	if (ringSpritePoolIndex_ >= ringSpritePool_.size()) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_, kRingTexPath);
		ringSpritePool_.push_back(std::move(sp));
	}
	return ringSpritePool_[ringSpritePoolIndex_++].get();
}

Sprite* StageHUD::AcquireDotSprite() {
	if (!spriteCommon_) {
		return nullptr;
	}
	if (dotSpritePoolIndex_ >= dotSpritePool_.size()) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_, kDotTexPath);
		dotSpritePool_.push_back(std::move(sp));
	}
	return dotSpritePool_[dotSpritePoolIndex_++].get();
}

void StageHUD::DrawLine(const Vector2& p1, const Vector2& p2, float thickness, const Vector4& color, BlendMode blendMode) {
	float dx = p2.x - p1.x;
	float dy = p2.y - p1.y;
	float length = std::sqrt(dx * dx + dy * dy);
	if (length < 0.001f) return;

	Sprite* sp = AcquireRectSprite();
	if (!sp) return;

	float angle = std::atan2(dy, dx);

	sp->SetBlendMode(blendMode);
	sp->SetPosition(p1);
	sp->SetAnchorPoint({ 0.0f, 0.5f });
	sp->SetSize({ length, thickness });
	sp->SetRotation(angle);
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

void StageHUD::DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& anchor, BlendMode blendMode) {
	Sprite* sp = AcquireRectSprite();
	if (!sp) return;

	sp->SetBlendMode(blendMode);
	sp->SetPosition(pos);
	sp->SetAnchorPoint(anchor);
	sp->SetSize(size);
	sp->SetRotation(0.0f);
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

void StageHUD::DrawBorderedPanel(const Vector2& pos, const Vector2& size, const Vector4& bgColor, const Vector4& borderColor, float borderWidth) {
	// 半透明背景
	DrawRect(pos, size, bgColor, { 0.0f, 0.0f }, BlendMode::kNormal);

	// 4辺の枠線
	DrawRect({ pos.x, pos.y }, { size.x, borderWidth }, borderColor, { 0.0f, 0.0f });                                // 上
	DrawRect({ pos.x, pos.y + size.y - borderWidth }, { size.x, borderWidth }, borderColor, { 0.0f, 0.0f });        // 下
	DrawRect({ pos.x, pos.y }, { borderWidth, size.y }, borderColor, { 0.0f, 0.0f });                                // 左
	DrawRect({ pos.x + size.x - borderWidth, pos.y }, { borderWidth, size.y }, borderColor, { 0.0f, 0.0f });        // 右

	// 4隅のミリタリー風コーナータグ
	float cornerLen = 8.0f;
	float cornerThick = borderWidth + 1.0f;
	Vector4 cornerColor = { borderColor.x, borderColor.y, borderColor.z, (std::min)(1.0f, borderColor.w + 0.2f) };

	// 左上
	DrawRect({ pos.x, pos.y }, { cornerLen, cornerThick }, cornerColor);
	DrawRect({ pos.x, pos.y }, { cornerThick, cornerLen }, cornerColor);
	// 右上
	DrawRect({ pos.x + size.x - cornerLen, pos.y }, { cornerLen, cornerThick }, cornerColor);
	DrawRect({ pos.x + size.x - cornerThick, pos.y }, { cornerThick, cornerLen }, cornerColor);
	// 左下
	DrawRect({ pos.x, pos.y + size.y - cornerThick }, { cornerLen, cornerThick }, cornerColor);
	DrawRect({ pos.x, pos.y + size.y - cornerLen }, { cornerThick, cornerLen }, cornerColor);
	// 右下
	DrawRect({ pos.x + size.x - cornerLen, pos.y + size.y - cornerThick }, { cornerLen, cornerThick }, cornerColor);
	DrawRect({ pos.x + size.x - cornerThick, pos.y + size.y - cornerLen }, { cornerThick, cornerLen }, cornerColor);
}

void StageHUD::DrawTargetBrackets(const Vector2& center, float halfSize, float cornerLen, float thickness, const Vector4& color) {
	const float x0 = center.x - halfSize;
	const float y0 = center.y - halfSize;
	const float x1 = center.x + halfSize;
	const float y1 = center.y + halfSize;

	DrawLine({ x0, y0 }, { x0 + cornerLen, y0 }, thickness, color);
	DrawLine({ x0, y0 }, { x0, y0 + cornerLen }, thickness, color);

	DrawLine({ x1, y0 }, { x1 - cornerLen, y0 }, thickness, color);
	DrawLine({ x1, y0 }, { x1, y0 + cornerLen }, thickness, color);

	DrawLine({ x0, y1 }, { x0 + cornerLen, y1 }, thickness, color);
	DrawLine({ x0, y1 }, { x0, y1 - cornerLen }, thickness, color);

	DrawLine({ x1, y1 }, { x1 - cornerLen, y1 }, thickness, color);
	DrawLine({ x1, y1 }, { x1, y1 - cornerLen }, thickness, color);
}

void StageHUD::DrawRing(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode, float thickness) {
	if (radius <= 0.5f || color.w <= 0.001f) {
		return;
	}

	const int segments = std::clamp(static_cast<int>(radius * 0.85f), 20, 64);
	constexpr float kTwoPi = 6.28318530718f;
	const float angleStep = kTwoPi / static_cast<float>(segments);
	const float extend = thickness * 0.25f;

	auto drawRingPass = [&](float passThickness, const Vector4& passColor) {
		for (int i = 0; i < segments; ++i) {
			const float a0 = static_cast<float>(i) * angleStep;
			const float a1 = static_cast<float>(i + 1) * angleStep;
			const Vector2 p0 = { center.x + std::cos(a0) * radius, center.y + std::sin(a0) * radius };
			const Vector2 p1 = { center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius };

			const float dx = p1.x - p0.x;
			const float dy = p1.y - p0.y;
			const float len = std::hypot(dx, dy);
			if (len < 0.001f) {
				continue;
			}
			const float ux = dx / len;
			const float uy = dy / len;
			const Vector2 segStart = { p0.x - ux * extend, p0.y - uy * extend };
			const Vector2 segEnd = { p1.x + ux * extend, p1.y + uy * extend };
			DrawLine(segStart, segEnd, passThickness, passColor, blendMode);
		}
	};

	if (blendMode == BlendMode::kNormal) {
		const Vector4 shadowCol = { 0.0f, 0.0f, 0.0f, color.w * 0.38f };
		drawRingPass(thickness + 1.4f, shadowCol);
	}
	drawRingPass(thickness, color);
}

void StageHUD::DrawDot(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode) {
	if (radius <= 0.1f || color.w <= 0.001f) {
		return;
	}

	const float diameter = (std::max)(1.5f, radius * 2.0f);
	if (blendMode == BlendMode::kNormal) {
		const Vector4 shadowCol = { 0.0f, 0.0f, 0.0f, color.w * 0.42f };
		DrawRect(center, { diameter + 1.6f, diameter + 1.6f }, shadowCol, { 0.5f, 0.5f }, BlendMode::kNormal);
	}
	DrawRect(center, { diameter, diameter }, color, { 0.5f, 0.5f }, blendMode);
}

bool StageHUD::ProjectToScreen(const Vector3& worldPos, const Matrix4x4& vpMat, float screenW, float screenH, Vector2& outScreen) const {
	float cX = worldPos.x * vpMat.m[0][0] + worldPos.y * vpMat.m[1][0] + worldPos.z * vpMat.m[2][0] + vpMat.m[3][0];
	float cY = worldPos.x * vpMat.m[0][1] + worldPos.y * vpMat.m[1][1] + worldPos.z * vpMat.m[2][1] + vpMat.m[3][1];
	float cW = worldPos.x * vpMat.m[0][3] + worldPos.y * vpMat.m[1][3] + worldPos.z * vpMat.m[2][3] + vpMat.m[3][3];

	if (cW <= 0.001f) {
		return false;
	}

	outScreen.x = (cX / cW * 0.5f + 0.5f) * screenW;
	outScreen.y = (-cY / cW * 0.5f + 0.5f) * screenH;
	return true;
}

void StageHUD::ProjectToScreenClamped(
	const Vector3& worldPos,
	const Matrix4x4& vpMat,
	float screenW,
	float screenH,
	float marginPx,
	Vector2& outScreen,
	bool& outIsClamped)
{
	const float cX = worldPos.x * vpMat.m[0][0] + worldPos.y * vpMat.m[1][0] + worldPos.z * vpMat.m[2][0] + vpMat.m[3][0];
	const float cY = worldPos.x * vpMat.m[0][1] + worldPos.y * vpMat.m[1][1] + worldPos.z * vpMat.m[2][1] + vpMat.m[3][1];
	const float cW = worldPos.x * vpMat.m[0][3] + worldPos.y * vpMat.m[1][3] + worldPos.z * vpMat.m[2][3] + vpMat.m[3][3];

	const float halfW = screenW * 0.5f;
	const float halfH = screenH * 0.5f;
	const float maxX = (std::max)(16.0f, halfW - marginPx);
	const float maxY = (std::max)(16.0f, halfH - marginPx);

	// 背面方向でもスクリーン方位を維持
	const float rx = cX * halfW;
	const float ry = -cY * halfH;

	// 角を滑らかに丸めたスーパー楕円で安全枠を評価し、cW = edgeFactor で1ビット単位まで連続接続する
	const float nx = std::fabs(rx) / maxX;
	const float ny = std::fabs(ry) / maxY;
	const float nx2 = nx * nx;
	const float ny2 = ny * ny;
	const float nx4 = nx2 * nx2;
	const float ny4 = ny2 * ny2;
	const float edgeFactor = std::pow(nx4 * nx4 + ny4 * ny4, 0.125f);

	if (cW > 1e-4f && cW >= edgeFactor) {
		outScreen.x = halfW + rx / cW;
		outScreen.y = halfH + ry / cW;
		outIsClamped = false;
		return;
	}

	outIsClamped = true;
	if (edgeFactor > 1e-6f) {
		outScreen.x = halfW + rx / edgeFactor;
		outScreen.y = halfH + ry / edgeFactor;
	} else {
		outScreen.x = halfW + maxX;
		outScreen.y = halfH;
	}
}

Vector3 StageHUD::ComputeLeadInterceptPoint(
	const Vector3& shooterPos,
	const Vector3& shooterVel,
	const Vector3& targetPos,
	const Vector3& targetVel,
	float muzzleSpeed,
	float muzzleForwardOffset,
	float maxLeadTime)
{
	Vector3 shooterFwd = { 0.0f, 0.0f, 1.0f };
	const float shooterSpeed = Length(shooterVel);
	if (shooterSpeed > 0.0001f) {
		shooterFwd = Multiply(1.0f / shooterSpeed, shooterVel);
	}
	const Vector3 muzzlePos = Add(shooterPos, Multiply(muzzleForwardOffset, shooterFwd));
	const Vector3 relPos = Subtract(targetPos, muzzlePos);
	const Vector3 relVel = Subtract(targetVel, shooterVel);

	const float a = Dot(relVel, relVel) - (muzzleSpeed * muzzleSpeed);
	const float b = 2.0f * Dot(relPos, relVel);
	const float c = Dot(relPos, relPos);

	float interceptTime = 0.0f;
	if (std::fabs(a) < 0.0001f) {
		if (std::fabs(b) > 0.0001f) {
			interceptTime = -c / b;
		}
	} else {
		const float discriminant = (b * b) - (4.0f * a * c);
		if (discriminant >= 0.0f) {
			const float sqrtDisc = std::sqrt(discriminant);
			const float t1 = (-b - sqrtDisc) / (2.0f * a);
			const float t2 = (-b + sqrtDisc) / (2.0f * a);
			if (t1 > 0.0001f && t2 > 0.0001f) {
				interceptTime = (std::min)(t1, t2);
			} else if (t1 > 0.0001f) {
				interceptTime = t1;
			} else if (t2 > 0.0001f) {
				interceptTime = t2;
			}
		}
	}

	if (interceptTime <= 0.0001f) {
		const float dist = Length(relPos);
		interceptTime = dist / (std::max)(180.0f, muzzleSpeed);
	}
	interceptTime = std::clamp(interceptTime, 0.0f, maxLeadTime);
	return Add(targetPos, Multiply(interceptTime, relVel));
}

void StageHUD::ComputeOffscreenChevronGeometry(
	const Vector2& screenPt,
	float screenW,
	float screenH,
	float boxHalf,
	float hudScale,
	Vector2& outTip,
	Vector2& outWingA,
	Vector2& outWingB,
	Vector2& outTextPos)
{
	const float cx = screenW * 0.5f;
	const float cy = screenH * 0.5f;
	float dx = screenPt.x - cx;
	float dy = screenPt.y - cy;
	const float len = std::hypot(dx, dy);
	if (len > 0.0001f) {
		dx /= len;
		dy /= len;
	} else {
		dx = 1.0f;
		dy = 0.0f;
	}
	const float px = -dy;
	const float py = dx;

	const float tipOffset = boxHalf + 13.0f * hudScale;
	const float baseOffset = boxHalf + 4.0f * hudScale;
	const float wingHalf = 7.0f * hudScale;

	outTip = { screenPt.x + dx * tipOffset, screenPt.y + dy * tipOffset };
	const Vector2 base = { screenPt.x + dx * baseOffset, screenPt.y + dy * baseOffset };
	outWingA = { base.x + px * wingHalf, base.y + py * wingHalf };
	outWingB = { base.x - px * wingHalf, base.y - py * wingHalf };

	// 距離テキストはブラケット内側に配置し、外側矢印や画面端と重ならないようにする
	const float inwardOffset = boxHalf + 20.0f * hudScale;
	outTextPos = { screenPt.x - dx * inwardOffset, screenPt.y - dy * inwardOffset };
}

// ================================================================
// ================================================================
// 各HUD要素の描画
// ================================================================

void StageHUD::DrawTimer() {
	// HUD順次展開
	if (deployTimer_ < 0.25f) return;

	// ステージクリア時のHUD順次シャットダウン
	float shutdownAlpha = 1.0f;
	bool isTimerOffline = false;
	if (isShuttingDown_) {
		if (shutdownTimer_ >= 1.0f) {
			return; // 完全に消滅
		} else if (shutdownTimer_ >= 0.60f) {
			isTimerOffline = true;
			shutdownAlpha = 1.0f - (shutdownTimer_ - 0.60f) / 0.40f;
		}
	}

	float deployT = std::clamp((deployTimer_ - 0.25f) / 0.40f, 0.0f, 1.0f);
	float alpha = deployT * shutdownAlpha;
	float slideY = -24.0f * (1.0f - deployT);

	float panelW = 210.0f;
	float panelH = 46.0f;
	float panelX = (hudData_.screenWidth - panelW) * 0.5f;
	float panelY = 16.0f + slideY;

	// 背景と枠線
	Vector4 bgColor = { 0.01f, 0.05f, 0.015f, 0.88f * alpha };
	Vector4 borderColor = { 0.38f, 0.74f, 0.41f, 0.85f * alpha };
	Vector4 textColor = { 0.48f, 0.84f, 0.50f, 0.98f * alpha }; // 通常: 実機HUD蛍光グリーン

	if (isWarningMode_) {
		// 被弾・撃墜時のテーマカラー赤色/オレンジ反転
		float pulse = 0.6f + 0.4f * std::sin(animTimer_ * 18.0f);
		textColor = { 1.0f, 0.20f, 0.20f, pulse * alpha };
		borderColor = { 1.0f, 0.35f, 0.10f, pulse * alpha };
		bgColor = { 0.22f, 0.02f, 0.02f, 0.90f * alpha };
	} else if (isTimerOffline) {
		// OFFLINE状態
		textColor = { 1.0f, 0.30f, 0.30f, 0.90f * alpha };
		borderColor = { 0.60f, 0.20f, 0.20f, 0.70f * alpha };
		bgColor = { 0.15f, 0.02f, 0.02f, 0.80f * alpha };
	} else if (!hudData_.isElapsedTimerMode && hudData_.remainingTime <= 30.0f) {
		// 30秒以下: 赤色点滅警告
		float pulse = 0.55f + 0.45f * std::sin(animTimer_ * 10.0f);
		textColor = { 1.0f, 0.25f, 0.25f, pulse * alpha };
		borderColor = { 1.0f, 0.3f, 0.3f, pulse * 0.9f * alpha };
		bgColor = { 0.18f, 0.03f, 0.03f, 0.80f * alpha };
	} else if (!hudData_.isElapsedTimerMode && hudData_.remainingTime <= 60.0f) {
		// 1分以下: 黄色注意
		textColor = { 1.0f, 0.85f, 0.25f, 1.0f * alpha };
		borderColor = { 0.85f, 0.70f, 0.20f, 0.85f * alpha };
	}

	DrawBorderedPanel({ panelX, panelY }, { panelW, panelH }, bgColor, borderColor, 1.5f);

	// 上部アクセントバー
	DrawRect({ panelX + panelW * 0.5f - 30.0f, panelY }, { 60.0f, 2.0f }, borderColor);

	// 時間文字列の生成
	char timeStr[32];
	if (isTimerOffline) {
		snprintf(timeStr, sizeof(timeStr), "[ TIMER : OFFLINE ]");
	} else {
		const float displayTime = hudData_.isElapsedTimerMode ? hudData_.elapsedTime : hudData_.remainingTime;
		int minutes = static_cast<int>(displayTime) / StageHUDTimerDefaults::kSecondsPerMinute;
		int seconds = static_cast<int>(displayTime) % StageHUDTimerDefaults::kSecondsPerMinute;
		snprintf(timeStr, sizeof(timeStr), "[ TIME %02d:%02d ]", minutes, seconds);
	}

	// テキスト描画
	TextRenderer::GetInstance()->Print(
		"HackGen",
		timeStr,
		hudData_.screenWidth * 0.5f,
		panelY + 10.0f,
		24.0f,
		textColor,
		{ 0.5f, 0.0f }
	);
}

void StageHUD::DrawBailoutGuide() {
	// HUD順次展開
	if (deployTimer_ < 0.50f) return;

	// ステージクリア時のHUD順次シャットダウン
	float shutdownAlpha = 1.0f;
	if (isShuttingDown_) {
		if (shutdownTimer_ >= 0.70f) {
			return;
		} else if (shutdownTimer_ >= 0.30f) {
			shutdownAlpha = 1.0f - (shutdownTimer_ - 0.30f) / 0.40f;
		}
	}

	float deployT = std::clamp((deployTimer_ - 0.50f) / 0.40f, 0.0f, 1.0f);
	float alpha = deployT * shutdownAlpha;
	float slideX = -35.0f * (1.0f - deployT);

	float panelW = 210.0f;
	float panelH = 50.0f;
	float panelX = 24.0f + slideX;
	float panelY = hudData_.screenHeight - panelH - 24.0f;

	Vector4 bgColor = { 0.01f, 0.05f, 0.015f, 0.88f * alpha };
	Vector4 borderColor = { 0.38f, 0.74f, 0.41f, 0.85f * alpha };
	Vector4 keyBg = { 0.09f, 0.24f, 0.10f, 0.95f * alpha };
	Vector4 keyText = { 1.0f, 1.0f, 1.0f, 0.98f * alpha };
	Vector4 titleColor = { 0.48f, 0.84f, 0.50f, 0.95f * alpha };
	Vector4 subColor = { 0.68f, 0.86f, 0.70f, 0.85f * alpha };

	if (isWarningMode_) {
		float pulse = 0.6f + 0.4f * std::sin(animTimer_ * 18.0f);
		borderColor = { 1.0f, 0.35f, 0.10f, pulse * alpha };
		bgColor = { 0.22f, 0.02f, 0.02f, 0.90f * alpha };
		keyBg = { 0.65f, 0.15f, 0.10f, pulse * alpha };
		titleColor = { 1.0f, 0.30f, 0.20f, pulse * alpha };
		subColor = { 1.0f, 0.70f, 0.60f, pulse * alpha };
	}

	DrawBorderedPanel({ panelX, panelY }, { panelW, panelH }, bgColor, borderColor, 1.5f);

	// [J] キータグのボックス
	float keyW = 32.0f;
	float keyH = 30.0f;
	float keyX = panelX + 12.0f;
	float keyY = panelY + 10.0f;
	DrawRect({ keyX, keyY }, { keyW, keyH }, keyBg);
	DrawRect({ keyX, keyY }, { keyW, 1.0f }, borderColor);
	DrawRect({ keyX, keyY + keyH - 1.0f }, { keyW, 1.0f }, borderColor);
	DrawRect({ keyX, keyY }, { 1.0f, keyH }, borderColor);
	DrawRect({ keyX + keyW - 1.0f, keyY }, { 1.0f, keyH }, borderColor);

	TextRenderer::GetInstance()->Print(
		"HackGen",
		"J",
		keyX + keyW * 0.5f,
		keyY + 3.0f,
		20.0f,
		keyText,
		{ 0.5f, 0.0f }
	);

	// BAIL OUT / 緊急脱出
	const char* titleStr = hudData_.isClearDemoMode ? "INSTANT CLEAR" : "BAIL OUT";
	const char* subStr = hudData_.isClearDemoMode ? "即時クリア (CLEAR)" : "緊急脱出 (EJECT)";

	TextRenderer::GetInstance()->Print(
		"HackGen",
		titleStr,
		panelX + 54.0f,
		panelY + 8.0f,
		15.0f,
		titleColor
	);

	TextRenderer::GetInstance()->Print(
		"HackGen",
		subStr,
		panelX + 54.0f,
		panelY + 27.0f,
		12.0f,
		subColor
	);
}

void StageHUD::DrawClearDemoCountdown() {
	if (!hudData_.isClearDemoMode || hudData_.clearCountdownSec < 0.0f) {
		return;
	}
	if (isShuttingDown_ && shutdownTimer_ >= 0.70f) {
		return;
	}

	float shutdownAlpha = 1.0f;
	if (isShuttingDown_ && shutdownTimer_ >= 0.30f) {
		shutdownAlpha = 1.0f - (shutdownTimer_ - 0.30f) / 0.40f;
	}
	float alpha = std::clamp(shutdownAlpha, 0.0f, 1.0f);
	if (alpha <= 0.01f) {
		return;
	}

	const int countSec = (std::max)(0, static_cast<int>(std::ceil(hudData_.clearCountdownSec)));
	const std::string text = "クリアまで..." + std::to_string(countSec) + "....";

	const float x = hudData_.screenWidth - 36.0f;
	const float y = hudData_.screenHeight - 28.0f;
	const float fontSize = 44.0f;

	// 視認性向上のためのドロップシャドウ
	TextRenderer::GetInstance()->Print(
		"HackGen",
		text,
		x + 2.0f,
		y + 2.0f,
		fontSize,
		{ 0.0f, 0.05f, 0.02f, 0.75f * alpha },
		{ 1.0f, 1.0f }
	);

	TextRenderer::GetInstance()->Print(
		"HackGen",
		text,
		x,
		y,
		fontSize,
		{ 0.52f, 0.96f, 0.62f, 0.96f * alpha },
		{ 1.0f, 1.0f }
	);
}

void StageHUD::DrawTargetMarkers() {
	if (hudData_.targetMarkers.empty()) {
		return;
	}
	if (isShuttingDown_ && shutdownTimer_ >= 0.35f) {
		return;
	}

	const float deployT = std::clamp(deployTimer_ / 0.40f, 0.0f, 1.0f);
	const float shutdownAlpha = isShuttingDown_ ? (1.0f - shutdownTimer_ / 0.35f) : 1.0f;
	const float alpha = deployT * std::clamp(shutdownAlpha, 0.0f, 1.0f);
	if (alpha <= 0.001f) {
		return;
	}

	const float scaleX = hudData_.screenWidth / 1280.0f;
	const float scaleY = hudData_.screenHeight / 720.0f;
	const float hudScale = std::clamp((std::min)(scaleX, scaleY), 0.70f, 1.45f);
	const float clampMargin = 44.0f * hudScale;
	const float boxHalf = 15.0f * hudScale;
	const float cornerLen = 6.0f * hudScale;
	const float lineThick = 1.8f;
	const float labelFont = 12.5f * hudScale;

	for (const auto& marker : hudData_.targetMarkers) {
		Vector4 col = marker.color;
		col.w *= alpha;

		Vector2 screenPt{};
		bool isClamped = false;
		ProjectToScreenClamped(
			marker.worldPos,
			hudData_.viewProjectionMatrix,
			hudData_.screenWidth,
			hudData_.screenHeight,
			clampMargin,
			screenPt,
			isClamped);

		DrawTargetBrackets(screenPt, boxHalf, cornerLen, lineThick, col);

		const int distMeters = static_cast<int>(Length(Subtract(marker.worldPos, hudData_.aircraftPosition)));
		char distBuf[32]{};
		std::snprintf(distBuf, sizeof(distBuf), "%d m", distMeters);

		if (!isClamped) {
			PrintShadowedMarkerText(
				marker.label,
				screenPt.x + 20.0f * hudScale,
				screenPt.y - 7.0f * hudScale,
				labelFont,
				col);
			PrintShadowedMarkerText(
				distBuf,
				screenPt.x + 20.0f * hudScale,
				screenPt.y + 8.0f * hudScale,
				labelFont,
				col);

			if (marker.showLeadSight) {
				const Vector3 leadWorldPos = ComputeLeadInterceptPoint(
					hudData_.aircraftPosition,
					hudData_.aircraftVelocity,
					marker.worldPos,
					marker.velocity);
				Vector2 leadScreen{};
				bool leadClamped = false;
				ProjectToScreenClamped(
					leadWorldPos,
					hudData_.viewProjectionMatrix,
					hudData_.screenWidth,
					hudData_.screenHeight,
					clampMargin,
					leadScreen,
					leadClamped);

				if (!leadClamped) {
					Vector4 leadCol = { 0.44f, 0.92f, 0.66f, 0.84f * alpha };
					Vector4 lineCol = leadCol;
					lineCol.w *= 0.32f;
					DrawLine(screenPt, leadScreen, 1.1f, lineCol, BlendMode::kNormal);
					DrawRing(leadScreen, 13.5f * hudScale, leadCol, BlendMode::kNormal, 1.5f);
					DrawDot(leadScreen, 1.5f * hudScale, leadCol, BlendMode::kNormal);
				}
				if (hudData_.hitMarkerTimer > 0.0f) {
					const float inner = 5.0f * hudScale;
					const float outer = 14.0f * hudScale;
					const Vector4 hitCol = { 1.00f, 0.96f, 0.62f, 1.00f * alpha };
					const Vector2 hitPos = leadClamped ? screenPt : leadScreen;
					DrawLine({ hitPos.x - outer, hitPos.y - outer }, { hitPos.x - inner, hitPos.y - inner }, 2.2f, hitCol);
					DrawLine({ hitPos.x + outer, hitPos.y - outer }, { hitPos.x + inner, hitPos.y - inner }, 2.2f, hitCol);
					DrawLine({ hitPos.x - outer, hitPos.y + outer }, { hitPos.x - inner, hitPos.y + inner }, 2.2f, hitCol);
					DrawLine({ hitPos.x + outer, hitPos.y + outer }, { hitPos.x + inner, hitPos.y + inner }, 2.2f, hitCol);
				}
			}
		} else {
			Vector2 tip{}, wingA{}, wingB{}, textPos{};
			ComputeOffscreenChevronGeometry(
				screenPt,
				hudData_.screenWidth,
				hudData_.screenHeight,
				boxHalf,
				hudScale,
				tip,
				wingA,
				wingB,
				textPos);
			DrawLine(wingA, tip, 2.2f, col);
			DrawLine(wingB, tip, 2.2f, col);
			PrintShadowedMarkerText(distBuf, textPos.x, textPos.y, labelFont, col, { 0.5f, 0.5f });
		}
	}
}

void StageHUD::DrawSystemLogs() {
	if (hudData_.systemLogs.empty()) {
		return;
	}
	const float scaleX = hudData_.screenWidth / 1280.0f;
	const float scaleY = hudData_.screenHeight / 720.0f;
	const float hudScale = std::clamp((std::min)(scaleX, scaleY), 0.70f, 1.45f);

	const float baseX = 22.0f * hudScale;
	const float baseY = 160.0f * hudScale;
	const float rowH = 20.0f * hudScale;
	const float fontSize = 12.0f * hudScale;
	const float bgW = 360.0f * hudScale;
	const float accentW = 3.0f * hudScale;
	const float padX = 8.0f * hudScale;

	constexpr float kFadeInSec = 0.16f;
	constexpr float kFadeOutSec = 0.80f;
	constexpr float kLifeSec = 9.0f;

	for (size_t i = 0; i < hudData_.systemLogs.size(); ++i) {
		const auto& entry = hudData_.systemLogs[i];
		float alpha = 1.0f;
		if (entry.ageSeconds < kFadeInSec) {
			alpha = entry.ageSeconds / kFadeInSec;
		} else if (entry.ageSeconds > (kLifeSec - kFadeOutSec)) {
			alpha = (kLifeSec - entry.ageSeconds) / kFadeOutSec;
		}
		alpha = std::clamp(alpha, 0.0f, 1.0f);

		const float rowY = baseY + static_cast<float>(i) * rowH;
		Vector4 bgCol = { 0.01f, 0.04f, 0.06f, 0.72f * alpha };
		DrawRect({ baseX, rowY }, { bgW, rowH - 2.0f }, bgCol);

		Vector4 tagCol = { 0.36f, 1.00f, 0.72f, 0.98f };
		if (entry.category == "WRN") {
			tagCol = { 1.00f, 0.84f, 0.28f, 0.98f };
		} else if (entry.category == "ALR" || entry.category == "RWR") {
			tagCol = { 1.00f, 0.30f, 0.28f, 1.00f };
		} else if (entry.category == "INTEL") {
			tagCol = { 0.56f, 0.90f, 1.00f, 0.98f };
		}
		tagCol.w *= alpha;
		DrawRect({ baseX, rowY }, { accentW, rowH - 2.0f }, tagCol);

		const std::string fullLine = entry.timestampText + "[" + entry.category + "] " + entry.message;
		PrintShadowedMarkerText(fullLine, baseX + accentW + padX, rowY + 2.0f, fontSize, tagCol);
	}
}

void StageHUD::DrawGuidePanel() {
	if (!hudData_.guide.visible) {
		return;
	}
	const float scaleX = hudData_.screenWidth / 1280.0f;
	const float scaleY = hudData_.screenHeight / 720.0f;
	const float hudScale = std::clamp((std::min)(scaleX, scaleY), 0.70f, 1.45f);

	const float panelW = 620.0f * hudScale;
	const float panelH = 72.0f * hudScale;
	const float panelX = (hudData_.screenWidth - panelW) * 0.5f;
	const float panelY = hudData_.screenHeight - (16.0f * hudScale) - panelH;

	const Vector4 bgCol = { 0.01f, 0.05f, 0.04f, 0.84f };
	const Vector4 borderCol = { 0.26f, 0.92f, 0.62f, 0.88f };
	DrawBorderedPanel({ panelX, panelY }, { panelW, panelH }, bgCol, borderCol, 1.5f);

	const float centerX = hudData_.screenWidth * 0.5f;
	PrintShadowedMarkerText(
		hudData_.guide.title,
		centerX,
		panelY + 8.0f * hudScale,
		15.5f * hudScale,
		{ 0.86f, 1.00f, 0.92f, 1.00f },
		{ 0.5f, 0.0f });

	PrintShadowedMarkerText(
		hudData_.guide.sub,
		centerX,
		panelY + 31.0f * hudScale,
		12.0f * hudScale,
		{ 0.68f, 0.92f, 0.82f, 0.95f },
		{ 0.5f, 0.0f });

	if (hudData_.guide.showProgressBar) {
		const float barMarginX = 22.0f * hudScale;
		const float barW = panelW - barMarginX * 2.0f;
		const float barH = 6.0f * hudScale;
		const float barX = panelX + barMarginX;
		const float barY = panelY + 54.0f * hudScale;

		DrawRect({ barX, barY }, { barW, barH }, { 0.06f, 0.16f, 0.12f, 0.85f });
		const float ratio = std::clamp(hudData_.guide.progressRatio, 0.0f, 1.0f);
		if (ratio > 0.0f) {
			DrawRect({ barX, barY }, { barW * ratio, barH }, { 0.32f, 1.00f, 0.68f, 0.98f });
		}
	}
}

void StageHUD::DrawTargetTelemetry() {
	if (!hudData_.structuralTelemetry.visible) {
		return;
	}
	const float scaleX = hudData_.screenWidth / 1280.0f;
	const float scaleY = hudData_.screenHeight / 720.0f;
	const float hudScale = std::clamp((std::min)(scaleX, scaleY), 0.70f, 1.45f);

	const float panelW = 228.0f * hudScale;
	const float panelH = 84.0f * hudScale;
	const float panelX = hudData_.screenWidth - (26.0f * hudScale) - panelW;
	const float panelY = 92.0f * hudScale;

	const Vector4 panelBg = { 0.01f, 0.05f, 0.04f, 0.84f };
	const Vector4 hostileCol = { 1.00f, 0.28f, 0.26f, 0.98f };
	DrawBorderedPanel({ panelX, panelY }, { panelW, panelH }, panelBg, hostileCol, 1.5f);

	PrintShadowedMarkerText(
		"TARGET STRUCTURAL TELEMETRY",
		panelX + 10.0f * hudScale,
		panelY + 7.0f * hudScale,
		11.0f * hudScale,
		hostileCol);

	auto drawPartRow = [&](int rowIndex, const char* label, float ratio, bool isBroken) {
		const float rowY = panelY + (27.0f + static_cast<float>(rowIndex) * 18.0f) * hudScale;
		const float clampedRatio = isBroken ? 0.0f : std::clamp(ratio, 0.0f, 1.0f);
		const Vector4 rowCol = isBroken
			? Vector4{ 1.00f, 0.30f, 0.28f, 1.00f }
			: (clampedRatio < 0.60f ? Vector4{ 1.00f, 0.84f, 0.28f, 0.98f } : Vector4{ 0.36f, 1.00f, 0.72f, 0.98f });

		PrintShadowedMarkerText(
			label,
			panelX + 10.0f * hudScale,
			rowY,
			11.0f * hudScale,
			rowCol);

		const float barX = panelX + 112.0f * hudScale;
		const float barY = rowY + 4.0f * hudScale;
		const float barW = 104.0f * hudScale;
		const float barH = 6.0f * hudScale;

		DrawRect({ barX, barY }, { barW, barH }, { 0.06f, 0.16f, 0.12f, 0.85f });
		if (isBroken) {
			PrintShadowedMarkerText(
				"BROKEN",
				barX + barW * 0.5f,
				rowY,
				11.0f * hudScale,
				{ 1.00f, 0.30f, 0.28f, 1.00f },
				{ 0.5f, 0.0f });
		} else if (clampedRatio > 0.0f) {
			DrawRect({ barX, barY }, { barW * clampedRatio, barH }, rowCol);
		}
	};

	const auto& st = hudData_.structuralTelemetry;
	drawPartRow(0, "PORT WING", st.portWingRatio, st.brokenPartIndex == 1);
	drawPartRow(1, "STBD WING", st.stbdWingRatio, st.brokenPartIndex == 2);
	drawPartRow(2, "ENGINE", st.engineRatio, st.brokenPartIndex == 3);
}

void StageHUD::DrawAlerts() {
	const float scaleX = hudData_.screenWidth / 1280.0f;
	const float scaleY = hudData_.screenHeight / 720.0f;
	const float hudScale = std::clamp((std::min)(scaleX, scaleY), 0.70f, 1.45f);

	const float blink = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(animTimer_ * 14.0f));
	const float bannerW = 420.0f * hudScale;
	const float bannerH = 36.0f * hudScale;
	const float bannerX = (hudData_.screenWidth - bannerW) * 0.5f;

	if (hudData_.showPullUpAlert) {
		const float bannerY = (hudData_.screenHeight * 0.5f) + (88.0f * hudScale);
		Vector4 bg = { 0.22f, 0.02f, 0.02f, 0.88f * blink };
		Vector4 border = { 1.00f, 0.24f, 0.20f, 0.98f * blink };
		DrawBorderedPanel({ bannerX, bannerY }, { bannerW, bannerH }, bg, border, 1.5f);
		PrintShadowedMarkerText(
			"[ ! PULL UP ! 地面接近警告 ]",
			hudData_.screenWidth * 0.5f,
			bannerY + 8.0f * hudScale,
			15.5f * hudScale,
			border,
			{ 0.5f, 0.0f });
	} else if (hudData_.showHighAltCaution) {
		const float bannerY = (hudData_.screenHeight * 0.5f) + (88.0f * hudScale);
		Vector4 bg = { 0.20f, 0.12f, 0.02f, 0.84f * blink };
		Vector4 border = { 1.00f, 0.82f, 0.24f, 0.95f * blink };
		DrawBorderedPanel({ bannerX, bannerY }, { bannerW, bannerH }, bg, border, 1.5f);
		PrintShadowedMarkerText(
			"[ ! ALTITUDE CAUTION : 峡谷低空を維持せよ ! ]",
			hudData_.screenWidth * 0.5f,
			bannerY + 8.0f * hudScale,
			14.0f * hudScale,
			border,
			{ 0.5f, 0.0f });
	}

	if (hudData_.showRwrAlert) {
		const float rwrY = 168.0f * hudScale;
		Vector4 bg = { 0.22f, 0.02f, 0.02f, 0.88f * blink };
		Vector4 border = { 1.00f, 0.24f, 0.20f, 0.98f * blink };
		DrawBorderedPanel({ bannerX, rwrY }, { bannerW, bannerH }, bg, border, 1.5f);
		PrintShadowedMarkerText(
			"[ ! RWR WARNING : HIGH-ENERGY CONTACT DIVING ! ]",
			hudData_.screenWidth * 0.5f,
			rwrY + 8.0f * hudScale,
			12.0f * hudScale,
			border,
			{ 0.5f, 0.0f });
	}
}

void StageHUD::DrawLeadGuidanceLine(const Vector2& noseScreen, const Vector2& aimScreen, float alpha) {
	float dist = std::sqrt(
		(aimScreen.x - noseScreen.x) * (aimScreen.x - noseScreen.x) +
		(aimScreen.y - noseScreen.y) * (aimScreen.y - noseScreen.y)
	);

	if (dist > 12.0f) {
		Vector4 lineCol = isWarningMode_
			? Vector4{ 0.95f, 0.32f, 0.22f, 0.42f * alpha }
			: Vector4{ 0.56f, 0.86f, 0.58f, 0.32f * alpha };
		DrawLine(noseScreen, aimScreen, 1.2f, lineCol, BlendMode::kNormal);

		Vector2 midPos = {
			(noseScreen.x + aimScreen.x) * 0.5f,
			(noseScreen.y + aimScreen.y) * 0.5f
		};
		DrawDot(midPos, 1.4f, isWarningMode_ ? Vector4{ 0.95f, 0.38f, 0.22f, 0.58f * alpha } : Vector4{ 0.56f, 0.86f, 0.58f, 0.50f * alpha }, BlendMode::kNormal);
	}
}

void StageHUD::DrawMouseAimReticle(const Vector2& aimScreen, float reticleScale, const Vector4& baseAimCol, const Vector4& baseTickCol) {
	// War Thunder準拠: 半径約 11px の極細中空エイムサークル＋中心ピップ
	float reticleRadius = 11.0f * reticleScale;

	// 中空外周リング
	DrawRing(aimScreen, reticleRadius, baseAimCol, BlendMode::kNormal, 1.5f);

	// 中心極小ピップ
	DrawDot(aimScreen, 1.5f, baseTickCol, BlendMode::kNormal);
}

void StageHUD::DrawBoresightReticle(const Vector2& noseScreen, float reticleScale, float alpha) {
	float pulseScale = (firePunchTimer_ > 0.0f) ? 1.15f : 1.0f;
	pulseScale *= reticleScale;
	Vector4 crossColor;
	if (isWarningMode_) {
		crossColor = { 0.96f, 0.32f, 0.22f, 0.88f * alpha };
	} else if (firePunchTimer_ > 0.0f) {
		crossColor = { 0.96f, 0.84f, 0.42f, 0.88f * alpha };
	} else {
		crossColor = { 0.56f, 0.88f, 0.58f, 0.82f * alpha }; // War Thunder風 落ち着いたHUDグリーン
	}

	// War Thunder準拠: 機首方向を示す中空大サークル＋外向き4方向ティック
	const float radius = 22.0f * pulseScale;
	const float tickLen = 5.0f * pulseScale;
	DrawRing(noseScreen, radius, crossColor, BlendMode::kNormal, 1.6f);

	const Vector4 tickShadow = { 0.0f, 0.0f, 0.0f, crossColor.w * 0.38f };
	auto drawOuterTick = [&](const Vector2& dir) {
		const Vector2 p0 = { noseScreen.x + dir.x * radius, noseScreen.y + dir.y * radius };
		const Vector2 p1 = { noseScreen.x + dir.x * (radius + tickLen), noseScreen.y + dir.y * (radius + tickLen) };
		DrawLine(p0, p1, 2.8f, tickShadow, BlendMode::kNormal);
		DrawLine(p0, p1, 1.5f, crossColor, BlendMode::kNormal);
	};
	drawOuterTick({ 1.0f, 0.0f });
	drawOuterTick({ -1.0f, 0.0f });
	drawOuterTick({ 0.0f, 1.0f });
	drawOuterTick({ 0.0f, -1.0f });

	// 中心極小ピップ
	DrawDot(noseScreen, 1.5f, crossColor, BlendMode::kNormal);
}

void StageHUD::DrawReticles() {
	if (!hudData_.showReticles) {
		return;
	}
	// ステージクリア時のHUD順次シャットダウン
	if (isShuttingDown_ && shutdownTimer_ >= 0.35f) {
		return;
	}

	Vector2 noseScreen{};
	Vector2 noseGuideScreen{};
	Vector2 aimScreen{};

	// 1. 自機が向いている方向
	Vector3 noseTargetWorld = Add(hudData_.aircraftPosition, Multiply(500.0f, hudData_.aircraftForward));
	bool noseInFront = ProjectToScreen(noseTargetWorld, hudData_.viewProjectionMatrix, hudData_.screenWidth, hudData_.screenHeight, noseScreen);
	bool noseVisible = noseInFront &&
		noseScreen.x >= -48.0f && noseScreen.x <= hudData_.screenWidth + 48.0f &&
		noseScreen.y >= -48.0f && noseScreen.y <= hudData_.screenHeight + 48.0f;

	// リードガイダンス用：機首が大きく画面外にある場合も巨大スプライト化を防ぎ画面外縁へクリップ
	bool noseGuideClamped = false;
	ProjectToScreenClamped(noseTargetWorld, hudData_.viewProjectionMatrix, hudData_.screenWidth, hudData_.screenHeight, -16.0f, noseGuideScreen, noseGuideClamped);

	// 2. マウスの向き
	bool aimVisible = false;
	bool aimClamped = false;
	if (hudData_.mouseAimEnabled) {
		Vector3 aimTargetWorld = Add(hudData_.aircraftPosition, Multiply(500.0f, hudData_.mouseAimTargetDirection));
		ProjectToScreenClamped(aimTargetWorld, hudData_.viewProjectionMatrix, hudData_.screenWidth, hudData_.screenHeight, 38.0f, aimScreen, aimClamped);
		aimVisible = true;
	}

	// HUD順次展開
	float deployT = std::clamp(deployTimer_ / 0.40f, 0.0f, 1.0f);
	float shutdownAlpha = isShuttingDown_ ? (1.0f - shutdownTimer_ / 0.35f) : 1.0f;
	float alpha = deployT * (std::clamp)(shutdownAlpha, 0.0f, 1.0f);
	float reticleScale = 1.0f + 0.45f * (1.0f - deployT);

	// テーマカラー反転時
	Vector4 baseAimCol = isWarningMode_
		? Vector4{ 1.0f, 0.25f, 0.15f, 0.95f * alpha }
		: Vector4{ 0.42f, 0.78f, 0.45f, 0.85f * alpha };
	Vector4 baseTickCol = isWarningMode_
		? Vector4{ 1.0f, 0.45f, 0.10f, 1.0f * alpha }
		: Vector4{ 0.50f, 0.86f, 0.53f, 0.95f * alpha };

	// -----------------------------------------------------------------
	// リードガイダンスライン
	// -----------------------------------------------------------------
	if (aimVisible) {
		DrawLeadGuidanceLine(noseGuideScreen, aimScreen, alpha);
	}

	// -----------------------------------------------------------------
	// マウス目標照準
	// -----------------------------------------------------------------
	if (aimVisible) {
		DrawMouseAimReticle(aimScreen, reticleScale, baseAimCol, baseTickCol);
	}

	// -----------------------------------------------------------------
	// 自機が向いている方向
	// -----------------------------------------------------------------
	if (noseVisible) {
		DrawBoresightReticle(noseScreen, reticleScale, alpha);
	}
}

void StageHUD::DrawMissileSeeker() {
	if (!hudData_.showReticles) {
		return;
	}
	if (isShuttingDown_ && shutdownTimer_ >= 0.35f) {
		return;
	}

	const float deployT = std::clamp(deployTimer_ / 0.40f, 0.0f, 1.0f);
	const float shutdownAlpha = isShuttingDown_ ? (1.0f - shutdownTimer_ / 0.35f) : 1.0f;
	const float alpha = deployT * std::clamp(shutdownAlpha, 0.0f, 1.0f);
	if (alpha <= 0.01f) {
		return;
	}

	const float scaleX = hudData_.screenWidth / 1280.0f;
	const float scaleY = hudData_.screenHeight / 720.0f;
	const float hudScale = std::clamp((std::min)(scaleX, scaleY), 0.70f, 1.45f);
	const auto& seeker = hudData_.missileSeeker;

	// 飛翔中ミサイルの位置マーカー描画（選択兵装に関わらず表示）
	for (const auto& mPos : seeker.activeMissilePositions) {
		Vector2 mScreen{};
		if (ProjectToScreen(mPos, hudData_.viewProjectionMatrix, hudData_.screenWidth, hudData_.screenHeight, mScreen)) {
			const float d = 7.0f * hudScale;
			const Vector4 mslCol = { 0.45f, 0.94f, 1.00f, 0.92f * alpha };
			DrawLine({ mScreen.x, mScreen.y - d }, { mScreen.x + d, mScreen.y }, 1.6f, mslCol);
			DrawLine({ mScreen.x + d, mScreen.y }, { mScreen.x, mScreen.y + d }, 1.6f, mslCol);
			DrawLine({ mScreen.x, mScreen.y + d }, { mScreen.x - d, mScreen.y }, 1.6f, mslCol);
			DrawLine({ mScreen.x - d, mScreen.y }, { mScreen.x, mScreen.y - d }, 1.6f, mslCol);
			PrintShadowedMarkerText("MSL", mScreen.x + 11.0f * hudScale, mScreen.y - 6.0f * hudScale, 11.0f * hudScale, mslCol);
		}
	}

	if (!seeker.isMissileSelected) {
		return;
	}

	constexpr float kDegToRad = 3.14159265358979f / 180.0f;
	constexpr float kRefDist = 500.0f;
	const Vector3 boresightRef = Add(hudData_.aircraftPosition, Multiply(kRefDist, hudData_.aircraftForward));

	auto computeConeRadiusPx = [&](float angleDeg, float fallbackPx) -> float {
		Vector2 cPt{}, ePt{};
		const float offset = kRefDist * std::tan(angleDeg * kDegToRad);
		const Vector3 edgeRef = Add(boresightRef, Multiply(offset, seeker.aircraftRight));
		if (ProjectToScreen(boresightRef, hudData_.viewProjectionMatrix, hudData_.screenWidth, hudData_.screenHeight, cPt) &&
			ProjectToScreen(edgeRef, hudData_.viewProjectionMatrix, hudData_.screenWidth, hudData_.screenHeight, ePt)) {
			const float r = std::hypot(ePt.x - cPt.x, ePt.y - cPt.y);
			if (r > 8.0f && r < hudData_.screenHeight * 0.65f) {
				return r;
			}
		}
		return fallbackPx;
	};

	const float outerRadius = computeConeRadiusPx(seeker.outerGimbalAngleDeg, 125.0f * hudScale);
	const float innerRadius = computeConeRadiusPx(seeker.innerAcqAngleDeg, 46.0f * hudScale);

	Vector2 outerCenter{};
	bool outerClamped = false;
	ProjectToScreenClamped(
		seeker.boresightWorldPoint,
		hudData_.viewProjectionMatrix,
		hudData_.screenWidth,
		hudData_.screenHeight,
		38.0f,
		outerCenter,
		outerClamped);

	Vector2 innerCenter = outerCenter;
	bool innerClamped = outerClamped;
	if (seeker.hasTarget && (seeker.seekerState == 3 || seeker.seekerState == 4)) {
		ProjectToScreenClamped(
			seeker.seekerTrackWorldPoint,
			hudData_.viewProjectionMatrix,
			hudData_.screenWidth,
			hudData_.screenHeight,
			38.0f,
			innerCenter,
			innerClamped);
	}

	const float labelFont = 13.0f * hudScale;
	const Vector2 statusAnchor = { 0.5f, 0.0f };
	const float statusTextY = outerCenter.y + outerRadius + 10.0f * hudScale;

	if (seeker.currentMissiles <= 0) {
		const Vector4 emptyCol = { 0.96f, 0.28f, 0.24f, 0.82f * alpha };
		DrawRing(outerCenter, outerRadius, emptyCol, BlendMode::kNormal, 1.6f);
		PrintShadowedMarkerText("AAM // NO MISSILES", outerCenter.x, statusTextY, labelFont, emptyCol, statusAnchor);
		return;
	}

	if (seeker.seekerState == 1) {
		// WarmingUp
		const float rWarm = outerRadius * (0.45f + 0.55f * std::clamp(seeker.warmupProgress, 0.0f, 1.0f));
		const Vector4 warmCol = { 0.62f, 0.90f, 0.70f, 0.68f * alpha };
		DrawRing(outerCenter, rWarm, warmCol, BlendMode::kNormal, 1.6f);
		PrintShadowedMarkerText("IR SEEKER // COOLING", outerCenter.x, statusTextY, labelFont, warmCol, statusAnchor);
	} else if (seeker.seekerState == 2) {
		// Searching
		const Vector4 outerCol = { 0.88f, 0.92f, 0.90f, 0.62f * alpha };
		const Vector4 innerCol = { 0.48f, 0.92f, 0.60f, 0.82f * alpha };
		DrawRing(outerCenter, outerRadius, outerCol, BlendMode::kNormal, 1.6f);
		DrawRing(outerCenter, innerRadius, innerCol, BlendMode::kNormal, 1.8f);
		PrintShadowedMarkerText("IR SEEKER // SEARCH [SCROLL: GUN]", outerCenter.x, statusTextY, labelFont, innerCol, statusAnchor);
	} else if (seeker.seekerState == 3) {
		// Locking
		const Vector4 outerCol = { 1.00f, 0.78f, 0.22f, 0.78f * alpha };
		const Vector4 innerCol = { 1.00f, 0.82f, 0.24f, 0.88f * alpha };
		const float trackRadius = innerRadius * (1.20f - 0.25f * std::clamp(seeker.lockProgress, 0.0f, 1.0f));
		DrawRing(outerCenter, outerRadius, outerCol, BlendMode::kNormal, 1.6f);
		DrawRing(innerCenter, trackRadius, innerCol, BlendMode::kNormal, 1.8f);
		char buf[64]{};
		std::snprintf(buf, sizeof(buf), "IR TRACKING [%d%%]", static_cast<int>(std::round(seeker.lockProgress * 100.0f)));
		PrintShadowedMarkerText(buf, outerCenter.x, statusTextY, labelFont, innerCol, statusAnchor);
	} else if (seeker.seekerState == 4) {
		// Locked (War Thunder風 中空赤ロックリング)
		const Vector4 lockRed = { 1.00f, 0.22f, 0.18f, 0.90f * alpha };
		DrawRing(outerCenter, outerRadius, lockRed, BlendMode::kNormal, 1.6f);
		DrawRing(innerCenter, innerRadius * 0.90f, lockRed, BlendMode::kNormal, 1.8f);
		DrawTargetBrackets(innerCenter, innerRadius * 0.95f + 6.0f * hudScale, 8.0f * hudScale, 1.8f, lockRed);

		const float blink = 0.55f + 0.45f * std::sin(animTimer_ * 18.0f);
		Vector4 textCol = lockRed;
		textCol.w *= blink;
		if (seeker.cooldownRatio > 0.01f) {
			PrintShadowedMarkerText("IR LOCK // RELOADING...", outerCenter.x, statusTextY, labelFont, lockRed, statusAnchor);
		} else {
			PrintShadowedMarkerText("IR LOCK // FIRE [LMB]", outerCenter.x, statusTextY, labelFont, textCol, statusAnchor);
		}
	}
}
