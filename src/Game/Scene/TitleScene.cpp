#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "TitleScene.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/UI/UITextRegistry.h"
#include "WinApp.h"
#include <cmath>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

// 画面サイズ定数
static constexpr float kScreenWidth = static_cast<float>(WinApp::kClientWidth);   // 1280
static constexpr float kScreenHeight = static_cast<float>(WinApp::kClientHeight); // 720

void TitleScene::Initialize() {
	Input::GetInstance()->UnlockCursor(); // メニュー用にマウスを表示・ロック解除
	sceneID = SCENE::TITLE;
	selectionManager_.Clear();
	animTimer_ = 0.0f;

	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

	// 必須テクスチャのロード
	TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");

	// ============================
	// 背景オーバーレイ（暗い半透明パネル）
	// ============================
	backgroundPanel_.Initialize(spriteCommon);
	backgroundPanel_.SetPosition({ 0.0f, 0.0f });
	backgroundPanel_.SetSize({ kScreenWidth, kScreenHeight });
	backgroundPanel_.SetBackgroundColor({ 0.02f, 0.03f, 0.08f, 0.95f });

	// ============================
	// タイトルテキスト
	// ============================
	titleText_.Initialize("Roboto", "戦雷", 80.0f);
	titleText_.SetAnchorPoint({ 0.5f, 0.0f });
	titleText_.SetPosition({ kScreenWidth * 0.5f, 120.0f });
	titleText_.SetColor({ 0.95f, 0.90f, 0.70f, 1.0f }); // ゴールド系

	// ============================
	// サブタイトルテキスト
	// ============================
	subtitleText_.Initialize("Roboto", "- THUNDER OF WAR -", 20.0f);
	subtitleText_.SetAnchorPoint({ 0.5f, 0.0f });
	subtitleText_.SetPosition({ kScreenWidth * 0.5f, 220.0f });
	subtitleText_.SetColor({ 0.6f, 0.65f, 0.75f, 0.8f });

	// ============================
	// メニューボタン
	// ============================
	float buttonWidth = 300.0f;
	float buttonHeight = 55.0f;
	float buttonX = (kScreenWidth - buttonWidth) * 0.5f;
	float buttonStartY = 340.0f;
	float buttonSpacing = 75.0f;

	// START ボタン
	startButton_.Initialize(spriteCommon, "START", 30.0f);
	startButton_.SetPosition({ buttonX, buttonStartY });
	startButton_.SetSize({ buttonWidth, buttonHeight });
	startButton_.SetNormalColor({ 0.10f, 0.12f, 0.18f, 0.85f });
	startButton_.SetHoverColor({ 0.15f, 0.25f, 0.45f, 0.95f });
	startButton_.SetSelectedColor({ 0.18f, 0.35f, 0.60f, 0.95f });
	startButton_.SetOnClick([this]() {
		sceneID = SCENE::STAGE;
	});

	// SETTINGS ボタン
	settingsButton_.Initialize(spriteCommon, "SETTINGS", 30.0f);
	settingsButton_.SetPosition({ buttonX, buttonStartY + buttonSpacing });
	settingsButton_.SetSize({ buttonWidth, buttonHeight });
	settingsButton_.SetNormalColor({ 0.10f, 0.12f, 0.18f, 0.85f });
	settingsButton_.SetHoverColor({ 0.15f, 0.25f, 0.45f, 0.95f });
	settingsButton_.SetSelectedColor({ 0.18f, 0.35f, 0.60f, 0.95f });
	settingsButton_.SetOnClick([this]() {
		// 設定画面は将来実装
	});

	// EXIT ボタン
	exitButton_.Initialize(spriteCommon, "EXIT", 30.0f);
	exitButton_.SetPosition({ buttonX, buttonStartY + buttonSpacing * 2.0f });
	exitButton_.SetSize({ buttonWidth, buttonHeight });
	exitButton_.SetNormalColor({ 0.10f, 0.12f, 0.18f, 0.85f });
	exitButton_.SetHoverColor({ 0.40f, 0.15f, 0.15f, 0.95f });
	exitButton_.SetSelectedColor({ 0.55f, 0.18f, 0.18f, 0.95f });
	exitButton_.SetOnClick([]() {
		PostQuitMessage(0);
	});

	selectionManager_.AddButton(&startButton_);
	selectionManager_.AddButton(&settingsButton_);
	selectionManager_.AddButton(&exitButton_);

	// UITextRegistryに登録（エディタから編集可能にする）
	UITextRegistry::GetInstance()->Register("Title_Main", &titleText_);
	UITextRegistry::GetInstance()->Register("Title_Sub", &subtitleText_);
	UITextRegistry::GetInstance()->Register("Title_BtnStart", startButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Title_BtnSettings", settingsButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Title_BtnExit", exitButton_.GetLabelText());
}

void TitleScene::Update() {
	animTimer_ += 1.0f / 60.0f;

	selectionManager_.Update();

	// ============================
	// 各UI要素の更新
	// ============================
	backgroundPanel_.Update();
	titleText_.Update();
	subtitleText_.Update();

#ifdef USE_IMGUI
	ImGui::Begin("TITLE SCENE DEBUG");
	ImGui::Text("AnimTimer: %.2f", animTimer_);
	ImGui::End();
#endif
}

void TitleScene::Draw() {
	// 3Dオブジェクト等があればここに記述
}

void TitleScene::DrawUI() {

	// 背景パネル
	backgroundPanel_.Draw();

	// タイトルテキスト
	titleText_.Draw();

	// サブタイトル
	subtitleText_.Draw();

	// メニューボタン
	startButton_.Draw();
	settingsButton_.Draw();
	exitButton_.Draw();

	// 操作ガイド
	TextRenderer::GetInstance()->Print(
		"Roboto",
		"↑↓: SELECT   SPACE/ENTER: DECIDE",
		kScreenWidth * 0.5f - 170.0f,
		kScreenHeight - 60.0f,
		16.0f,
		{ 0.5f, 0.5f, 0.55f, 0.6f }
	);
}

void TitleScene::Finalize() {
	UITextRegistry::GetInstance()->Clear();
}
