#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "GameOverScene.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../System/GameResult.h"
#include "../System/AsyncPreloadManager.h"
#include "../../engine/Graphics/UI/UITextRegistry.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../io/Input.h"
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

void GameOverScene::PrewarmAssets() {
	if (!isUIInitialized_) {
		SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

		// 必須テクスチャのロード
		TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");

		// 黒フェード・CRT収束用スプライト
		rebootBlackoutSprite_ = std::make_unique<Sprite>();
		rebootBlackoutSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");

		// ============================
		// 背景オーバーレイ（深紅のミリタリーOSカーネルパニック）
		// ============================
		backgroundPanel_.Initialize(spriteCommon);
		backgroundPanel_.SetPosition({ 0.0f, 0.0f });
		backgroundPanel_.SetSize({ kScreenWidth, kScreenHeight });
		backgroundPanel_.SetBackgroundColor({ 0.12f, 0.012f, 0.012f, 0.95f });

		// ============================
		// 疑似OS環境（カーネルパニック仕様：KernelPanicRed）
		// ============================
		pseudoOS_.Initialize(spriteCommon, PseudoOS::Theme::KernelPanicRed);
		pseudoOS_.SetOSName("DAWN_OS CRITICAL EXCEPTION RECOVERY [v4.12.0]");
		pseudoOS_.SetSecurityClearance("EXCEPTION 0xC0000005 // KERNEL PANIC TRACE");
		pseudoOS_.SetStartMenuTitle("DAWN_OS : RECOVERY MENU");

		// スタートメニュー項目の登録
		std::vector<PseudoOS::StartMenuItem> smItems = {
			{ "[01] RETRY COMBAT MISSION", [this]() { retryButton_.Click(); }, false },
			{ "[02] REBOOT OS TO TERMINAL", [this]() { titleButton_.Click(); }, false },
			{ "[03] RESET WINDOW POSITIONS", [this]() {
				auto* w0 = pseudoOS_.GetWindow(0); if (w0) w0->pos = w0->normalPos;
				auto* w1 = pseudoOS_.GetWindow(1); if (w1) w1->pos = w1->normalPos;
				auto* w2 = pseudoOS_.GetWindow(2); if (w2) w2->pos = w2->normalPos;
			}, false },
			{ "[04] EMERGENCY SHUTDOWN", []() { ::PostQuitMessage(0); }, true }
		};
		pseudoOS_.SetStartMenuItems(smItems);

		pseudoOS_.ClearWindows();

		// Window 0: メインクラッシュ復旧コンソール
		PseudoOS::Window win0;
		win0.id = 0;
		win0.title = "PANIC_RECOVERY : DAWN_CORE_DUMP";
		win0.pos = { 392.0f, 44.0f };
		win0.size = { 530.0f, 616.0f };
		win0.normalPos = win0.pos;
		win0.normalSize = win0.size;
		win0.titleBarHeight = 34.0f;
		win0.isOpen = true;
		win0.isMinimized = false;
		win0.animScale = 0.0f;
		win0.onDrawContent = [this](const PseudoOS::Window& win, PseudoOS* os) {
			DrawCrashRecoveryWindow(win);
		};
		pseudoOS_.AddWindow(win0);

		// Window 1: メモリエクセプション診断
		PseudoOS::Window win1;
		win1.id = 1;
		win1.title = "SYS_FAULT : MEMORY EXCEPTION DUMP";
		win1.pos = { 18.0f, 44.0f };
		win1.size = { 356.0f, 260.0f };
		win1.normalPos = win1.pos;
		win1.normalSize = win1.size;
		win1.titleBarHeight = 26.0f;
		win1.isOpen = true;
		win1.isMinimized = false;
		win1.animScale = 0.0f;
		win1.onDrawContent = [this](const PseudoOS::Window& win, PseudoOS* os) {
			DrawMemoryExceptionWindow(win);
		};
		pseudoOS_.AddWindow(win1);

		// Window 2: ハードウェア障害ステータス
		PseudoOS::Window win2;
		win2.id = 2;
		win2.title = "HARDWARE_STATUS : SYSTEM RECOVERY";
		win2.pos = { 18.0f, 316.0f };
		win2.size = { 356.0f, 344.0f };
		win2.normalPos = win2.pos;
		win2.normalSize = win2.size;
		win2.titleBarHeight = 26.0f;
		win2.isOpen = true;
		win2.isMinimized = false;
		win2.animScale = 0.0f;
		win2.onDrawContent = [this](const PseudoOS::Window& win, PseudoOS* os) {
			DrawHardwareFailureWindow(win);
		};
		pseudoOS_.AddWindow(win2);

		pseudoOS_.BringWindowToFront(0);

		// メモリダンププール生成
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

		// 操作ボタン
		float buttonWidth = 240.0f;
		float buttonHeight = 36.0f;

		retryButton_.Initialize(spriteCommon, "[01] RETRY MISSION", 18.0f);
		retryButton_.SetLeftAligned(true, 30.0f);
		retryButton_.SetSize({ buttonWidth, buttonHeight });
		retryButton_.SetNormalColor({ 0.28f, 0.05f, 0.05f, 0.90f });
		retryButton_.SetHoverColor({ 0.65f, 0.14f, 0.14f, 0.98f });
		retryButton_.SetSelectedColor({ 0.85f, 0.20f, 0.20f, 1.0f });
		retryButton_.SetTextColor({ 1.0f, 0.55f, 0.50f, 0.95f });
		retryButton_.SetSelectedTextColor({ 1.0f, 0.95f, 0.90f, 1.0f });
		retryButton_.SetOnClick([this]() {
			if (!isRetrying_ && !isRebooting_) {
				btnClickFlashTimer_ = 0.25f;
				clickedButtonIdx_ = 0;
				isRetrying_ = true;
				retryGlitchTimer_ = 0.0f;
				GameResult::GetInstance().isRetry = true;
				AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();
			}
		});

		titleButton_.Initialize(spriteCommon, "[02] REBOOT OS", 18.0f);
		titleButton_.SetLeftAligned(true, 30.0f);
		titleButton_.SetSize({ buttonWidth, buttonHeight });
		titleButton_.SetNormalColor({ 0.28f, 0.05f, 0.05f, 0.90f });
		titleButton_.SetHoverColor({ 0.65f, 0.14f, 0.14f, 0.98f });
		titleButton_.SetSelectedColor({ 0.85f, 0.20f, 0.20f, 1.0f });
		titleButton_.SetTextColor({ 1.0f, 0.55f, 0.50f, 0.95f });
		titleButton_.SetSelectedTextColor({ 1.0f, 0.95f, 0.90f, 1.0f });
		titleButton_.SetOnClick([this]() {
			if (!isRebooting_ && !isRetrying_) {
				btnClickFlashTimer_ = 0.25f;
				clickedButtonIdx_ = 1;
				isRebooting_ = true;
				rebootTimer_ = 0.0f;
			}
		});

		isUIInitialized_ = true;
	}
}

void GameOverScene::Initialize() {
	sceneID = SCENE::GAMEOVER;
	Input::GetInstance()->UnlockCursor();
	selectionManager_.Clear();
	animTimer_ = 0.0f;
	fadeInAlpha_ = 0.75f;
	isRebooting_ = false;
	rebootTimer_ = 0.0f;
	dumpScrollIndex_ = 0;
	dumpScrollTimer_ = 0.0f;
	logDeployTimer_ = 0.0f;
	visibleLogCount_ = 0;
	isRetrying_ = false;
	retryGlitchTimer_ = 0.0f;
	btnClickFlashTimer_ = 0.0f;
	clickedButtonIdx_ = -1;
	headerTypeTimer_ = 0.0f;
	headerVisibleChars_ = 0;

	PrewarmAssets();

	// ウィンドウ展開アニメーションスケールのみリセット
	auto* w0 = pseudoOS_.GetWindow(0); if (w0) { w0->animScale = 0.0f; w0->pos = w0->normalPos; }
	auto* w1 = pseudoOS_.GetWindow(1); if (w1) { w1->animScale = 0.0f; w1->pos = w1->normalPos; }
	auto* w2 = pseudoOS_.GetWindow(2); if (w2) { w2->animScale = 0.0f; w2->pos = w2->normalPos; }
	pseudoOS_.BringWindowToFront(0);

	selectionManager_.Clear();
	selectionManager_.AddButton(&retryButton_);
	selectionManager_.AddButton(&titleButton_);

	UITextRegistry::GetInstance()->Register("GameOver_Header", &gameOverText_);
	UITextRegistry::GetInstance()->Register("GameOver_BtnRetry", retryButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("GameOver_BtnTitle", titleButton_.GetLabelText());
}

void GameOverScene::Update() {
	float dt = 1.0f / 60.0f;
	animTimer_ += dt;

	if (fadeInAlpha_ < 1.0f) {
		fadeInAlpha_ += 0.15f;
		if (fadeInAlpha_ > 1.0f) fadeInAlpha_ = 1.0f;
	}

	// マウス入力状態の取得
	Input* input = Input::GetInstance();
	Input::MousePosition mPos = input->GetMouseScreenPosition();
	Vector2 mousePos = { static_cast<float>(mPos.x), static_cast<float>(mPos.y) };

	HWND hwnd = input->GetHwnd();
	if (hwnd) {
		RECT rc;
		if (::GetClientRect(hwnd, &rc) && (rc.right - rc.left > 0) && (rc.bottom - rc.top > 0)) {
			float actualW = static_cast<float>(rc.right - rc.left);
			float actualH = static_cast<float>(rc.bottom - rc.top);
			mousePos.x *= (kScreenWidth / actualW);
			mousePos.y *= (kScreenHeight / actualH);
		}
	}

	bool isLDown = input->PushMouse(0);
	bool isLTrig = input->TriggerMouse(0);

	// 疑似OSデスクトップ環境更新
	pseudoOS_.Update(dt, mousePos, isLDown, isLTrig);

	// ログ展開アニメーション
	if (visibleLogCount_ < 6) {
		logDeployTimer_ += dt;
		if (logDeployTimer_ >= 0.055f) {
			logDeployTimer_ = 0.0f;
			visibleLogCount_++;
		}
	} else {
		dumpScrollTimer_ += dt;
		if (dumpScrollTimer_ >= 0.040f) {
			dumpScrollTimer_ = 0.0f;
			dumpScrollIndex_ = (dumpScrollIndex_ + 1) % (std::max)(1, static_cast<int>(memoryDumpPool_.size()));
		}
	}

	// ウィンドウ展開アニメーション（指摘2: 一瞬での全表示を解消し、0.35秒で滑らかに展開）
	float expandT = std::clamp(animTimer_ / 0.35f, 0.0f, 1.0f);
	float easedScale = 1.0f - std::pow(1.0f - expandT, 3.0f);
	for (auto& w : pseudoOS_.GetWindows()) {
		if (w.isOpen && !w.isMinimized) {
			w.animScale = easedScale;
		}
	}

	// 項目3: ヘッダータイピングアニメーション（リアルタイムにエラーを検知・記録している稼働感）
	const std::string fullHeader = "MISSION FAILED // SYSTEM DOWN";
	headerTypeTimer_ += dt;
	headerVisibleChars_ = static_cast<int>(headerTypeTimer_ / 0.030f);
	if (headerVisibleChars_ > static_cast<int>(fullHeader.size())) {
		headerVisibleChars_ = static_cast<int>(fullHeader.size());
	}
	std::string displayHeader = fullHeader.substr(0, headerVisibleChars_);
	if (headerVisibleChars_ < static_cast<int>(fullHeader.size()) && (static_cast<int>(headerTypeTimer_ * 12.0f) % 2 == 0)) {
		displayHeader += "_";
	}
	gameOverText_.SetText(displayHeader);

	if (btnClickFlashTimer_ > 0.0f) {
		btnClickFlashTimer_ -= dt;
		if (btnClickFlashTimer_ < 0.0f) btnClickFlashTimer_ = 0.0f;
	}

	// ============================
	// ポストエフェクト制御（画面全体のCRTモニター質感＆リアルタイム演出：指摘4, 5）
	// ============================
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->ClearActiveEffects();

	// リトライ時のホワイトノイズ＆クリーンアップグリッチ（指摘3, 4）
	if (isRetrying_) {
		retryGlitchTimer_ += dt;

		// 1. 赤エラー解除 ＆ クリーンアップ走査線
		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.intensity = 0.40f;
		scan.dirX = 350.0f;
		scan.dirY = -25.0f;
		postEffect->AddActiveEffect(scan);

		// 2. システム再構築デジタルノイズ
		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.35f;
		postEffect->AddActiveEffect(noise);

		// 3. 色収差
		ActivePostEffect ca;
		ca.type = PostEffectType::kChromaticAberration;
		ca.intensity = 0.05f;
		postEffect->AddActiveEffect(ca);

		if (retryGlitchTimer_ >= 0.70f) {
			postEffect->ClearActiveEffects();
			sceneID = SCENE::STAGE;
			return;
		}
	}
	// リブート演出進行（指摘1: CRT画面収束）
	else if (isRebooting_) {
		rebootTimer_ += dt;

		ActivePostEffect dist;
		dist.type = PostEffectType::kLensDistortion;
		dist.intensity = 0.25f + (rebootTimer_ / 0.65f) * 0.20f;
		postEffect->AddActiveEffect(dist);

		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.intensity = 0.35f;
		scan.dirX = 700.0f;
		scan.dirY = 10.0f;
		postEffect->AddActiveEffect(scan);

		if (rebootTimer_ >= 0.65f) {
			postEffect->ClearActiveEffects();
			GameResult::GetInstance().titleEntryReason = TitleEntryReason::RebootFromGameOver;
			sceneID = SCENE::TITLE;
			return;
		}
	} else {
		// 通常待機時: 冒頭0.4sは激しい緊急警告ストロボ点滅（指摘5）、以降はCRTモニター質感（指摘4）
		if (animTimer_ < 0.40f) {
			bool blinkOn = (std::fmod(animTimer_, 0.08f) < 0.04f);
			ActivePostEffect tint;
			tint.type = PostEffectType::kColorTint;
			tint.colorR = 1.0f;
			tint.colorG = blinkOn ? 0.15f : 0.60f;
			tint.colorB = blinkOn ? 0.10f : 0.40f;
			postEffect->AddActiveEffect(tint);

			ActivePostEffect noise;
			noise.type = PostEffectType::kRandom;
			noise.intensity = 0.35f;
			postEffect->AddActiveEffect(noise);

			ActivePostEffect ca;
			ca.type = PostEffectType::kChromaticAberration;
			ca.intensity = 0.06f;
			postEffect->AddActiveEffect(ca);
		} else {
			// CRTモニター質感シェーダー（走査線 + レンズ湾曲 + ビネット + 色収差 + 赤ティント）
			ActivePostEffect scan;
			scan.type = PostEffectType::kScanLine;
			scan.intensity = 0.16f;
			scan.dirX = 600.0f;
			scan.dirY = 1.5f;
			postEffect->AddActiveEffect(scan);

			ActivePostEffect dist;
			dist.type = PostEffectType::kLensDistortion;
			dist.intensity = 0.04f;
			postEffect->AddActiveEffect(dist);

			ActivePostEffect vig;
			vig.type = PostEffectType::kVignette;
			vig.intensity = 0.85f;
			postEffect->AddActiveEffect(vig);

			ActivePostEffect ca;
			ca.type = PostEffectType::kChromaticAberration;
			ca.intensity = 0.015f;
			postEffect->AddActiveEffect(ca);
		}

		selectionManager_.Update();
	}

	// 中央ウィンドウの追従（ドラッグ移動へのボタン同期）
	auto* w0 = pseudoOS_.GetWindow(0);
	if (w0) {
		float btnY = w0->pos.y + w0->size.y - 68.0f;
		float b1X = w0->pos.x + 25.0f;
		float b2X = w0->pos.x + w0->size.x - 255.0f;
		retryButton_.SetPosition({ b1X, btnY });
		retryButton_.Update();
		titleButton_.SetPosition({ b2X, btnY });
		titleButton_.Update();

		// 完成形の文字列幅を基準に左端を決定（タイピング中も文字位置がずれないよう左上アンカーで固定）
		Vector2 fullHeaderSize = TextRenderer::GetInstance()->CalculateTextSize(
			gameOverText_.GetFontName(), fullHeader, gameOverText_.GetFontSize());
		gameOverText_.SetAnchorPoint({ 0.0f, 0.0f });
		gameOverText_.SetPosition({ w0->pos.x + (w0->size.x - fullHeaderSize.x) * 0.5f, w0->pos.y + 48.0f });
		gameOverText_.Update();
	}

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

	// 疑似OSデスクトップ環境（マルチウィンドウ、タスクバー、カスタムカーソル）
	pseudoOS_.Draw();

	// ------------------------------------------------------------
	// リトライ時のシステム再構築・クリーンアッププログレスバー（指摘4, 5）
	// ------------------------------------------------------------
	if (isRetrying_) {
		TextRenderer* tr = TextRenderer::GetInstance();
		float progress = std::clamp(retryGlitchTimer_ / 0.65f, 0.0f, 1.0f);

		// 全画面シアン調クリーンアップベール（赤エラー画面の解除感を強調：指摘4）
		pseudoOS_.DrawPanel({ 0.0f, 0.0f }, { kScreenWidth, kScreenHeight },
			{ 0.02f, 0.12f, 0.16f, 0.85f }, { 0.20f, 0.90f, 1.0f, 0.95f }, 2.0f);

		float cardW = 680.0f;
		float cardH = 140.0f;
		float cardX = (kScreenWidth - cardW) * 0.5f;
		float cardY = (kScreenHeight - cardH) * 0.5f;

		pseudoOS_.DrawPanel({ cardX, cardY }, { cardW, cardH },
			{ 0.01f, 0.04f, 0.08f, 0.95f }, { 0.35f, 0.95f, 1.0f, 1.0f }, 2.0f);

		tr->Print("HackGen", "REBUILDING RUNTIME ENVIRONMENT // CORE MEMORY RESTORED",
			kScreenWidth * 0.5f, cardY + 20.0f, 18.0f, { 0.35f, 0.95f, 1.0f, 1.0f }, { 0.5f, 0.0f });

		char progBuf[64];
		snprintf(progBuf, sizeof(progBuf), "CLEANUP PROGRESS : %3.0f %%", progress * 100.0f);
		tr->Print("HackGen", progBuf, kScreenWidth * 0.5f, cardY + 54.0f, 15.0f, { 0.85f, 1.0f, 0.95f, 0.95f }, { 0.5f, 0.0f });

		// プログレスバー描画（指摘5）
		float barW = cardW - 60.0f;
		float barH = 16.0f;
		pseudoOS_.DrawGauge({ cardX + 30.0f, cardY + 86.0f }, { barW, barH }, progress,
			{ 0.25f, 0.95f, 1.0f, 1.0f }, { 0.05f, 0.20f, 0.30f, 0.85f });
	}

	// ボタンクリックフィードバック発光（指摘2）
	if (btnClickFlashTimer_ > 0.0f) {
		float alpha = btnClickFlashTimer_ / 0.25f;
		Vector2 flashPos = (clickedButtonIdx_ == 0) ? retryButton_.GetPosition() : titleButton_.GetPosition();
		Vector2 flashSize = (clickedButtonIdx_ == 0) ? retryButton_.GetSize() : titleButton_.GetSize();
		float expand = (1.0f - alpha) * 16.0f;
		pseudoOS_.DrawBorder({ flashPos.x - expand, flashPos.y - expand },
			{ flashSize.x + expand * 2.0f, flashSize.y + expand * 2.0f },
			{ 1.0f, 0.95f, 0.85f, alpha }, 2.5f);
	}

	// ------------------------------------------------------------
	// 項目3-1: リブート演出進行（SYSTEM REBOOTING... → ブラウン管画面収束でタイトルへ復帰）
	// ------------------------------------------------------------
	if (isRebooting_ && rebootBlackoutSprite_) {
		TextRenderer* tr = TextRenderer::GetInstance();
		float rebootProgress = rebootTimer_ / 0.65f;
		if (rebootProgress < 0.35f) {
			float p = rebootProgress / 0.35f;
			float curH = kScreenHeight * (1.0f - p);
			float curY = (kScreenHeight - curH) * 0.5f;

			rebootBlackoutSprite_->SetBlendMode(BlendMode::kNormal);
			rebootBlackoutSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });

			rebootBlackoutSprite_->SetPosition({ 0.0f, 0.0f });
			rebootBlackoutSprite_->SetSize({ kScreenWidth, curY });
			rebootBlackoutSprite_->Update();
			rebootBlackoutSprite_->Draw();

			rebootBlackoutSprite_->SetPosition({ 0.0f, curY + curH });
			rebootBlackoutSprite_->SetSize({ kScreenWidth, kScreenHeight - (curY + curH) });
			rebootBlackoutSprite_->Update();
			rebootBlackoutSprite_->Draw();

			bool blink = (std::fmod(rebootTimer_, 0.12f) < 0.06f);
			if (blink) {
				tr->Print("HackGen", ">> SYSTEM REBOOT INITIATED // RELOADING DAWN KERNEL <<",
					kScreenWidth * 0.5f, kScreenHeight * 0.5f - 10.0f, 18.0f, { 1.0f, 0.35f, 0.35f, 1.0f }, { 0.5f, 0.5f });
			}
		} else {
			float p = (rebootProgress - 0.35f) / 0.30f;
			p = (std::min)(1.0f, p);
			float curW = kScreenWidth * (1.0f - p);
			float curX = (kScreenWidth - curW) * 0.5f;
			float lineH = 3.0f * (1.0f - p);
			float lineY = (kScreenHeight - lineH) * 0.5f;

			rebootBlackoutSprite_->SetBlendMode(BlendMode::kNormal);
			rebootBlackoutSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
			rebootBlackoutSprite_->SetPosition({ 0.0f, 0.0f });
			rebootBlackoutSprite_->SetSize({ kScreenWidth, kScreenHeight });
			rebootBlackoutSprite_->Update();
			rebootBlackoutSprite_->Draw();

			if (curW > 2.0f && lineH > 0.5f) {
				rebootBlackoutSprite_->SetBlendMode(BlendMode::kAdd);
				rebootBlackoutSprite_->SetColor({ 1.0f, 0.40f, 0.40f, 1.0f - p });
				rebootBlackoutSprite_->SetPosition({ curX, lineY });
				rebootBlackoutSprite_->SetSize({ curW, lineH });
				rebootBlackoutSprite_->Update();
				rebootBlackoutSprite_->Draw();
			}
		}
	}
}

void GameOverScene::Finalize() {
	PostEffect::GetInstance()->ClearActiveEffects();
	UITextRegistry::GetInstance()->Clear();
}
