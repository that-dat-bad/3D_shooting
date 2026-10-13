#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "GameOverScene.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../System/GameResult.h"
#include "../System/AsyncPreloadManager.h"
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
	fadeInAlpha_ = 0.75f; // 暗転の空白時間を完全排除し、瞬時に画面を展開
	isRebooting_ = false;
	rebootTimer_ = 0.0f;
	dumpScrollIndex_ = 0;
	dumpScrollTimer_ = 0.0f;
	logDeployTimer_ = 0.0f;
	visibleLogCount_ = 0;
	isRetrying_ = false;
	retryGlitchTimer_ = 0.0f;

	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

	// 必須テクスチャのロード
	TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");

	// 黒フェード・CRT収束用スプライト
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
	// ボタン配置（ブラケット [ ] 付きハッカー風デザイン）
	// ============================
	float buttonWidth = 270.0f;
	float buttonHeight = 52.0f;
	float buttonSpacing = 35.0f;
	float buttonsStartY = kScreenHeight - 110.0f;
	
	float totalButtonsWidth = buttonWidth * 2.0f + buttonSpacing;
	float leftButtonX = (kScreenWidth - totalButtonsWidth) * 0.5f;

	retryButton_.Initialize(spriteCommon, "[ RETRY MISSION ]", 22.0f);
	retryButton_.SetPosition({ leftButtonX, buttonsStartY });
	retryButton_.SetSize({ buttonWidth, buttonHeight });
	retryButton_.SetNormalColor({ 0.28f, 0.05f, 0.05f, 0.90f });
	retryButton_.SetHoverColor({ 0.60f, 0.12f, 0.12f, 0.98f });
	retryButton_.SetSelectedColor({ 0.75f, 0.18f, 0.18f, 1.0f });
	retryButton_.SetOnClick([this]() {
		if (!isRetrying_ && !isRebooting_) {
			isRetrying_ = true;
			retryGlitchTimer_ = 0.0f;
			GameResult::GetInstance().isRetry = true;
			// 非同期処理による軽量化：リトライ演出の裏でバックグラウンドスレッドでステージリソースを非同期プリロード！
			AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();
		}
	});

	// 項目8: TITLE ボタン押下時にリブートシーケンス開始
	titleButton_.Initialize(spriteCommon, "[ REBOOT OS (TITLE) ]", 22.0f);
	titleButton_.SetPosition({ leftButtonX + buttonWidth + buttonSpacing, buttonsStartY });
	titleButton_.SetSize({ buttonWidth, buttonHeight });
	titleButton_.SetNormalColor({ 0.28f, 0.05f, 0.05f, 0.90f });
	titleButton_.SetHoverColor({ 0.60f, 0.12f, 0.12f, 0.98f });
	titleButton_.SetSelectedColor({ 0.75f, 0.18f, 0.18f, 1.0f });
	titleButton_.SetOnClick([this]() {
		if (!isRebooting_ && !isRetrying_) {
			isRebooting_ = true;
			rebootTimer_ = 0.0f;
		}
	});

	selectionManager_.AddButton(&retryButton_);
	selectionManager_.AddButton(&titleButton_);

	UITextRegistry::GetInstance()->Register("GameOver_Header", &gameOverText_);
	UITextRegistry::GetInstance()->Register("GameOver_BtnRetry", retryButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("GameOver_BtnTitle", titleButton_.GetLabelText());
}

void GameOverScene::Update() {
	animTimer_ += 1.0f / 60.0f;
	
	// フェードイン（開始0.75から即座に1.0へ到達し空白を完全排除）
	if (fadeInAlpha_ < 1.0f) {
		fadeInAlpha_ += 0.15f;
		if (fadeInAlpha_ > 1.0f) fadeInAlpha_ = 1.0f;
	}

	// 項目3: コンソール風ログ展開アニメーション（1行あたり0.055sで上から順にカタカタ印字）
	if (visibleLogCount_ < 6) {
		logDeployTimer_ += 1.0f / 60.0f;
		if (logDeployTimer_ >= 0.055f) {
			logDeployTimer_ = 0.0f;
			visibleLogCount_++;
		}
	} else {
		// 全6行展開完了後はリアルタイム高速スクロールへ移行
		dumpScrollTimer_ += 1.0f / 60.0f;
		if (dumpScrollTimer_ >= 0.040f) {
			dumpScrollTimer_ = 0.0f;
			dumpScrollIndex_ = (dumpScrollIndex_ + 1) % (std::max)(1, static_cast<int>(memoryDumpPool_.size()));
		}
	}

	// 項目2-1: リトライ時のホワイトノイズ＆グリッチトランジション（0.22sで単調なハードカットを打破）
	if (isRetrying_) {
		retryGlitchTimer_ += 1.0f / 60.0f;
		if (retryGlitchTimer_ >= 0.22f) {
			sceneID = SCENE::STAGE;
			return;
		}
	}
	// 項目3-1: リブート演出進行（SYSTEM REBOOTING... → ブラウン管画面収束でタイトルへ復帰）
	else if (isRebooting_) {
		rebootTimer_ += 1.0f / 60.0f;
		if (rebootTimer_ >= 0.65f) {
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
	ImGui::Text("VisibleLogs: %d/6", visibleLogCount_);
	ImGui::Text("Retrying: %s (%.2f)", isRetrying_ ? "YES" : "NO", retryGlitchTimer_);
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

	// 項目3: 16進数メモリエラーログ（コンソール風順次展開 & 印字中カーソル点滅）
	float dumpStartX = 120.0f;
	float dumpStartY = 160.0f;
	int displayLineCount = visibleLogCount_; // 展開された行数のみ描画

	for (int i = 0; i < displayLineCount; ++i) {
		int idx = (dumpScrollIndex_ + i) % static_cast<int>(memoryDumpPool_.size());
		float lineY = dumpStartY + i * 28.0f;

		float lineAlpha = 0.90f - (static_cast<float>(i) / 6.0f) * 0.20f;
		Vector4 textColor = (i == 0 || i == 3)
			? Vector4{ 1.0f, 0.90f, 0.40f, lineAlpha }  // 警告行
			: Vector4{ 0.95f, 0.35f, 0.35f, lineAlpha }; // 通常赤行

		std::string lineStr = memoryDumpPool_[idx];
		// まだ全展開中で末尾の行の場合、打鍵中カーソル _ を点滅表示
		if (visibleLogCount_ < 6 && i == visibleLogCount_ - 1) {
			lineStr += "_";
		}

		TextRenderer::GetInstance()->Print(
			"HackGen",
			lineStr.c_str(),
			dumpStartX,
			lineY,
			16.0f,
			textColor
		);
	}

	// 展開中にも最下行に点滅カーソル
	if (visibleLogCount_ < 6) {
		float nextY = dumpStartY + visibleLogCount_ * 28.0f;
		float cursorPulse = (std::fmod(animTimer_, 0.20f) < 0.10f) ? 1.0f : 0.0f;
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"> _",
			dumpStartX,
			nextY,
			16.0f,
			{ 1.0f, 0.60f, 0.60f, cursorPulse }
		);
	}

	// 区切り線
	TextRenderer::GetInstance()->Print(
		"HackGen",
		"----------------------------------------------------------------",
		kScreenWidth * 0.5f,
		dumpStartY + 6 * 28.0f + 8.0f,
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
	// 項目2-1: リトライ時のホワイトノイズ＆グリッチトランジション（0.22秒）
	// 単調なハードカットを排除し、ミリタリー端末の走査線リブートを表現
	// ------------------------------------------------------------
	if (isRetrying_ && rebootBlackoutSprite_) {
		float flashAlpha = 0.5f + 0.5f * std::sin(retryGlitchTimer_ * 60.0f);
		rebootBlackoutSprite_->SetBlendMode(BlendMode::kAdd);
		rebootBlackoutSprite_->SetPosition({ 0.0f, 0.0f });
		rebootBlackoutSprite_->SetSize({ kScreenWidth, kScreenHeight });
		rebootBlackoutSprite_->SetColor({ 0.9f, 0.95f, 1.0f, flashAlpha * 0.40f });
		rebootBlackoutSprite_->Update();
		rebootBlackoutSprite_->Draw();

		// ランダムグリッチバー
		rebootBlackoutSprite_->SetBlendMode(BlendMode::kNormal);
		float gY = std::fmod(retryGlitchTimer_ * 1200.0f, kScreenHeight);
		rebootBlackoutSprite_->SetPosition({ 0.0f, gY });
		rebootBlackoutSprite_->SetSize({ kScreenWidth, 30.0f });
		rebootBlackoutSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.85f });
		rebootBlackoutSprite_->Update();
		rebootBlackoutSprite_->Draw();

		TextRenderer::GetInstance()->Print(
			"HackGen",
			">> RAPID RE-ENGAGE SEQUENCE INITIATED <<",
			kScreenWidth * 0.5f,
			kScreenHeight * 0.5f,
			28.0f,
			{ 1.0f, 0.9f, 0.9f, 0.95f },
			{ 0.5f, 0.5f }
		);
	}

	// ------------------------------------------------------------
	// 項目3-1: システム再起動（リブート）演出オーバーレイ & ブラウン管電源遮断トランジション
	// 0:37の唐突な暗転カットを打破し、画面が水平収束してプツッと消える美しい演出
	// ------------------------------------------------------------
	if (isRebooting_ && rebootBlackoutSprite_) {
		if (rebootTimer_ < 0.35f) {
			// Phase 1 (0.00〜0.35s): SYSTEM REBOOTING... 点滅ダイアログ
			float blackAlpha = (std::min)(0.80f, rebootTimer_ / 0.20f);
			rebootBlackoutSprite_->SetBlendMode(BlendMode::kNormal);
			rebootBlackoutSprite_->SetPosition({ 0.0f, 0.0f });
			rebootBlackoutSprite_->SetSize({ kScreenWidth, kScreenHeight });
			rebootBlackoutSprite_->SetColor({ 0.0f, 0.0f, 0.0f, blackAlpha });
			rebootBlackoutSprite_->Update();
			rebootBlackoutSprite_->Draw();

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
		} else {
			// Phase 2 (0.35〜0.65s): ブラウン管テレビ電源遮断（CRT収束・プツッ消滅）トランジション！
			float crtT = std::clamp((rebootTimer_ - 0.35f) / 0.30f, 0.0f, 1.0f);
			float centerY = kScreenHeight * 0.5f;
			float centerX = kScreenWidth * 0.5f;

			rebootBlackoutSprite_->SetBlendMode(BlendMode::kNormal);

			if (crtT < 0.65f) {
				// 上下から黒マスク収束
				float p = crtT / 0.65f;
				float collapse = 1.0f - std::pow(1.0f - p, 3.0f);
				float halfH = (kScreenHeight * 0.5f) * (1.0f - collapse);
				if (halfH < 2.0f) halfH = 2.0f;

				rebootBlackoutSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
				// 上
				rebootBlackoutSprite_->SetPosition({ 0.0f, 0.0f });
				rebootBlackoutSprite_->SetSize({ kScreenWidth, centerY - halfH });
				rebootBlackoutSprite_->Update(); rebootBlackoutSprite_->Draw();
				// 下
				rebootBlackoutSprite_->SetPosition({ 0.0f, centerY + halfH });
				rebootBlackoutSprite_->SetSize({ kScreenWidth, kScreenHeight - (centerY + halfH) });
				rebootBlackoutSprite_->Update(); rebootBlackoutSprite_->Draw();

				// 中央発光水平線
				rebootBlackoutSprite_->SetBlendMode(BlendMode::kAdd);
				rebootBlackoutSprite_->SetPosition({ 0.0f, centerY - 2.0f });
				rebootBlackoutSprite_->SetSize({ kScreenWidth, 4.0f });
				rebootBlackoutSprite_->SetColor({ 0.85f, 0.95f, 1.0f, 0.95f });
				rebootBlackoutSprite_->Update(); rebootBlackoutSprite_->Draw();
			} else if (crtT < 0.90f) {
				// 水平線が左右から中央へ収縮
				float p = (crtT - 0.65f) / 0.25f;
				float shrink = 1.0f - p;
				float halfW = (kScreenWidth * 0.5f) * shrink;

				rebootBlackoutSprite_->SetPosition({ 0.0f, 0.0f });
				rebootBlackoutSprite_->SetSize({ kScreenWidth, kScreenHeight });
				rebootBlackoutSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
				rebootBlackoutSprite_->Update(); rebootBlackoutSprite_->Draw();

				rebootBlackoutSprite_->SetBlendMode(BlendMode::kAdd);
				rebootBlackoutSprite_->SetPosition({ centerX - halfW, centerY - 2.5f });
				rebootBlackoutSprite_->SetSize({ halfW * 2.0f, 5.0f });
				rebootBlackoutSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
				rebootBlackoutSprite_->Update(); rebootBlackoutSprite_->Draw();
			} else {
				// プツッと極小光点が消滅
				rebootBlackoutSprite_->SetPosition({ 0.0f, 0.0f });
				rebootBlackoutSprite_->SetSize({ kScreenWidth, kScreenHeight });
				rebootBlackoutSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
				rebootBlackoutSprite_->Update(); rebootBlackoutSprite_->Draw();
			}
		}
	}
}

void GameOverScene::Finalize() {
	UITextRegistry::GetInstance()->Clear();
}
