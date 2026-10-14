#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "ClearScene.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../System/GameResult.h"
#include "../System/AsyncPreloadManager.h"
#include "../../engine/Graphics/UI/UITextRegistry.h"
#include "../../io/Input.h"
#include "WinApp.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <chrono>

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

void ClearScene::PrewarmAssets() {
	if (!isUIInitialized_) {
		auto tStart = std::chrono::high_resolution_clock::now();
		SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

		// 必須テクスチャのロード
		TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");

		// ============================
		// 背景オーバーレイ
		// ============================
		backgroundPanel_.Initialize(spriteCommon);
		backgroundPanel_.SetPosition({ 0.0f, 0.0f });
		backgroundPanel_.SetSize({ kScreenWidth, kScreenHeight });
		backgroundPanel_.SetBackgroundColor({ 0.01f, 0.02f, 0.05f, 0.95f });

		// ============================
		// 疑似OS環境（タクティカルシアン仕様：TacticalCyan）
		// ============================
		pseudoOS_.Initialize(spriteCommon, PseudoOS::Theme::TacticalCyan);
		pseudoOS_.SetOSName("DAWN_OS TACTICAL OPERATION DEBRIEF [v4.12.0]");
		pseudoOS_.SetSecurityClearance("MISSION DIRECTIVE // STATUS: OBJECTIVE ACCOMPLISHED");
		pseudoOS_.SetStartMenuTitle("DAWN_OS : MISSION DEBRIEF");

		// スタートメニュー項目の登録
		std::vector<PseudoOS::StartMenuItem> smItems = {
			{ "[01] REPLAY COMBAT SORTIE", [this]() { replayButton_.Click(); }, false },
			{ "[02] RETURN TO ROOT OS (TITLE)", [this]() { titleButton_.Click(); }, false },
			{ "[03] RESET WINDOW POSITIONS", [this]() {
				auto* w0 = pseudoOS_.GetWindow(0); if (w0) w0->pos = w0->normalPos;
				auto* w1 = pseudoOS_.GetWindow(1); if (w1) w1->pos = w1->normalPos;
				auto* w2 = pseudoOS_.GetWindow(2); if (w2) w2->pos = w2->normalPos;
				auto* w3 = pseudoOS_.GetWindow(3); if (w3) w3->pos = w3->normalPos;
			}, false },
			{ "[04] SYSTEM LOGOUT", []() { ::PostQuitMessage(0); }, true }
		};
		pseudoOS_.SetStartMenuItems(smItems);

		pseudoOS_.ClearWindows();

		// Window 1: フライトレコーダーテレメトリ（左列上）
		PseudoOS::Window win1;
		win1.id = 1;
		win1.title = "TELEMETRY : FLIGHT RECORDER LOG";
		win1.pos = { 16.0f, 44.0f };
		win1.size = { 346.0f, 300.0f };
		win1.normalPos = win1.pos;
		win1.normalSize = win1.size;
		win1.titleBarHeight = 26.0f;
		win1.isOpen = true;
		win1.isMinimized = false;
		win1.animScale = 0.0f;
		win1.onDrawContent = [this](const PseudoOS::Window& win, PseudoOS* os) {
			DrawFlightLogWindow(win);
		};
		pseudoOS_.AddWindow(win1);

		// Window 2: 敵損害・戦果分析（左列下）
		PseudoOS::Window win2;
		win2.id = 2;
		win2.title = "INTEL : ENEMY CASUALTY ANALYSIS";
		win2.pos = { 16.0f, 356.0f };
		win2.size = { 346.0f, 320.0f };
		win2.normalPos = win2.pos;
		win2.normalSize = win2.size;
		win2.titleBarHeight = 26.0f;
		win2.isOpen = true;
		win2.isMinimized = false;
		win2.animScale = 0.0f;
		win2.onDrawContent = [this](const PseudoOS::Window& win, PseudoOS* os) {
			DrawIntelAssessmentWindow(win);
		};
		pseudoOS_.AddWindow(win2);

		// Window 3: 作戦エリア地形図 & BDA機体損害評価（項目5: 右側デッドスペース解消・新設）
		PseudoOS::Window win3;
		win3.id = 3;
		win3.title = "THEATER_RECON : AIRSPACE TOPOGRAPHY & BDA";
		win3.pos = { 926.0f, 44.0f };
		win3.size = { 338.0f, 632.0f };
		win3.normalPos = win3.pos;
		win3.normalSize = win3.size;
		win3.titleBarHeight = 26.0f;
		win3.isOpen = true;
		win3.isMinimized = false;
		win3.animScale = 0.0f;
		win3.onDrawContent = [this](const PseudoOS::Window& win, PseudoOS* os) {
			DrawTheaterReconWindow(win);
		};
		pseudoOS_.AddWindow(win3);

		// Window 0: メインデブリーフィングコンソール（中央列・操作対象メインウィンドウ）
		PseudoOS::Window win0;
		win0.id = 0;
		win0.title = "DEBRIEF : OPERATION DAWN [SUCCESS]";
		win0.pos = { 374.0f, 44.0f };
		win0.size = { 540.0f, 632.0f };
		win0.normalPos = win0.pos;
		win0.normalSize = win0.size;
		win0.titleBarHeight = 34.0f;
		win0.isOpen = true;
		win0.isMinimized = false;
		win0.animScale = 0.0f;
		win0.onDrawContent = [this](const PseudoOS::Window& win, PseudoOS* os) {
			DrawDebriefWindow(win);
		};
		pseudoOS_.AddWindow(win0);

		// メイン操作ウィンドウ（DEBRIEF: win0）を初期状態で最前面＆FOCUSに設定
		pseudoOS_.BringWindowToFront(0);

		// ============================
		// 「MISSION COMPLETE」テキスト（項目6: シアン系テーマカラーに統一）
		// ============================
		missionCompleteText_.Initialize("HackGen", "MISSION COMPLETE // DEBRIEF", 26.0f);
		missionCompleteText_.SetAnchorPoint({ 0.5f, 0.0f });
		missionCompleteText_.SetColor({ 0.25f, 0.95f, 0.85f, 1.0f });

		// ============================
		// リザルト情報テキスト
		// ============================
		float fontSize = 18.0f;
		Vector4 labelColor = { 0.55f, 0.85f, 0.95f, 0.95f };
		Vector4 valueColor = { 0.95f, 1.0f, 1.0f, 1.0f };

		// 撃破数
		enemiesDestroyedLabel_.Initialize("HackGen", "ENEMIES DESTROYED     :", fontSize);
		enemiesDestroyedLabel_.SetColor(labelColor);

		enemiesDestroyedValue_.Initialize("HackGen", "0 UNITS", 22.0f);
		enemiesDestroyedValue_.SetAnchorPoint({ 1.0f, 0.0f });
		enemiesDestroyedValue_.SetColor(valueColor);

		// クリアタイム
		clearTimeLabel_.Initialize("HackGen", "TOTAL SORTIE TIME     :", fontSize);
		clearTimeLabel_.SetColor(labelColor);

		clearTimeValue_.Initialize("HackGen", "00:00", 22.0f);
		clearTimeValue_.SetAnchorPoint({ 1.0f, 0.0f });
		clearTimeValue_.SetColor(valueColor);

		// 残りHP
		hpRemainingLabel_.Initialize("HackGen", "SURVIVING INTEGRITY   :", fontSize);
		hpRemainingLabel_.SetColor(labelColor);

		hpRemainingValue_.Initialize("HackGen", "100 %", 22.0f);
		hpRemainingValue_.SetAnchorPoint({ 1.0f, 0.0f });
		hpRemainingValue_.SetColor({ 0.3f, 1.0f, 0.5f, 1.0f });

		// ============================
		// ボタン配置
		// ============================
		float buttonWidth = 236.0f;
		float buttonHeight = 44.0f;

		replayButton_.Initialize(spriteCommon, "[01] REPLAY MISSION", 18.0f);
		replayButton_.SetLeftAligned(true, 42.0f);
		replayButton_.SetSize({ buttonWidth, buttonHeight });
		replayButton_.SetNormalColor({ 0.03f, 0.14f, 0.20f, 0.88f });
		replayButton_.SetHoverColor({ 0.08f, 0.35f, 0.50f, 0.95f });
		replayButton_.SetSelectedColor({ 0.12f, 0.55f, 0.75f, 0.98f });
		replayButton_.SetTextColor({ 0.40f, 0.90f, 1.00f, 0.95f });
		replayButton_.SetSelectedTextColor({ 0.90f, 1.00f, 1.00f, 1.0f });
		replayButton_.SetOnClick([this]() {
			btnClickFlashTimer_ = 0.18f;
			clickedButtonIdx_ = 0;
			isRewinding_ = true;
			rewindTimer_ = 0.0f;
			AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();
		});

		titleButton_.Initialize(spriteCommon, "[02] RETURN TO BASE", 18.0f);
		titleButton_.SetLeftAligned(true, 42.0f);
		titleButton_.SetSize({ buttonWidth, buttonHeight });
		titleButton_.SetNormalColor({ 0.03f, 0.14f, 0.20f, 0.88f });
		titleButton_.SetHoverColor({ 0.08f, 0.35f, 0.50f, 0.95f });
		titleButton_.SetSelectedColor({ 0.12f, 0.55f, 0.75f, 0.98f });
		titleButton_.SetTextColor({ 0.40f, 0.90f, 1.00f, 0.95f });
		titleButton_.SetSelectedTextColor({ 0.90f, 1.00f, 1.00f, 1.0f });
		titleButton_.SetOnClick([this]() {
			btnClickFlashTimer_ = 0.18f;
			clickedButtonIdx_ = 1;
			isUploading_ = true;
			uploadTimer_ = 0.0f;
		});

		selectionManager_.AddButton(&replayButton_);
		selectionManager_.AddButton(&titleButton_);

		// アップロード用スプライト
		uploadSprite_ = std::make_unique<Sprite>();
		uploadSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");

		isUIInitialized_ = true;

		auto tEnd = std::chrono::high_resolution_clock::now();
		double prewarmMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		char buf[256];
		snprintf(buf, sizeof(buf), "  [ClearScene::PrewarmAssets] Assets prewarmed in %.2f ms\n", prewarmMs);
		OutputDebugStringA(buf);

		FILE* fp = nullptr;
		if (fopen_s(&fp, "scene_transition.log", "a") == 0 && fp) {
			fputs(buf, fp);
			fclose(fp);
		}
	}
}

void ClearScene::Initialize() {
	auto tStart = std::chrono::high_resolution_clock::now();
	sceneID = SCENE::CLEAR;
	Input::GetInstance()->UnlockCursor();
	selectionManager_.Clear();
	animTimer_ = 0.0f;
	fadeInAlpha_ = 0.0f;

	const GameResult& result = GameResult::GetInstance();

	PrewarmAssets();

	// ウィンドウアニメーションスケールのみリセット
	auto* w0 = pseudoOS_.GetWindow(0); if (w0) { w0->animScale = 0.0f; w0->pos = w0->normalPos; }
	auto* w1 = pseudoOS_.GetWindow(1); if (w1) { w1->animScale = 0.0f; w1->pos = w1->normalPos; }
	auto* w2 = pseudoOS_.GetWindow(2); if (w2) { w2->animScale = 0.0f; w2->pos = w2->normalPos; }
	auto* w3 = pseudoOS_.GetWindow(3); if (w3) { w3->animScale = 0.0f; w3->pos = w3->normalPos; }
	pseudoOS_.BringWindowToFront(0);

	// 最新リザルト値の反映
	char enemyBuf[32];
	snprintf(enemyBuf, sizeof(enemyBuf), "%d UNITS", result.enemiesDestroyed);
	enemiesDestroyedValue_.SetText(enemyBuf);

	int minutes = static_cast<int>(result.clearTimeSeconds) / 60;
	int seconds = static_cast<int>(result.clearTimeSeconds) % 60;
	char timeBuf[32];
	snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", minutes, seconds);
	clearTimeValue_.SetText(timeBuf);

	int hpPercent = 0;
	if (result.playerMaxHP > 0.0f) {
		hpPercent = static_cast<int>((result.playerHPRemaining / result.playerMaxHP) * 100.0f);
	}
	char hpBuf[32];
	snprintf(hpBuf, sizeof(hpBuf), "%d %%", hpPercent);
	hpRemainingValue_.SetText(hpBuf);
	if (hpPercent >= 70) {
		hpRemainingValue_.SetColor({ 0.3f, 1.0f, 0.5f, 1.0f });
	} else if (hpPercent >= 30) {
		hpRemainingValue_.SetColor({ 1.0f, 0.9f, 0.3f, 1.0f });
	} else {
		hpRemainingValue_.SetColor({ 1.0f, 0.3f, 0.3f, 1.0f });
	}

	selectionManager_.Clear();
	selectionManager_.AddButton(&replayButton_);
	selectionManager_.AddButton(&titleButton_);

	isUploading_ = false;
	uploadTimer_ = 0.0f;
	isRewinding_ = false;
	rewindTimer_ = 0.0f;
	btnClickFlashTimer_ = 0.0f;
	clickedButtonIdx_ = -1;

	UITextRegistry::GetInstance()->Register("Clear_MissionComplete", &missionCompleteText_);
	UITextRegistry::GetInstance()->Register("Clear_EnemiesLabel", &enemiesDestroyedLabel_);
	UITextRegistry::GetInstance()->Register("Clear_EnemiesValue", &enemiesDestroyedValue_);
	UITextRegistry::GetInstance()->Register("Clear_TimeLabel", &clearTimeLabel_);
	UITextRegistry::GetInstance()->Register("Clear_TimeValue", &clearTimeValue_);
	UITextRegistry::GetInstance()->Register("Clear_HPLabel", &hpRemainingLabel_);
	UITextRegistry::GetInstance()->Register("Clear_HPValue", &hpRemainingValue_);
	UITextRegistry::GetInstance()->Register("Clear_BtnReplay", replayButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Clear_BtnTitle", titleButton_.GetLabelText());

	auto tEnd = std::chrono::high_resolution_clock::now();
	double totalMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	char buf[256];
	snprintf(buf, sizeof(buf), "  [ClearScene::Initialize] Initialized in %.2f ms (%.4f s)\n", totalMs, totalMs / 1000.0);
	OutputDebugStringA(buf);

	FILE* fp = nullptr;
	if (fopen_s(&fp, "scene_transition.log", "a") == 0 && fp) {
		fputs(buf, fp);
		fclose(fp);
	}
}

void ClearScene::Update() {
	float dt = 1.0f / 60.0f;
	animTimer_ += dt;

	if (fadeInAlpha_ < 1.0f) {
		fadeInAlpha_ += 1.5f * dt;
		if (fadeInAlpha_ > 1.0f) fadeInAlpha_ = 1.0f;
	}

	if (btnClickFlashTimer_ > 0.0f) {
		btnClickFlashTimer_ -= dt;
		if (btnClickFlashTimer_ < 0.0f) btnClickFlashTimer_ = 0.0f;
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

	// ウィンドウ展開アニメーション（指摘5: 0.35秒で滑らかにズームイン展開）
	if (!isUploading_) {
		float expandT = std::clamp(animTimer_ / 0.35f, 0.0f, 1.0f);
		float easedScale = 1.0f - std::pow(1.0f - expandT, 3.0f);
		for (auto& w : pseudoOS_.GetWindows()) {
			if (w.isOpen && !w.isMinimized) {
				w.animScale = easedScale;
			}
		}
	} else {
		// ウィンドウ縮小閉鎖アニメーション（指摘2: 0.28秒で縮小消滅してデスクトップ復帰）
		float shrinkT = std::clamp(uploadTimer_ / 0.28f, 0.0f, 1.0f);
		float easedShrink = std::pow(1.0f - shrinkT, 2.0f);
		for (auto& w : pseudoOS_.GetWindows()) {
			w.animScale = easedShrink;
		}
	}

	// VHS巻き戻し演出
	if (isRewinding_) {
		rewindTimer_ += dt;
		if (rewindTimer_ >= kRewindDuration) {
			GameResult::GetInstance().isRetry = true;
			sceneID = SCENE::STAGE;
			return;
		}
	}
	// アップロード＆タイトル復帰演出
	else if (isUploading_) {
		uploadTimer_ += dt;
		if (uploadTimer_ >= 1.40f) {
			GameResult::GetInstance().titleEntryReason = TitleEntryReason::ReturnFromClear;
			sceneID = SCENE::TITLE;
			return;
		}
	} else {
		int prevIndex = selectionManager_.GetSelectedIndex();
		selectionManager_.Update();
		int currIndex = selectionManager_.GetSelectedIndex();

		// キーボード/ゲームパッド操作があった場合、またはメニュー選択時はメインウィンドウ（win0）を最前面フォーカスに維持（項目1, 2）
		if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_DOWN) ||
		    input->TriggerKey(DIK_LEFT) || input->TriggerKey(DIK_RIGHT) ||
		    input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) ||
		    currIndex != prevIndex) {
			pseudoOS_.BringWindowToFront(0);
		}
	}

	// 中央ウィンドウの現在位置にボタン座標を追従
	auto* w0 = pseudoOS_.GetWindow(0);
	if (w0) {
		float btnY = w0->pos.y + w0->size.y - 70.0f;
		float b1X = w0->pos.x + 24.0f;
		float b2X = w0->pos.x + w0->size.x - 260.0f;
		replayButton_.SetPosition({ b1X, btnY });
		replayButton_.Update();
		titleButton_.SetPosition({ b2X, btnY });
		titleButton_.Update();

		missionCompleteText_.SetPosition({ w0->pos.x + w0->size.x * 0.5f, w0->pos.y + 44.0f });
		missionCompleteText_.Update();
	}

#ifdef USE_IMGUI
	ImGui::Begin("CLEAR SCENE DEBUG");
	ImGui::Text("FadeIn: %.2f", fadeInAlpha_);
	ImGui::Text("Rewinding: %s (%.2f)", isRewinding_ ? "YES" : "NO", rewindTimer_);
	ImGui::Text("Uploading: %s (%.2f)", isUploading_ ? "YES" : "NO", uploadTimer_);
	const GameResult& result = GameResult::GetInstance();
	ImGui::Text("Enemies: %d", result.enemiesDestroyed);
	ImGui::Text("Time: %.1fs", result.clearTimeSeconds);
	ImGui::Text("HP: %.1f/%.1f", result.playerHPRemaining, result.playerMaxHP);
	ImGui::End();
#endif
}

void ClearScene::Draw() {
	// 項目8: CRTモニター感・走査線・ブルーム・微細色収差の適用（指摘1: VHS巻き戻しシェーダー）
	PostEffect* postEffect = PostEffect::GetInstance();
	if (postEffect) {
		postEffect->ClearActiveEffects();

		if (isRewinding_) {
			// 指摘1: 本物のVHS巻き戻しシェーダー（走査線乱れ＋高速スクロール＋水平ノイズ＋色収差＋湾曲）
			ActivePostEffect scanline;
			scanline.type = PostEffectType::kScanLine;
			scanline.intensity = 0.60f;
			scanline.dirX = 260.0f;
			scanline.dirY = -55.0f; // 超高速巻き戻しスクロール
			postEffect->AddActiveEffect(scanline);

			ActivePostEffect noise;
			noise.type = PostEffectType::kRandom;
			noise.intensity = 0.45f;
			postEffect->AddActiveEffect(noise);

			ActivePostEffect chromatic;
			chromatic.type = PostEffectType::kChromaticAberration;
			chromatic.intensity = 0.08f;
			postEffect->AddActiveEffect(chromatic);

			ActivePostEffect dist;
			dist.type = PostEffectType::kLensDistortion;
			dist.intensity = 0.10f;
			postEffect->AddActiveEffect(dist);
		} else {
			// 1. ネオン・シアン発光ブルーム（しっとりとした軍用モニターの輝き）
			ActivePostEffect bloom;
			bloom.type = PostEffectType::kBloom;
			bloom.intensity = 0.85f;
			bloom.dirX = 1.10f; // ブルーム強度
			bloom.dirY = 0.72f; // 輝度閾値
			postEffect->AddActiveEffect(bloom);

			// 2. CRTモニター走査線（繊細なスキャンライン）
			ActivePostEffect scanline;
			scanline.type = PostEffectType::kScanLine;
			scanline.intensity = 0.08f;
			scanline.dirX = 520.0f; // 走査線の細かさ
			scanline.dirY = 5.0f;   // 穏やかなスクロール
			postEffect->AddActiveEffect(scanline);

			// 3. 微細モニターノイズ
			ActivePostEffect noise;
			noise.type = PostEffectType::kRandom;
			noise.intensity = 0.12f;
			postEffect->AddActiveEffect(noise);

			// 4. 上品な色収差（RGBフリンジ）
			ActivePostEffect chromatic;
			chromatic.type = PostEffectType::kChromaticAberration;
			chromatic.intensity = 0.006f;
			postEffect->AddActiveEffect(chromatic);

			// 5. 四隅のCRTビネット
			ActivePostEffect vig;
			vig.type = PostEffectType::kVignette;
			vig.intensity = 0.28f;
			postEffect->AddActiveEffect(vig);
		}
	}
}

void ClearScene::DrawDebriefWindow(const PseudoOS::Window& win) {
	TextRenderer* tr = TextRenderer::GetInstance();
	float cx = win.pos.x + win.size.x * 0.5f;
	float cy = win.pos.y + win.titleBarHeight + 14.0f;

	// 1. MISSION COMPLETE 大見出し（項目6: シアン色テーマに調和した発光ヘッダー）
	missionCompleteText_.Draw();
	cy += 44.0f;

	tr->Print("HackGen", ">> COMBAT SORTIE REPORT : AIRSPACE SECURED <<",
		cx, cy, 13.5f, { 0.35f, 0.95f, 0.75f, 0.95f }, { 0.5f, 0.0f });
	cy += 24.0f;

	// 2. リザルト情報パネル（エレクトリックシアン枠・等幅帳票レイアウト）
	float frameW = win.size.x - 36.0f;
	float frameH = 224.0f;
	pseudoOS_.DrawPanel({ win.pos.x + 18.0f, cy }, { frameW, frameH },
		{ 0.01f, 0.03f, 0.05f, 0.92f }, { 0.20f, 0.65f, 0.85f, 0.75f }, 1.0f);

	float rowStartY = cy + 18.0f;
	float rowGap = 46.0f;
	float labelX = win.pos.x + 36.0f;
	float valX = win.pos.x + win.size.x - 36.0f;

	// 行1: 撃破数
	enemiesDestroyedLabel_.SetPosition({ labelX, rowStartY });
	enemiesDestroyedLabel_.Draw();
	enemiesDestroyedValue_.SetPosition({ valX, rowStartY - 2.0f });
	enemiesDestroyedValue_.Draw();
	pseudoOS_.DrawRect({ labelX, rowStartY + 28.0f }, { frameW - 36.0f, 1.0f }, { 0.15f, 0.40f, 0.55f, 0.30f });

	// 行2: クリアタイム
	clearTimeLabel_.SetPosition({ labelX, rowStartY + rowGap });
	clearTimeLabel_.Draw();
	clearTimeValue_.SetPosition({ valX, rowStartY + rowGap - 2.0f });
	clearTimeValue_.Draw();
	pseudoOS_.DrawRect({ labelX, rowStartY + rowGap + 28.0f }, { frameW - 36.0f, 1.0f }, { 0.15f, 0.40f, 0.55f, 0.30f });

	// 行3: 残存HP
	hpRemainingLabel_.SetPosition({ labelX, rowStartY + rowGap * 2.0f });
	hpRemainingLabel_.Draw();
	hpRemainingValue_.SetPosition({ valX, rowStartY + rowGap * 2.0f - 2.0f });
	hpRemainingValue_.Draw();
	pseudoOS_.DrawRect({ labelX, rowStartY + rowGap * 2.0f + 28.0f }, { frameW - 36.0f, 1.0f }, { 0.15f, 0.40f, 0.55f, 0.30f });

	// 行4: 作戦評価
	tr->Print("HackGen", "OPERATIONAL EVALUATION:", labelX, rowStartY + rowGap * 3.0f, 18.0f, { 0.55f, 0.85f, 0.95f, 0.95f });
	tr->Print("HackGen", "RANK [S]  100% CLEAR", valX, rowStartY + rowGap * 3.0f - 2.0f, 22.0f, { 1.0f, 0.95f, 0.35f, 1.0f }, { 1.0f, 0.0f });

	cy += frameH + 16.0f;

	// 3. 作戦達成率ゲージ（100%: リアルタイム発光パルス・項目10）
	float pulseRatio = 0.96f + 0.04f * std::sin(animTimer_ * 4.0f);
	Vector4 gaugeCol = { 0.25f, 0.95f * pulseRatio, 0.85f * pulseRatio, 0.95f };
	tr->Print("HackGen", "MISSION OBJECTIVE COMPLETION RATE : 100% [VERIFIED]", win.pos.x + 24.0f, cy, 11.5f, { 0.35f, 0.90f, 1.0f, 0.98f });
	cy += 16.0f;
	pseudoOS_.DrawGauge({ win.pos.x + 24.0f, cy }, { frameW - 12.0f, 10.0f }, 1.0f, gaugeCol, { 0.20f, 0.65f, 0.85f, 0.75f });
	cy += 20.0f;

	tr->Print("HackGen", "FLIGHT DATA ARCHIVED // SELECT PROTOCOL TO CONTINUE",
		cx, cy, 12.0f, { 0.40f, 0.90f, 0.70f, 0.95f }, { 0.5f, 0.0f });

	// 4. ボタン描画
	replayButton_.Draw();
	titleButton_.Draw();
}

void ClearScene::DrawFlightLogWindow(const PseudoOS::Window& win) {
	TextRenderer* tr = TextRenderer::GetInstance();
	float startX = win.pos.x + 16.0f;
	float cy = win.pos.y + win.titleBarHeight + 12.0f;

	tr->Print("HackGen", "SORTIE THEATER : SECTOR 04 [TUNNEL & MOUNTAIN]", startX, cy, 11.5f, { 0.45f, 0.90f, 1.0f, 0.98f });
	cy += 18.0f;
	tr->Print("HackGen", "FLIGHT AIRFRAME: MiG-21Bis TACTICAL JET", startX, cy, 11.0f, { 0.35f, 0.80f, 0.90f, 0.90f });
	cy += 18.0f;

	// 点滅データインテグリティLED（項目10）
	bool validBlink = (std::fmod(animTimer_, 1.2f) < 0.95f);
	Vector4 validCol = validBlink ? Vector4{ 0.35f, 1.0f, 0.60f, 0.95f } : Vector4{ 0.15f, 0.50f, 0.30f, 0.70f };
	tr->Print("HackGen", "DATA INTEGRITY : SHA-256 SIGNED [VALID]", startX, cy, 11.0f, validCol);
	cy += 22.0f;

	// テレメトリ情報枠（リアルタイム微細変動ノイズ演出: 項目10）
	float frameW = win.size.x - 32.0f;
	pseudoOS_.DrawPanel({ startX, cy }, { frameW, 94.0f }, { 0.01f, 0.02f, 0.04f, 0.82f }, { 0.15f, 0.45f, 0.65f, 0.65f }, 1.0f);

	float machNoise = 1.85f + 0.015f * std::sin(animTimer_ * 5.2f);
	char machBuf[48];
	snprintf(machBuf, sizeof(machBuf), "MAX AIR SPEED : MACH %.2f [CRUISE]", machNoise);
	tr->Print("HackGen", machBuf, startX + 8.0f, cy + 8.0f, 10.5f, { 0.45f, 0.95f, 0.85f, 0.90f });

	float gNoise = 7.8f + 0.1f * std::sin(animTimer_ * 3.1f);
	char gBuf[48];
	snprintf(gBuf, sizeof(gBuf), "PEAK G-LOAD   : +%.1f G [STRUCTURAL OK]", gNoise);
	tr->Print("HackGen", gBuf, startX + 8.0f, cy + 24.0f, 10.5f, { 0.45f, 0.95f, 0.85f, 0.90f });

	int fuelVal = 420 - static_cast<int>(std::fmod(animTimer_ * 0.4f, 4.0f));
	char fuelBuf[48];
	snprintf(fuelBuf, sizeof(fuelBuf), "FUEL REMAINING: %d L [SAFE MARGIN]", fuelVal);
	tr->Print("HackGen", fuelBuf, startX + 8.0f, cy + 40.0f, 10.5f, { 0.45f, 0.95f, 0.85f, 0.90f });

	int egtVal = 638 + static_cast<int>(6.0f * std::sin(animTimer_ * 4.5f));
	char egtBuf[48];
	snprintf(egtBuf, sizeof(egtBuf), "AFTERBURNER   : %d C [EGT COOLDOWN]", egtVal);
	tr->Print("HackGen", egtBuf, startX + 8.0f, cy + 56.0f, 10.5f, { 0.35f, 0.85f, 0.65f, 0.90f });

	// 動的センサーバッファプログレス（項目10）
	float bufRatio = 0.985f + 0.012f * std::sin(animTimer_ * 6.0f);
	char bufStr[48];
	snprintf(bufStr, sizeof(bufStr), "SENSOR BUFFER : %.1f%% SYNCHRONIZED", bufRatio * 100.0f);
	tr->Print("HackGen", bufStr, startX + 8.0f, cy + 74.0f, 10.0f, { 0.30f, 0.85f, 0.65f, 0.95f });
	cy += 104.0f;

	// フライトスペクトラム（リアルタイムに有機的に揺れ動くアナライザー・項目10）
	pseudoOS_.DrawSpectrumBars({ startX, cy }, frameW, 22.0f, 14, 5.0f, { 0.30f, 0.88f, 1.0f, 0.90f });
	cy += 28.0f;

	// 点滅通信キャリアリンクLED
	bool carBlink = (std::fmod(animTimer_, 0.8f) < 0.60f);
	Vector4 carLed = carBlink ? Vector4{ 0.35f, 1.0f, 0.65f, 1.0f } : Vector4{ 0.15f, 0.40f, 0.25f, 0.6f };
	pseudoOS_.DrawRect({ startX, cy + 3.0f }, { 6.0f, 6.0f }, carLed);
	tr->Print("HackGen", "CARRIER LINK  : ENCRYPTED RF LINK [ONLINE]", startX + 12.0f, cy, 10.5f, { 0.35f, 0.90f, 0.70f, 0.90f });
}

void ClearScene::DrawIntelAssessmentWindow(const PseudoOS::Window& win) {
	TextRenderer* tr = TextRenderer::GetInstance();
	float startX = win.pos.x + 16.0f;
	float cy = win.pos.y + win.titleBarHeight + 12.0f;
	float frameW = win.size.x - 32.0f;

	tr->Print("HackGen", "COMBAT ASSESSMENT REPORT:", startX, cy, 11.5f, { 0.45f, 0.95f, 0.85f, 0.98f });
	cy += 20.0f;

	// 戦果ステータス
	const char* const intelStatus[] = {
		"ENEMY FIGHTERS : [INTERCEPTORS NEUTRALIZED]",
		"RADAR JAMMING  : [AIRSPACE RESTORED]",
		"SAM INSTALL    : [DESTROYED / MISSION ACCORD]",
		"FORWARD BASE   : [TACTICAL RETREAT TRIGGERED]"
	};
	for (int i = 0; i < 4; ++i) {
		tr->Print("HackGen", intelStatus[i], startX, cy, 10.5f, { 0.35f, 0.85f, 0.65f, 0.90f });
		cy += 16.0f;
	}
	cy += 8.0f;

	// 空域掌握率ゲージ (100% ブリージング発光パルス・項目10)
	float supRatio = 0.95f + 0.05f * std::sin(animTimer_ * 3.5f);
	tr->Print("HackGen", "AIR SUPERIORITY DOMINANCE: 100 % [ABSOLUTE]", startX, cy, 10.5f, { 0.35f, 0.95f, 0.85f, 0.98f });
	cy += 14.0f;
	pseudoOS_.DrawGauge({ startX, cy }, { frameW, 8.0f }, 1.0f, { 0.25f, 0.95f * supRatio, 0.65f * supRatio, 0.95f }, { 0.15f, 0.65f, 0.45f, 0.75f });
	cy += 18.0f;

	// 機体再出撃可能ゲージ (92% 微小ジッター・項目10)
	float readyJitter = 0.92f + 0.008f * std::sin(animTimer_ * 4.8f);
	char readyBuf[48];
	snprintf(readyBuf, sizeof(readyBuf), "COMBAT READINESS FOR RE-SORTIE: %d %%", static_cast<int>(readyJitter * 100.0f));
	tr->Print("HackGen", readyBuf, startX, cy, 10.5f, { 0.35f, 0.85f, 1.0f, 0.95f });
	cy += 14.0f;
	pseudoOS_.DrawGauge({ startX, cy }, { frameW, 8.0f }, readyJitter, { 0.25f, 0.85f, 1.0f, 0.95f }, { 0.15f, 0.55f, 0.75f, 0.75f });
	cy += 24.0f;

	// 作戦承認完了ステータスバッジ（項目4: ボタンと明確に差別化したREAD-ONLYバッジデザイン）
	// ベタ塗り背景を完全廃止し、透明背景＋繊細なコーナーブラケット＋点滅認証LED
	pseudoOS_.DrawRect({ startX, cy }, { frameW, 26.0f }, { 0.01f, 0.04f, 0.03f, 0.40f }); // ごく薄い透明ベース
	pseudoOS_.DrawBorder({ startX, cy }, { frameW, 26.0f }, { 0.20f, 0.75f, 0.50f, 0.60f }, 1.0f);
	// 四隅コーナーブラケット
	const float cL = 6.0f;
	pseudoOS_.DrawRect({ startX, cy }, { cL, 2.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX, cy }, { 2.0f, cL }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX + frameW - cL, cy }, { cL, 2.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX + frameW - 2.0f, cy }, { 2.0f, cL }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX, cy + 24.0f }, { cL, 2.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX, cy + 26.0f - cL }, { 2.0f, cL }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX + frameW - cL, cy + 24.0f }, { cL, 2.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX + frameW - 2.0f, cy + 26.0f - cL }, { 2.0f, cL }, { 0.40f, 1.0f, 0.70f, 0.95f });

	// 点滅ステータスLED
	bool authBlink = (std::fmod(animTimer_, 0.9f) < 0.65f);
	Vector4 authLedCol = authBlink ? Vector4{ 0.35f, 1.0f, 0.65f, 1.0f } : Vector4{ 0.15f, 0.50f, 0.30f, 0.60f };
	pseudoOS_.DrawRect({ startX + 10.0f, cy + 9.0f }, { 8.0f, 8.0f }, authLedCol);
	tr->Print("HackGen", "[STATUS: CERTIFIED] AUTH // MISSION DEBRIEF", startX + 24.0f, cy + 6.0f, 10.5f, { 0.40f, 1.0f, 0.75f, 0.98f });
}

void ClearScene::DrawTheaterReconWindow(const PseudoOS::Window& win) {
	// 項目5: 画面右側デッドスペース解消・作戦エリア地形図＆機体被害評価ウィンドウ
	TextRenderer* tr = TextRenderer::GetInstance();
	float startX = win.pos.x + 14.0f;
	float cy = win.pos.y + win.titleBarHeight + 10.0f;
	float frameW = win.size.x - 28.0f;

	tr->Print("HackGen", "SECTOR TOPOGRAPHY [SURFACE RECON]:", startX, cy, 11.5f, { 0.45f, 0.95f, 0.85f, 0.98f });
	cy += 18.0f;

	// 1. 3Dワイヤーフレーム作戦マップ (h = 160)
	float mapH = 160.0f;
	pseudoOS_.DrawPanel({ startX, cy }, { frameW, mapH }, { 0.01f, 0.03f, 0.05f, 0.88f }, { 0.20f, 0.60f, 0.80f, 0.70f }, 1.0f);

	// ワイヤーフレーム等高線グリッド描画
	float gridTime = animTimer_ * 0.4f;
	for (int gy = 1; gy < 6; ++gy) {
		float lineY = cy + mapH * (gy * 0.166f);
		float wave = 4.0f * std::sin(gridTime + gy * 0.8f);
		pseudoOS_.DrawRect({ startX + 4.0f, lineY + wave }, { frameW - 8.0f, 1.0f }, { 0.15f, 0.45f, 0.60f, 0.40f });
	}
	for (int gx = 1; gx < 5; ++gx) {
		float lineX = startX + frameW * (gx * 0.20f);
		pseudoOS_.DrawRect({ lineX, cy + 4.0f }, { 1.0f, mapH - 8.0f }, { 0.15f, 0.45f, 0.60f, 0.35f });
	}

	// 360度回転レーダースキャンライン（アニメーション）
	float sweepAngle = animTimer_ * 2.4f;
	float scx = startX + frameW * 0.5f;
	float scy = cy + mapH * 0.5f;
	float sLen = 54.0f;
	float endX = scx + sLen * std::cos(sweepAngle);
	float endY = scy + sLen * std::sin(sweepAngle);
	pseudoOS_.DrawRect({ scx - 2.0f, scy - 2.0f }, { 4.0f, 4.0f }, { 0.35f, 1.0f, 0.80f, 1.0f });
	pseudoOS_.DrawRect({ (scx + endX) * 0.5f - 18.0f, (scy + endY) * 0.5f }, { 36.0f, 1.0f }, { 0.20f, 0.90f, 1.0f, 0.75f });

	// 撃破マーカー（点滅）
	bool tgtBlink = (std::fmod(animTimer_, 0.7f) < 0.45f);
	Vector4 tgtCol = tgtBlink ? Vector4{ 1.0f, 0.30f, 0.25f, 0.95f } : Vector4{ 0.50f, 0.15f, 0.10f, 0.60f };
	tr->Print("HackGen", "[X] BOGEY-01 (K.I.A.)", scx - 70.0f, scy - 35.0f, 10.0f, tgtCol);
	tr->Print("HackGen", "[X] BOGEY-02 (K.I.A.)", scx + 10.0f, scy + 25.0f, 10.0f, tgtCol);
	tr->Print("HackGen", ">> AIRSPACE SANITIZED // ALL HOSTILES DOWN", startX + 8.0f, cy + mapH - 18.0f, 10.0f, { 0.35f, 1.0f, 0.70f, 0.95f });
	cy += mapH + 16.0f;

	// 2. 自機 MiG-21 戦闘被害評価 (BDA: Battle Damage Assessment)
	tr->Print("HackGen", "AIRFRAME BDA [MiG-21Bis]:", startX, cy, 11.5f, { 0.45f, 0.95f, 0.85f, 0.98f });
	cy += 18.0f;

	struct ComponentStatus {
		const char* name;
		const char* status;
		bool isOk;
	};
	const ComponentStatus bdaList[] = {
		{ "RADOME / SENSORS", "[100% NOMINAL]", true },
		{ "CANOPY PRESSURE ", "[SEALED // OK]", true },
		{ "PORT WING STRUC ", "[INTACT // OK]", true },
		{ "STBD WING STRUC ", "[INTACT // OK]", true },
		{ "R-25 TURBOJET   ", "[EGT COOLDOWN]", true },
		{ "HYDRAULIC SYS   ", "[20.8 MPa OK ]", true },
		{ "AVIONICS BUS    ", "[LINK SECURE ]", true }
	};

	float bdaH = 176.0f;
	pseudoOS_.DrawPanel({ startX, cy }, { frameW, bdaH }, { 0.01f, 0.025f, 0.04f, 0.85f }, { 0.15f, 0.50f, 0.70f, 0.65f }, 1.0f);
	float bdaRowY = cy + 8.0f;
	for (const auto& comp : bdaList) {
		pseudoOS_.DrawRect({ startX + 8.0f, bdaRowY + 4.0f }, { 6.0f, 6.0f }, { 0.35f, 1.0f, 0.65f, 0.95f });
		tr->Print("HackGen", comp.name, startX + 20.0f, bdaRowY, 10.5f, { 0.40f, 0.85f, 0.95f, 0.90f });
		tr->Print("HackGen", comp.status, startX + frameW - 8.0f, bdaRowY, 10.5f, { 0.35f, 1.0f, 0.65f, 0.95f }, { 1.0f, 0.0f });
		bdaRowY += 22.0f;
	}
	cy += bdaH + 14.0f;

	// 再出撃認可スタンプ
	pseudoOS_.DrawPanel({ startX, cy }, { frameW, 28.0f }, { 0.02f, 0.12f, 0.08f, 0.85f }, { 0.30f, 0.95f, 0.65f, 0.90f }, 1.0f);
	tr->Print("HackGen", "AIRFRAME READY FOR NEXT COMBAT SORTIE", startX + frameW * 0.5f, cy + 7.0f, 11.0f, { 0.40f, 1.0f, 0.75f, 0.98f }, { 0.5f, 0.0f });
}

void ClearScene::DrawUI() {
	TextRenderer* tr = TextRenderer::GetInstance();

	auto DrawRectHelper = [this](const Vector2& pos, const Vector2& size, const Vector4& col, BlendMode blend = BlendMode::kNormal) {
		if (!uploadSprite_) return;
		uploadSprite_->SetBlendMode(blend);
		uploadSprite_->SetPosition(pos);
		uploadSprite_->SetAnchorPoint({ 0.0f, 0.0f });
		uploadSprite_->SetSize(size);
		uploadSprite_->SetRotation(0.0f);
		uploadSprite_->SetColor(col);
		uploadSprite_->Update();
		uploadSprite_->Draw();
	};

	// 1. 背景パネル
	backgroundPanel_.Draw();

	// 背景ミリタリーグリッドパターン（項目9: デスクトップ空間の奥行き・立体感）
	const float gridStep = 48.0f;
	for (float gx = 0.0f; gx < kScreenWidth; gx += gridStep) {
		DrawRectHelper({ gx, 0.0f }, { 1.0f, kScreenHeight - 32.0f }, { 0.15f, 0.40f, 0.60f, 0.08f });
	}
	for (float gy = 0.0f; gy < kScreenHeight - 32.0f; gy += gridStep) {
		DrawRectHelper({ 0.0f, gy }, { kScreenWidth, 1.0f }, { 0.15f, 0.40f, 0.60f, 0.08f });
	}

	// 画面全体のゆっくり動くサイバースキャンビーム（項目8: CRT・走査線質感）
	float beamY = std::fmod(animTimer_ * 80.0f, kScreenHeight - 32.0f);
	DrawRectHelper({ 0.0f, beamY }, { kScreenWidth, 2.0f }, { 0.25f, 0.85f, 1.0f, 0.18f }, BlendMode::kAdd);

	// 2. 疑似OSデスクトップ環境（マルチウィンドウ、タスクバー、カスタムカーソル）
	pseudoOS_.Draw();

	// ------------------------------------------------------------
	// 項目14: ボタン決定フラッシュ＆波紋フィードバック（0.18秒）
	// ------------------------------------------------------------
	if (btnClickFlashTimer_ > 0.0f && uploadSprite_) {
		float flashAlpha = btnClickFlashTimer_ / 0.18f;
		DrawRectHelper({ 0.0f, 0.0f }, { kScreenWidth, kScreenHeight }, { 0.85f, 1.0f, 0.95f, flashAlpha * 0.40f }, BlendMode::kAdd);
	}

	// ------------------------------------------------------------
	// 項目15: VHSテープのリワインド（巻き戻し）演出（指摘1, 2, 3）
	// ------------------------------------------------------------
	if (isRewinding_ && uploadSprite_) {
		float rewindProgress = rewindTimer_ / kRewindDuration;

		// 激しいVHS走査バー
		float noiseY = std::fmod(rewindTimer_ * 1800.0f, kScreenHeight);
		DrawRectHelper({ 0.0f, noiseY - 20.0f }, { kScreenWidth, 40.0f }, { 0.85f, 0.95f, 1.0f, 0.65f }, BlendMode::kAdd);

		for (int i = 0; i < 6; ++i) {
			float lineY = std::fmod(rewindTimer_ * 2600.0f + i * 140.0f, kScreenHeight);
			DrawRectHelper({ 0.0f, lineY }, { kScreenWidth, 3.0f }, { 1.0f, 1.0f, 1.0f, 0.80f }, BlendMode::kAdd);
		}

		if (rewindTimer_ < 0.55f) {
			tr->Print("HackGen", "<<< REWINDING MISSION DATA TAPE <<<",
				kScreenWidth * 0.5f, kScreenHeight * 0.5f - 25.0f, 28.0f, { 0.90f, 1.0f, 0.95f, 1.0f }, { 0.5f, 0.5f });
			char timeBuf[32];
			snprintf(timeBuf, sizeof(timeBuf), "TC -%02d:%02d:%02d", static_cast<int>(rewindTimer_ * 60.0f), static_cast<int>(rewindTimer_ * 3600.0f) % 60, static_cast<int>(rewindProgress * 100.0f));
			tr->Print("HackGen", timeBuf, kScreenWidth * 0.5f, kScreenHeight * 0.5f + 20.0f, 22.0f, { 0.30f, 1.0f, 0.60f, 0.95f }, { 0.5f, 0.5f });
		} else {
			// 指摘3: 「RAPID COMBAT RE-ENGAGE」高速タイピング＆システム起動感
			float engageT = rewindTimer_ - 0.55f;
			const std::string fullEngage = "RAPID COMBAT RE-ENGAGE";
			int chars = static_cast<int>(engageT / 0.015f);
			if (chars > static_cast<int>(fullEngage.size())) chars = static_cast<int>(fullEngage.size());
			std::string dispEngage = fullEngage.substr(0, chars);
			if (chars < static_cast<int>(fullEngage.size())) dispEngage += "_";

			// デジタルターゲット枠
			float ew = 560.0f;
			float eh = 90.0f;
			pseudoOS_.DrawPanel({ (kScreenWidth - ew) * 0.5f, (kScreenHeight - eh) * 0.5f },
				{ ew, eh }, { 0.01f, 0.06f, 0.10f, 0.92f }, { 0.35f, 1.0f, 0.65f, 1.0f }, 2.0f);

			tr->Print("HackGen", dispEngage.c_str(),
				kScreenWidth * 0.5f, kScreenHeight * 0.5f - 14.0f, 30.0f, { 0.40f, 1.0f, 0.70f, 1.0f }, { 0.5f, 0.5f });
			tr->Print("HackGen", "// SYSTEM RE-ARMED // IMMEDIATE DEPLOYMENT",
				kScreenWidth * 0.5f, kScreenHeight * 0.5f + 20.0f, 14.0f, { 0.85f, 1.0f, 0.95f, 0.90f }, { 0.5f, 0.5f });
		}

		// 指摘2: ハードカットせず、巻き戻し終了直前（0.80s〜0.90s）にホワイトノイズフラッシュで滑らかに繋ぐ
		if (rewindTimer_ >= 0.78f) {
			float fadeWhite = (rewindTimer_ - 0.78f) / 0.12f;
			DrawRectHelper({ 0.0f, 0.0f }, { kScreenWidth, kScreenHeight }, { 1.0f, 1.0f, 1.0f, fadeWhite * 0.75f }, BlendMode::kAdd);
		}
	}

	// ------------------------------------------------------------
	// 項目16, 17, 18: データアップロードと帰還システムログブリッジ演出（指摘2, 3）
	// ------------------------------------------------------------
	if (isUploading_ && uploadSprite_) {
		float progress = (std::min)(1.0f, uploadTimer_ / 0.90f);

		// 指摘3: ウィンドウ閉鎖後（0.25s以降）、全画面ブリッジ演出
		float dialogW = 600.0f;
		float dialogH = 170.0f;
		float dialogX = (kScreenWidth - dialogW) * 0.5f;
		float dialogY = (kScreenHeight - dialogH) * 0.5f;

		pseudoOS_.DrawPanel({ dialogX, dialogY }, { dialogW, dialogH },
			{ 0.01f, 0.04f, 0.07f, 0.98f }, { 0.25f, 0.95f, 1.0f, 0.95f }, 1.5f);
		DrawRectHelper({ dialogX, dialogY }, { dialogW, 30.0f }, { 0.03f, 0.20f, 0.32f, 0.98f });

		tr->Print("HackGen", "DAWN_OS : TELEMETRY CLOUD UPLOAD & COMM LINK",
			dialogX + 14.0f, dialogY + 8.0f, 13.0f, { 0.40f, 0.95f, 1.0f, 1.0f });

		if (progress < 1.0f) {
			tr->Print("HackGen", "UPLOADING FLIGHT LOG TO BASE TACTICAL CLOUD...",
				dialogX + 22.0f, dialogY + 48.0f, 14.0f, { 0.85f, 0.95f, 1.0f, 0.95f });
			tr->Print("HackGen", "COMM_LINK: DISCONNECTING FROM COMBAT THEATER...",
				dialogX + 22.0f, dialogY + 70.0f, 12.0f, { 0.40f, 0.85f, 0.95f, 0.85f });
		} else {
			tr->Print("HackGen", "UPLOAD COMPLETED // BASE TERMINAL SYNCHRONIZED",
				dialogX + 22.0f, dialogY + 48.0f, 14.0f, { 0.35f, 1.0f, 0.65f, 1.0f });
			tr->Print("HackGen", "DISCONNECTED: ENTERING HANGAR TERMINAL ROOT...",
				dialogX + 22.0f, dialogY + 70.0f, 12.0f, { 0.60f, 1.0f, 0.80f, 0.95f });
		}

		float barW = dialogW - 44.0f;
		pseudoOS_.DrawGauge({ dialogX + 22.0f, dialogY + 98.0f }, { barW, 16.0f }, progress,
			{ 0.20f, 0.90f, 1.0f, 1.0f }, { 0.25f, 0.70f, 0.90f, 0.85f });

		char pctBuf[32];
		snprintf(pctBuf, sizeof(pctBuf), "%3.0f %%", progress * 100.0f);
		tr->Print("HackGen", pctBuf, dialogX + dialogW - 22.0f, dialogY + 122.0f, 14.0f, { 0.40f, 0.95f, 1.0f, 1.0f }, { 1.0f, 0.0f });
	}
}

void ClearScene::Finalize() {
	PostEffect::GetInstance()->ClearActiveEffects();
	UITextRegistry::GetInstance()->Clear();
}
