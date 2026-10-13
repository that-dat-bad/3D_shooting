#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "ClearScene.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../System/GameResult.h"
#include "../System/AsyncPreloadManager.h"
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
	// 「MISSION COMPLETE」テキスト（項目12: タクティカル・ネオングリーンに統一）
	// ============================
	missionCompleteText_.Initialize("HackGen", "MISSION COMPLETE", 56.0f);
	missionCompleteText_.SetAnchorPoint({ 0.5f, 0.0f });
	missionCompleteText_.SetPosition({ kScreenWidth * 0.5f, 60.0f });
	missionCompleteText_.SetColor({ 0.20f, 1.0f, 0.55f, 1.0f }); // 統一タクティカル・ネオングリーン

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
	// ボタン（項目13: ハッカー風ブラケット装飾＆統一デザイン）
	// ============================
	float buttonWidth = 270.0f;
	float buttonHeight = 50.0f;
	float buttonSpacing = 30.0f;
	float buttonsStartY = panelY + panelHeight + 50.0f;
	// 2つのボタンを横並びに中央配置
	float totalButtonsWidth = buttonWidth * 2.0f + buttonSpacing;
	float leftButtonX = (kScreenWidth - totalButtonsWidth) * 0.5f;

	replayButton_.Initialize(spriteCommon, "[ REPLAY MISSION ]", 20.0f);
	replayButton_.SetPosition({ leftButtonX, buttonsStartY });
	replayButton_.SetSize({ buttonWidth, buttonHeight });
	replayButton_.SetNormalColor({ 0.04f, 0.12f, 0.08f, 0.88f });
	replayButton_.SetHoverColor({ 0.08f, 0.32f, 0.18f, 0.95f });
	replayButton_.SetSelectedColor({ 0.12f, 0.45f, 0.25f, 0.98f });
	replayButton_.SetOnClick([this]() {
		btnClickFlashTimer_ = 0.18f;
		clickedButtonIdx_ = 0;
		isRewinding_ = true;
		rewindTimer_ = 0.0f;
		// 非同期処理による軽量化：VHS巻き戻し演出の裏でバックグラウンドスレッドでステージリソースを非同期プリロード！
		AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();
	});

	titleButton_.Initialize(spriteCommon, "[ RETURN TO BASE ]", 20.0f);
	titleButton_.SetPosition({ leftButtonX + buttonWidth + buttonSpacing, buttonsStartY });
	titleButton_.SetSize({ buttonWidth, buttonHeight });
	titleButton_.SetNormalColor({ 0.04f, 0.12f, 0.08f, 0.88f });
	titleButton_.SetHoverColor({ 0.08f, 0.32f, 0.18f, 0.95f });
	titleButton_.SetSelectedColor({ 0.12f, 0.45f, 0.25f, 0.98f });
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
	isUploading_ = false;
	uploadTimer_ = 0.0f;
	isRewinding_ = false;
	rewindTimer_ = 0.0f;
	btnClickFlashTimer_ = 0.0f;
	clickedButtonIdx_ = -1;

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
	float dt = 1.0f / 60.0f;
	animTimer_ += dt;

	// フェードイン
	if (fadeInAlpha_ < 1.0f) {
		fadeInAlpha_ += 1.5f * dt; // 約0.67秒でフェードイン完了
		if (fadeInAlpha_ > 1.0f) fadeInAlpha_ = 1.0f;
	}

	// 項目14: ボタン決定フラッシュ減衰
	if (btnClickFlashTimer_ > 0.0f) {
		btnClickFlashTimer_ -= dt;
		if (btnClickFlashTimer_ < 0.0f) btnClickFlashTimer_ = 0.0f;
	}

	// 項目15: VHSテープのリワインド（巻き戻し）シーケンス更新
	if (isRewinding_) {
		rewindTimer_ += dt;
		if (rewindTimer_ >= kRewindDuration) {
			GameResult::GetInstance().isRetry = true;
			sceneID = SCENE::STAGE;
			return;
		}
	}
	// 項目16, 17, 18: アップロード＆ウィンドウ閉鎖シーケンス更新
	else if (isUploading_) {
		uploadTimer_ += dt;
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
	// 3Dオブジェクト等があればここに記述
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

	auto DrawHollowBorder = [&DrawRectHelper](const Vector2& pos, const Vector2& size, const Vector4& col, float thick = 1.5f) {
		DrawRectHelper(pos, { size.x, thick }, col);
		DrawRectHelper({ pos.x, pos.y + size.y - thick }, { size.x, thick }, col);
		DrawRectHelper(pos, { thick, size.y }, col);
		DrawRectHelper({ pos.x + size.x - thick, pos.y }, { thick, size.y }, col);
	};

	// 項目18: アップロード完了後のウィンドウ最小化閉鎖スケール計算（1.10s〜1.50s）
	float winScale = 1.0f;
	Vector2 winOffset = { 0.0f, 0.0f };
	if (isUploading_ && uploadTimer_ >= 1.10f) {
		float closeT = std::clamp((uploadTimer_ - 1.10f) / 0.38f, 0.0f, 1.0f);
		float easeT = closeT * closeT * closeT; // 急激に加速して吸い込まれる
		winScale = (1.0f - easeT);
		winOffset.y = easeT * (kScreenHeight * 0.45f); // 画面下部（タスクバー方向）へ吸い込まれる
	}

	// 背景
	backgroundPanel_.Draw();

	if (winScale > 0.02f) {
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

		// 項目13: ボタンの四隅サイバーブラケット（[ ] 枠線装飾）
		auto DrawCyberBrackets = [&](const Vector2& pos, const Vector2& size, const Vector4& col) {
			const float cLen = 10.0f;
			const float cThick = 2.0f;
			DrawRectHelper(pos, { cLen, cThick }, col);
			DrawRectHelper(pos, { cThick, cLen }, col);
			DrawRectHelper({ pos.x + size.x - cLen, pos.y }, { cLen, cThick }, col);
			DrawRectHelper({ pos.x + size.x - cThick, pos.y }, { cThick, cLen }, col);
			DrawRectHelper({ pos.x, pos.y + size.y - cThick }, { cLen, cThick }, col);
			DrawRectHelper({ pos.x, pos.y + size.y - cLen }, { cThick, cLen }, col);
			DrawRectHelper({ pos.x + size.x - cLen, pos.y + size.y - cThick }, { cLen, cThick }, col);
			DrawRectHelper({ pos.x + size.x - cThick, pos.y + size.y - cLen }, { cThick, cLen }, col);
		};

		DrawCyberBrackets(replayButton_.GetPosition(), replayButton_.GetSize(), { 0.25f, 0.95f, 0.55f, 0.90f });
		DrawCyberBrackets(titleButton_.GetPosition(), titleButton_.GetSize(), { 0.25f, 0.95f, 0.55f, 0.90f });

		// 項目14: ボタンクリック時の白反転フラッシュ＆波紋フィードバック
		if (btnClickFlashTimer_ > 0.0f && clickedButtonIdx_ >= 0) {
			Vector2 bPos = (clickedButtonIdx_ == 0) ? replayButton_.GetPosition() : titleButton_.GetPosition();
			Vector2 bSize = (clickedButtonIdx_ == 0) ? replayButton_.GetSize() : titleButton_.GetSize();
			float flashAlpha = btnClickFlashTimer_ / 0.18f;

			// 白反転フラッシュ
			DrawRectHelper(bPos, bSize, { 1.0f, 1.0f, 1.0f, 0.85f * flashAlpha }, BlendMode::kAdd);

			// 外側へ広がる波紋リング
			float rip = (1.0f - flashAlpha);
			float expandX = rip * 22.0f;
			float expandY = rip * 14.0f;
			DrawHollowBorder({ bPos.x - expandX, bPos.y - expandY }, { bSize.x + expandX * 2.0f, bSize.y + expandY * 2.0f }, { 0.40f, 1.0f, 0.70f, flashAlpha * 0.90f }, 2.0f);
		}

		// 操作ガイド
		tr->Print(
			"HackGen",
			"←→: SELECT   SPACE/ENTER: DECIDE",
			kScreenWidth * 0.5f - 170.0f,
			kScreenHeight - 50.0f,
			16.0f,
			{ 0.5f, 0.5f, 0.55f, 0.6f }
		);
	}

	// ------------------------------------------------------------
	// 項目15: 「VHSテープのリワインド（巻き戻し）」エフェクト
	// ------------------------------------------------------------
	if (isRewinding_) {
		float rProg = rewindTimer_ / kRewindDuration;
		float jitterX = ((rand() % 100) / 100.0f - 0.5f) * 16.0f;
		float jitterY = ((rand() % 100) / 100.0f - 0.5f) * 8.0f;

		// 画面全体のブルー・シアンVHSティント
		DrawRectHelper({ 0.0f, 0.0f }, { kScreenWidth, kScreenHeight }, { 0.02f, 0.08f, 0.15f, 0.45f }, BlendMode::kNormal);

		// 高速逆スクロール走査線バンド
		const int numBands = 8;
		for (int b = 0; b < numBands; ++b) {
			float bandY = std::fmod((1.0f - rProg * 4.0f + b * 0.15f) * kScreenHeight, kScreenHeight);
			if (bandY < 0.0f) bandY += kScreenHeight;
			float bH = 8.0f + static_cast<float>(rand() % 24);
			float bAlpha = 0.35f + 0.30f * std::sin(rProg * 30.0f + b);
			DrawRectHelper({ 0.0f, bandY + jitterY }, { kScreenWidth, bH }, { 0.40f, 0.85f, 1.0f, bAlpha }, BlendMode::kAdd);
		}

		// テープノイズの水平ラインスプリット
		for (int i = 0; i < 12; ++i) {
			float ny = static_cast<float>(rand() % static_cast<int>(kScreenHeight));
			float nw = 80.0f + static_cast<float>(rand() % 400);
			float nx = static_cast<float>(rand() % static_cast<int>(kScreenWidth - nw));
			DrawRectHelper({ nx + jitterX, ny }, { nw, 2.0f }, { 0.90f, 1.0f, 1.0f, 0.80f }, BlendMode::kAdd);
		}

		// 画面中央の巻き戻しステータスHUD
		float hudBoxW = 560.0f;
		float hudBoxH = 90.0f;
		float hudBoxX = (kScreenWidth - hudBoxW) * 0.5f + jitterX;
		float hudBoxY = (kScreenHeight - hudBoxH) * 0.5f + jitterY;

		DrawRectHelper({ hudBoxX, hudBoxY }, { hudBoxW, hudBoxH }, { 0.01f, 0.04f, 0.08f, 0.90f });
		DrawHollowBorder({ hudBoxX, hudBoxY }, { hudBoxW, hudBoxH }, { 0.30f, 0.90f, 1.0f, 0.95f }, 2.0f);

		tr->Print(
			"HackGen",
			"<< REWINDING COMBAT LOG <<",
			hudBoxX + hudBoxW * 0.5f,
			hudBoxY + 16.0f,
			24.0f,
			{ 0.35f, 1.0f, 0.80f, 1.0f },
			{ 0.5f, 0.0f }
		);

		// 逆回転タイムコード（例: 00:03:42:15 -> 00:00:00:00）
		float remainTime = (1.0f - rProg) * 222.0f;
		int rMin = static_cast<int>(remainTime) / 60;
		int rSec = static_cast<int>(remainTime) % 60;
		int rFrm = static_cast<int>((remainTime - std::floor(remainTime)) * 60.0f);
		char timecodeBuf[64];
		snprintf(timecodeBuf, sizeof(timecodeBuf), "TCR - %02d:%02d:%02d [FAST REWIND 16x]", rMin, rSec, rFrm);

		tr->Print(
			"HackGen",
			timecodeBuf,
			hudBoxX + hudBoxW * 0.5f,
			hudBoxY + 52.0f,
			17.0f,
			{ 1.0f, 0.85f, 0.25f, 0.95f },
			{ 0.5f, 0.0f }
		);
	}

	// ------------------------------------------------------------
	// 項目16, 17, 18: 「UPLOADING COMBAT DATA... 0% -> 100%」& ウィンドウ最小化閉鎖演出
	// ------------------------------------------------------------
	if (isUploading_ && uploadSprite_) {
		// 0.0s〜1.10s: データアップロードダイアログ（確実に0%から伸びるアニメーション）
		if (uploadTimer_ < 1.10f) {
			float dlgW = 640.0f;
			float dlgH = 130.0f;
			float dlgX = (kScreenWidth - dlgW) * 0.5f;
			float dlgY = (kScreenHeight - dlgH) * 0.5f + 160.0f;

			// 半透明ダーク背景
			DrawRectHelper({ dlgX, dlgY }, { dlgW, dlgH }, { 0.02f, 0.06f, 0.12f, 0.95f });
			// シアン枠線
			DrawHollowBorder({ dlgX, dlgY }, { dlgW, dlgH }, { 0.25f, 0.85f, 1.0f, 0.95f }, 2.0f);

			// 項目16: 0%から確実に滑らかに100%まで伸びるイージング計算
			float rawProg = std::clamp(uploadTimer_ / 0.85f, 0.0f, 1.0f);
			float prog = rawProg * rawProg * (3.0f - 2.0f * rawProg); // SmoothStep
			int pct = static_cast<int>(prog * 100.0f);

			char headerText[64];
			if (pct >= 100) {
				snprintf(headerText, sizeof(headerText), "UPLOADING COMBAT DATA... 100%% [OK]");
			} else {
				snprintf(headerText, sizeof(headerText), "UPLOADING COMBAT DATA... %d%%", pct);
			}

			tr->Print(
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
			float barH = 16.0f;
			DrawRectHelper({ barX, barY }, { barW, barH }, { 0.05f, 0.12f, 0.20f, 0.80f });
			DrawHollowBorder({ barX, barY }, { barW, barH }, { 0.20f, 0.50f, 0.70f, 0.60f }, 1.0f);

			// プログレスバー本体（0%から伸びる）
			if (prog > 0.001f) {
				DrawRectHelper({ barX, barY }, { barW * prog, barH }, (pct >= 100) ? Vector4{ 0.20f, 0.95f, 0.50f, 0.95f } : Vector4{ 0.25f, 0.80f, 1.0f, 0.95f });
			}

			// 項目17: プログレスバー下部の暗号化処理テキストを拡大＆高コントラスト化 (14.0f -> 18.0f)
			const char* subText = (pct >= 100) ? "STATUS: SYNCHRONIZATION COMPLETE // CLOSING CLIENT" : "ENCRYPTING TELEMETRY STREAM VIA SECURE LINK...";
			Vector4 subCol = (pct >= 100) ? Vector4{ 0.40f, 1.0f, 0.70f, 1.0f } : Vector4{ 0.35f, 0.95f, 1.0f, 0.95f };

			tr->Print(
				"HackGen",
				subText,
				dlgX + 24.0f,
				dlgY + 84.0f,
				18.0f,
				subCol
			);
		} else {
			// 項目18: 1.10s〜1.50s: リザルト画面全体がOSウィンドウとしてタスクバーへ「最小化して閉じる」アニメーション！
			float closeT = std::clamp((uploadTimer_ - 1.10f) / 0.38f, 0.0f, 1.0f);
			float winEase = (1.0f - closeT);
			float halfW = (kScreenWidth * 0.5f) * winEase;
			float halfH = (kScreenHeight * 0.5f) * winEase;
			float centerX = kScreenWidth * 0.5f;
			float centerY = (kScreenHeight * 0.5f) + closeT * (kScreenHeight * 0.45f); // タスクバーへ降下

			// 外側4つの黒マスクでウィンドウ縮小閉鎖を表現
			// 上
			DrawRectHelper({ 0.0f, 0.0f }, { kScreenWidth, centerY - halfH }, { 0.0f, 0.0f, 0.0f, 1.0f });
			// 下
			DrawRectHelper({ 0.0f, centerY + halfH }, { kScreenWidth, kScreenHeight - (centerY + halfH) }, { 0.0f, 0.0f, 0.0f, 1.0f });
			// 左
			DrawRectHelper({ 0.0f, centerY - halfH }, { centerX - halfW, halfH * 2.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });
			// 右
			DrawRectHelper({ centerX + halfW, centerY - halfH }, { kScreenWidth - (centerX + halfW), halfH * 2.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });

			// 縮小ウィンドウのサイバー枠線
			DrawHollowBorder({ centerX - halfW, centerY - halfH }, { halfW * 2.0f, halfH * 2.0f }, { 0.35f, 0.85f, 1.0f, 0.95f * winEase }, 2.0f);
		}
	}
}

void ClearScene::Finalize() {
	UITextRegistry::GetInstance()->Clear();
}


