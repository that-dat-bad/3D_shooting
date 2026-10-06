#define NOMINMAX
#include "StageHUD.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

void StageHUD::Initialize(SpriteCommon* spriteCommon) {
	spriteCommon_ = spriteCommon;

	// テクスチャ読み込み確認
	TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("assets/textures/circle2.png");
	TextureManager::GetInstance()->LoadTexture("assets/textures/circle.png");

	rectSprite_ = std::make_unique<Sprite>();
	rectSprite_->Initialize(spriteCommon_, "assets/textures/white1x1.png");

	ringSprite_ = std::make_unique<Sprite>();
	ringSprite_->Initialize(spriteCommon_, "assets/textures/circle2.png");

	dotSprite_ = std::make_unique<Sprite>();
	dotSprite_->Initialize(spriteCommon_, "assets/textures/circle.png");

	animTimer_ = 0.0f;
	firePunchTimer_ = 0.0f;
}

void StageHUD::Update(float dt, const StageHUDData& data) {
	hudData_ = data;
	animTimer_ += dt;

	if (hudData_.isFiring) {
		firePunchTimer_ = 0.08f;
	} else if (firePunchTimer_ > 0.0f) {
		firePunchTimer_ -= dt;
		if (firePunchTimer_ < 0.0f) { firePunchTimer_ = 0.0f; }
	}
}

void StageHUD::Draw() {
	if (!spriteCommon_) return;

	DrawTimer();
	DrawAmmo();
	DrawReticles();
}

// ================================================================
// 2D 描画ヘルパー
// ================================================================

void StageHUD::DrawLine(const Vector2& p1, const Vector2& p2, float thickness, const Vector4& color, BlendMode blendMode) {
	float dx = p2.x - p1.x;
	float dy = p2.y - p1.y;
	float length = std::sqrt(dx * dx + dy * dy);
	if (length < 0.001f) return;

	float angle = std::atan2(dy, dx);

	rectSprite_->SetBlendMode(blendMode);
	rectSprite_->SetPosition(p1);
	rectSprite_->SetAnchorPoint({ 0.0f, 0.5f });
	rectSprite_->SetSize({ length, thickness });
	rectSprite_->SetRotation(angle);
	rectSprite_->SetColor(color);
	rectSprite_->Update();
	rectSprite_->Draw();
}

void StageHUD::DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& anchor, BlendMode blendMode) {
	rectSprite_->SetBlendMode(blendMode);
	rectSprite_->SetPosition(pos);
	rectSprite_->SetAnchorPoint(anchor);
	rectSprite_->SetSize(size);
	rectSprite_->SetRotation(0.0f);
	rectSprite_->SetColor(color);
	rectSprite_->Update();
	rectSprite_->Draw();
}

void StageHUD::DrawBorderedPanel(const Vector2& pos, const Vector2& size, const Vector4& bgColor, const Vector4& borderColor, float borderWidth) {
	// 半透明背景
	DrawRect(pos, size, bgColor, { 0.0f, 0.0f }, BlendMode::kNormal);

	// 4辺の枠線
	DrawRect({ pos.x, pos.y }, { size.x, borderWidth }, borderColor, { 0.0f, 0.0f });                                // 上
	DrawRect({ pos.x, pos.y + size.y - borderWidth }, { size.x, borderWidth }, borderColor, { 0.0f, 0.0f });        // 下
	DrawRect({ pos.x, pos.y }, { borderWidth, size.y }, borderColor, { 0.0f, 0.0f });                                // 左
	DrawRect({ pos.x + size.x - borderWidth, pos.y }, { borderWidth, size.y }, borderColor, { 0.0f, 0.0f });        // 右

	// 4隅のミリタリー風コーナータグ（装飾）
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

void StageHUD::DrawRing(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode) {
	ringSprite_->SetBlendMode(blendMode);
	ringSprite_->SetPosition(center);
	ringSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	ringSprite_->SetSize({ radius * 2.0f, radius * 2.0f });
	ringSprite_->SetRotation(0.0f);
	ringSprite_->SetColor(color);
	ringSprite_->Update();
	ringSprite_->Draw();
}

void StageHUD::DrawDot(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode) {
	dotSprite_->SetBlendMode(blendMode);
	dotSprite_->SetPosition(center);
	dotSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	dotSprite_->SetSize({ radius * 2.0f, radius * 2.0f });
	dotSprite_->SetRotation(0.0f);
	dotSprite_->SetColor(color);
	dotSprite_->Update();
	dotSprite_->Draw();
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

// ================================================================
// 各HUD要素の描画
// ================================================================

void StageHUD::DrawTimer() {
	float panelW = 200.0f;
	float panelH = 46.0f;
	float panelX = (hudData_.screenWidth - panelW) * 0.5f;
	float panelY = 16.0f;

	// 背景と枠線
	Vector4 bgColor = { 0.03f, 0.06f, 0.10f, 0.75f };
	Vector4 borderColor = { 0.20f, 0.45f, 0.65f, 0.85f };

	// タイムに応じたステータスカラー
	Vector4 textColor = { 0.90f, 0.95f, 1.0f, 1.0f }; // 通常（シアン白）
	if (hudData_.remainingTime <= 30.0f) {
		// 30秒以下: 赤色点滅警告
		float pulse = 0.55f + 0.45f * std::sin(animTimer_ * 10.0f);
		textColor = { 1.0f, 0.25f, 0.25f, pulse };
		borderColor = { 1.0f, 0.3f, 0.3f, pulse * 0.9f };
		bgColor = { 0.18f, 0.03f, 0.03f, 0.80f };
	} else if (hudData_.remainingTime <= 60.0f) {
		// 1分以下: 黄色注意
		textColor = { 1.0f, 0.85f, 0.25f, 1.0f };
		borderColor = { 0.85f, 0.70f, 0.20f, 0.85f };
	}

	DrawBorderedPanel({ panelX, panelY }, { panelW, panelH }, bgColor, borderColor, 1.5f);

	// 上部アクセントバー
	DrawRect({ panelX + panelW * 0.5f - 30.0f, panelY }, { 60.0f, 2.0f }, borderColor);

	// 時間文字列の生成
	int minutes = static_cast<int>(hudData_.remainingTime) / 60;
	int seconds = static_cast<int>(hudData_.remainingTime) % 60;
	char timeStr[32];
	snprintf(timeStr, sizeof(timeStr), "TIME %02d:%02d", minutes, seconds);

	// テキスト描画 (TextRenderer)
	TextRenderer::GetInstance()->Print(
		"HackGen",
		timeStr,
		hudData_.screenWidth * 0.5f,
		panelY + 10.0f,
		26.0f,
		textColor,
		{ 0.5f, 0.0f }
	);
}

void StageHUD::DrawAmmo() {
	float panelW = 230.0f;
	float panelH = 76.0f;
	float panelX = hudData_.screenWidth - panelW - 24.0f;
	float panelY = hudData_.screenHeight - panelH - 24.0f;

	Vector4 bgColor = { 0.03f, 0.06f, 0.10f, 0.75f };
	Vector4 borderColor = { 0.20f, 0.45f, 0.65f, 0.85f };

	float ammoRatio = (hudData_.maxAmmo > 0)
		? static_cast<float>(hudData_.currentAmmo) / static_cast<float>(hudData_.maxAmmo)
		: 0.0f;

	// 残弾数に応じたステータスカラー
	Vector4 ammoTextColor = { 0.90f, 0.95f, 1.0f, 1.0f };
	Vector4 gaugeFillColor = { 0.20f, 0.85f, 0.45f, 0.95f }; // 通常: 緑

	if (hudData_.currentAmmo == 0) {
		// 弾切れ
		float pulse = 0.5f + 0.5f * std::sin(animTimer_ * 12.0f);
		ammoTextColor = { 1.0f, 0.20f, 0.20f, pulse };
		gaugeFillColor = { 1.0f, 0.20f, 0.20f, pulse };
		borderColor = { 1.0f, 0.30f, 0.30f, 0.80f };
	} else if (ammoRatio <= 0.15f) {
		// 15%以下: 赤色警告
		ammoTextColor = { 1.0f, 0.30f, 0.30f, 1.0f };
		gaugeFillColor = { 1.0f, 0.30f, 0.30f, 0.95f };
	} else if (ammoRatio <= 0.35f) {
		// 35%以下: 黄色注意
		ammoTextColor = { 1.0f, 0.85f, 0.25f, 1.0f };
		gaugeFillColor = { 1.0f, 0.80f, 0.20f, 0.95f };
	}

	// 射撃時のパンチ演出
	if (firePunchTimer_ > 0.0f) {
		borderColor = { 0.40f, 0.85f, 1.0f, 1.0f };
	}

	DrawBorderedPanel({ panelX, panelY }, { panelW, panelH }, bgColor, borderColor, 1.5f);

	// ヘッダーラベル
	TextRenderer::GetInstance()->Print(
		"HackGen",
		"GUN 20MM",
		panelX + 16.0f,
		panelY + 8.0f,
		14.0f,
		{ 0.55f, 0.75f, 0.95f, 0.90f }
	);

	// 残弾数数値
	char ammoStr[32];
	if (hudData_.currentAmmo == 0) {
		snprintf(ammoStr, sizeof(ammoStr), "EMPTY");
	} else {
		snprintf(ammoStr, sizeof(ammoStr), "%d / %d", hudData_.currentAmmo, hudData_.maxAmmo);
	}

	TextRenderer::GetInstance()->Print(
		"HackGen",
		ammoStr,
		panelX + panelW - 16.0f,
		panelY + 18.0f,
		24.0f,
		ammoTextColor,
		{ 1.0f, 0.0f }
	);

	// --- 残弾ゲージバー ---
	float gaugeX = panelX + 16.0f;
	float gaugeY = panelY + 52.0f;
	float gaugeW = panelW - 32.0f;
	float gaugeH = 8.0f;

	// スロット背景
	DrawRect({ gaugeX, gaugeY }, { gaugeW, gaugeH }, { 0.08f, 0.12f, 0.16f, 0.85f });
	// スロット枠線
	DrawRect({ gaugeX, gaugeY }, { gaugeW, 1.0f }, { 0.25f, 0.35f, 0.45f, 0.60f });
	DrawRect({ gaugeX, gaugeY + gaugeH - 1.0f }, { gaugeW, 1.0f }, { 0.25f, 0.35f, 0.45f, 0.60f });

	// アクティブゲージ
	float fillW = gaugeW * (std::clamp)(ammoRatio, 0.0f, 1.0f);
	if (fillW > 0.0f) {
		DrawRect({ gaugeX, gaugeY }, { fillW, gaugeH }, gaugeFillColor);
	}

	// ゲージのセグメント区切り線（4等分）
	for (int i = 1; i < 4; ++i) {
		float divX = gaugeX + (gaugeW * 0.25f * i);
		DrawLine({ divX, gaugeY }, { divX, gaugeY + gaugeH }, 1.0f, { 0.03f, 0.06f, 0.10f, 0.90f });
	}
}

void StageHUD::DrawReticles() {
	Vector2 noseScreen{};
	Vector2 aimScreen{};

	// 1. 自機が向いている方向（機首方向 / ボアシサイト照準）
	Vector3 noseTargetWorld = Add(hudData_.aircraftPosition, Multiply(500.0f, hudData_.aircraftForward));
	bool noseVisible = ProjectToScreen(noseTargetWorld, hudData_.viewProjectionMatrix, hudData_.screenWidth, hudData_.screenHeight, noseScreen);

	// 2. マウスの向き（目標照準）
	bool aimVisible = false;
	if (hudData_.mouseAimEnabled) {
		Vector3 aimTargetWorld = Add(hudData_.aircraftPosition, Multiply(500.0f, hudData_.mouseAimTargetDirection));
		aimVisible = ProjectToScreen(aimTargetWorld, hudData_.viewProjectionMatrix, hudData_.screenWidth, hudData_.screenHeight, aimScreen);
	}

	// -----------------------------------------------------------------
	// リードガイダンスライン（機首照準とマウス目標照準を結ぶHUDライン）
	// -----------------------------------------------------------------
	if (noseVisible && aimVisible) {
		float dist = std::sqrt(
			(aimScreen.x - noseScreen.x) * (aimScreen.x - noseScreen.x) +
			(aimScreen.y - noseScreen.y) * (aimScreen.y - noseScreen.y)
		);

		if (dist > 12.0f) {
			// 半透明のHUDコネクトライン
			Vector4 lineCol = { 0.0f, 0.95f, 0.50f, 0.40f };
			DrawLine(noseScreen, aimScreen, 1.5f, lineCol);

			// 中間地点に小さなガイドドット
			Vector2 midPos = {
				(noseScreen.x + aimScreen.x) * 0.5f,
				(noseScreen.y + aimScreen.y) * 0.5f
			};
			DrawDot(midPos, 2.0f, { 0.0f, 0.95f, 0.50f, 0.60f });
		}
	}

	// -----------------------------------------------------------------
	// マウス目標照準（サークル＋十字目盛＋中心ドット）
	// -----------------------------------------------------------------
	if (aimVisible) {
		float reticleRadius = 18.0f;
		Vector4 reticleColor = { 0.0f, 0.95f, 0.45f, 0.85f };
		Vector4 tickColor = { 0.0f, 1.0f, 0.55f, 0.95f };

		// 外周リング
		DrawRing(aimScreen, reticleRadius, reticleColor);

		// 中心ドット
		DrawDot(aimScreen, 2.5f, tickColor);

		// 外周4箇所の十字目盛（上下左右）
		float tickInner = reticleRadius + 3.0f;
		float tickOuter = reticleRadius + 10.0f;

		// 上
		DrawLine({ aimScreen.x, aimScreen.y - tickOuter }, { aimScreen.x, aimScreen.y - tickInner }, 1.5f, tickColor);
		// 下
		DrawLine({ aimScreen.x, aimScreen.y + tickInner }, { aimScreen.x, aimScreen.y + tickOuter }, 1.5f, tickColor);
		// 左
		DrawLine({ aimScreen.x - tickOuter, aimScreen.y }, { aimScreen.x - tickInner, aimScreen.y }, 1.5f, tickColor);
		// 右
		DrawLine({ aimScreen.x + tickInner, aimScreen.y }, { aimScreen.x + tickOuter, aimScreen.y }, 1.5f, tickColor);
	}

	// -----------------------------------------------------------------
	// 自機が向いている方向（戦闘機HUD風ガンクロス / ボアシサイト）
	// -----------------------------------------------------------------
	if (noseVisible) {
		float pulseScale = (firePunchTimer_ > 0.0f) ? 1.25f : 1.0f;
		Vector4 crossColor = (firePunchTimer_ > 0.0f)
			? Vector4{ 1.0f, 0.85f, 0.40f, 1.0f }  // 射撃時フラッシュ
			: Vector4{ 0.95f, 0.98f, 1.0f, 0.90f }; // 通常: クリアホワイト

		// 中心小リング
		DrawRing(noseScreen, 5.0f * pulseScale, crossColor);

		// 水平ウイングバー（左右の翼マーク）
		float wingInner = 8.0f * pulseScale;
		float wingOuter = 22.0f * pulseScale;
		// 左ウイング
		DrawLine({ noseScreen.x - wingOuter, noseScreen.y }, { noseScreen.x - wingInner, noseScreen.y }, 2.0f, crossColor);
		// 右ウイング
		DrawLine({ noseScreen.x + wingInner, noseScreen.y }, { noseScreen.x + wingOuter, noseScreen.y }, 2.0f, crossColor);

		// 上部ピッチマーク
		float topInner = 8.0f * pulseScale;
		float topOuter = 15.0f * pulseScale;
		DrawLine({ noseScreen.x, noseScreen.y - topOuter }, { noseScreen.x, noseScreen.y - topInner }, 2.0f, crossColor);
	}
}
