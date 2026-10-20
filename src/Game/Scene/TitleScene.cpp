#include "TitleSceneInternal.h"
#include <chrono>

using namespace TitleSceneInternal;

// 静的リソースキャッシュの実体定義
TitleScene::TitleResourceCache TitleScene::s_cache_;

void TitleScene::ClearStaticCache() {
	s_cache_ = TitleResourceCache{};
}

void TitleScene::SetCursorVisible(bool visible) {
	isCursorShown_ = visible;
#ifdef USE_IMGUI
	ImGui::SetMouseCursor(visible ? ImGuiMouseCursor_Arrow : ImGuiMouseCursor_None);
#endif
	HWND hwnd = Input::GetInstance() ? Input::GetInstance()->GetHwnd() : nullptr;
	bool isForeground = (hwnd && ::GetForegroundWindow() == hwnd);
	if (visible || !isForeground) {
		while (ShowCursor(TRUE) < 0) {}
		::SetCursor(LoadCursor(nullptr, IDC_ARROW));
	} else {
		while (ShowCursor(FALSE) >= 0) {}
		::SetCursor(nullptr);
	}
}

void TitleScene::Initialize() {
	auto tStart = std::chrono::high_resolution_clock::now();
	SetCursorVisible(false); // ドローン視点・コンソール画面ではOSカーソルを完全非表示（項目7）
	sceneID = SCENE::TITLE;
	pseudoOS_.ResetSpritePool();
	selectionManager_.Clear();
	lastMenuSelIdx_ = 0;
	subWindowSwitchTimer_ = 0.0f;
	animTimer_ = 0.0f;
	menuEnterTimer_ = 0.0f;
	activeSatLayer_ = 1;
	activeSysDetailTab_ = 0;
	activeIntelTarget_ = 0;
	isEngineTesting_ = false;
	engineTestTimer_ = 0.0f;
	defconLevel_ = 2;
	loadingProgress_ = 0.0f;
	isLoadingComplete_ = false;
	completeHoldTimer_ = 0.0f;
	prewarmPhase_ = 0;
	isTitleCamerasPrewarmed_ = false;
	loadingPhaseText_ = "INITIALIZING OPTICAL CAMERAS...";
	isStartMenuOpen_ = false;
	isMissionSelectModalOpen_ = false;
	missionSelectModalAnim_ = 0.0f;

	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();
	TextureManager* tm = TextureManager::GetInstance();

	// テキストスプライトプールを事前確保（実行時のD3D12バッファ生成ヒッチを完全防止）
	TextRenderer::GetInstance()->PrewarmPool(2500);

	// 必須テクスチャのロード
	tm->LoadTexture("assets/textures/white1x1.png");
	whiteTexIndex_ = tm->GetTextureIndexByFilePath("assets/textures/white1x1.png");

	tm->LoadTexture("assets/textures/soft_shadow.png");
	softShadowTexIndex_ = tm->GetTextureIndexByFilePath("assets/textures/soft_shadow.png");

	tm->LoadTexture("assets/textures/crash_scorch.png");
	crashScorchTexIndex_ = tm->GetTextureIndexByFilePath("assets/textures/crash_scorch.png");

	tm->LoadTexture("assets/textures/tunnel_wall.png");
	tunnelWallTexIndex_ = tm->GetTextureIndexByFilePath("assets/textures/tunnel_wall.png");

	tm->LoadTexture("assets/textures/title_logo_dawn_cropped.png");

	// パーティクルグループの作成（CAM 03 墜落現場用: リアルな濃煙パフテクスチャ）
	ParticleManager::GetInstance()->CreateParticleGroup("TitleSmoke", "assets/textures/smoke_puff.png");
	ParticleManager::GetInstance()->SetBlendMode("TitleSmoke", BlendMode::kNormal);

	ParticleManager::GetInstance()->CreateParticleGroup("TitleSpark", "assets/textures/circle.png");
	ParticleManager::GetInstance()->SetBlendMode("TitleSpark", BlendMode::kAdd);

	// ============================
	// 3D背景の初期化 (A+B)
	// ============================
	CameraManager::GetInstance()->CreateCamera("TitleCamera");
	CameraManager::GetInstance()->SetActiveCamera("TitleCamera");
	Camera* titleCamera = CameraManager::GetInstance()->GetActiveCamera();
	if (titleCamera) {
		titleCamera->SetTranslate({0.0f, 2.0f, -15.0f});
		titleCamera->SetRotate({0.0f, 0.0f, 0.0f});
		titleCamera->Update();
	}

	tm->LoadTexture("assets/textures/cedar_bridge_sunset_1_2k.dds");
	uint32_t skyboxTexIndex = tm->GetTextureIndexByFilePath("assets/textures/cedar_bridge_sunset_1_2k.dds");
	Object3dCommon::GetInstance()->SetDefaultEnvTextureIndex(skyboxTexIndex);

	const bool fromCache = s_cache_.isCached;
	if (fromCache) {
		// ============================================================
		// キャッシュからの0ms高速復元（D3D12リソース生成2,000バッファ完全スキップ）
		// ============================================================
		skybox_ = std::move(s_cache_.skybox);
		locations_ = std::move(s_cache_.locations);
		afterburner_ = std::move(s_cache_.afterburner);
		consoleBgSprite_ = std::move(s_cache_.consoleBgSprite);
		sortieFlashSprite_ = std::move(s_cache_.sortieFlashSprite);
		recDotSprite_ = std::move(s_cache_.recDotSprite);
		titleLogoSprite_ = std::move(s_cache_.titleLogoSprite);
		headerLampSprite_ = std::move(s_cache_.headerLampSprite);
		pressSpaceBannerBg_ = std::move(s_cache_.pressSpaceBannerBg);
		osSpritePool_ = std::move(s_cache_.osSpritePool);
		cursorSprites_ = std::move(s_cache_.cursorSprites);
		windowCloseMaskSprites_ = std::move(s_cache_.windowCloseMaskSprites);
		windowCloseBorderSprites_ = std::move(s_cache_.windowCloseBorderSprites);
		menuBorderSprites_ = std::move(s_cache_.menuBorderSprites);
		buttonBorders_ = std::move(s_cache_.buttonBorders);
		glitchSprites_ = std::move(s_cache_.glitchSprites);
		osdSprites_ = std::move(s_cache_.osdSprites);
		pressSpaceBorders_ = std::move(s_cache_.pressSpaceBorders);
		menuBorderOffsets_ = s_cache_.menuBorderOffsets;
		buttonBorderOutlineOffsets_ = s_cache_.buttonBorderOutlineOffsets;
		buttonBorderBracketOffsets_ = s_cache_.buttonBorderBracketOffsets;
		buttonBorderIndicatorOffsets_ = s_cache_.buttonBorderIndicatorOffsets;
		s_cache_.isCached = false; // 所有権を this に移行

		// カメラを全オブジェクトに再接続
		if (skybox_) {
			skybox_->SetCamera(titleCamera);
		}
		for (auto& loc : locations_) {
			if (loc.envObject) loc.envObject->SetCamera(titleCamera);
			for (auto& extra : loc.extraEnvParts) {
				if (extra.object) extra.object->SetCamera(titleCamera);
			}
			if (loc.visualModel) loc.visualModel->SetCamera(titleCamera);
			if (loc.debrisModel) loc.debrisModel->SetCamera(titleCamera);
		}

		// スプライトステートとウィンドウ位置のリセット
		osSpritePoolIdx_ = 0;
		if (consoleBgSprite_) {
			consoleBgSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
			consoleBgSprite_->Update();
		}
		if (sortieFlashSprite_) {
			sortieFlashSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
			sortieFlashSprite_->Update();
		}

		menuWindowPos_ = { 392.0f, 44.0f };
		menuCardPanel_.Initialize(spriteCommon);
		menuCardPanel_.SetPosition(menuWindowPos_);
		menuCardPanel_.SetSize({ 520.0f, 616.0f });
		menuCardPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f });

		menuHeaderPanel_.Initialize(spriteCommon);
		menuHeaderPanel_.SetPosition(menuWindowPos_);
		menuHeaderPanel_.SetSize({ 520.0f, 34.0f });
		menuHeaderPanel_.SetBackgroundColor({ 0.03f, 0.10f, 0.06f, 1.0f });

		isWindowDragging_ = false;
		wasLButtonDown_ = false;
		isStartMenuOpen_ = false;
		isMissionSelectModalOpen_ = false;
		missionSelectModalAnim_ = 0.0f;
		ResetOSDesktopWindows();
	} else {
		// ============================================================
		// 初回生成（パーツ・セグメント・モデルの削減は一切行わず100%保持）
		// ============================================================
		// コンソールブート時の完全漆黒背景スプライト（項目1: 格納庫の透けを100%遮蔽）
		consoleBgSprite_ = std::make_unique<Sprite>();
		consoleBgSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
		consoleBgSprite_->SetAnchorPoint({ 0.0f, 0.0f });
		consoleBgSprite_->SetPosition({ 0.0f, 0.0f });
		consoleBgSprite_->SetSize({ kScreenWidth, kScreenHeight });
		consoleBgSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		consoleBgSprite_->Update();

		// 出撃シーケンス用ブラックアウトスプライト（黒フェード）
		sortieFlashSprite_ = std::make_unique<Sprite>();
		sortieFlashSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sortieFlashSprite_->SetAnchorPoint({ 0.0f, 0.0f });
		sortieFlashSprite_->SetPosition({ 0.0f, 0.0f });
		sortieFlashSprite_->SetSize({ kScreenWidth, kScreenHeight });
		sortieFlashSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
		sortieFlashSprite_->Update();

		// フルOSデスクトップ環境の初期化（512枚プール＆24枚カーソル）
		InitializeOSDesktop(spriteCommon);

		skybox_ = std::make_unique<Skybox>();
		skybox_->Initialize(SkyboxCommon::GetInstance());
		skybox_->SetCamera(titleCamera);
		skybox_->SetTextureIndex(skyboxTexIndex);

		tm->LoadTexture("assets/textures/qwantani_dusk_2_puresky_2k.dds");
		const uint32_t duskSky = tm->GetTextureIndexByFilePath("assets/textures/qwantani_dusk_2_puresky_2k.dds");
		const uint32_t whiteSky = tm->GetTextureIndexByFilePath("assets/textures/white1x1.png");

		InitializeLocations3D(titleCamera, duskSky, whiteSky);
	}

	// 誘導灯・回転灯の自発光（エミッシブ）設定
	Model* guideModel = ModelManager::GetInstance()->FindModel("Resources/models/title/Tunnel_guide.obj");
	if (guideModel) {
		guideModel->SetMaterialEnableLighting(0, false);
		guideModel->SetMaterialColor(0, { guideEmissiveColor_.x, guideEmissiveColor_.y, guideEmissiveColor_.z, 1.0f });
		guideModel->SetMaterialEmissive(0, guideEmissiveColor_, guideEmissiveIntensity_);
	}
	Model* rotateModel = ModelManager::GetInstance()->FindModel("Resources/models/title/Tunnel_rotate.obj");
	if (rotateModel) {
		rotateModel->SetMaterialEnableLighting(0, false);
		rotateModel->SetMaterialColor(0, { 2.5f, 0.8f, 0.1f, 1.0f });
		rotateModel->SetMaterialEmissive(0, { 2.5f, 0.8f, 0.1f }, 3.5f);
	}

	// エントリ理由に応じたステート初期化
	InitializeEntryState();

	// ライティング設定
	Object3dCommon::GetInstance()->SetShadingModel(1);
	Object3dCommon::GetInstance()->SetSpecularModel(2);
	ChangeLocation(0);

	// UI初期化
	InitializeUI(spriteCommon, fromCache);

	auto tEnd = std::chrono::high_resolution_clock::now();
	double totalMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	char buf[256];
	snprintf(buf, sizeof(buf), "  [TitleScene::Initialize] Initialized in %.2f ms (%.4f s) [%s]\n",
		totalMs, totalMs / 1000.0, fromCache ? "FROM CACHE" : "FRESH LOAD");
	OutputDebugStringA(buf);

	FILE* fp = nullptr;
	if (fopen_s(&fp, "scene_transition.log", "a") == 0 && fp) {
		fputs(buf, fp);
		fclose(fp);
	}
}

void TitleScene::Update() {
	float dt = 1.0f / 60.0f;
	animTimer_ += dt;
	stateTimer_ += dt;

	// マウスカーソルが非表示モードの場合、自アプリがフォアグラウンドの時のみ消去（他アプリ操作時はOS標準カーソルを表示）
	HWND hwnd = Input::GetInstance() ? Input::GetInstance()->GetHwnd() : nullptr;
	bool isForeground = (hwnd && ::GetForegroundWindow() == hwnd);
	if (!isForeground) {
		while (ShowCursor(TRUE) < 0) {}
		::SetCursor(LoadCursor(nullptr, IDC_ARROW));
	} else if (!isCursorShown_) {
#ifdef USE_IMGUI
		ImGui::SetMouseCursor(ImGuiMouseCursor_None);
#endif
		::SetCursor(nullptr);
	}

	if (afterburner_) {
		afterburner_->Update(dt, afterburner_->GetIntensity());
	}

	if (transitionTimer_ > 0.0f) {
		transitionTimer_ -= dt;
		if (transitionTimer_ < 0.0f) { transitionTimer_ = 0.0f; }
	}

	if (state_ == TitleState::DroneView) {
		cutTimer_ += dt;
		// 一定時間ごとに次のロケーションへカット切替
		if (!lockLocation_ && cutTimer_ > kCutDuration) {
			ChangeLocation((currentLocationIndex_ + 1) % static_cast<int>(locations_.size()));
		}
	}

	LocationData& loc = CurrentLocation();

	// ★ 重要：カメラ更新を機体・背景モデルの行列計算より「先」に実行！
	if (state_ == TitleState::DroneView || state_ == TitleState::WindowClose || state_ == TitleState::ConsoleBoot) {
		UpdateLocationCamera(loc, dt, false);
	} else if (state_ == TitleState::Menu) {
		UpdateLocationCamera(loc, dt, true);
	}

	// 現在ロケーションの機体・残骸・背景・パーティクルを更新
	UpdateLocations3D(dt);

	// 各ステートごとの更新ロジック
	switch (state_) {
	case TitleState::DroneView:
		UpdateDroneViewState(dt);
		break;
	case TitleState::WindowClose:
		UpdateWindowCloseState(dt);
		break;
	case TitleState::ConsoleBoot:
		UpdateConsoleBootState(dt);
		break;
	case TitleState::ReAuthentication:
		UpdateReAuthState(dt);
		break;
	case TitleState::Menu:
		UpdateMenuState(dt);
		break;
	case TitleState::SortieSequence:
		UpdateSortieState(dt);
		break;
	}

	// 環境切り替え（Skybox）
	if (skybox_) {
		skybox_->SetTextureIndex(loc.skyboxTexIndex);
		skybox_->SetColor(loc.skyboxColor);
		skybox_->Update();
	}

#ifdef USE_IMGUI
	ImGui::Begin("TITLE SCENE DEBUG");
	ImGui::Text("State: %s", state_ == TitleState::DroneView ? "DroneView" : "Menu");
	ImGui::Checkbox("Lock Location", &lockLocation_);
	ImGui::SameLine();
	ImGui::Checkbox("A/B Editor", &showAfterburnerEditor_);
	for (int i = 0; i < static_cast<int>(locations_.size()); ++i) {
		if (i > 0) { ImGui::SameLine(); }
		if (ImGui::RadioButton(locations_[i].name, currentLocationIndex_ == i)) {
			ChangeLocation(i);
		}
	}
	ImGui::Separator();
	LocationData& dbg = CurrentLocation();
	ImGui::Text("Env Model: %s (%s)", dbg.envModelPath.c_str(), dbg.envObject ? "LOADED" : "MISSING");
	if (ImGui::TreeNode("Transform")) {
		ImGui::DragFloat3("Origin", &dbg.origin.x, 0.1f);
		ImGui::DragFloat3("Env Scale", &dbg.envScale.x, 0.01f);
		ImGui::DragFloat3("Env Rotation", &dbg.envRotation.x, 0.01f);
		ImGui::DragFloat3("Aircraft Offset", &dbg.aircraftOffset.x, 0.05f);
		ImGui::DragFloat3("Aircraft Rotation", &dbg.aircraftRotation.x, 0.01f);
		if (dbg.debrisModel) {
			ImGui::DragFloat3("Debris Offset", &dbg.debrisOffset.x, 0.05f);
			ImGui::DragFloat3("Debris Rotation", &dbg.debrisRotation.x, 0.01f);
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Camera")) {
		ImGui::DragFloat("Distance", &dbg.camera.distance, 0.1f, 1.0f, 100.0f);
		ImGui::DragFloat("Distance Sway", &dbg.camera.distanceSway, 0.05f, 0.0f, 10.0f);
		ImGui::DragFloat("Height", &dbg.camera.height, 0.05f);
		ImGui::DragFloat("Height Sway", &dbg.camera.heightSway, 0.05f, 0.0f, 10.0f);
		ImGui::DragFloat("LookAt Height", &dbg.camera.lookAtHeight, 0.05f);
		ImGui::DragFloat("Orbit Speed", &dbg.camera.orbitSpeed, 0.005f);
		ImGui::DragFloat("Bank Tilt", &dbg.camera.bankTilt, 0.01f, 0.0f, 2.0f);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Lighting")) {
		LocationLighting& l = dbg.lighting;
		bool useDir = (l.lightType & 1) != 0, usePoint = (l.lightType & 2) != 0, useSpot = (l.lightType & 4) != 0;
		ImGui::Checkbox("Dir", &useDir); ImGui::SameLine();
		ImGui::Checkbox("Point", &usePoint); ImGui::SameLine();
		ImGui::Checkbox("Spot", &useSpot);
		l.lightType = (useDir ? 1 : 0) | (usePoint ? 2 : 0) | (useSpot ? 4 : 0);
		ImGui::ColorEdit3("Dir Color", &l.dirColor.x);
		ImGui::DragFloat3("Dir Direction", &l.dirDirection.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Dir Intensity", &l.dirIntensity, 0.01f, 0.0f, 10.0f);
		ImGui::ColorEdit3("Point Color", &l.pointColor.x);
		ImGui::DragFloat3("Point Offset", &l.pointOffset.x, 0.05f);
		ImGui::DragFloat("Point Intensity", &l.pointIntensity, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("Point Radius", &l.pointRadius, 0.1f, 0.1f, 200.0f);
		ImGui::DragFloat("Point Decay", &l.pointDecay, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Point Flicker", &l.pointFlicker, 0.01f, 0.0f, 1.0f);
		ImGui::ColorEdit3("Spot Color", &l.spotColor.x);
		ImGui::DragFloat3("Spot Offset", &l.spotOffset.x, 0.05f);
		ImGui::DragFloat3("Spot Direction", &l.spotDirection.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Spot Intensity", &l.spotIntensity, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("Spot Distance", &l.spotDistance, 0.1f, 0.1f, 200.0f);
		ImGui::DragFloat("Spot Decay", &l.spotDecay, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Spot Angle", &l.spotAngleDeg, 0.1f, 1.0f, 89.0f);
		ImGui::DragFloat("Spot Falloff Start", &l.spotFalloffStartDeg, 0.1f, 0.0f, 89.0f);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Grade")) {
		ImGui::ColorEdit3("Tint", &dbg.grade.tint.x);
		ImGui::DragFloat("Tint Intensity", &dbg.grade.tintIntensity, 0.005f, 0.0f, 1.0f);
		ImGui::DragFloat("Vignette", &dbg.grade.vignette, 0.01f, 0.0f, 3.0f);
		ImGui::ColorEdit4("Skybox Color", &dbg.skyboxColor.x);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Guide Light (Emissive)")) {
		ImGui::ColorEdit3("Guide Color", &guideEmissiveColor_.x);
		ImGui::DragFloat("Guide Intensity", &guideEmissiveIntensity_, 0.1f, 0.0f, 20.0f);
		ImGui::TreePop();
	}
	ImGui::End();

	if (showAfterburnerEditor_) {
		DrawAfterburnerEditor();
	}
#endif
}

void TitleScene::Draw() {
	if (state_ == TitleState::ConsoleBoot) {
		// コンソールブート時のみ3D描画をスキップ（黒画面からスタート）
		return;
	}

	LocationData& loc = CurrentLocation();
	Camera* titleCamera = CameraManager::GetInstance()->GetActiveCamera();

	// 墜落現場専用：「何も映さずノイズだけがいいね」
	// ドローン視点時のみ3Dは一切映さず、フラットな背景のみ描画してポストエフェクトの全面砂嵐ノイズに委ねる！
	if (loc.id == TitleLocation::CrashedForest && state_ == TitleState::DroneView) {
		if (skybox_) {
			skybox_->SetColor({ 0.18f, 0.18f, 0.20f, 1.0f });
			SkyboxCommon::GetInstance()->SetupCommonState();
			skybox_->Draw();
		}
		return;
	}

	if (skybox_) {
		if (loc.id == TitleLocation::Tunnel) {
			// 地下トンネル内は完全な暗黒空間（開口部の外側も漆黒にして天球を完全遮蔽）
			skybox_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		} else {
			skybox_->SetColor(loc.skyboxColor);
		}
		SkyboxCommon::GetInstance()->SetupCommonState();
		skybox_->Draw();
	}

	Object3dCommon::GetInstance()->SetupCommonState();

	// 背景セット（モデル未作成ならスキップ）
	if (loc.envObject) {
		loc.envObject->Draw();
	}
	for (auto& extra : loc.extraEnvParts) {
		if (extra.object) {
			extra.object->Draw();
		}
	}

	// 接地影（項目7: ソフトシャドウの濃さ・エッジ調整 - 黒潰れを解消し自然な接地感を表現）
	if (loc.id == TitleLocation::Hangar && titleCamera) {
		PrimitiveModel* pm = PrimitiveModel::GetInstance();
		MyMath::Vector3 origin = loc.origin;
		uint32_t shadowTex = (softShadowTexIndex_ != 0) ? softShadowTexIndex_ : whiteTexIndex_;
		// 主脚（左右車輪下）
		pm->DrawPlane({ 2.8f, 1.0f, 2.8f }, { 0.0f, 0.0f, 0.0f }, { origin.x - 1.4f, origin.y + 0.02f, origin.z + 0.5f }, { 0.0f, 0.0f, 0.0f, 0.32f }, shadowTex, titleCamera);
		pm->DrawPlane({ 2.8f, 1.0f, 2.8f }, { 0.0f, 0.0f, 0.0f }, { origin.x + 1.4f, origin.y + 0.02f, origin.z + 0.5f }, { 0.0f, 0.0f, 0.0f, 0.32f }, shadowTex, titleCamera);
		// 前脚（機首車輪下）
		pm->DrawPlane({ 2.0f, 1.0f, 2.0f }, { 0.0f, 0.0f, 0.0f }, { origin.x, origin.y + 0.02f, origin.z - 2.8f }, { 0.0f, 0.0f, 0.0f, 0.28f }, shadowTex, titleCamera);
		// 胴体中央のアンビエントオクルージョン影（淡いソフトフェード）
		pm->DrawPlane({ 5.5f, 1.0f, 9.5f }, { 0.0f, 0.5f, 0.0f }, { origin.x, origin.y + 0.015f, origin.z }, { 0.0f, 0.0f, 0.0f, 0.20f }, shadowTex, titleCamera);
	}

	// 墜落現場の焦げ跡・激突滑走溝デカール（項目6: 胴体着陸の長大な滑走溝と左翼激突痕）
	if (loc.id == TitleLocation::CrashedForest && titleCamera) {
		PrimitiveModel* pm = PrimitiveModel::GetInstance();
		MyMath::Vector3 origin = loc.origin;
		uint32_t scorchTex = (crashScorchTexIndex_ != 0) ? crashScorchTexIndex_ : whiteTexIndex_;
		// 激突・胴体滑走の長大な溝デカール (Scorch Trench: 手前から激突点まで伸びる黒焦げブレーキ痕)
		pm->DrawPlane({ 4.5f, 1.0f, 22.0f }, { 0.0f, -0.85f, 0.0f }, { origin.x + 4.0f, origin.y + 0.010f, origin.z - 7.0f }, { 0.01f, 0.01f, 0.01f, 0.92f }, scorchTex, titleCamera);
		// 胴体激突地点の大きな焦げ跡クレーターデカール
		pm->DrawPlane({ 10.5f, 1.0f, 16.0f }, { 0.0f, -0.8f, 0.0f }, { origin.x, origin.y + 0.012f, origin.z }, { 0.02f, 0.02f, 0.02f, 0.92f }, scorchTex, titleCamera);
		// 千切れた左翼の激突痕デカール
		pm->DrawPlane({ 6.5f, 1.0f, 9.5f }, { 0.0f, 0.6f, 0.0f }, { origin.x - 6.5f, origin.y + 0.014f, origin.z + 4.2f }, { 0.02f, 0.02f, 0.02f, 0.88f }, scorchTex, titleCamera);
	}

	// 機体・残骸は現在のロケーションのみ描画
	loc.visualModel->Draw();
	if (loc.debrisModel) {
		loc.debrisModel->Draw();
	}

	// トンネル飛行中の演出（項目9: スピード感向上 - 高密度な流動ライト＆推進炎）
	if (loc.id == TitleLocation::Tunnel && titleCamera) {
		PrimitiveModel* pm = PrimitiveModel::GetInstance();

		// 高速で壁面・天井・路面を前方に流れるリニア誘導指標ライト（過剰発光・白飛びを抑制：項目1）
		constexpr float kFlightSpeed = 130.0f;
		constexpr float kMarkerSpacing = 2.4f;
		float scrollZ = -std::fmod(animTimer_ * kFlightSpeed, kMarkerSpacing);
		for (float z = -40.0f; z <= 40.0f; z += kMarkerSpacing) {
			float markerZ = z + scrollZ;
			// 左右の壁面（X = ±6.25m, Y = 1.8m）: 適正なシアンガイドライン（項目1）
			pm->DrawPlane({ 0.14f, 1.0f, 2.8f }, { 0.0f, 0.0f, 1.5707963f }, { -6.25f, 1.8f, markerZ }, { 0.15f, 0.65f, 1.4f, 0.85f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
			pm->DrawPlane({ 0.14f, 1.0f, 2.8f }, { 0.0f, 0.0f, 1.5707963f }, {  6.25f, 1.8f, markerZ }, { 0.15f, 0.65f, 1.4f, 0.85f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);

			// 天井誘導ライン（X = 0.0m, Y = 4.8m）: 適正な黄色ライン（白飛び防止：項目1）
			pm->DrawPlane({ 0.50f, 1.0f, 2.4f }, { 3.14159265f, 0.0f, 0.0f }, { 0.0f, 4.8f, markerZ }, { 1.25f, 0.95f, 0.25f, 0.85f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
			pm->DrawPlane({ 1.10f, 1.0f, 2.6f }, { 3.14159265f, 0.0f, 0.0f }, { 0.0f, 4.805f, markerZ }, { 0.45f, 0.35f, 0.08f, 0.35f }, softShadowTexIndex_ != 0 ? softShadowTexIndex_ : whiteTexIndex_, titleCamera, BlendMode::kAdd);

			// 路面高速流動マーカー（X = ±2.5m, Y = 0.03m）: 適正な黄色マーカー
			pm->DrawPlane({ 0.12f, 1.0f, 1.8f }, { 0.0f, 0.0f, 0.0f }, { -2.5f, 0.03f, markerZ }, { 1.10f, 0.85f, 0.20f, 0.80f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
			pm->DrawPlane({ 0.12f, 1.0f, 1.8f }, { 0.0f, 0.0f, 0.0f }, {  2.5f, 0.03f, markerZ }, { 1.10f, 0.85f, 0.20f, 0.80f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
		}

		// スラスター推進器噴射炎（Afterburner クラスによる単発エンジン・ショックダイヤモンド高品位描画）
		if (afterburner_) {
			afterburner_->Draw(currentAircraftMatrix_, titleCamera);

			// ノズル開口部内部のプラズマ発光ディスク＆照り返しグロー（過剰発光を抑え、機体形状をくっきり見せる：項目1）
			MyMath::Vector3 localNozzleCore = { 0.0f, 0.011f, 14.85f };
			MyMath::Vector3 worldNozzleCore = MyMath::TransformV3(localNozzleCore, currentAircraftMatrix_);
			MyMath::Vector3 nozzleRot = loc.aircraftRotation;
			// ノズル中心のプラズマディスク（開口部サイズジャストに調整）
			pm->DrawPlane({ 0.35f, 1.0f, 0.35f }, { 1.5707963f, nozzleRot.y, 0.0f }, worldNozzleCore, { 1.2f, 1.6f, 2.2f, 0.85f }, softShadowTexIndex_ != 0 ? softShadowTexIndex_ : whiteTexIndex_, titleCamera, BlendMode::kAdd);
			// ノズル外周の控えめなグローハロ
			pm->DrawPlane({ 0.70f, 1.0f, 0.70f }, { 1.5707963f, nozzleRot.y, 0.0f }, worldNozzleCore, { 0.15f, 0.45f, 1.0f, 0.35f }, softShadowTexIndex_ != 0 ? softShadowTexIndex_ : whiteTexIndex_, titleCamera, BlendMode::kAdd);
		}
	}
}

void TitleScene::DrawUI() {
	pseudoOS_.ResetSpritePool();

	if (state_ == TitleState::DroneView) {
		DrawOSD();
		// 操作案内テキストの半透明ダーク帯を描画（項目9: 主翼や車輪との被りを100%防止）
		if (pressSpaceBannerBg_) {
			pressSpaceBannerBg_->Draw();
		}
		for (auto& sp : pressSpaceBorders_) {
			sp->Draw();
		}
		pressSpaceText_.Draw();
		DrawTransitionGlitch();
	} else if (state_ == TitleState::WindowClose) {
		DrawWindowClose();
	} else if (state_ == TitleState::ConsoleBoot) {
		DrawConsoleBoot();
	} else if (state_ == TitleState::ReAuthentication) {
		DrawReAuthentication();
	} else if (state_ == TitleState::Menu) {
		// 背景オーバーレイ
		backgroundPanel_.Draw();

		// メニュー移行トランジション（0.35秒間の滑らかな起動フェードイン＆走査線展開：項目3）
		if (menuEnterTimer_ > 0.0f) {
			DrawTransitionGlitch();
		}

		// フルOSデスクトップ環境（マルチウィンドウ、タスクバー、デスクトップアイコン、カスタムカーソル）
		DrawOSDesktop();
	} else if (state_ == TitleState::SortieSequence) {
		DrawSortieSequence();
	}
}

void TitleScene::Finalize() {
	SetCursorVisible(false); // ゲーム画面へ遷移するためOSカーソルを確実に非表示（項目9）
	UITextRegistry::GetInstance()->Clear();
	isStartMenuOpen_ = false;
	isMissionSelectModalOpen_ = false;
	missionSelectModalAnim_ = 0.0f;

	// ------------------------------------------------------------
	// 静的リソースキャッシュへの退避（リファクタリング最適化）
	// 次回タイトル訪問時のD3D12リソース生成（2,000バッファ以上）と
	// 3D背景109個のObject3d再初期化をスキップして0ms高速遷移を実現
	// ------------------------------------------------------------
	s_cache_.skybox = std::move(skybox_);
	s_cache_.locations = std::move(locations_);
	s_cache_.afterburner = std::move(afterburner_);
	s_cache_.consoleBgSprite = std::move(consoleBgSprite_);
	s_cache_.sortieFlashSprite = std::move(sortieFlashSprite_);
	s_cache_.recDotSprite = std::move(recDotSprite_);
	s_cache_.titleLogoSprite = std::move(titleLogoSprite_);
	s_cache_.headerLampSprite = std::move(headerLampSprite_);
	s_cache_.pressSpaceBannerBg = std::move(pressSpaceBannerBg_);
	s_cache_.osSpritePool = std::move(osSpritePool_);
	s_cache_.cursorSprites = std::move(cursorSprites_);
	s_cache_.windowCloseMaskSprites = std::move(windowCloseMaskSprites_);
	s_cache_.windowCloseBorderSprites = std::move(windowCloseBorderSprites_);
	s_cache_.menuBorderSprites = std::move(menuBorderSprites_);
	s_cache_.buttonBorders = std::move(buttonBorders_);
	s_cache_.glitchSprites = std::move(glitchSprites_);
	s_cache_.osdSprites = std::move(osdSprites_);
	s_cache_.pressSpaceBorders = std::move(pressSpaceBorders_);
	s_cache_.menuBorderOffsets = menuBorderOffsets_;
	s_cache_.buttonBorderOutlineOffsets = buttonBorderOutlineOffsets_;
	s_cache_.buttonBorderBracketOffsets = buttonBorderBracketOffsets_;
	s_cache_.buttonBorderIndicatorOffsets = buttonBorderIndicatorOffsets_;
	s_cache_.isCached = true;
}
