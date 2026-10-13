#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "ClearScene.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../System/GameResult.h"
#include "../../engine/Graphics/UI/UITextRegistry.h"
#include "WinApp.h"
#include <cmath>
#include <cstdio>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

// 画面サイズ定数
static constexpr float kScreenWidth = static_cast<float>(WinApp::kClientWidth);
static constexpr float kScreenHeight = static_cast<float>(WinApp::kClientHeight);

ClearScene::ClearScene() {
	sceneID = SCENE::CLEAR;
}

ClearScene::~ClearScene() {
}

void ClearScene::Initialize() {
	Input::GetInstance()->UnlockCursor();
	selectionManager_.Clear();
	animTimer_ = 0.0f;
	fadeInAlpha_ = 0.0f;

	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

	// 必須テクスチャのロード
	TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");
	const GameResult& result = GameResult::GetInstance();

	// ============================
	// 背景オーバーレイ
	// ============================
	backgroundPanel_.Initialize(spriteCommon);
	backgroundPanel_.SetPosition({ 0.0f, 0.0f });
	backgroundPanel_.SetSize({ kScreenWidth, kScreenHeight });
	backgroundPanel_.SetBackgroundColor({ 0.01f, 0.02f, 0.05f, 0.90f });

	// ============================
	// 「MISSION COMPLETE」テキスト
	// ============================
	missionCompleteText_.Initialize("HackGen", "MISSION COMPLETE", 56.0f);
	missionCompleteText_.SetAnchorPoint({ 0.5f, 0.0f });
	missionCompleteText_.SetPosition({ kScreenWidth * 0.5f, 60.0f });
	missionCompleteText_.SetColor({ 0.90f, 0.85f, 0.40f, 1.0f }); // ゴールド

	// ============================
	// リザルト情報パネル
	// ============================
	float panelWidth = 500.0f;
	float panelHeight = 220.0f;
	float panelX = (kScreenWidth - panelWidth) * 0.5f;
	float panelY = 160.0f;

	resultPanel_.Initialize(spriteCommon);
	resultPanel_.SetPosition({ panelX, panelY });
	resultPanel_.SetSize({ panelWidth, panelHeight });
	resultPanel_.SetBackgroundColor({ 0.08f, 0.10f, 0.15f, 0.80f });

	// --- リザルト情報テキスト ---
	float labelX = panelX + 40.0f;
	float valueX = panelX + panelWidth - 180.0f;
	float rowHeight = 50.0f;
	float startY = panelY + 25.0f;
	float fontSize = 24.0f;
	Vector4 labelColor = { 0.65f, 0.70f, 0.80f, 1.0f };
	Vector4 valueColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 撃破数
	enemiesDestroyedLabel_.Initialize("HackGen", "ENEMIES DESTROYED", fontSize);
	enemiesDestroyedLabel_.SetPosition({ labelX, startY });
	enemiesDestroyedLabel_.SetColor(labelColor);

	char enemyBuf[32];
	snprintf(enemyBuf, sizeof(enemyBuf), "%d", result.enemiesDestroyed);
	enemiesDestroyedValue_.Initialize("HackGen", enemyBuf, 32.0f);
	enemiesDestroyedValue_.SetAnchorPoint({ 1.0f, 0.0f });
	enemiesDestroyedValue_.SetPosition({ panelX + panelWidth - 40.0f, startY - 2.0f });
	enemiesDestroyedValue_.SetColor(valueColor);

	// クリアタイム
	clearTimeLabel_.Initialize("HackGen", "CLEAR TIME", fontSize);
	clearTimeLabel_.SetPosition({ labelX, startY + rowHeight });
	clearTimeLabel_.SetColor(labelColor);

	int minutes = static_cast<int>(result.clearTimeSeconds) / 60;
	int seconds = static_cast<int>(result.clearTimeSeconds) % 60;
	char timeBuf[32];
	snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", minutes, seconds);
	clearTimeValue_.Initialize("HackGen", timeBuf, 32.0f);
	clearTimeValue_.SetAnchorPoint({ 1.0f, 0.0f });
	clearTimeValue_.SetPosition({ panelX + panelWidth - 40.0f, startY + rowHeight - 2.0f });
	clearTimeValue_.SetColor(valueColor);

	// 残りHP
	hpRemainingLabel_.Initialize("HackGen", "HP REMAINING", fontSize);
	hpRemainingLabel_.SetPosition({ labelX, startY + rowHeight * 2.0f });
	hpRemainingLabel_.SetColor(labelColor);

	int hpPercent = 0;
	if (result.playerMaxHP > 0.0f) {
		hpPercent = static_cast<int>((result.playerHPRemaining / result.playerMaxHP) * 100.0f);
	}
	char hpBuf[32];
	snprintf(hpBuf, sizeof(hpBuf), "%d%%", hpPercent);
	hpRemainingValue_.Initialize("HackGen", hpBuf, 32.0f);
	hpRemainingValue_.SetAnchorPoint({ 1.0f, 0.0f });
	hpRemainingValue_.SetPosition({ panelX + panelWidth - 40.0f, startY + rowHeight * 2.0f - 2.0f });
	// HPに応じて色を変更
	if (hpPercent >= 70) {
		hpRemainingValue_.SetColor({ 0.3f, 1.0f, 0.4f, 1.0f }); // 緑
	} else if (hpPercent >= 30) {
		hpRemainingValue_.SetColor({ 1.0f, 0.9f, 0.3f, 1.0f }); // 黄
	} else {
		hpRemainingValue_.SetColor({ 1.0f, 0.3f, 0.3f, 1.0f }); // 赤
	}

	// ============================
	// ボタン
	// ============================
	float buttonWidth = 250.0f;
	float buttonHeight = 50.0f;
	float buttonSpacing = 30.0f;
	float buttonsStartY = panelY + panelHeight + 50.0f;
	// 2つのボタンを横並びに中央配置
	float totalButtonsWidth = buttonWidth * 2.0f + buttonSpacing;
	float leftButtonX = (kScreenWidth - totalButtonsWidth) * 0.5f;

	replayButton_.Initialize(spriteCommon, "REPLAY", 28.0f);
	replayButton_.SetPosition({ leftButtonX, buttonsStartY });
	replayButton_.SetSize({ buttonWidth, buttonHeight });
	replayButton_.SetNormalColor({ 0.10f, 0.15f, 0.22f, 0.85f });
	replayButton_.SetHoverColor({ 0.12f, 0.30f, 0.50f, 0.95f });
	replayButton_.SetSelectedColor({ 0.15f, 0.40f, 0.65f, 0.95f });
	replayButton_.SetOnClick([this]() {
		sceneID = SCENE::STAGE;
	});

	titleButton_.Initialize(spriteCommon, "TITLE", 28.0f);
	titleButton_.SetPosition({ leftButtonX + buttonWidth + buttonSpacing, buttonsStartY });
	titleButton_.SetSize({ buttonWidth, buttonHeight });
	titleButton_.SetNormalColor({ 0.10f, 0.15f, 0.22f, 0.85f });
	titleButton_.SetHoverColor({ 0.12f, 0.30f, 0.50f, 0.95f });
	titleButton_.SetSelectedColor({ 0.15f, 0.40f, 0.65f, 0.95f });
	// 項目9: TITLE 押下時にデータアップロード演出を開始
	titleButton_.SetOnClick([this]() {
		isUploading_ = true;
		uploadTimer_ = 0.0f;
	});

	selectionManager_.AddButton(&replayButton_);
	selectionManager_.AddButton(&titleButton_);

	// アップロード用スプライト
	uploadSprite_ = std::make_unique<Sprite>();
	uploadSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
	isUploading_ = false;
	uploadTimer_ = 0.0f;

	// UITextRegistryに登録
	UITextRegistry::GetInstance()->Register("Clear_MissionComplete", &missionCompleteText_);
	UITextRegistry::GetInstance()->Register("Clear_EnemiesLabel", &enemiesDestroyedLabel_);
	UITextRegistry::GetInstance()->Register("Clear_EnemiesValue", &enemiesDestroyedValue_);
	UITextRegistry::GetInstance()->Register("Clear_TimeLabel", &clearTimeLabel_);
	UITextRegistry::GetInstance()->Register("Clear_TimeValue", &clearTimeValue_);
	UITextRegistry::GetInstance()->Register("Clear_HPLabel", &hpRemainingLabel_);
	UITextRegistry::GetInstance()->Register("Clear_HPValue", &hpRemainingValue_);
	UITextRegistry::GetInstance()->Register("Clear_BtnReplay", replayButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Clear_BtnTitle", titleButton_.GetLabelText());
}

void ClearScene::Update() {
	animTimer_ += 1.0f / 60.0f;

	// フェードイン
	if (fadeInAlpha_ < 1.0f) {
		fadeInAlpha_ += 1.5f * (1.0f / 60.0f); // 約0.67秒でフェードイン完了
		if (fadeInAlpha_ > 1.0f) fadeInAlpha_ = 1.0f;
	}

	// 項目9: アップロード＆ウィンドウ閉鎖シーケンス更新
	if (isUploading_) {
		uploadTimer_ += 1.0f / 60.0f;
		if (uploadTimer_ >= 1.50f) {
			// タイトルへ「ReturnFromClear」で遷移！
			GameResult::GetInstance().titleEntryReason = TitleEntryReason::ReturnFromClear;
			sceneID = SCENE::TITLE;
			return;
		}
	} else {
		selectionManager_.Update();
	}

	// 各UI要素の更新
	backgroundPanel_.Update();
	resultPanel_.Update();
	missionCompleteText_.Update();
	enemiesDestroyedLabel_.Update();
	enemiesDestroyedValue_.Update();
	clearTimeLabel_.Update();
	clearTimeValue_.Update();
	hpRemainingLabel_.Update();
	hpRemainingValue_.Update();

#ifdef USE_IMGUI
	ImGui::Begin("CLEAR SCENE DEBUG");
	ImGui::Text("FadeIn: %.2f", fadeInAlpha_);
	ImGui::Text("Uploading: %s (%.2f)", isUploading_ ? "YES" : "NO", uploadTimer_);
	const GameResult& result = GameResult::GetInstance();
	ImGui::Text("Enemies: %d", result.enemiesDestroyed);
	ImGui::Text("Time: %.1fs", result.clearTimeSeconds);
	ImGui::Text("HP: %.1f/%.1f", result.playerHPRemaining, result.playerMaxHP);
	ImGui::End();
#endif
}

void ClearScene::Draw() {
	// 3Dオブジェクト等があればここに記述
}

void ClearScene::DrawUI() {
	// 背景
	backgroundPanel_.Draw();

	// MISSION COMPLETE
	missionCompleteText_.Draw();

	// リザルトパネル
	resultPanel_.Draw();

	// リザルト情報
	enemiesDestroyedLabel_.Draw();
	enemiesDestroyedValue_.Draw();
	clearTimeLabel_.Draw();
	clearTimeValue_.Draw();
	hpRemainingLabel_.Draw();
	hpRemainingValue_.Draw();

	// ボタン
	replayButton_.Draw();
	titleButton_.Draw();

	// 操作ガイド
	TextRenderer::GetInstance()->Print(
		"HackGen",
		"←→: SELECT   SPACE/ENTER: DECIDE",
		kScreenWidth * 0.5f - 170.0f,
		kScreenHeight - 50.0f,
		16.0f,
		{ 0.5f, 0.5f, 0.55f, 0.6f }
	);

	// ------------------------------------------------------------
	// 項目9: 「UPLOADING COMBAT DATA... 100% [OK]」& ウィンドウ縮小演出
	// ------------------------------------------------------------
	if (isUploading_ && uploadSprite_) {
		auto DrawRectHelper = [this](const Vector2& pos, const Vector2& size, const Vector4& col) {
			uploadSprite_->SetPosition(pos);
			uploadSprite_->SetAnchorPoint({ 0.0f, 0.0f });
			uploadSprite_->SetSize(size);
			uploadSprite_->SetColor(col);
			uploadSprite_->Update();
			uploadSprite_->Draw();
		};

		// 0.0s〜1.1s: データアップロードダイアログ
		if (uploadTimer_ < 1.15f) {
			float dlgW = 620.0f;
			float dlgH = 120.0f;
			float dlgX = (kScreenWidth - dlgW) * 0.5f;
			float dlgY = (kScreenHeight - dlgH) * 0.5f + 160.0f;

			// 半透明ダーク背景
			DrawRectHelper({ dlgX, dlgY }, { dlgW, dlgH }, { 0.02f, 0.06f, 0.12f, 0.95f });
			// シアン枠線
			DrawRectHelper({ dlgX, dlgY }, { dlgW, 2.0f }, { 0.25f, 0.85f, 1.0f, 0.95f });
			DrawRectHelper({ dlgX, dlgY + dlgH - 2.0f }, { dlgW, 2.0f }, { 0.25f, 0.85f, 1.0f, 0.95f });
			DrawRectHelper({ dlgX, dlgY }, { 2.0f, dlgH }, { 0.25f, 0.85f, 1.0f, 0.95f });
			DrawRectHelper({ dlgX + dlgW - 2.0f, dlgY }, { 2.0f, dlgH }, { 0.25f, 0.85f, 1.0f, 0.95f });

			// アップロード進行度（0.0s〜0.9sで100%へ到達）
			float prog = std::clamp(uploadTimer_ / 0.85f, 0.0f, 1.0f);
			int pct = static_cast<int>(prog * 100.0f);

			char headerText[64];
			if (pct >= 100) {
				snprintf(headerText, sizeof(headerText), "UPLOADING COMBAT DATA... 100%% [OK]");
			} else {
				snprintf(headerText, sizeof(headerText), "UPLOADING COMBAT DATA... %d%%", pct);
			}

			TextRenderer::GetInstance()->Print(
				"HackGen",
				headerText,
				dlgX + 24.0f,
				dlgY + 16.0f,
				20.0f,
				(pct >= 100) ? Vector4{ 0.35f, 1.0f, 0.60f, 1.0f } : Vector4{ 0.30f, 0.85f, 1.0f, 0.95f }
			);

			// プログレスバー背景
			float barX = dlgX + 24.0f;
			float barY = dlgY + 54.0f;
			float barW = dlgW - 48.0f;
			float barH = 14.0f;
			DrawRectHelper({ barX, barY }, { barW, barH }, { 0.05f, 0.12f, 0.20f, 0.80f });

			// プログレスバー本体
			DrawRectHelper({ barX, barY }, { barW * prog, barH }, (pct >= 100) ? Vector4{ 0.20f, 0.95f, 0.50f, 0.95f } : Vector4{ 0.20f, 0.75f, 1.0f, 0.90f });

			TextRenderer::GetInstance()->Print(
				"HackGen",
				(pct >= 100) ? "STATUS: SYNCHRONIZATION COMPLETE // CLOSING CLIENT" : "ENCRYPTING TELEMETRY STREAM VIA SECURE LINK...",
				dlgX + 24.0f,
				dlgY + 82.0f,
				14.0f,
				{ 0.70f, 0.80f, 0.90f, 0.80f }
			);
		} else {
			// 1.15s〜1.50s: 戦闘画面自体が「OSウィンドウ」として中央へ縮小されカチッと閉じる演出！
			float closeT = std::clamp((uploadTimer_ - 1.15f) / 0.35f, 0.0f, 1.0f);
			float winScale = 1.0f - closeT * closeT;
			float halfW = (kScreenWidth * 0.5f) * winScale;
			float halfH = (kScreenHeight * 0.5f) * winScale;
			float centerX = kScreenWidth * 0.5f;
			float centerY = kScreenHeight * 0.5f;

			// 外側4つの黒マスクでウィンドウ縮小を表現
			// 上
			DrawRectHelper({ 0.0f, 0.0f }, { kScreenWidth, centerY - halfH }, { 0.0f, 0.0f, 0.0f, 1.0f });
			// 下
			DrawRectHelper({ 0.0f, centerY + halfH }, { kScreenWidth, kScreenHeight - (centerY + halfH) }, { 0.0f, 0.0f, 0.0f, 1.0f });
			// 左
			DrawRectHelper({ 0.0f, centerY - halfH }, { centerX - halfW, halfH * 2.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });
			// 右
			DrawRectHelper({ centerX + halfW, centerY - halfH }, { kScreenWidth - (centerX + halfW), halfH * 2.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });

			// 縮小ウィンドウのOS枠線
			DrawRectHelper({ centerX - halfW, centerY - halfH }, { halfW * 2.0f, 2.0f }, { 0.30f, 0.70f, 1.0f, 0.90f });
			DrawRectHelper({ centerX - halfW, centerY + halfH - 2.0f }, { halfW * 2.0f, 2.0f }, { 0.30f, 0.70f, 1.0f, 0.90f });
			DrawRectHelper({ centerX - halfW, centerY - halfH }, { 2.0f, halfH * 2.0f }, { 0.30f, 0.70f, 1.0f, 0.90f });
			DrawRectHelper({ centerX + halfW - 2.0f, centerY - halfH }, { 2.0f, halfH * 2.0f }, { 0.30f, 0.70f, 1.0f, 0.90f });
		}
	}
}

void ClearScene::Finalize() {
	UITextRegistry::GetInstance()->Clear();
}


