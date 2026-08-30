#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "ResultScene.h"
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

ResultScene::ResultScene() {
	sceneID = SCENE::RESULT;
}

ResultScene::~ResultScene() {
}

void ResultScene::Initialize() {
	Input::GetInstance()->UnlockCursor();
	selectionManager_.Clear();
	animTimer_ = 0.0f;
	fadeInAlpha_ = 0.0f;

	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

	// 必須テクスチャのロード
	TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");
	
	// ============================
	// 背景オーバーレイ
	// ============================
	backgroundPanel_.Initialize(spriteCommon);
	backgroundPanel_.SetPosition({ 0.0f, 0.0f });
	backgroundPanel_.SetSize({ kScreenWidth, kScreenHeight });
	backgroundPanel_.SetBackgroundColor({ 0.08f, 0.01f, 0.01f, 0.85f }); // 暗い赤色

	// ============================
	// 「GAME OVER」テキスト
	// ============================
	gameOverText_.Initialize("Roboto", "GAME OVER", 64.0f);
	gameOverText_.SetAnchorPoint({ 0.5f, 0.0f });
	gameOverText_.SetPosition({ kScreenWidth * 0.5f, 150.0f });
	gameOverText_.SetColor({ 0.90f, 0.20f, 0.20f, 1.0f }); // 真紅

	// ============================
	// ボタン
	// ============================
	float buttonWidth = 250.0f;
	float buttonHeight = 50.0f;
	float buttonSpacing = 30.0f;
	float buttonsStartY = 450.0f;
	
	// 2つのボタンを横並びに中央配置
	float totalButtonsWidth = buttonWidth * 2.0f + buttonSpacing;
	float leftButtonX = (kScreenWidth - totalButtonsWidth) * 0.5f;

	retryButton_.Initialize(spriteCommon, "RETRY", 28.0f);
	retryButton_.SetPosition({ leftButtonX, buttonsStartY });
	retryButton_.SetSize({ buttonWidth, buttonHeight });
	retryButton_.SetNormalColor({ 0.20f, 0.10f, 0.10f, 0.85f });
	retryButton_.SetHoverColor({ 0.40f, 0.12f, 0.12f, 0.95f });
	retryButton_.SetSelectedColor({ 0.50f, 0.15f, 0.15f, 0.95f });
	retryButton_.SetOnClick([this]() {
		sceneID = SCENE::STAGE;
	});

	titleButton_.Initialize(spriteCommon, "TITLE", 28.0f);
	titleButton_.SetPosition({ leftButtonX + buttonWidth + buttonSpacing, buttonsStartY });
	titleButton_.SetSize({ buttonWidth, buttonHeight });
	titleButton_.SetNormalColor({ 0.20f, 0.10f, 0.10f, 0.85f });
	titleButton_.SetHoverColor({ 0.40f, 0.12f, 0.12f, 0.95f });
	titleButton_.SetSelectedColor({ 0.50f, 0.15f, 0.15f, 0.95f });
	titleButton_.SetOnClick([this]() {
		sceneID = SCENE::TITLE;
	});

	selectionManager_.AddButton(&retryButton_);
	selectionManager_.AddButton(&titleButton_);

	// UITextRegistryに登録
	UITextRegistry::GetInstance()->Register("Result_GameOver", &gameOverText_);
	UITextRegistry::GetInstance()->Register("Result_BtnRetry", retryButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Result_BtnTitle", titleButton_.GetLabelText());
}

void ResultScene::Update() {
	// ============================
	// アニメーション更新
	// ============================
	animTimer_ += 1.0f / 60.0f;
	
	// フェードイン
	if (fadeInAlpha_ < 1.0f) {
		fadeInAlpha_ += 0.02f;
		if (fadeInAlpha_ > 1.0f) fadeInAlpha_ = 1.0f;
	}

	selectionManager_.Update();
	gameOverText_.Update();

#ifdef USE_IMGUI
	ImGui::Begin("RESULT SCENE DEBUG");
	ImGui::Text("AnimTimer: %.2f", animTimer_);
	ImGui::End();
#endif
}

void ResultScene::Draw() {
	// 3D背景などがあれば描画（ClearSceneと同じく、ここでは特に描画せずUIのみ）
}

void ResultScene::DrawUI() {
	backgroundPanel_.Draw();
	gameOverText_.Draw();
	
	// ボタンがフェードインするようにアルファ適用
	retryButton_.Draw();
	titleButton_.Draw();
}

void ResultScene::Finalize() {
	UITextRegistry::GetInstance()->Clear();
}
