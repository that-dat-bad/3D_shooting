#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "GameOverScene.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../System/GameResult.h"
#include "../../engine/Graphics/UI/UITextRegistry.h"
#include "WinApp.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

// 画面サイズ定数
static constexpr float kScreenWidth = static_cast<float>(WinApp::kClientWidth);
static constexpr float kScreenHeight = static_cast<float>(WinApp::kClientHeight);

GameOverScene::GameOverScene() {
	sceneID = SCENE::GAMEOVER;
}

GameOverScene::~GameOverScene() {
}

void GameOverScene::Initialize() {
	Input::GetInstance()->UnlockCursor();
	selectionManager_.Clear();
	animTimer_ = 0.0f;
	fadeInAlpha_ = 0.0f;
	isRebooting_ = false;
	rebootTimer_ = 0.0f;
	dumpScrollIndex_ = 0;
	dumpScrollTimer_ = 0.0f;

	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

	// 必須テクスチャのロード
	TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");

	// 黒フェード用スプライト
	rebootBlackoutSprite_ = std::make_unique<Sprite>();
	rebootBlackoutSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
	
	// ============================
	// 項目3: 背景オーバーレイ（深紅のミリタリーOSカーネルパニック）
	// ============================
	backgroundPanel_.Initialize(spriteCommon);
	backgroundPanel_.SetPosition({ 0.0f, 0.0f });
	backgroundPanel_.SetSize({ kScreenWidth, kScreenHeight });
	backgroundPanel_.SetBackgroundColor({ 0.18f, 0.015f, 0.015f, 0.95f });

	// メモリダンププール生成（ミリタリーOSのブルースクリーン風: テキスト描画負荷を最小限に凝縮）
	memoryDumpPool_.clear();
	const char* const kDumpTemplates[] = {
		"CRITICAL_PROCESS_DIED: DAWN_CORE_HAL.SYS",
		"EXCEPTION_RECORD: 0xC0000005 (ACCESS_VIOLATION)",
		"STACK_TRACE: 0x7FFF9821A4B0 0x7FFF983060AA",
		"RAX: 0x00000000 RBX: 0x7FFF9840 RCX: 0xC0000005",
		"PAGE_FAULT_IN_NONPAGED: 0xDEADF00D",
		"DUMP [0x00]: 48 89 5C 24 08 48 89 74 24 10 57",
		"DUMP [0x10]: 8B F9 E8 4E FE FF FF 48 8B D8 48",
		"BUS_ERROR: TELEMETRY CARRIER DROP (LINK_LOST)",
		"WATCHDOG_TIMEOUT: MISSILE_GUIDANCE (3000ms)",
		"KERNEL_SECURITY_FAILURE: FLIGHT_CTRL_M21",
		"DUMPING PHYSICAL MEMORY: 100% COMPLETE"
	};
	for (int repeat = 0; repeat < 2; ++repeat) {
		for (const auto* line : kDumpTemplates) {
			char buf[96];
			snprintf(buf, sizeof(buf), "[0x%08X] %s", 0x10000000 + static_cast<int>(memoryDumpPool_.size()) * 0x40, line);
			memoryDumpPool_.push_back(buf);
		}
	}

	// ============================
	// 「GAME OVER」メインヘッダー
	// ============================
	gameOverText_.Initialize("HackGen", "MISSION FAILED // SYSTEM DOWN", 42.0f);
	gameOverText_.SetAnchorPoint({ 0.5f, 0.0f });
	gameOverText_.SetPosition({ kScreenWidth * 0.5f, 75.0f });
	gameOverText_.SetColor({ 1.0f, 0.25f, 0.25f, 1.0f });

	// ============================
	// ボタン配置
	// ============================
	float buttonWidth = 260.0f;
	float buttonHeight = 52.0f;
	float buttonSpacing = 35.0f;
	float buttonsStartY = kScreenHeight - 110.0f;
	
	float totalButtonsWidth = buttonWidth * 2.0f + buttonSpacing;
	float leftButtonX = (kScreenWidth - totalButtonsWidth) * 0.5f;

	retryButton_.Initialize(spriteCommon, "RETRY MISSION", 22.0f);
	retryButton_.SetPosition({ leftButtonX, buttonsStartY });
	retryButton_.SetSize({ buttonWidth, buttonHeight });
	retryButton_.SetNormalColor({ 0.28f, 0.05f, 0.05f, 0.90f });
	retryButton_.SetHoverColor({ 0.55f, 0.12f, 0.12f, 0.98f });
	retryButton_.SetSelectedColor({ 0.70f, 0.18f, 0.18f, 1.0f });
	retryButton_.SetOnClick([this]() {
		sceneID = SCENE::STAGE;
	});

	// 項目8: TITLE ボタン押下時にリブートシーケンス開始
	titleButton_.Initialize(spriteCommon, "REBOOT OS (TITLE)", 22.0f);
	titleButton_.SetPosition({ leftButtonX + buttonWidth + buttonSpacing, buttonsStartY });
	titleButton_.SetSize({ buttonWidth, buttonHeight });
	titleButton_.SetNormalColor({ 0.28f, 0.05f, 0.05f, 0.90f });
	titleButton_.SetHoverColor({ 0.55f, 0.12f, 0.12f, 0.98f });
	titleButton_.SetSelectedColor({ 0.70f, 0.18f, 0.18f, 1.0f });
	titleButton_.SetOnClick([this]() {
		isRebooting_ = true;
		rebootTimer_ = 0.0f;
	});

	selectionManager_.AddButton(&retryButton_);
	selectionManager_.AddButton(&titleButton_);

	UITextRegistry::GetInstance()->Register("GameOver_Header", &gameOverText_);
	UITextRegistry::GetInstance()->Register("GameOver_BtnRetry", retryButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("GameOver_BtnTitle", titleButton_.GetLabelText());
}

void GameOverScene::Update() {
	animTimer_ += 1.0f / 60.0f;
	
	// フェードイン
	if (fadeInAlpha_ < 1.0f) {
		fadeInAlpha_ += 0.04f;
		if (fadeInAlpha_ > 1.0f) fadeInAlpha_ = 1.0f;
	}

	// 項目3: 16進数メモリダンプのリアルタイム高速スクロール
	dumpScrollTimer_ += 1.0f / 60.0f;
	if (dumpScrollTimer_ >= 0.035f) {
		dumpScrollTimer_ = 0.0f;
		dumpScrollIndex_ = (dumpScrollIndex_ + 1) % (std::max)(1, static_cast<int>(memoryDumpPool_.size()));
	}

	// 項目8: リブート演出進行（0.50秒でキビキビとタイトルへ復帰）
	if (isRebooting_) {
		rebootTimer_ += 1.0f / 60.0f;
		if (rebootTimer_ >= 0.50f) {
			// タイトルシーンへ「RebootFromGameOver」理由で遷移！
			GameResult::GetInstance().titleEntryReason = TitleEntryReason::RebootFromGameOver;
			sceneID = SCENE::TITLE;
			return;
		}
	} else {
		selectionManager_.Update();
	}

	gameOverText_.Update();

#ifdef USE_IMGUI
	ImGui::Begin("GAME OVER SCENE DEBUG");
	ImGui::Text("AnimTimer: %.2f", animTimer_);
	ImGui::Text("Rebooting: %s (%.2f)", isRebooting_ ? "YES" : "NO", rebootTimer_);
	ImGui::End();
#endif
}

void GameOverScene::Draw() {
}

void GameOverScene::DrawUI() {
	// 背景パネル（深紅）
	backgroundPanel_.Draw();

	// 上部カーネルパニック警告ヘッダーバー
	TextRenderer::GetInstance()->Print(
		"HackGen",
		"*** DAWN-OS CRITICAL KERNEL PANIC: EXCEPTION_STAGE_FAILURE (0xC0000005) ***",
		kScreenWidth * 0.5f,
		24.0f,
		18.0f,
		{ 1.0f, 0.40f, 0.35f, 0.95f },
		{ 0.5f, 0.0f }
	);

	// メインタイトル
	gameOverText_.Draw();

	// 項目3: 16進数メモリエラーログ（6行に厳選してDrawCallオーバーヘッドを劇的削減）
	float dumpStartX = 120.0f;
	float dumpStartY = 160.0f;
	int displayLineCount = 6;

	for (int i = 0; i < displayLineCount; ++i) {
		int idx = (dumpScrollIndex_ + i) % static_cast<int>(memoryDumpPool_.size());
		float lineY = dumpStartY + i * 28.0f;

		float lineAlpha = 0.90f - (static_cast<float>(i) / static_cast<float>(displayLineCount)) * 0.25f;
		Vector4 textColor = (i == 0 || i == 3)
			? Vector4{ 1.0f, 0.90f, 0.40f, lineAlpha }  // 警告行
			: Vector4{ 0.95f, 0.35f, 0.35f, lineAlpha }; // 通常赤行

		TextRenderer::GetInstance()->Print(
			"HackGen",
			memoryDumpPool_[idx].c_str(),
			dumpStartX,
			lineY,
			16.0f,
			textColor
		);
	}

	// 区切り線
	TextRenderer::GetInstance()->Print(
		"HackGen",
		"----------------------------------------------------------------",
		kScreenWidth * 0.5f,
		dumpStartY + displayLineCount * 28.0f + 8.0f,
		14.0f,
		{ 0.70f, 0.20f, 0.20f, 0.80f },
		{ 0.5f, 0.0f }
	);

	// 操作案内
	TextRenderer::GetInstance()->Print(
		"HackGen",
		"SYSTEM CRASH REPORT COMPLETED // SELECT ACTION TO CONTINUE",
		kScreenWidth * 0.5f,
		kScreenHeight - 140.0f,
		16.0f,
		{ 1.0f, 0.75f, 0.75f, 0.90f },
		{ 0.5f, 0.0f }
	);

	// ボタン描画
	retryButton_.Draw();
	titleButton_.Draw();

	// ------------------------------------------------------------
	// 項目8: システム再起動（リブート）演出オーバーレイ
	// ------------------------------------------------------------
	if (isRebooting_ && rebootBlackoutSprite_) {
		float blackAlpha = (std::min)(1.0f, rebootTimer_ / 0.35f);
		rebootBlackoutSprite_->SetPosition({ 0.0f, 0.0f });
		rebootBlackoutSprite_->SetSize({ kScreenWidth, kScreenHeight });
		rebootBlackoutSprite_->SetColor({ 0.0f, 0.0f, 0.0f, blackAlpha });
		rebootBlackoutSprite_->Update();
		rebootBlackoutSprite_->Draw();

		// SYSTEM REBOOTING... 点滅テキスト
		float pulse = 0.7f + 0.3f * std::sin(rebootTimer_ * 30.0f);
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"SYSTEM REBOOTING...",
			kScreenWidth * 0.5f,
			kScreenHeight * 0.5f - 20.0f,
			32.0f,
			{ 1.0f, 0.95f, 0.95f, pulse },
			{ 0.5f, 0.5f }
		);

		TextRenderer::GetInstance()->Print(
			"HackGen",
			"INITIALIZING HARDWARE RESTORE SEQUENCE",
			kScreenWidth * 0.5f,
			kScreenHeight * 0.5f + 25.0f,
			16.0f,
			{ 0.75f, 0.75f, 0.80f, 0.85f },
			{ 0.5f, 0.5f }
		);
	}
}

void GameOverScene::Finalize() {
	UITextRegistry::GetInstance()->Clear();
}
