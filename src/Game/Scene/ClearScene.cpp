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
	missionCompleteText_.Initialize("Roboto", "MISSION COMPLETE", 56.0f);
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
	enemiesDestroyedLabel_.Initialize("Roboto", "ENEMIES DESTROYED", fontSize);
	enemiesDestroyedLabel_.SetPosition({ labelX, startY });
	enemiesDestroyedLabel_.SetColor(labelColor);

	char enemyBuf[32];
	snprintf(enemyBuf, sizeof(enemyBuf), "%d", result.enemiesDestroyed);
	enemiesDestroyedValue_.Initialize("Roboto", enemyBuf, 32.0f);
	enemiesDestroyedValue_.SetAnchorPoint({ 1.0f, 0.0f });
	enemiesDestroyedValue_.SetPosition({ panelX + panelWidth - 40.0f, startY - 2.0f });
	enemiesDestroyedValue_.SetColor(valueColor);

	// クリアタイム
	clearTimeLabel_.Initialize("Roboto", "CLEAR TIME", fontSize);
	clearTimeLabel_.SetPosition({ labelX, startY + rowHeight });
	clearTimeLabel_.SetColor(labelColor);

	int minutes = static_cast<int>(result.clearTimeSeconds) / 60;
	int seconds = static_cast<int>(result.clearTimeSeconds) % 60;
	char timeBuf[32];
	snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", minutes, seconds);
	clearTimeValue_.Initialize("Roboto", timeBuf, 32.0f);
	clearTimeValue_.SetAnchorPoint({ 1.0f, 0.0f });
	clearTimeValue_.SetPosition({ panelX + panelWidth - 40.0f, startY + rowHeight - 2.0f });
	clearTimeValue_.SetColor(valueColor);

	// 残りHP
	hpRemainingLabel_.Initialize("Roboto", "HP REMAINING", fontSize);
	hpRemainingLabel_.SetPosition({ labelX, startY + rowHeight * 2.0f });
	hpRemainingLabel_.SetColor(labelColor);

	int hpPercent = 0;
	if (result.playerMaxHP > 0.0f) {
		hpPercent = static_cast<int>((result.playerHPRemaining / result.playerMaxHP) * 100.0f);
	}
	char hpBuf[32];
	snprintf(hpBuf, sizeof(hpBuf), "%d%%", hpPercent);
	hpRemainingValue_.Initialize("Roboto", hpBuf, 32.0f);
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
	titleButton_.SetOnClick([this]() {
		sceneID = SCENE::TITLE;
	});

	selectionManager_.AddButton(&replayButton_);
	selectionManager_.AddButton(&titleButton_);

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

	selectionManager_.Update();

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

	// MISSION COMPLETE（フェードインの影響を受ける）
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
		"Roboto",
		"←→: SELECT   SPACE/ENTER: DECIDE",
		kScreenWidth * 0.5f - 170.0f,
		kScreenHeight - 50.0f,
		16.0f,
		{ 0.5f, 0.5f, 0.55f, 0.6f }
	);
}

void ClearScene::Finalize() {
	UITextRegistry::GetInstance()->Clear();
}


