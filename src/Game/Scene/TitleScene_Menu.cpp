#include "TitleSceneInternal.h"

using namespace TitleSceneInternal;

void TitleScene::InitializeUI(SpriteCommon* spriteCommon, bool fromCache) {
	// ============================
	// 背景オーバーレイ（暗い半透明パネル）
	// ============================
	backgroundPanel_.Initialize(spriteCommon);
	backgroundPanel_.SetPosition({ 0.0f, 0.0f });
	backgroundPanel_.SetSize({ kScreenWidth, kScreenHeight });
	backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 0.3f }); // 少し透明度を上げて後ろの3Dが見えるように

	// ============================
	// タイトルテキスト（コンソールプロジェクト識別バナー: 鮮烈なターミナルグリーン）
	// ============================
	titleText_.Initialize("HackGen", "DAWN", 48.0f);
	titleText_.SetAnchorPoint({ 0.5f, 0.0f });
	titleText_.SetPosition({ kScreenWidth * 0.5f, 106.0f });
	titleText_.SetColor({ 0.22f, 1.0f, 0.52f, 1.0f }); // 鮮烈なエレクトリック・ターミナルグリーン
	titleText_.SetDropShadow(true, { 2.0f, 3.0f }, { 0.0f, 0.0f, 0.0f, 0.95f });
	titleText_.SetOutline(true, 1.5f, { 0.02f, 0.15f, 0.06f, 0.95f });

	// ============================
	// サブタイトルテキスト（ターミナルグリーン統一＆重なり解消）
	// ============================
	subtitleText_.Initialize("HackGen", "- OPERATION : DAWN -", 14.0f);
	subtitleText_.SetAnchorPoint({ 0.5f, 0.0f });
	subtitleText_.SetPosition({ kScreenWidth * 0.5f, 196.0f });
	subtitleText_.SetColor({ 0.35f, 0.95f, 0.60f, 0.95f }); // ターミナルグリーン
	subtitleText_.SetDropShadow(true, { 1.5f, 2.0f }, { 0.0f, 0.0f, 0.0f, 0.90f });

	// ============================
	// メニューボタン（コンソール端末コマンド行）
	// ============================
	float buttonWidth = 390.0f;
	float buttonHeight = 44.0f;
	float buttonX = (kScreenWidth - buttonWidth) * 0.5f;
	float buttonStartY = 248.0f;
	float buttonSpacing = 56.0f;

	const Vector4 btnNormalBg   = { 0.0f, 0.0f, 0.0f, 0.0f };       // 漆黒ウィンドウを活かしたクリーンな背景
	const Vector4 btnHoverBg    = { 0.05f, 0.25f, 0.12f, 0.50f };  // 上品な深緑半透明ハイライト
	const Vector4 btnSelectedBg = { 0.05f, 0.25f, 0.12f, 0.50f };  // 上品な深緑半透明ハイライト
	const Vector4 btnNormalText = { 0.35f, 0.92f, 0.55f, 0.95f };  // ターミナルグリーン文字
	const Vector4 btnSelText    = { 0.85f, 1.00f, 0.90f, 1.00f };  // 鮮やかな白熱発光グリーン文字

	const Vector4 btnLockedBg   = { 0.04f, 0.08f, 0.06f, 0.70f };
	const Vector4 btnLockedText = { 0.40f, 0.52f, 0.45f, 0.65f };

	// START ボタン -> [01] MISSION SELECT
	startButton_.Initialize(spriteCommon, "[01] MISSION SELECT", 21.0f);
	startButton_.SetLeftAligned(true, 100.0f);
	startButton_.SetTextOffset({ 0.0f, 3.0f });
	startButton_.GetLabelText()->SetFontName("HackGen");
	startButton_.SetPosition({ buttonX, buttonStartY });
	startButton_.SetSize({ buttonWidth, buttonHeight });
	startButton_.SetNormalColor(btnNormalBg);
	startButton_.SetHoverColor(btnHoverBg);
	startButton_.SetSelectedColor(btnSelectedBg);
	startButton_.SetTextColor(btnNormalText);
	startButton_.SetSelectedTextColor(btnSelText);
	startButton_.SetOnClick([this]() {
		// メインターミナルウィンドウをミッション選択画面に切り替え
		isMissionSelectModalOpen_ = true;
		missionSelectModalAnim_ = 1.0f;
		if (!missionCards_.empty()) {
			selectedMissionIdx_ = std::clamp(selectedMissionIdx_, 0, static_cast<int>(missionCards_.size()) - 1);
			MissionManager::GetInstance().SetCurrentFilePath(missionCards_[selectedMissionIdx_].filePath);
		}
	});

	// FREE FLIGHT ボタン (未実装ロック表示・開けないように無効化)
	editorButton_.Initialize(spriteCommon, "[02] FREE FLIGHT [LOCKED]", 21.0f);
	editorButton_.SetLeftAligned(true, 100.0f);
	editorButton_.SetTextOffset({ 0.0f, 3.0f });
	editorButton_.GetLabelText()->SetFontName("HackGen");
	editorButton_.SetPosition({ buttonX, buttonStartY + buttonSpacing });
	editorButton_.SetSize({ buttonWidth, buttonHeight });
	editorButton_.SetNormalColor(btnLockedBg);
	editorButton_.SetHoverColor(btnLockedBg);
	editorButton_.SetSelectedColor(btnLockedBg);
	editorButton_.SetTextColor(btnLockedText);
	editorButton_.SetSelectedTextColor(btnLockedText);
	editorButton_.SetOnClick([this]() {
		lockedAlertTimer_ = 1.8f;
	});

	// SETTINGS ボタン (未実装ロック表示・開けないように無効化)
	settingsButton_.Initialize(spriteCommon, "[03] SYSTEM CONFIG [LOCKED]", 21.0f);
	settingsButton_.SetLeftAligned(true, 100.0f);
	settingsButton_.SetTextOffset({ 0.0f, 3.0f });
	settingsButton_.GetLabelText()->SetFontName("HackGen");
	settingsButton_.SetPosition({ buttonX, buttonStartY + buttonSpacing * 2.0f });
	settingsButton_.SetSize({ buttonWidth, buttonHeight });
	settingsButton_.SetNormalColor(btnLockedBg);
	settingsButton_.SetHoverColor(btnLockedBg);
	settingsButton_.SetSelectedColor(btnLockedBg);
	settingsButton_.SetTextColor(btnLockedText);
	settingsButton_.SetSelectedTextColor(btnLockedText);
	settingsButton_.SetOnClick([this]() {
		lockedAlertTimer_ = 1.8f;
	});

	// EXIT ボタン
	exitButton_.Initialize(spriteCommon, "[04] ABORT / EXIT", 21.0f);
	exitButton_.SetLeftAligned(true, 100.0f);
	exitButton_.SetTextOffset({ 0.0f, 3.0f });
	exitButton_.GetLabelText()->SetFontName("HackGen");
	exitButton_.SetPosition({ buttonX, buttonStartY + buttonSpacing * 3.0f });
	exitButton_.SetSize({ buttonWidth, buttonHeight });
	exitButton_.SetNormalColor(btnNormalBg);
	exitButton_.SetHoverColor(btnHoverBg);
	exitButton_.SetSelectedColor(btnSelectedBg);
	exitButton_.SetTextColor(btnNormalText);
	exitButton_.SetSelectedTextColor(btnSelText);
	exitButton_.SetOnClick([]() {
		PostQuitMessage(0);
	});

	selectionManager_.AddButton(&startButton_);
	selectionManager_.AddButton(&editorButton_);
	selectionManager_.AddButton(&settingsButton_);
	selectionManager_.AddButton(&exitButton_);

	// ============================================================
	// 初回のみ実行するスプライト・エフェクト初期化（キャッシュ復元時は0msスキップ）
	// ============================================================
	if (!fromCache) {
		// コンソールメニューカードの初期化
		InitializeMenuCard(spriteCommon);

		// 操作案内テキスト＆ミリタリー黒帯（枠線中央にジャストフィット配置：UIずれ解消）
		const float bannerW = 600.0f;
		const float bannerH = 34.0f; // 高さを34pxに設定し、テキストが上下枠線に重ならない余裕を確保
		const float bannerX = (kScreenWidth - bannerW) * 0.5f;
		const float bannerY = 672.0f; // 画面最下部（672～706px）の安全領域

		pressSpaceBannerBg_ = std::make_unique<Sprite>();
		pressSpaceBannerBg_->Initialize(spriteCommon, "assets/textures/white1x1.png");
		pressSpaceBannerBg_->SetAnchorPoint({ 0.0f, 0.0f });
		pressSpaceBannerBg_->SetPosition({ bannerX, bannerY });
		pressSpaceBannerBg_->SetSize({ bannerW, bannerH });
		pressSpaceBannerBg_->SetColor({ 0.01f, 0.02f, 0.01f, 0.98f }); // 完全不透明に近い漆黒ミリタリー帯
		pressSpaceBannerBg_->Update();

		pressSpaceBorders_.clear();
		auto addBannerLine = [&](const Vector2& pos, const Vector2& size, const MyMath::Vector4& col) {
			auto sp = std::make_unique<Sprite>();
			sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
			sp->SetAnchorPoint({ 0.0f, 0.0f });
			sp->SetPosition(pos);
			sp->SetSize(size);
			sp->SetColor(col);
			sp->Update();
			pressSpaceBorders_.push_back(std::move(sp));
		};
		const MyMath::Vector4 bannerLineCol = { 0.20f, 0.85f, 0.50f, 0.70f };
		// 上下の境界線 (1px)
		addBannerLine({ bannerX, bannerY }, { bannerW, 1.0f }, bannerLineCol);
		addBannerLine({ bannerX, bannerY + bannerH - 1.0f }, { bannerW, 1.0f }, bannerLineCol);
		// 左右のコーナーブラケット (2px)
		addBannerLine({ bannerX, bannerY }, { 8.0f, 2.0f }, bannerLineCol);
		addBannerLine({ bannerX, bannerY }, { 2.0f, 8.0f }, bannerLineCol);
		addBannerLine({ bannerX + bannerW - 8.0f, bannerY }, { 8.0f, 2.0f }, bannerLineCol);
		addBannerLine({ bannerX + bannerW - 2.0f, bannerY }, { 2.0f, 8.0f }, bannerLineCol);
		addBannerLine({ bannerX, bannerY + bannerH - 2.0f }, { 8.0f, 2.0f }, bannerLineCol);
		addBannerLine({ bannerX, bannerY + bannerH - 8.0f }, { 2.0f, 8.0f }, bannerLineCol);
		addBannerLine({ bannerX + bannerW - 8.0f, bannerY + bannerH - 2.0f }, { 8.0f, 2.0f }, bannerLineCol);
		addBannerLine({ bannerX + bannerW - 2.0f, bannerY + bannerH - 8.0f }, { 2.0f, 8.0f }, bannerLineCol);

		// DAWN タイトルロゴスプライト初期化（白文字 + MiG-21通過）
		titleLogoSprite_ = std::make_unique<Sprite>();
		titleLogoSprite_->Initialize(spriteCommon, "assets/textures/title_logo_dawn_cropped.png");
		titleLogoSprite_->SetAnchorPoint({ 0.5f, 0.5f });
		titleLogoSprite_->SetPosition({ kScreenWidth * 0.5f, 158.0f });
		titleLogoSprite_->SetSize({ 410.0f, 91.8f });
		titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // 鮮烈な白文字＆機体
		titleLogoSprite_->SetBlendMode(BlendMode::kNormal);
		titleLogoSprite_->Update();

		// ドローンカメラOSD初期化
		InitializeOSD(spriteCommon);

		// カメラ映像ウィンドウ閉じる演出の初期化（項目1）
		InitializeWindowCloseVisuals(spriteCommon);

		// 高品位アフターバーナーの初期化（Mig-21 単発エンジン・ショックダイヤモンド）
		afterburner_ = std::make_unique<Afterburner>();
		afterburner_->Initialize();
		afterburner_->SetTwinEngine(false);
		afterburner_->SetSingleNozzleOffset({ 0.0f, 0.011f, 14.95f });
		afterburner_->SetFlameRadius(0.38f);
		afterburner_->SetFlameLength(6.2f);
		afterburner_->SetShockDiamondCount(5);
		afterburner_->SetFlameColor({ 0.28f, 0.65f, 1.0f });
		afterburner_->SetCoreColor({ 0.88f, 0.95f, 1.0f });
		afterburner_->SetIntensity(1.35f);

#ifdef USE_IMGUI
		if (!afterburnerConfigPath_.empty()) {
			afterburner_->LoadConfig(afterburnerConfigPath_);
		}
#endif
	}

	// 正確なascent/descentアンカー計算により、バナー枠の中心にジャストフィット
	const float bannerH = 34.0f;
	const float bannerY = 672.0f;
	pressSpaceText_.Initialize("HackGen", "[ PRESS SPACE OR CLICK TO START / SKIP ]", 14.0f);
	pressSpaceText_.SetAnchorPoint({ 0.5f, 0.5f });
	pressSpaceText_.SetPosition({ kScreenWidth * 0.5f, bannerY + bannerH * 0.5f });
	pressSpaceText_.SetColor({ 0.35f, 1.0f, 0.65f, 0.95f });
	pressSpaceText_.SetDropShadow(true, { 1.5f, 1.5f }, { 0.0f, 0.0f, 0.0f, 0.95f });
	UIStateStyle pulseStyle;
	pulseStyle.color = { 0.35f, 1.0f, 0.65f, 0.95f };
	pulseStyle.scale = 1.0f;
	pulseStyle.loopMotion = UILoopMotion::Pulse;
	pulseStyle.motionIntensity = 1.15f;
	pulseStyle.motionSpeed = 2.2f;
	pressSpaceText_.SetStyle("Normal", pulseStyle);

	// UITextRegistryに登録（エディタから編集可能にする）
	UITextRegistry::GetInstance()->Register("Title_Main", &titleText_);
	UITextRegistry::GetInstance()->Register("Title_Sub", &subtitleText_);
	UITextRegistry::GetInstance()->Register("Title_BtnStart", startButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Title_BtnSettings", settingsButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Title_BtnExit", exitButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Title_PressSpace", &pressSpaceText_);

	// 全UI初期化完了後にメニューウィンドウ位置を確定更新
	UpdateMenuWindowPosition(menuWindowPos_);
}

void TitleScene::InitializeMenuCard(SpriteCommon* spriteCommon) {
	menuBorderSprites_.clear();
	menuBorderOffsets_.clear();
	buttonBorders_.clear();
	buttonBorderOutlineOffsets_.clear();
	buttonBorderBracketOffsets_.clear();
	buttonBorderIndicatorOffsets_.clear();

	const float cardWidth = 520.0f;
	const float cardHeight = 616.0f;
	const float cardX = menuWindowPos_.x; // 380.0f
	const float cardY = menuWindowPos_.y; // 52.0f

	auto addBorderLine = [this, spriteCommon, cardX, cardY](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition(pos);
		sp->SetSize(size);
		sp->SetColor(color);
		sp->Update();
		menuBorderOffsets_.push_back({ pos.x - cardX, pos.y - cardY });
		menuBorderSprites_.push_back(std::move(sp));
	};

	// 0. ウィンドウ立体ドロップシャドウ（背後のぼかし背景からウィンドウを美しく浮かび上がらせる）
	addBorderLine({ cardX + 8.0f, cardY + 10.0f }, { cardWidth, cardHeight }, { 0.0f, 0.0f, 0.0f, 0.75f });

	// 1. ターミナルウィンドウ本体背景パネル（完全な漆黒・純黒ブラック、不透明度100%）
	menuCardPanel_.Initialize(spriteCommon);
	menuCardPanel_.SetPosition({ cardX, cardY });
	menuCardPanel_.SetSize({ cardWidth, cardHeight });
	menuCardPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f }); // 100%不透明の完全黒背景

	// 2. ターミナルウィンドウ・タイトルバー背景パネル
	menuHeaderPanel_.Initialize(spriteCommon);
	menuHeaderPanel_.SetPosition({ cardX, cardY });
	menuHeaderPanel_.SetSize({ cardWidth, 32.0f });
	menuHeaderPanel_.SetBackgroundColor({ 0.03f, 0.10f, 0.06f, 1.0f }); // 重厚なダークグリーンのタイトルバー

	// 3. タイトルバーのステータスLEDインジケーターランプ
	headerLampSprite_ = std::make_unique<Sprite>();
	headerLampSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
	headerLampSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	headerLampSprite_->SetPosition({ cardX + 14.0f, cardY + 12.0f });
	headerLampSprite_->SetSize({ 8.0f, 8.0f });
	headerLampSprite_->SetColor({ 0.15f, 1.0f, 0.35f, 0.95f });
	headerLampSprite_->Update();

	// 4. タイトルバー右側のシステムステータスバッジ（誤解を招く飾りボタン [_][口][X] を廃止し、洗練されたOSステータス枠へ刷新）
	const float badgeX = cardX + cardWidth - 110.0f;
	const float badgeY = cardY + 7.0f;
	const float badgeW = 96.0f;
	const float badgeH = 18.0f;
	addBorderLine({ badgeX, badgeY }, { badgeW, badgeH }, { 0.04f, 0.14f, 0.08f, 0.90f });
	addBorderLine({ badgeX, badgeY }, { badgeW, 1.0f }, { 0.22f, 0.75f, 0.42f, 0.85f });
	addBorderLine({ badgeX, badgeY + badgeH - 1.0f }, { badgeW, 1.0f }, { 0.22f, 0.75f, 0.42f, 0.85f });
	addBorderLine({ badgeX, badgeY }, { 1.0f, badgeH }, { 0.22f, 0.75f, 0.42f, 0.85f });
	addBorderLine({ badgeX + badgeW - 1.0f, badgeY }, { 1.0f, badgeH }, { 0.22f, 0.75f, 0.42f, 0.85f });

	// 5. ウィンドウ外周境界線 (太さ 2px)
	const MyMath::Vector4 borderColor = { 0.18f, 0.82f, 0.45f, 0.85f };
	const MyMath::Vector4 cornerColor = { 0.35f, 1.0f, 0.65f, 1.00f };
	const float cLen = 20.0f;
	const float cThick = 3.0f;

	addBorderLine({ cardX, cardY }, { cardWidth, 2.0f }, borderColor);
	addBorderLine({ cardX, cardY + cardHeight - 2.0f }, { cardWidth, 2.0f }, borderColor);
	addBorderLine({ cardX, cardY }, { 2.0f, cardHeight }, borderColor);
	addBorderLine({ cardX + cardWidth - 2.0f, cardY }, { 2.0f, cardHeight }, borderColor);

	// 四隅のコーナーブラケット (太さ 3px)
	addBorderLine({ cardX, cardY }, { cLen, cThick }, cornerColor);
	addBorderLine({ cardX, cardY }, { cThick, cLen }, cornerColor);
	addBorderLine({ cardX + cardWidth - cLen, cardY }, { cLen, cThick }, cornerColor);
	addBorderLine({ cardX + cardWidth - cThick, cardY }, { cThick, cLen }, cornerColor);
	addBorderLine({ cardX, cardY + cardHeight - cThick }, { cLen, cThick }, cornerColor);
	addBorderLine({ cardX, cardY + cardHeight - cLen }, { cThick, cLen }, cornerColor);
	addBorderLine({ cardX + cardWidth - cLen, cardY + cardHeight - cThick }, { cLen, cThick }, cornerColor);
	addBorderLine({ cardX + cardWidth - cThick, cardY + cardHeight - cLen }, { cThick, cLen }, cornerColor);

	// タイトルバー下部区切り線 (太さ 2px, Y = cardY + 32.0f)
	addBorderLine({ cardX, cardY + 32.0f }, { cardWidth, 2.0f }, { 0.20f, 0.85f, 0.50f, 0.80f });

	// タイトル下の水平区切り線 (Y = cardY + 172.0f)
	addBorderLine({ cardX + 24.0f, cardY + 172.0f }, { cardWidth - 48.0f, 1.0f }, { 0.18f, 0.75f, 0.45f, 0.45f });

	// 操作案内枠（Y=482、高さ38px、幅 cardWidth - 50px）
	const float gfX = cardX + 25.0f;
	const float gfY = cardY + 430.0f;
	const float gfW = cardWidth - 50.0f;
	const float gfH = 38.0f;
	const MyMath::Vector4 gfCorner = { 0.22f, 0.85f, 0.50f, 0.65f };
	addBorderLine({ gfX, gfY }, { 10.0f, 1.0f }, gfCorner);
	addBorderLine({ gfX, gfY }, { 1.0f, 10.0f }, gfCorner);
	addBorderLine({ gfX + gfW - 10.0f, gfY }, { 10.0f, 1.0f }, gfCorner);
	addBorderLine({ gfX + gfW - 1.0f, gfY }, { 1.0f, 10.0f }, gfCorner);
	addBorderLine({ gfX, gfY + gfH - 1.0f }, { 10.0f, 1.0f }, gfCorner);
	addBorderLine({ gfX, gfY + gfH - 10.0f }, { 1.0f, 10.0f }, gfCorner);
	addBorderLine({ gfX + gfW - 10.0f, gfY + gfH - 1.0f }, { 10.0f, 1.0f }, gfCorner);
	addBorderLine({ gfX + gfW - 1.0f, gfY + gfH - 10.0f }, { 1.0f, 10.0f }, gfCorner);

	// ウィンドウ最下部ステータスバー背景＆境界線 (高さ 26px)
	const float sbY = cardY + cardHeight - 26.0f;
	addBorderLine({ cardX, sbY }, { cardWidth, 26.0f }, { 0.02f, 0.07f, 0.04f, 1.0f });
	addBorderLine({ cardX, sbY }, { cardWidth, 1.0f }, { 0.18f, 0.70f, 0.40f, 0.60f });

	// ------------------------------------------------------------
	// 各ボタンのアウトライン枠線＆サイバーブラケット（buttonBorders_）
	// ------------------------------------------------------------
	const float btnW = 390.0f;
	const float btnH = 44.0f;
	const float btnX = cardX + 65.0f;
	const float btnStartY = cardY + 196.0f;
	const float btnSpacing = 56.0f;
	const float bcLen = 8.0f;
	const float bcThick = 2.0f;

	auto createLineSprite = [spriteCommon](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition(pos);
		sp->SetSize(size);
		sp->SetColor(color);
		sp->Update();
		return sp;
	};

	for (int i = 0; i < 4; ++i) {
		float by = btnStartY + btnSpacing * static_cast<float>(i);
		ButtonBorderGroup group;
		std::vector<Vector2> outOffsets;
		std::vector<Vector2> brkOffsets;

		auto addOutline = [&](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
			outOffsets.push_back({ pos.x - cardX, pos.y - cardY });
			group.outlines.push_back(createLineSprite(pos, size, color));
		};
		auto addBracket = [&](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
			brkOffsets.push_back({ pos.x - cardX, pos.y - cardY });
			group.brackets.push_back(createLineSprite(pos, size, color));
		};

		// 外周枠線 (1px) 4本
		addOutline({ btnX, by }, { btnW, 1.0f }, { 0.15f, 0.50f, 0.30f, 0.35f });
		addOutline({ btnX, by + btnH - 1.0f }, { btnW, 1.0f }, { 0.15f, 0.50f, 0.30f, 0.35f });
		addOutline({ btnX, by }, { 1.0f, btnH }, { 0.15f, 0.50f, 0.30f, 0.35f });
		addOutline({ btnX + btnW - 1.0f, by }, { 1.0f, btnH }, { 0.15f, 0.50f, 0.30f, 0.35f });

		// 四隅ブラケット (2px) 8本
		addBracket({ btnX, by }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX, by }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX + btnW - bcLen, by }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX + btnW - bcThick, by }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX, by + btnH - bcThick }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX, by + btnH - bcLen }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX + btnW - bcLen, by + btnH - bcThick }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX + btnW - bcThick, by + btnH - bcLen }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f });

		// ボタン左端内側のソリッド・アクティブバー（ボタン枠線と完璧にアライン・統合）
		buttonBorderIndicatorOffsets_.push_back({ (btnX + 1.0f) - cardX, (by + 2.0f) - cardY });
		group.indicator = createLineSprite({ btnX + 1.0f, by + 2.0f }, { 3.0f, btnH - 4.0f }, { 0.08f, 0.25f, 0.15f, 0.25f });

		buttonBorderOutlineOffsets_.push_back(std::move(outOffsets));
		buttonBorderBracketOffsets_.push_back(std::move(brkOffsets));
		buttonBorders_.push_back(std::move(group));
	}
}

void TitleScene::UpdateMenuWindowPosition(const Vector2& newPos) {
	menuWindowPos_ = newPos;

	menuCardPanel_.SetPosition(menuWindowPos_);
	menuCardPanel_.Update();

	menuHeaderPanel_.SetPosition(menuWindowPos_);
	menuHeaderPanel_.Update();

	if (headerLampSprite_) {
		headerLampSprite_->SetPosition({ menuWindowPos_.x + 14.0f, menuWindowPos_.y + 12.0f });
		headerLampSprite_->Update();
	}

	for (size_t i = 0; i < menuBorderSprites_.size() && i < menuBorderOffsets_.size(); ++i) {
		if (menuBorderSprites_[i]) {
			menuBorderSprites_[i]->SetPosition({ menuWindowPos_.x + menuBorderOffsets_[i].x, menuWindowPos_.y + menuBorderOffsets_[i].y });
			menuBorderSprites_[i]->Update();
		}
	}

	if (titleLogoSprite_) {
		titleLogoSprite_->SetPosition({ menuWindowPos_.x + 260.0f, menuWindowPos_.y + 106.0f });
		titleLogoSprite_->SetSize({ 410.0f, 91.8f });
		titleLogoSprite_->Update();
	}

	subtitleText_.SetPosition({ menuWindowPos_.x + 260.0f, menuWindowPos_.y + 154.0f });
	subtitleText_.Update();

	const float btnX = menuWindowPos_.x + 65.0f;
	const float btnStartY = menuWindowPos_.y + 196.0f;
	const float btnSpacing = 56.0f;

	startButton_.SetPosition({ btnX, btnStartY });
	startButton_.Update();
	editorButton_.SetPosition({ btnX, btnStartY + btnSpacing });
	editorButton_.Update();
	settingsButton_.SetPosition({ btnX, btnStartY + btnSpacing * 2.0f });
	settingsButton_.Update();
	exitButton_.SetPosition({ btnX, btnStartY + btnSpacing * 3.0f });
	exitButton_.Update();

	for (size_t i = 0; i < buttonBorders_.size(); ++i) {
		auto& group = buttonBorders_[i];
		if (i < buttonBorderOutlineOffsets_.size()) {
			for (size_t j = 0; j < group.outlines.size() && j < buttonBorderOutlineOffsets_[i].size(); ++j) {
				if (group.outlines[j]) {
					group.outlines[j]->SetPosition({ menuWindowPos_.x + buttonBorderOutlineOffsets_[i][j].x, menuWindowPos_.y + buttonBorderOutlineOffsets_[i][j].y });
					group.outlines[j]->Update();
				}
			}
		}
		if (i < buttonBorderBracketOffsets_.size()) {
			for (size_t j = 0; j < group.brackets.size() && j < buttonBorderBracketOffsets_[i].size(); ++j) {
				if (group.brackets[j]) {
					group.brackets[j]->SetPosition({ menuWindowPos_.x + buttonBorderBracketOffsets_[i][j].x, menuWindowPos_.y + buttonBorderBracketOffsets_[i][j].y });
					group.brackets[j]->Update();
				}
			}
		}
		if (i < buttonBorderIndicatorOffsets_.size() && group.indicator) {
			group.indicator->SetPosition({ menuWindowPos_.x + buttonBorderIndicatorOffsets_[i].x, menuWindowPos_.y + buttonBorderIndicatorOffsets_[i].y });
			group.indicator->Update();
		}
	}
}

void TitleScene::UpdateMenuConsoleVisuals() {
	int selIdx = isMissionSelectModalOpen_ ? selectedMissionIdx_ : selectionManager_.GetSelectedIndex();
	bool blink = (std::fmod(animTimer_, 0.36f) < 0.18f);

	// ヘッダーランプの点滅更新
	if (headerLampSprite_) {
		headerLampSprite_->SetColor(blink ? MyMath::Vector4{ 0.15f, 1.0f, 0.35f, 0.95f } : MyMath::Vector4{ 0.05f, 0.30f, 0.12f, 0.30f });
		headerLampSprite_->Update();
	}

	// タイトル＆サブタイトルを完全な鮮烈グリーンに固定（外部INIファイルによる白文字上書きを完全防止）
	titleText_.SetColor({ 0.22f, 1.0f, 0.52f, 1.0f });
	subtitleText_.SetColor({ 0.35f, 0.95f, 0.60f, 0.95f });

	// ボタンテキスト色を動的設定（ロック中ボタンは減衰・アンバー調）
	const Vector4 normalGreen = { 0.35f, 0.92f, 0.55f, 0.95f };
	const Vector4 selectedGreen = { 0.85f, 1.00f, 0.90f, 1.00f };
	const Vector4 lockedNormal = { 0.35f, 0.42f, 0.38f, 0.60f };
	const Vector4 lockedSelected = { 0.85f, 0.55f, 0.30f, 0.85f };

	startButton_.SetTextColor(normalGreen);
	startButton_.SetSelectedTextColor(selectedGreen);
	editorButton_.SetTextColor(lockedNormal);
	editorButton_.SetSelectedTextColor(lockedSelected);
	settingsButton_.SetTextColor(lockedNormal);
	settingsButton_.SetSelectedTextColor(lockedSelected);
	exitButton_.SetTextColor(normalGreen);
	exitButton_.SetSelectedTextColor(selectedGreen);

	// 各ボタンのラベル文字列は左揃え固定インデント配置
	startButton_.SetLabel("[01] MISSION SELECT");
	editorButton_.SetLabel("[02] FREE FLIGHT [LOCKED]");
	settingsButton_.SetLabel("[03] SYSTEM CONFIG [LOCKED]");
	exitButton_.SetLabel("[04] ABORT / EXIT");

	// 各ボタン枠スプライトの色動的更新
	for (size_t i = 0; i < buttonBorders_.size(); ++i) {
		bool isSel = (static_cast<int>(i) == selIdx);
		bool isLocked = !isMissionSelectModalOpen_ && (i == 1 || i == 2);
		MyMath::Vector4 outlineCol;
		MyMath::Vector4 bracketCol;
		MyMath::Vector4 indicatorCol;

		if (isLocked) {
			outlineCol   = isSel ? MyMath::Vector4{ 0.85f, 0.45f, 0.25f, 0.85f } : MyMath::Vector4{ 0.18f, 0.22f, 0.18f, 0.25f };
			bracketCol   = isSel ? MyMath::Vector4{ 1.00f, 0.55f, 0.30f, 0.95f } : MyMath::Vector4{ 0.25f, 0.30f, 0.22f, 0.35f };
			indicatorCol = isSel ? MyMath::Vector4{ 1.00f, 0.45f, 0.20f, 0.95f } : MyMath::Vector4{ 0.12f, 0.08f, 0.05f, 0.15f };
		} else {
			outlineCol   = isSel ? MyMath::Vector4{ 0.75f, 1.0f, 0.85f, 0.95f } : MyMath::Vector4{ 0.15f, 0.45f, 0.28f, 0.35f };
			bracketCol   = isSel ? MyMath::Vector4{ 0.90f, 1.0f, 0.95f, 1.00f } : MyMath::Vector4{ 0.20f, 0.65f, 0.42f, 0.50f };
			indicatorCol = isSel ? MyMath::Vector4{ 0.35f, 1.0f, 0.65f, 1.00f } : MyMath::Vector4{ 0.08f, 0.22f, 0.16f, 0.20f };
		}

		for (auto& sp : buttonBorders_[i].outlines) {
			sp->SetColor(outlineCol);
			sp->Update();
		}
		for (auto& sp : buttonBorders_[i].brackets) {
			sp->SetColor(bracketCol);
			sp->Update();
		}
		if (buttonBorders_[i].indicator) {
			buttonBorders_[i].indicator->SetColor(indicatorCol);
			buttonBorders_[i].indicator->Update();
		}
	}
}

void TitleScene::DrawMenuCard() {
	menuCardPanel_.Draw();
	menuHeaderPanel_.Draw();
	if (headerLampSprite_) {
		headerLampSprite_->Draw();
	}
	for (auto& sp : menuBorderSprites_) {
		sp->Draw();
	}
	if (state_ != TitleState::SortieSequence) {
		for (auto& group : buttonBorders_) {
			for (auto& sp : group.outlines) { sp->Draw(); }
			for (auto& sp : group.brackets) { sp->Draw(); }
			if (group.indicator) { group.indicator->Draw(); }
		}
	}

	TextRenderer* tr = TextRenderer::GetInstance();
	const float cardWidth = 520.0f;
	const float cardHeight = 616.0f;
	const float cardX = menuWindowPos_.x;
	const float cardY = menuWindowPos_.y;

	// 端末ヘッダーテキスト (HackGen 13.0f) - タイトルバー左側（アクティブ時は鮮烈発光＆FOCUSバッジ）
	bool isActive = (activeWindowId_ == 0) || isMissionSelectModalOpen_;
	if (isActive) {
		DrawOSRect({ cardX, cardY }, { cardWidth, 2.0f }, { 0.70f, 1.0f, 0.88f, 1.0f }); // 上端ネオンアクセントライン
		DrawOSRect({ cardX + 14.0f, cardY + 11.0f }, { 8.0f, 8.0f }, { 0.40f, 1.0f, 0.70f, 1.0f }); // 高輝度LED
		if (isMissionSelectModalOpen_) {
			tr->Print("HackGen", "[FOCUS] TERMINAL : MISSION SELECT [v4.12]", cardX + 28.0f, cardY + 9.0f, 13.0f, { 0.80f, 1.0f, 0.92f, 1.0f });
		} else {
			tr->Print("HackGen", "[FOCUS] TERMINAL : DAWN_OS [v4.12]", cardX + 28.0f, cardY + 9.0f, 13.0f, { 0.80f, 1.0f, 0.92f, 1.0f });
		}
	} else {
		DrawOSRect({ cardX + 14.0f, cardY + 11.0f }, { 8.0f, 8.0f }, { 0.12f, 0.30f, 0.16f, 0.45f }); // 消灯LED
		if (isMissionSelectModalOpen_) {
			tr->Print("HackGen", "[IDLE]  TERMINAL : MISSION SELECT [v4.12]", cardX + 28.0f, cardY + 9.0f, 13.0f, { 0.22f, 0.50f, 0.30f, 0.65f });
		} else {
			tr->Print("HackGen", "[IDLE]  TERMINAL : DAWN_OS [v4.12]", cardX + 28.0f, cardY + 9.0f, 13.0f, { 0.22f, 0.50f, 0.30f, 0.65f });
		}
	}

	// タイトルバー右側のシステムステータス表示 (SEC: LV5 [OK])
	const float badgeX = cardX + cardWidth - (isMissionSelectModalOpen_ ? 142.0f : 110.0f);
	const float badgeY = cardY + 7.0f;
	bool secBlink = (std::fmod(animTimer_, 1.2f) < 0.6f);
	Vector4 ledColor = secBlink ? Vector4{ 0.35f, 1.0f, 0.65f, 1.0f } : Vector4{ 0.15f, 0.70f, 0.35f, 0.75f };
	DrawOSRect({ badgeX + 7.0f, badgeY + 6.0f }, { 6.0f, 6.0f }, ledColor);
	tr->Print("HackGen", "SEC:LV5 [OK]", badgeX + 18.0f, badgeY + 3.0f, 10.5f, { 0.45f, 1.0f, 0.70f, 0.95f });

	// 作戦指令ヘッダーラベル - タイトルバー直下の独立行（ロゴの上に綺麗に独立配置）
	tr->Print("HackGenRegular", "/// CLASSIFIED OPERATION DIRECTIVE ///", cardX + cardWidth * 0.5f, cardY + 44.0f, 11.5f, { 0.35f, 0.95f, 0.60f, 0.90f }, { 0.5f, 0.0f });

	// メインタイトル：DAWN ロゴスプライト（Y: 60.1〜151.9px、指令テキストと一切被らず堂々配置）
	if (titleLogoSprite_) {
		titleLogoSprite_->Draw();
	}

	// サブタイトル
	if (isMissionSelectModalOpen_) {
		tr->Print("HackGen", "- TARGET OPERATION SELECTION -", cardX + cardWidth * 0.5f, cardY + 154.0f, 14.0f, { 0.35f, 0.95f, 0.60f, 0.95f }, { 0.5f, 0.0f });
	} else {
		subtitleText_.Draw();
	}

	// システム認証ステータス行（ディバイダーラインにインラインマウント）
	// テキスト背面に完全不透明の黒ベタ矩形を敷くことで、背後の水平線を完璧にマスクして可読性を100%確保
	const float authY = cardY + 172.0f;
	const float authCenterX = cardX + cardWidth * 0.5f;
	const char* authStr = isMissionSelectModalOpen_
		? "AUTH LEVEL 5 GRANTED // TARGET OPERATION INPUT"
		: "AUTH LEVEL 5 GRANTED // READY FOR MISSION INPUT";
	Vector2 authSize = tr->CalculateTextSize("HackGenRegular", authStr, 11.0f);
	DrawOSRect({ authCenterX - (authSize.x * 0.5f + 8.0f), authY - 8.0f }, { authSize.x + 16.0f, 16.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });
	tr->Print("HackGenRegular", authStr, authCenterX, authY, 11.0f, { 0.28f, 0.88f, 0.52f, 0.90f }, { 0.5f, 0.5f });

	if (state_ == TitleState::SortieSequence) {
		// ============================================================
		// 出撃シーケンス専用コンソール（メインターミナル内部に完全統合描画）
		// 他のUI（ボタンやガイド）と一切被らず、美しい映画的演出を実現
		// ============================================================
		float consoleY = cardY + 196.0f;
		const float consoleW = cardWidth - 36.0f;
		const float consoleX = cardX + 18.0f;
		const float consoleH = 390.0f;

		// 1. 出撃コンソール専用パネル（背景ダーク ＋ ネオングリーン/ゴールド二重枠）
		bool blinkFast = (static_cast<int>(sortieTimer_ * 16.0f) % 2) == 0;
		Vector4 panelBorder = blinkFast ? Vector4{ 1.0f, 0.85f, 0.25f, 0.95f } : Vector4{ 0.35f, 1.0f, 0.65f, 0.90f };
		DrawOSPanel({ consoleX, consoleY }, { consoleW, consoleH }, { 0.01f, 0.04f, 0.02f, 0.95f }, panelBorder, 1.5f);

		// 2. 出撃バナーヘッダー
		DrawOSRect({ consoleX + 2.0f, consoleY + 2.0f }, { consoleW - 4.0f, 28.0f }, { 0.04f, 0.18f, 0.09f, 0.95f });
		tr->Print("HackGen", "[ ! ] COMBAT SORTIE INITIATED [ ! ]", consoleX + consoleW * 0.5f, consoleY + 7.0f, 13.0f, panelBorder, { 0.5f, 0.0f });

		// 3. カタパルトチャージプログレスバー（項目3: 負荷タメ・認証停滞の緩急カーブ）
		float catProgress = 0.0f;
		if (sortieTimer_ < 0.40f) {
			// 急速初期チャージ (0% -> 38%)
			float t = sortieTimer_ / 0.40f;
			catProgress = (1.0f - std::pow(1.0f - t, 2.0f)) * 0.38f;
		} else if (sortieTimer_ < 0.75f) {
			// 負荷タメ・蒸気圧充填停滞 (38% -> 42% で一瞬停滞！)
			float t = (sortieTimer_ - 0.40f) / 0.35f;
			catProgress = 0.38f + t * 0.04f;
		} else if (sortieTimer_ < 1.30f) {
			// 一気にチャージ上昇 (42% -> 89%)
			float t = (sortieTimer_ - 0.75f) / 0.55f;
			catProgress = 0.42f + (t * t * (3.0f - 2.0f * t)) * 0.47f;
		} else if (sortieTimer_ < 1.60f) {
			// インターロック解除待ちのタメ (89% -> 91% で一瞬停滞！)
			float t = (sortieTimer_ - 1.30f) / 0.30f;
			catProgress = 0.89f + t * 0.02f;
		} else if (sortieTimer_ < 1.75f) {
			// 100%突破！ (91% -> 100% 一気に完了！)
			float t = (sortieTimer_ - 1.60f) / 0.15f;
			catProgress = 0.91f + (1.0f - std::pow(1.0f - t, 3.0f)) * 0.09f;
		} else {
			catProgress = 1.0f;
		}
		catProgress = std::clamp(catProgress, 0.0f, 1.0f);

		float barY = consoleY + 40.0f;
		tr->Print("HackGen", "CATAPULT CHARGE :", consoleX + 16.0f, barY, 11.0f, { 0.35f, 0.85f, 0.55f, 0.90f });
		char catBuf[32];
		snprintf(catBuf, sizeof(catBuf), "%3.0f %%", catProgress * 100.0f);
		tr->Print("HackGen", catBuf, consoleX + consoleW - 56.0f, barY, 11.0f, { 1.0f, 0.90f, 0.25f, 0.95f });
		barY += 18.0f;

		const float gW = consoleW - 32.0f;
		DrawOSRect({ consoleX + 16.0f, barY }, { gW, 8.0f }, { 0.03f, 0.10f, 0.05f, 0.80f });
		DrawOSRect({ consoleX + 16.0f, barY }, { gW * catProgress, 8.0f }, { 0.25f, 0.95f, 0.50f, 0.95f });
		DrawOSBorder({ consoleX + 16.0f, barY }, { gW, 8.0f }, { 0.20f, 0.65f, 0.35f, 0.75f }, 1.0f);

		// 4. リアルタイム・シーケンス認証ログ
		float logY = barY + 20.0f;
		const float lineGap = 26.0f;
		const Vector4 greenLog = { 0.40f, 1.0f, 0.65f, 0.95f };
		const Vector4 goldLog  = { 1.0f, 0.90f, 0.30f, 0.98f };

		tr->Print("HackGen", "SYSTEM LOGS:", consoleX + 16.0f, logY, 10.5f, { 0.30f, 0.70f, 0.45f, 0.80f });
		logY += 18.0f;

		if (sortieTimer_ >= 0.10f) {
			tr->Print("HackGen", "[0x0400] UPLINK TO MiG-21 AVIONICS... [ONLINE]", consoleX + 18.0f, logY, 11.0f, greenLog);
			logY += lineGap;
		}
		if (sortieTimer_ >= 0.40f) {
			tr->Print("HackGen", "[0x0420] STEAM PRESSURE: 24.8 MPa [NOMINAL]", consoleX + 18.0f, logY, 11.0f, greenLog);
			logY += lineGap;
		}
		if (sortieTimer_ >= 0.80f) {
			tr->Print("HackGen", "[0x0450] WEAPONS ARMED: 20MM CANNON + R-60M", consoleX + 18.0f, logY, 11.0f, greenLog);
			logY += lineGap;
		}
		if (sortieTimer_ >= 1.30f) {
			tr->Print("HackGen", "[0x0480] CANOPY LOCKED // CLEAR FOR TAKEOFF", consoleX + 18.0f, logY, 11.0f, greenLog);
			logY += lineGap;
		}
		if (sortieTimer_ >= 1.70f) {
			tr->Print("HackGen", ">>> AFTERBURNER IGNITION // LAUNCH >>>", consoleX + consoleW * 0.5f, logY + 4.0f, 12.5f, goldLog, { 0.5f, 0.0f });
		}

		// 5. 最下部ステータスバッジ
		float badgeY = consoleY + consoleH - 34.0f;
		DrawOSPanel({ consoleX + 14.0f, badgeY }, { consoleW - 28.0f, 24.0f }, { 0.05f, 0.18f, 0.08f, 0.85f }, { 0.35f, 0.95f, 0.55f, 0.85f }, 1.0f);
		tr->Print("HackGen", "STATUS // SCRAMBLE AUTHORIZED - INTERCEPT READY", consoleX + consoleW * 0.5f, badgeY + 5.0f, 10.5f, { 1.0f, 0.95f, 0.30f, 0.98f }, { 0.5f, 0.0f });
	} else {
		// メニューボタン（コマンド行）
		startButton_.Draw();
		editorButton_.Draw();
		settingsButton_.Draw();
		exitButton_.Draw();

		// 操作ガイド (HackGen 14.0f) - ロック中の警告メッセージ表示対応
		if (lockedAlertTimer_ > 0.0f) {
			bool blinkAlert = (static_cast<int>(lockedAlertTimer_ * 14.0f) % 2) == 0;
			Vector4 alertCol = blinkAlert ? Vector4{ 1.0f, 0.40f, 0.25f, 1.0f } : Vector4{ 0.70f, 0.25f, 0.15f, 0.85f };
			tr->Print(
				"HackGen",
				"[ ! ] MODULE RESTRICTED // FEATURE CURRENTLY LOCKED",
				cardX + cardWidth * 0.5f,
				cardY + 449.0f,
				14.0f,
				alertCol,
				{ 0.5f, 0.5f }
			);
		} else {
			tr->Print(
				"HackGen",
				"[ UP / DOWN ] SELECT     [ ENTER / SPACE ] EXECUTE",
				cardX + cardWidth * 0.5f,
				cardY + 449.0f,
				14.0f,
				{ 0.35f, 0.95f, 0.55f, 0.95f },
				{ 0.5f, 0.5f }
			);
		}
	}

	// ウィンドウ最下部ステータスバー (高さ26pxのバーの中央にジャスト配置・文字つぶれ防止)
	const float sbCenterY = cardY + cardHeight - 13.0f;
	const float sbFontSize = 13.0f;
	tr->Print(
		"HackGenRegular",
		"ROOT@TACTICAL_OS: ONLINE",
		cardX + 16.0f,
		sbCenterY,
		sbFontSize,
		{ 0.35f, 0.95f, 0.60f, 0.95f },
		{ 0.0f, 0.5f }
	);
	tr->Print(
		"HackGenRegular",
		"PORT: 115200 BAUD // UTF-8",
		cardX + cardWidth - 16.0f,
		sbCenterY,
		sbFontSize,
		{ 0.30f, 0.90f, 0.55f, 0.90f },
		{ 1.0f, 0.5f }
	);

	if (isActive) {
		// アクティブウィンドウ強調: 外周ネオン発光ボーダー ＆ 四隅コーナーブラケット輝度ブースト
		DrawOSBorder({ cardX, cardY }, { cardWidth, cardHeight }, { 0.45f, 1.0f, 0.70f, 0.95f }, 1.5f);
		const float cLen = 22.0f;
		const float cThick = 3.0f;
		const Vector4 cCol = { 0.85f, 1.0f, 0.95f, 1.0f };
		DrawOSRect({ cardX, cardY }, { cLen, cThick }, cCol);
		DrawOSRect({ cardX, cardY }, { cThick, cLen }, cCol);
		DrawOSRect({ cardX + cardWidth - cLen, cardY }, { cLen, cThick }, cCol);
		DrawOSRect({ cardX + cardWidth - cThick, cardY }, { cThick, cLen }, cCol);
		DrawOSRect({ cardX, cardY + cardHeight - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ cardX, cardY + cardHeight - cLen }, { cThick, cLen }, cCol);
		DrawOSRect({ cardX + cardWidth - cLen, cardY + cardHeight - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ cardX + cardWidth - cThick, cardY + cardHeight - cLen }, { cThick, cLen }, cCol);
	} else {
		// 非アクティブウィンドウ減光: ディムオーバーレイで背景に沈め、フォーカスウィンドウを手前に浮き立たせる
		DrawOSRect({ cardX, cardY }, { cardWidth, cardHeight }, { 0.0f, 0.015f, 0.008f, 0.35f });
	}
}

void TitleScene::DrawTransitionGlitch() {
	if (transitionTimer_ <= 0.0f) { return; }

	float t = transitionTimer_ / kTransitionDuration;

	// グリッチ用水平帯の動的更新＆描画
	auto pseudoRand = [](uint32_t& s) -> float {
		s = s * 1664525u + 1013904223u;
		return static_cast<float>(s & 0xFFFF) / 65535.0f;
	};

	uint32_t s = static_cast<uint32_t>(transitionTimer_ * 1000.0f) + 77;
	for (size_t i = 0; i < glitchSprites_.size(); ++i) {
		float y = pseudoRand(s) * kScreenHeight;
		float h = 2.0f + pseudoRand(s) * 32.0f;
		float w = (0.5f + 0.5f * pseudoRand(s)) * kScreenWidth;
		float x = (pseudoRand(s) - 0.2f) * kScreenWidth;
		float alpha = (0.25f + 0.55f * pseudoRand(s)) * t;

		MyMath::Vector4 col;
		float pick = pseudoRand(s);
		if (pick < 0.40f) {
			col = { 0.02f, 0.02f, 0.02f, alpha * 1.2f }; // 黒帯
		} else if (pick < 0.75f) {
			col = { 0.1f, 0.95f, 1.0f, alpha * 0.85f };   // エレクトリックシアン
		} else {
			col = { 0.95f, 0.95f, 0.98f, alpha * 0.75f }; // 白ノイズ
		}

		glitchSprites_[i]->SetPosition({ x, y });
		glitchSprites_[i]->SetSize({ w, h });
		glitchSprites_[i]->SetColor(col);
		glitchSprites_[i]->Update();
		glitchSprites_[i]->Draw();
	}

	// 画面中央の警告テキスト（ミリタリー通信同期アラート）
	TextRenderer* tr = TextRenderer::GetInstance();
	if (std::fmod(transitionTimer_, 0.09f) < 0.06f) {
		const float cx = kScreenWidth * 0.5f;
		const float cy = kScreenHeight * 0.5f;
		tr->Print("HackGen", ">> OPTICAL FEED DISRUPTED - CHANNEL SYNCHRONIZING <<", cx, cy - 40.0f, 17.0f, { 1.0f, 0.82f, 0.15f, 0.95f }, { 0.5f, 0.5f });
		tr->Print("HackGen", "/// SENSOR RE-ACQUISITION IN PROGRESS - STAND BY ///", cx, cy - 16.0f, 13.0f, { 0.2f, 0.92f, 1.0f, 0.85f }, { 0.5f, 0.5f });
	}
}

void TitleScene::InitializeOSD(SpriteCommon* spriteCommon) {
	osdSprites_.clear();
	glitchSprites_.clear();

	auto addBar = [this, spriteCommon](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition(pos);
		sp->SetSize(size);
		sp->SetColor(color);
		sp->Update();
		osdSprites_.push_back(std::move(sp));
	};

	const float left = 48.0f;
	const float right = kScreenWidth - 48.0f;
	const float top = 36.0f;
	const float bottom = kScreenHeight - 36.0f;
	const float len = 44.0f;
	const float thick = 2.0f;
	const MyMath::Vector4 cornerColor = { 0.90f, 0.96f, 1.0f, 0.85f };
	const MyMath::Vector4 shadowColor = { 0.0f, 0.0f, 0.0f, 0.70f };

	// 四隅のL字ブラケット（黒縁ドロップシャドウ付きで明所でもクッキリ視認可能）
	auto addShadowedBar = [&](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
		addBar({ pos.x + 1.0f, pos.y + 1.0f }, size, shadowColor);
		addBar(pos, size, color);
	};

	// 左上 ┌
	addShadowedBar({ left, top }, { len, thick }, cornerColor);
	addShadowedBar({ left, top }, { thick, len }, cornerColor);
	// 右上 ┐
	addShadowedBar({ right - len, top }, { len, thick }, cornerColor);
	addShadowedBar({ right - thick, top }, { thick, len }, cornerColor);
	// 左下 └
	addShadowedBar({ left, bottom - thick }, { len, thick }, cornerColor);
	addShadowedBar({ left, bottom - len }, { thick, len }, cornerColor);
	// 右下 ┘
	addShadowedBar({ right - len, bottom - thick }, { len, thick }, cornerColor);
	addShadowedBar({ right - thick, bottom - len }, { thick, len }, cornerColor);

	// 左右の水平マーカー（ピッチラダー風）
	const float cy = kScreenHeight * 0.5f;
	const MyMath::Vector4 markerColor = { 0.90f, 0.96f, 1.0f, 0.60f };
	addShadowedBar({ left, cy - 1.0f }, { 14.0f, 2.0f }, markerColor);
	addShadowedBar({ right - 14.0f, cy - 1.0f }, { 14.0f, 2.0f }, markerColor);

	// 中央十字レティクル（HUD全体と統一した上品なミリタリーターミナルグリーン）
	const float cx = kScreenWidth * 0.5f;
	const MyMath::Vector4 reticleColor = { 0.22f, 0.88f, 0.48f, 0.65f }; // タクティカルグリーン
	const float rLen = 14.0f;
	const float rGap = 8.0f;
	const float rThick = 2.0f;

	// 十字の4本
	addShadowedBar({ cx - 1.0f, cy - rGap - rLen }, { rThick, rLen }, reticleColor);
	addShadowedBar({ cx - 1.0f, cy + rGap }, { rThick, rLen }, reticleColor);
	addShadowedBar({ cx - rGap - rLen, cy - 1.0f }, { rLen, rThick }, reticleColor);
	addShadowedBar({ cx + rGap, cy - 1.0f }, { rLen, rThick }, reticleColor);

	// センターターゲットサークル/ブラケット（直径 24px の外枠ブラケット）
	const float bSize = 12.0f;
	const float bCorner = 4.0f;
	// 左上
	addShadowedBar({ cx - bSize, cy - bSize }, { bCorner, rThick }, reticleColor);
	addShadowedBar({ cx - bSize, cy - bSize }, { rThick, bCorner }, reticleColor);
	// 右上
	addShadowedBar({ cx + bSize - bCorner, cy - bSize }, { bCorner, rThick }, reticleColor);
	addShadowedBar({ cx + bSize - rThick, cy - bSize }, { rThick, bCorner }, reticleColor);
	// 左下
	addShadowedBar({ cx - bSize, cy + bSize - rThick }, { bCorner, rThick }, reticleColor);
	addShadowedBar({ cx - bSize, cy + bSize - bCorner }, { rThick, bCorner }, reticleColor);
	// 右下
	addShadowedBar({ cx + bSize - bCorner, cy + bSize - rThick }, { bCorner, rThick }, reticleColor);
	addShadowedBar({ cx + bSize - rThick, cy + bSize - bCorner }, { rThick, bCorner }, reticleColor);

	// センタードット
	addShadowedBar({ cx - 1.5f, cy - 1.5f }, { 3.0f, 3.0f }, { 0.75f, 0.90f, 0.98f, 0.65f });

	// REC点滅インジケータ（赤ランプ）
	recDotSprite_ = std::make_unique<Sprite>();
	recDotSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
	recDotSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	recDotSprite_->SetPosition({ left + 10.0f, top + 13.0f });
	recDotSprite_->SetSize({ 8.0f, 8.0f });
	recDotSprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.95f });
	recDotSprite_->Update();

	// グリッチ用スプライト（8個）の事前初期化
	for (int i = 0; i < 8; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition({ 0.0f, 0.0f });
		sp->SetSize({ 0.0f, 0.0f });
		sp->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
		sp->Update();
		glitchSprites_.push_back(std::move(sp));
	}
}

void TitleScene::DrawOSD() {
	const float left = 48.0f;
	const float right = kScreenWidth - 48.0f;
	const float top = 36.0f;
	const float bottom = kScreenHeight - 36.0f;
	const bool isDisconnected = (CurrentLocation().id == TitleLocation::CrashedForest);

	// RECランプの点滅更新
	if (recDotSprite_) {
		if (isDisconnected) {
			// 切断時：高速な警告レッド点滅
			bool errBlink = (std::fmod(animTimer_, 0.4f) < 0.2f);
			recDotSprite_->SetColor(errBlink ? MyMath::Vector4{ 1.0f, 0.15f, 0.15f, 0.95f } : MyMath::Vector4{ 0.3f, 0.05f, 0.05f, 0.20f });
		} else {
			bool recBlink = (std::fmod(animTimer_, 1.0f) < 0.6f);
			recDotSprite_->SetColor(recBlink ? MyMath::Vector4{ 1.0f, 0.15f, 0.15f, 0.95f } : MyMath::Vector4{ 0.3f, 0.05f, 0.05f, 0.20f });
		}
		recDotSprite_->Update();
	}

	// 枠線・レティクルの動的カラー設定（墜落現場 CrashedForest では全て警告赤に！）
	float blinkAlpha = isDisconnected ? (0.70f + 0.30f * std::sin(animTimer_ * 10.0f)) : 1.0f;
	const MyMath::Vector4 redBorderCol = { 1.0f, 0.20f, 0.20f, 0.88f * blinkAlpha };
	const MyMath::Vector4 redReticleCol = { 0.98f, 0.25f, 0.25f, 0.78f * blinkAlpha };

	for (size_t i = 0; i < osdSprites_.size(); ++i) {
		auto& sp = osdSprites_[i];
		if (i % 2 == 1) { // 奇数インデックスが表面のバー（偶数は黒ドロップシャドウ）
			if (isDisconnected) {
				sp->SetColor(redBorderCol);
			} else {
				if (i <= 15) {
					// 四隅ブラケット
					sp->SetColor({ 0.90f, 0.96f, 1.0f, 0.85f });
				} else if (i <= 19) {
					// 水平ピッチマーカー
					sp->SetColor({ 0.90f, 0.96f, 1.0f, 0.60f });
				} else {
					// センターレティクル
					sp->SetColor({ 0.22f, 0.88f, 0.48f, 0.65f });
				}
			}
			sp->Update();
		}
		sp->Draw();
	}
	if (recDotSprite_) {
		recDotSprite_->Draw();
	}

	TextRenderer* tr = TextRenderer::GetInstance();
	const float cx = kScreenWidth * 0.5f;
	const float cy = kScreenHeight * 0.5f;

	if (isDisconnected) {
		// ============================================================
		// 墜落現場専用：DISCONNECTED / 信号途絶HUD表示
		// ============================================================

		// 水平グリッチノイズバーの描画（控えめに時々走る程度に調整）
		for (size_t i = 0; i < glitchSprites_.size(); ++i) {
			if (rand() % 100 < 10) { // 10%の控えめな確率で出現
				float gy = static_cast<float>(rand() % static_cast<int>(kScreenHeight));
				float gh = static_cast<float>(1 + rand() % 12);
				float gw = kScreenWidth;
				float gAlpha = 0.12f + static_cast<float>(rand() % 100) / 100.0f * 0.18f;
				MyMath::Vector4 gCol = (rand() % 3 == 0)
					? MyMath::Vector4{ 0.0f, 0.0f, 0.0f, gAlpha * 1.2f }
					: ((rand() % 2 == 0) ? MyMath::Vector4{ 0.95f, 0.95f, 0.98f, gAlpha } : MyMath::Vector4{ 0.9f, 0.15f, 0.15f, gAlpha * 0.5f });
				glitchSprites_[i]->SetPosition({ 0.0f, gy });
				glitchSprites_[i]->SetSize({ gw, gh });
				glitchSprites_[i]->SetColor(gCol);
				glitchSprites_[i]->Update();
				glitchSprites_[i]->Draw();
			}
		}

		// --- 左上: NO SIGNAL & フリーズタイムコード ---
		tr->Print("HackGen", "NO SIGNAL", left + 24.0f, top + 9.0f, 15.0f, { 1.0f, 0.25f, 0.25f, 0.95f });
		tr->Print("HackGen", "--:--:--:-- [FROZEN]", left + 120.0f, top + 9.0f, 15.0f, { 0.85f, 0.30f, 0.30f, 0.85f });
		tr->Print("HackGen", "CARRIER LINK: SEVERED // SENSOR FAULT DETECTED", left + 8.0f, top + 30.0f, 11.5f, { 1.0f, 0.45f, 0.45f, 0.80f });

		// --- 右上: DISCONNECTED 警告テキスト（点滅）---
		bool blinkRight = (std::fmod(animTimer_, 0.6f) < 0.4f);
		MyMath::Vector4 rightCol = blinkRight ? MyMath::Vector4{ 1.0f, 0.20f, 0.20f, 0.98f } : MyMath::Vector4{ 0.65f, 0.12f, 0.12f, 0.45f };
		tr->Print("HackGen", "BAT --%  SIG [NO CARRIER]  0.0Mb/s  DISCONNECTED", right - 365.0f, top + 9.0f, 13.5f, rightCol);

		// --- 画面中央: 点滅する緊急アラート ---
		bool centerBlink = (std::fmod(animTimer_, 0.8f) < 0.55f);
		if (centerBlink) {
			tr->Print("HackGen", ">> [ DISCONNECTED : SIGNAL LOST ] <<", cx, cy - 42.0f, 17.0f, { 1.0f, 0.15f, 0.15f, 0.95f }, { 0.5f, 0.5f });
			tr->Print("HackGen", "/// CRASH PROTOCOL ENGAGED - RECONNECT PENDING ///", cx, cy - 18.0f, 12.0f, { 1.0f, 0.45f, 0.45f, 0.80f }, { 0.5f, 0.5f });
		}

		// --- 左下: 途絶ロケーション ---
		tr->Print("HackGen", "CAM 03 // CRASH SITE - SECTOR 7 [DISCONNECTED]", left + 8.0f, bottom - 38.0f, 17.0f, { 1.0f, 0.30f, 0.30f, 0.95f });
		tr->Print("HackGen", "TERMINATED OPTICAL LINK // SENSOR OFFLINE", left + 8.0f, bottom - 18.0f, 12.0f, { 0.85f, 0.30f, 0.30f, 0.70f });

		// --- 右下: 切断ステータス ---
		tr->Print("HackGen", "OPTICS: CORRUPTED", right - 210.0f, bottom - 38.0f, 14.0f, { 1.0f, 0.35f, 0.35f, 0.80f });
		tr->Print("HackGen", "LINK: DISCONNECTED", right - 210.0f, bottom - 18.0f, 12.0f, { 1.0f, 0.20f, 0.20f, 0.95f });

	} else {
		// ============================================================
		// 通常ロケーション（Tunnel, Hangar）HUD表示
		// ============================================================

		// --- 左上: REC & タイムコード (HackGen) ---
		tr->Print("HackGen", "REC", left + 24.0f, top + 9.0f, 15.0f, { 1.0f, 0.95f, 0.95f, 0.95f });

		char timeBuf[32];
		int totalSec = static_cast<int>(animTimer_);
		int hours = (totalSec / 3600) % 24;
		int mins = (totalSec / 60) % 60;
		int secs = totalSec % 60;
		int frames = static_cast<int>((animTimer_ - totalSec) * 60.0f);
		snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d:%02d", hours, mins, secs, frames);
		tr->Print("HackGen", timeBuf, left + 64.0f, top + 9.0f, 15.0f, { 0.85f, 0.95f, 1.0f, 0.85f });

		// ドローン姿勢・ジャイロテレメトリ
		float dronePitch = 2.4f * std::sin(animTimer_ * 0.6f);
		float droneRoll = -1.2f * std::cos(animTimer_ * 0.8f);
		float droneAlt = 12.0f + 0.4f * std::sin(animTimer_ * 0.4f);
		char telemBuf[64];
		snprintf(telemBuf, sizeof(telemBuf), "PITCH %+.1f deg  ROLL %+.1f deg  ALT %.1fm", dronePitch, droneRoll, droneAlt);
		tr->Print("HackGen", telemBuf, left + 8.0f, top + 30.0f, 11.5f, { 0.25f, 0.88f, 0.55f, 0.70f });

		// --- 右上: バッテリー / シグナル / データレート動的表示 ---
		int batPercent = 94 - ((static_cast<int>(animTimer_ * 0.04f)) % 2);
		const char* sigBars = (static_cast<int>(animTimer_ * 1.5f) % 7 == 4) ? "|||." : "||||";
		float dataRate = 48.2f + 0.35f * std::sin(animTimer_ * 2.2f);
		char rightHudBuf[64];
		snprintf(rightHudBuf, sizeof(rightHudBuf), "BAT %d%%  SIG [%s]  %.1fMb/s  4K/60P", batPercent, sigBars, dataRate);
		tr->Print("HackGen", rightHudBuf, right - 280.0f, top + 9.0f, 13.5f, { 0.85f, 0.95f, 1.0f, 0.80f });

		// --- 左下: カメラ番号 & ロケーション (HackGen) ---
		std::string camName;
		std::string camCoord;
		if (transitionTimer_ > 0.0f) {
			camName = "CONNECTING... // ACQUIRING OPTICAL FEED";
			camCoord = "FEED SWITCH IN PROGRESS - PLEASE STAND BY";
		} else {
			switch (CurrentLocation().id) {
			case TitleLocation::Hangar:
				camName = "CAM 01 // HANGAR - BAY 4";
				camCoord = "LAT 35.6895 N  LON 139.6917 E  ALT +12m";
				break;
			case TitleLocation::Tunnel:
				camName = "CAM 02 // PURSUIT DRONE - TUNNEL 04";
				camCoord = "SPEED: MACH 1.15 // AFTERBURNER: MAXIMUM";
				break;
			default:
				camName = "CAM 01 // RECON DRONE";
				camCoord = "LAT --.-- N  LON ---.-- E";
				break;
			}
		}
		tr->Print("HackGen", camName, left + 8.0f, bottom - 38.0f, 17.0f, { 0.85f, 0.95f, 1.0f, 0.90f });
		tr->Print("HackGen", camCoord, left + 8.0f, bottom - 18.0f, 12.0f, { 0.65f, 0.75f, 0.85f, 0.60f });

		// --- 右下: 光学系 & リンク情報 (HackGen) ---
		tr->Print("HackGen", "ZOOM 1.0x   F/2.8   ISO 400", right - 210.0f, bottom - 38.0f, 14.0f, { 0.85f, 0.95f, 1.0f, 0.75f });
		tr->Print("HackGen", "LINK: SECURE LIVE FEED", right - 210.0f, bottom - 18.0f, 12.0f, { 0.45f, 0.85f, 0.55f, 0.70f });
	}
}

void TitleScene::InitializeWindowCloseVisuals(SpriteCommon* spriteCommon) {
	windowCloseMaskSprites_.clear();
	// 4枚のマスクスプライト（上下左右の黒枠）
	for (int i = 0; i < 4; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		sp->Update();
		windowCloseMaskSprites_.push_back(std::move(sp));
	}

	windowCloseBorderSprites_.clear();
	// ウィンドウタイトルバーと外枠線、クライアント内部背景、BOOTINGステータス枠（計11本）
	for (int i = 0; i < 11; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetColor({ 0.20f, 0.90f, 0.50f, 0.85f });
		sp->Update();
		windowCloseBorderSprites_.push_back(std::move(sp));
	}
}

void TitleScene::DrawWindowClose() {
	float t = windowCloseTimer_ / kWindowCloseDuration;
	t = (std::min)((std::max)(t, 0.0f), 1.0f);
	// スムーズなイージング (Smoothstep: 最初滑らかに動き始め、中央へ吸い込まれるように最小化：項目7)
	float shrink = t * t * (3.0f - 2.0f * t);
	float curW = (kScreenWidth - 32.0f) * (1.0f - shrink);
	float curH = (kScreenHeight - 32.0f) * (1.0f - shrink);
	float curX = (kScreenWidth - curW) * 0.5f;
	float curY = (kScreenHeight - curH) * 0.5f;

	if (windowCloseMaskSprites_.size() >= 4) {
		// 上マスク: (0, 0) ～ (kScreenWidth, curY)
		windowCloseMaskSprites_[0]->SetPosition({ 0.0f, 0.0f });
		windowCloseMaskSprites_[0]->SetSize({ kScreenWidth, (std::max)(0.0f, curY) });
		windowCloseMaskSprites_[0]->Update();
		windowCloseMaskSprites_[0]->Draw();

		// 下マスク: (0, curY + curH) ～ (kScreenWidth, kScreenHeight - (curY + curH))
		float bottomY = curY + curH;
		windowCloseMaskSprites_[1]->SetPosition({ 0.0f, bottomY });
		windowCloseMaskSprites_[1]->SetSize({ kScreenWidth, (std::max)(0.0f, kScreenHeight - bottomY) });
		windowCloseMaskSprites_[1]->Update();
		windowCloseMaskSprites_[1]->Draw();

		// 左マスク: (0, curY) ～ (curX, curH)
		windowCloseMaskSprites_[2]->SetPosition({ 0.0f, curY });
		windowCloseMaskSprites_[2]->SetSize({ (std::max)(0.0f, curX), (std::max)(0.0f, curH) });
		windowCloseMaskSprites_[2]->Update();
		windowCloseMaskSprites_[2]->Draw();

		// 右マスク: (curX + curW, curY) ～ (kScreenWidth - (curX + curW), curH)
		float rightX = curX + curW;
		windowCloseMaskSprites_[3]->SetPosition({ rightX, curY });
		windowCloseMaskSprites_[3]->SetSize({ (std::max)(0.0f, kScreenWidth - rightX), (std::max)(0.0f, curH) });
		windowCloseMaskSprites_[3]->Update();
		windowCloseMaskSprites_[3]->Draw();
	}

	if (curW > 16.0f && curH > 16.0f && windowCloseBorderSprites_.size() >= 11) {
		// 1. ウィンドウ内部クライアント領域の暗転背景（映像リンク切断に伴い画面がダークターミナルへ移行）
		float innerDarken = (std::min)(0.92f, 0.40f + 0.52f * t);
		windowCloseBorderSprites_[5]->SetPosition({ curX, curY });
		windowCloseBorderSprites_[5]->SetSize({ curW, curH });
		windowCloseBorderSprites_[5]->SetColor({ 0.01f, 0.04f, 0.02f, innerDarken });
		windowCloseBorderSprites_[5]->Update();
		windowCloseBorderSprites_[5]->Draw();

		// 2. ウィンドウ中央のステータス情報枠（BOOTINGメッセージを綺麗に囲むサイバーフレーム）
		// アニメーション中のアルファ曲線: 開始時に素早くフェードインし、終盤ウィンドウ消滅に合わせてフェードアウト
		float fadeIn = (std::min)(1.0f, t / 0.12f);
		float fadeOut = (std::clamp)((0.88f - t) / 0.18f, 0.0f, 1.0f);
		float textAlpha = fadeIn * fadeOut;

		TextRenderer* tr = TextRenderer::GetInstance();
		const std::string line1 = "/// VIDEO LINK DISENGAGED ///";
		const std::string line2 = "/// BOOTING DAWN TERMINAL ///";
		const float baseFontSize = 13.5f;

		Vector2 size1 = tr->CalculateTextSize("HackGenRegular", line1, baseFontSize);
		Vector2 size2 = tr->CalculateTextSize("HackGenRegular", line2, baseFontSize);
		float maxTextW = (std::max)(size1.x, size2.x);
		float lineGap = 6.0f;
		float totalTextH = baseFontSize * 2.0f + lineGap;

		// ウィンドウ枠（グリーンボーダー）の内側に完全に収めるためのマージン
		const float marginX = 20.0f;
		const float marginY = 14.0f;
		float maxBoxW = (std::max)(0.0f, curW - marginX * 2.0f);
		float maxBoxH = (std::max)(0.0f, curH - marginY * 2.0f);

		const float padX = 20.0f;
		const float padY = 12.0f;
		float desiredBoxW = maxTextW + padX * 2.0f;
		float desiredBoxH = totalTextH + padY * 2.0f;

		// ウィンドウ縮小に伴い、ステータス枠と文字サイズを滑らかに同期縮小
		float scaleX = (maxBoxW < desiredBoxW && desiredBoxW > 0.0f) ? (maxBoxW / desiredBoxW) : 1.0f;
		float scaleY = (maxBoxH < desiredBoxH && desiredBoxH > 0.0f) ? (maxBoxH / desiredBoxH) : 1.0f;
		float boxScale = (std::min)(scaleX, scaleY);

		float actualBoxW = desiredBoxW * boxScale;
		float actualBoxH = desiredBoxH * boxScale;
		float actualFontSize = baseFontSize * boxScale;
		float actualLineGap = lineGap * boxScale;

		float centerX = kScreenWidth * 0.5f;
		float centerY = kScreenHeight * 0.5f;
		float boxX = centerX - actualBoxW * 0.5f;
		float boxY = centerY - actualBoxH * 0.5f;

		// ウィンドウ内に十分なスペースがあり、文字が判読できるサイズの間だけ描画
		if (textAlpha > 0.01f && actualFontSize >= 6.5f && actualBoxW >= 40.0f && actualBoxH >= 20.0f) {
			// ステータス枠背景プレート
			windowCloseBorderSprites_[6]->SetPosition({ boxX, boxY });
			windowCloseBorderSprites_[6]->SetSize({ actualBoxW, actualBoxH });
			windowCloseBorderSprites_[6]->SetColor({ 0.02f, 0.09f, 0.04f, 0.85f * textAlpha });
			windowCloseBorderSprites_[6]->Update();
			windowCloseBorderSprites_[6]->Draw();

			// ステータス枠のサイバーボーダー（上・下・左・右）
			MyMath::Vector4 innerBorderCol = { 0.25f, 0.95f, 0.55f, 0.70f * textAlpha };
			float borderThick = (std::max)(1.0f, 1.5f * boxScale);

			windowCloseBorderSprites_[7]->SetPosition({ boxX, boxY });
			windowCloseBorderSprites_[7]->SetSize({ actualBoxW, borderThick }); // 上
			windowCloseBorderSprites_[7]->SetColor(innerBorderCol);
			windowCloseBorderSprites_[7]->Update();
			windowCloseBorderSprites_[7]->Draw();

			windowCloseBorderSprites_[8]->SetPosition({ boxX, boxY + actualBoxH - borderThick });
			windowCloseBorderSprites_[8]->SetSize({ actualBoxW, borderThick }); // 下
			windowCloseBorderSprites_[8]->SetColor(innerBorderCol);
			windowCloseBorderSprites_[8]->Update();
			windowCloseBorderSprites_[8]->Draw();

			windowCloseBorderSprites_[9]->SetPosition({ boxX, boxY });
			windowCloseBorderSprites_[9]->SetSize({ borderThick, actualBoxH }); // 左
			windowCloseBorderSprites_[9]->SetColor(innerBorderCol);
			windowCloseBorderSprites_[9]->Update();
			windowCloseBorderSprites_[9]->Draw();

			windowCloseBorderSprites_[10]->SetPosition({ boxX + actualBoxW - borderThick, boxY });
			windowCloseBorderSprites_[10]->SetSize({ borderThick, actualBoxH }); // 右
			windowCloseBorderSprites_[10]->SetColor(innerBorderCol);
			windowCloseBorderSprites_[10]->Update();
			windowCloseBorderSprites_[10]->Draw();

			// 枠内に綺麗にセンタリングしてBOOTINGテキストを描画
			float line1Y = centerY - (actualFontSize * 0.65f + actualLineGap * 0.5f);
			float line2Y = centerY + (actualFontSize * 0.65f + actualLineGap * 0.5f);

			tr->Print(
				"HackGenRegular",
				line1,
				centerX,
				line1Y,
				actualFontSize,
				{ 0.35f, 0.85f, 0.65f, 0.90f * textAlpha },
				{ 0.5f, 0.5f }
			);
			tr->Print(
				"HackGenRegular",
				line2,
				centerX,
				line2Y,
				actualFontSize,
				{ 0.30f, 1.00f, 0.58f, 1.00f * textAlpha },
				{ 0.5f, 0.5f }
			);
		}

		// 3. OSウィンドウタイトルバー
		float titleBarH = 24.0f;
		windowCloseBorderSprites_[0]->SetPosition({ curX, curY - titleBarH });
		windowCloseBorderSprites_[0]->SetSize({ curW, titleBarH });
		windowCloseBorderSprites_[0]->SetColor({ 0.03f, 0.12f, 0.06f, 0.98f });
		windowCloseBorderSprites_[0]->Update();
		windowCloseBorderSprites_[0]->Draw();

		// 4. 外枠線4辺 (鮮烈なエレクトリックネオングリーン)
		MyMath::Vector4 borderCol = { 0.25f, 0.98f, 0.58f, 0.95f };
		windowCloseBorderSprites_[1]->SetPosition({ curX, curY });
		windowCloseBorderSprites_[1]->SetSize({ curW, 2.0f }); // 上
		windowCloseBorderSprites_[1]->SetColor(borderCol);
		windowCloseBorderSprites_[1]->Update();
		windowCloseBorderSprites_[1]->Draw();

		windowCloseBorderSprites_[2]->SetPosition({ curX, curY + curH - 2.0f });
		windowCloseBorderSprites_[2]->SetSize({ curW, 2.0f }); // 下
		windowCloseBorderSprites_[2]->SetColor(borderCol);
		windowCloseBorderSprites_[2]->Update();
		windowCloseBorderSprites_[2]->Draw();

		windowCloseBorderSprites_[3]->SetPosition({ curX, curY });
		windowCloseBorderSprites_[3]->SetSize({ 2.0f, curH }); // 左
		windowCloseBorderSprites_[3]->SetColor(borderCol);
		windowCloseBorderSprites_[3]->Update();
		windowCloseBorderSprites_[3]->Draw();

		windowCloseBorderSprites_[4]->SetPosition({ curX + curW - 2.0f, curY });
		windowCloseBorderSprites_[4]->SetSize({ 2.0f, curH }); // 右
		windowCloseBorderSprites_[4]->SetColor(borderCol);
		windowCloseBorderSprites_[4]->Update();
		windowCloseBorderSprites_[4]->Draw();

		// 5. タイトルバーテキスト（最小化アニメーション中：幅に応じて動的短縮して枠内維持）
		if (curW > 80.0f) {
			float titleAlpha = (std::min)(1.0f, (curW - 80.0f) / 100.0f);
			if (curW >= 460.0f) {
				tr->Print("HackGen", "[CAM-01 : OPTICAL FEED - MINIMIZING TO SYSTEM CONSOLE...]", curX + 10.0f, curY - 19.0f, 11.5f, { 0.35f, 1.0f, 0.65f, 0.95f * titleAlpha });
			} else if (curW >= 260.0f) {
				tr->Print("HackGen", "[CAM-01 : MINIMIZING...]", curX + 10.0f, curY - 19.0f, 11.5f, { 0.35f, 1.0f, 0.65f, 0.95f * titleAlpha });
			} else if (curW >= 120.0f) {
				tr->Print("HackGen", "[CAM-01]", curX + 8.0f, curY - 19.0f, 11.0f, { 0.35f, 1.0f, 0.65f, 0.95f * titleAlpha });
			}

			if (curW >= 180.0f) {
				tr->Print("HackGen", "[ _ ] [口] [X]", curX + curW - 86.0f, curY - 19.0f, 11.5f, { 0.85f, 0.85f, 0.85f, 0.95f * titleAlpha });
			}
		}
	}
}

void TitleScene::DrawConsoleBoot() {
	pseudoOS_.ResetSpritePool();

	// 背景の完全漆黒コンソールスプライト（格納庫の透けを100%遮蔽）
	if (consoleBgSprite_) {
		consoleBgSprite_->Draw();
	}
	backgroundPanel_.SetBackgroundColor({ 0.012f, 0.02f, 0.035f, 1.0f });
	backgroundPanel_.Draw();

	float textAlpha = 1.0f;
	float exitT = 0.0f;
	if (consoleExitTimer_ > 0.0f) {
		exitT = std::clamp(consoleExitTimer_ / kConsoleExitDuration, 0.0f, 1.0f);
		textAlpha = (std::max)(0.0f, 1.0f - exitT * 1.8f);
	}

	TextRenderer* tr = TextRenderer::GetInstance();
	const float centerX = kScreenWidth * 0.5f;
	const float centerY = kScreenHeight * 0.5f;

	// 中央ターミナルパネル枠（ロード完了時にサイバーブートログ領域がスッとスムーズに展開）
	const float basePanelW = 680.0f;
	const float basePanelH = 210.0f;
	float panelW = basePanelW;
	float panelH = basePanelH;

	float bootExpand = 0.0f;
	if (isLoadingComplete_) {
		float expT = std::clamp(completeHoldTimer_ / 0.12f, 0.0f, 1.0f);
		bootExpand = expT * (2.0f - expT);
		panelH = basePanelH + 164.0f * bootExpand; // 210px -> 374px
	}

	float panelX = centerX - panelW * 0.5f;
	float panelY = centerY - panelH * 0.5f;

	// パネル背景とサイバー枠線
	Vector4 panelBorderCol = { 0.20f, 0.70f, 0.90f, 0.85f * textAlpha };
	if (consoleExitTimer_ > 0.0f) {
		// モード切替時：サイバーパルス発光
		panelBorderCol = { 0.35f + 0.50f * (1.0f - exitT), 0.90f, 0.98f, (1.0f - exitT * 0.3f) };
	}
	pseudoOS_.DrawPanel({ panelX, panelY }, { panelW, panelH },
		{ 0.01f, 0.03f, 0.05f, 0.94f * textAlpha },
		panelBorderCol, 1.2f);

	// ヘッダータイトル
	if (consoleExitTimer_ > 0.0f) {
		tr->Print("HackGen", ">> MODE SWITCH: TTY1 -> GUI_COMPOSITOR [1920x1080@60Hz] <<",
			centerX, panelY + 16.0f, 12.0f,
			{ 0.95f, 0.90f, 0.30f, 0.98f * (1.0f - exitT * 0.4f) }, { 0.5f, 0.0f });
	} else {
		tr->Print("HackGen", "DAWN_OS TACTICAL TERMINAL [v4.12.0] // SECURE SESSION",
			centerX, panelY + 16.0f, 12.0f,
			{ 0.45f, 0.85f, 0.95f, 0.90f * textAlpha }, { 0.5f, 0.0f });
	}

	pseudoOS_.DrawRect({ panelX + 16.0f, panelY + 36.0f }, { panelW - 32.0f, 1.0f },
		{ 0.20f, 0.50f, 0.65f, 0.40f * textAlpha });

	// 1. コマンド行演出（コマンドのやつ）
	std::string fullCmd = isRebootLog_
		? "root@dawn-core:~$ ./emergency_recovery --restore-desktop"
		: "root@dawn-core:~$ ./authenticate_sortie --launch-system";

	float typingProg = std::clamp(consoleBootTimer_ / 0.38f, 0.0f, 1.0f);
	int showChars = static_cast<int>(typingProg * fullCmd.size());
	std::string dispCmd = fullCmd.substr(0, showChars);

	// 点滅カーソル
	bool cursorBlink = (std::fmod(consoleBootTimer_, 0.22f) < 0.12f);
	if (typingProg < 1.0f || consoleBootTimer_ < kCommandTypingDuration) {
		if (cursorBlink) { dispCmd += "_"; }
	}

	tr->Print("HackGen", dispCmd.c_str(), panelX + 28.0f, panelY + 50.0f, 16.0f,
		{ 0.35f, 1.0f, 0.65f, 0.98f * textAlpha });

	// 2. ローディング演出（コマンドのやつ -> loading...）
	if (consoleBootTimer_ >= kCommandTypingDuration) {
		// ローディングバーの文字列構築（ユーザー指定: loading...[||||||||||]）
		constexpr int kTotalBars = 24;
		int filledBars = static_cast<int>(loadingProgress_ * kTotalBars);
		if (filledBars > kTotalBars) filledBars = kTotalBars;

		std::string barStr = "loading... [";
		for (int i = 0; i < filledBars; ++i) {
			barStr += "|";
		}
		for (int i = 0; i < (kTotalBars - filledBars); ++i) {
			barStr += " "; // 未完了空白スロット
		}
		barStr += "]";

		// 色
		Vector4 barColor = isLoadingComplete_
			? Vector4{ 0.35f, 1.0f, 0.65f, 1.0f * textAlpha }   // complete時はエレクトリックグリーン
			: Vector4{ 0.25f, 0.90f, 1.0f, 0.98f * textAlpha }; // 進行中は鮮やかなシアン

		tr->Print("HackGen", barStr.c_str(), panelX + 28.0f, panelY + 95.0f, 20.0f, barColor);

		// パーセント or complete 表示
		if (isLoadingComplete_) {
			bool blink = (std::fmod(completeHoldTimer_, 0.16f) < 0.10f);
			Vector4 compCol = blink ? Vector4{ 0.45f, 1.0f, 0.70f, 1.0f * textAlpha } : Vector4{ 0.25f, 0.85f, 0.55f, 0.85f * textAlpha };
			tr->Print("HackGen", "complete!", panelX + panelW - 28.0f, panelY + 95.0f, 20.0f, compCol, { 1.0f, 0.0f });

			// 完了時ステータス
			tr->Print("HackGen", ">> ACCESS GRANTED // INITIALIZING SYSTEM DAEMONS <<",
				centerX, panelY + 144.0f, 14.0f, compCol, { 0.5f, 0.0f });

			// 区切り線（ブートログ領域とのセパレーター）
			if (bootExpand > 0.05f) {
				pseudoOS_.DrawRect({ panelX + 16.0f, panelY + 164.0f }, { (panelW - 32.0f) * bootExpand, 1.0f },
					{ 0.20f, 0.60f, 0.75f, 0.45f * textAlpha * bootExpand });

				// 3. 高速ブートログストリーム（コンピュータ演出）
				struct BootLogEntry {
					float time;
					const char* tag;
					const char* msg;
				};

				static const BootLogEntry kNormalLogs[] = {
					{ 0.04f, "[  OK  ]", "CORE: DAWN Kernel v4.12.0-TAC (SMP x86_64, 16 Cores Online)" },
					{ 0.10f, "[  OK  ]", "VRAM: Direct3D 12.2 GPU Pipeline // Descriptor Heap Ready" },
					{ 0.16f, "[  OK  ]", "LINK: Secure Optical Drone Carrier [bay4_cam01: 60FPS LOCKED]" },
					{ 0.22f, "[  OK  ]", "BUS: MIL-STD-1553B Avionics Bus Synced // Telemetry Active" },
					{ 0.28f, "[  OK  ]", "WM: Starting Tactical Multi-Window Compositor (DAWN_WM v4.12)" },
					{ 0.34f, "[  OK  ]", "WORKSPACES: Spawning [01:MAIN_TERM, 02:CAM_FEED, 03:TELEMETRY]" },
					{ 0.40f, ">>",      "LAUNCHING SECURE DESKTOP GUI ENVIRONMENT..." }
				};

				static const BootLogEntry kRebootLogs[] = {
					{ 0.04f, "[  OK  ]", "RECOVERY: Restoring kernel state from crashdump // TTY1" },
					{ 0.10f, "[  OK  ]", "REPAIR: Journal verified clean // Memory pages zeroed" },
					{ 0.16f, "[  OK  ]", "VRAM: Rebinding D3D12 device & swapchain descriptors" },
					{ 0.22f, "[  OK  ]", "NET: Optical drone carrier telemetry re-synchronized" },
					{ 0.28f, "[  OK  ]", "WM: Re-launching DAWN_WM desktop compositor daemon" },
					{ 0.34f, "[  OK  ]", "SESSION: Restoring operational mission workspaces" },
					{ 0.40f, ">>",      "DESKTOP SESSION RESTORED // READY FOR COMMAND..." }
				};

				const BootLogEntry* logs = isRebootLog_ ? kRebootLogs : kNormalLogs;
				const int numLogs = 7;
				for (int i = 0; i < numLogs; ++i) {
					if (completeHoldTimer_ >= logs[i].time) {
						float lineY = panelY + 174.0f + i * 22.0f;
						bool isPrompt = (i == numLogs - 1);
						bool isLatest = (i == numLogs - 1) || (completeHoldTimer_ < logs[i + 1].time);

						if (isPrompt) {
							tr->Print("HackGen", logs[i].tag, panelX + 28.0f, lineY, 11.5f,
								{ 1.0f, 0.90f, 0.30f, 1.0f * textAlpha });
							std::string promptMsg = logs[i].msg;
							if (isLatest && std::fmod(completeHoldTimer_, 0.16f) < 0.10f) {
								promptMsg += " _";
							}
							tr->Print("HackGen", promptMsg.c_str(), panelX + 54.0f, lineY, 11.5f,
								{ 0.40f, 1.0f, 0.70f, 0.98f * textAlpha });
						} else {
							tr->Print("HackGen", logs[i].tag, panelX + 28.0f, lineY, 10.5f,
								{ 0.30f, 1.0f, 0.60f, 0.95f * textAlpha });
							tr->Print("HackGen", logs[i].msg, panelX + 90.0f, lineY, 10.5f,
								{ 0.60f, 0.88f, 0.98f, 0.88f * textAlpha });
						}
					}
				}

				// 右下のハードウェアテレメトリ
				tr->Print("HackGen", "RAM: [64GB OK]  GPU: [D3D12 OK]  SYS: [ONLINE]",
					panelX + panelW - 28.0f, panelY + panelH - 18.0f, 10.0f,
					{ 0.30f, 0.75f, 0.50f, 0.65f * textAlpha }, { 1.0f, 0.0f });
			}
		} else {
			char pctBuf[32];
			snprintf(pctBuf, sizeof(pctBuf), "%3d %%", static_cast<int>(loadingProgress_ * 100.0f));
			tr->Print("HackGen", pctBuf, panelX + panelW - 28.0f, panelY + 95.0f, 18.0f,
				{ 0.50f, 0.85f, 0.95f, 0.90f * textAlpha }, { 1.0f, 0.0f });

			// ロードフェーズテキスト（タイトルの全カメラ等の事前読み込み状況をリアルタイム表示）
			const char* phaseText = loadingPhaseText_;
			if (!phaseText || phaseText[0] == '\0') {
				phaseText = "LOADING ALL TITLE CAMERAS & SENSORS...";
			}
			tr->Print("HackGen", phaseText, centerX, panelY + 152.0f, 13.0f,
				{ 0.35f, 0.75f, 0.85f, 0.80f * textAlpha }, { 0.5f, 0.0f });
		}
	}

	// 画面全体のゆっくり動く繊細な走査線
	float scanY = std::fmod(consoleBootTimer_ * 120.0f, kScreenHeight);
	pseudoOS_.DrawRect({ 0.0f, scanY }, { kScreenWidth, 1.0f }, { 0.20f, 0.70f, 0.90f, 0.15f * textAlpha });

	// ============================================================
	// コンソールからデスクトップへのGUIモード切替演出
	// ============================================================
	if (consoleExitTimer_ > 0.0f) {
		// 1. 横断CRTラスタースキャンビーム（画面上端から下端へ一気に走査してデスクトップを展開）
		float beamY = exitT * kScreenHeight;
		pseudoOS_.DrawRect({ 0.0f, beamY - 1.0f }, { kScreenWidth, 3.0f },
			{ 0.65f, 1.0f, 0.85f, (1.0f - exitT * 0.5f) * 0.90f });
		pseudoOS_.DrawRect({ 0.0f, beamY - 8.0f }, { kScreenWidth, 16.0f },
			{ 0.15f, 0.80f, 0.45f, (1.0f - exitT) * 0.30f });

		// 2. ランダムサイバーグリッチスライス
		uint32_t seed = static_cast<uint32_t>(consoleExitTimer_ * 1000.0f) + 123;
		for (size_t gi = 0; gi < glitchSprites_.size(); ++gi) {
			seed = seed * 1664525u + 1013904223u;
			float gy = (static_cast<float>(seed & 0xFFFF) / 65535.0f) * kScreenHeight;
			seed = seed * 1664525u + 1013904223u;
			float gh = 1.0f + (static_cast<float>(seed & 0xFFFF) / 65535.0f) * 10.0f;
			seed = seed * 1664525u + 1013904223u;
			float gx = (static_cast<float>(seed & 0xFFFF) / 65535.0f) * kScreenWidth * 0.6f;
			seed = seed * 1664525u + 1013904223u;
			float gw = 80.0f + (static_cast<float>(seed & 0xFFFF) / 65535.0f) * 320.0f;

			glitchSprites_[gi]->SetPosition({ gx, gy });
			glitchSprites_[gi]->SetSize({ gw, gh });
			glitchSprites_[gi]->SetColor({ 0.35f, 1.0f, 0.65f, (1.0f - exitT) * 0.40f });
			glitchSprites_[gi]->Update();
			glitchSprites_[gi]->Draw();
		}
	}
}

void TitleScene::DrawSortieSequence() {
	// 背景オーバーレイ
	backgroundPanel_.Draw();

	// フルOSデスクトップ環境（マルチウィンドウ・タスクバー・ターミナル内統合出撃コンソール）
	DrawOSDesktop();

	// 終盤のブラックアウト発光（黒フェードアウト）
	if (sortieFlashSprite_) {
		float blackAlpha = 0.0f;
		if (sortieTimer_ > 1.25f) {
			float p = (sortieTimer_ - 1.25f) / (kSortieDuration - 1.25f);
			p = (std::min)(1.0f, (std::max)(0.0f, p));
			blackAlpha = p * (3.0f - 2.0f * p); // スムーズなイージングで暗転
		}
		if (blackAlpha > 0.001f) {
			TextRenderer::GetInstance()->Flush(); // UIテキストを先に描画し、その上に黒フェードを覆い被せる
		}
		sortieFlashSprite_->SetColor({ 0.0f, 0.0f, 0.0f, blackAlpha });
		sortieFlashSprite_->Update();
		sortieFlashSprite_->Draw();
	}
}

// ------------------------------------------------------------
// 項目10: セッション終了・再認証セキュリティ演出描画
// ------------------------------------------------------------
void TitleScene::DrawReAuthentication() {
	if (consoleBgSprite_) {
		consoleBgSprite_->Draw();
	}
	backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	backgroundPanel_.Draw();

	TextRenderer* tr = TextRenderer::GetInstance();
	float centerX = kScreenWidth * 0.5f;
	float centerY = kScreenHeight * 0.5f;

	// タイトル
	tr->Print(
		"HackGen",
		"[ SESSION CLOSED. RE-AUTHENTICATION REQUIRED ]",
		centerX,
		centerY - 100.0f,
		28.0f,
		{ 1.0f, 0.35f, 0.35f, 0.95f },
		{ 0.5f, 0.0f }
	);

	tr->Print(
		"HackGen",
		"(セッション終了。再認証が必要です)",
		centerX,
		centerY - 60.0f,
		18.0f,
		{ 0.85f, 0.85f, 0.85f, 0.85f },
		{ 0.5f, 0.0f }
	);

	// ユーザー名
	if (reauthTimer_ >= 0.35f) {
		tr->Print(
			"HackGen",
			"USER IDENTITY: PILOT_01 // LEVEL-4 SECURITY CLEARANCE",
			centerX,
			centerY - 10.0f,
			18.0f,
			{ 0.30f, 0.90f, 0.60f, 0.90f },
			{ 0.5f, 0.0f }
		);
	}

	// 自動パスワード入力 (0.7s〜1.4s で ***** が1文字ずつ打たれる)
	if (reauthTimer_ >= 0.70f) {
		float pwTime = reauthTimer_ - 0.70f;
		int starCount = std::clamp(static_cast<int>(pwTime / 0.12f), 0, 5);
		std::string stars(starCount, '*');

		bool blink = (std::fmod(reauthTimer_, 0.20f) < 0.10f);
		std::string passDisplay = "PASSWORD: [ " + stars;
		if (starCount < 5 && blink) {
			passDisplay += "_";
		}
		passDisplay += " ]";

		tr->Print(
			"HackGen",
			passDisplay.c_str(),
			centerX,
			centerY + 30.0f,
			22.0f,
			{ 0.25f, 1.0f, 0.55f, 1.0f },
			{ 0.5f, 0.0f }
		);
	}

	// 認証成功表示 (1.5s以降)
	if (reauthTimer_ >= 1.50f) {
		tr->Print(
			"HackGen",
			">> [ AUTHENTICATION SUCCESSFUL. WELCOME BACK ] <<",
			centerX,
			centerY + 75.0f,
			20.0f,
			{ 1.0f, 0.90f, 0.30f, 1.0f },
			{ 0.5f, 0.0f }
		);
	}

	// スキップ案内
	tr->Print(
		"HackGen",
		"[ PRESS SPACE OR CLICK TO SKIP ]",
		centerX,
		kScreenHeight - 50.0f,
		14.0f,
		{ 0.5f, 0.7f, 0.6f, 0.6f },
		{ 0.5f, 0.5f }
	);
}
