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
#include "../../engine/base/Math/Easing.h"

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
#ifdef _DEBUG
		auto tStart = std::chrono::high_resolution_clock::now();
#endif
		SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

		// 必須テクスチャのロード
		TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");

		// ============================
		// 疑似OS環境（タクティカルシアン仕様：TacticalCyan）および背景パネル初期化
		// ============================
		InitializeOS(spriteCommon, PseudoOS::Theme::TacticalCyan,
			"DAWN_OS TACTICAL OPERATION DEBRIEF [v4.12.0]",
			"MISSION DIRECTIVE // STATUS: OBJECTIVE ACCOMPLISHED",
			"DAWN_OS : MISSION DEBRIEF",
			{ 0.01f, 0.02f, 0.05f, 0.95f });

		// スタートメニュー項目の登録
		std::vector<PseudoOS::StartMenuItem> smItems = {
			{ "[01] REPLAY COMBAT SORTIE", [this]() { replayButton_.Click(); }, false },
			{ "[02] RETURN TO ROOT OS (TITLE)", [this]() { titleButton_.Click(); }, false },
			{ "[03] RESET WINDOW POSITIONS", [this]() {
				pseudoOS_.ResetWindowPositions();
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
		win1.titleBarHeight = 34.0f;
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
		win2.titleBarHeight = 34.0f;
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
		win3.titleBarHeight = 34.0f;
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
			pseudoOS_.PlayWindowCloseAnimation(0.28f);
		});

		selectionManager_.AddButton(&replayButton_);
		selectionManager_.AddButton(&titleButton_);

		// アップロード用スプライト
		uploadSprite_ = std::make_unique<Sprite>();
		uploadSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");

		isUIInitialized_ = true;

#ifdef _DEBUG
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
#endif
	}
}

void ClearScene::Initialize() {
#ifdef _DEBUG
	auto tStart = std::chrono::high_resolution_clock::now();
#endif
	sceneID = SCENE::CLEAR;
	Input::GetInstance()->UnlockCursor();
	selectionManager_.Clear();
	animTimer_ = 0.0f;
	fadeInAlpha_ = 0.0f;

	const GameResult& result = GameResult::GetInstance();

	PrewarmAssets();

	// ウィンドウ位置リセット＆展開アニメーション開始
	pseudoOS_.ResetWindowPositions();
	pseudoOS_.PlayWindowOpenAnimation(0.35f);
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

#ifdef _DEBUG
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
#endif
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

	Input* input = Input::GetInstance();

	// 疑似OSデスクトップ環境更新（入力自動取得＆ウィンドウ開閉アニメーション自動管理）
	pseudoOS_.Update(dt);

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

void ClearScene::DrawUI() {
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
	// 項目15: VHSテープのリワインド（巻き戻し）演出
	// ------------------------------------------------------------
	DrawRewindEffectUI();

	// ------------------------------------------------------------
	// 項目16, 17, 18: データアップロードと帰還システムログブリッジ演出
	// ------------------------------------------------------------
	DrawUploadSequenceUI();
}

void ClearScene::Finalize() {
	PostEffect::GetInstance()->ClearActiveEffects();
	UITextRegistry::GetInstance()->Clear();
}
