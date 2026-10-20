#include "StageSceneInternal.h"

using namespace MyMath;
using namespace StageSceneInternal;

void StageScene::Initialize() {
#ifdef _DEBUG
	auto tStart = std::chrono::high_resolution_clock::now();
	double assetsMs = 0.0;
	bool ranAssets = false;
#endif
	sceneID = SCENE::STAGE;

	if (!isAssetsInitialized_) {
#ifdef _DEBUG
		auto aStart = std::chrono::high_resolution_clock::now();
#endif
		InitializeAssets();
		isAssetsInitialized_ = true;
#ifdef _DEBUG
		auto aEnd = std::chrono::high_resolution_clock::now();
		assetsMs = std::chrono::duration<double, std::milli>(aEnd - aStart).count();
		ranAssets = true;
#endif
	}

#ifdef _DEBUG
	auto rStart = std::chrono::high_resolution_clock::now();
#endif
	ResetGameplayState();
#ifdef _DEBUG
	auto rEnd = std::chrono::high_resolution_clock::now();
	double resetMs = std::chrono::duration<double, std::milli>(rEnd - rStart).count();

	auto tEnd = std::chrono::high_resolution_clock::now();
	double totalMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

	char buf[256];
	snprintf(buf, sizeof(buf), 
		"  [StageScene::Initialize] Total: %.2f ms (Assets: %.2f ms [%s], ResetGameplayState: %.2f ms)\n",
		totalMs, assetsMs, ranAssets ? "FRESH LOAD" : "PREWARMED", resetMs);
	OutputDebugStringA(buf);

	FILE* fp = nullptr;
	if (fopen_s(&fp, "scene_transition.log", "a") == 0 && fp) {
		fputs(buf, fp);
		fclose(fp);
	}
#endif
}

void StageScene::PrewarmAssets() {
	if (!isAssetsInitialized_) {
#ifdef _DEBUG
		auto aStart = std::chrono::high_resolution_clock::now();
#endif
		InitializeAssets();
		isAssetsInitialized_ = true;
#ifdef _DEBUG
		auto aEnd = std::chrono::high_resolution_clock::now();
		double assetsMs = std::chrono::duration<double, std::milli>(aEnd - aStart).count();

		char buf[256];
		snprintf(buf, sizeof(buf), "  [StageScene::PrewarmAssets] GPU & Assets prewarmed in %.2f ms\n", assetsMs);
		OutputDebugStringA(buf);

		FILE* fp = nullptr;
		if (fopen_s(&fp, "scene_transition.log", "a") == 0 && fp) {
			fputs(buf, fp);
			fclose(fp);
		}
#endif
	}
}

void StageScene::InitializeAssets() {
	// ============================
	// 機体設定（CFG）のロード
	// ============================
	std::string aircraftCfgPath = aircraftConfig_.GetCurrentFilePath();
	if (aircraftCfgPath.empty()) {
		aircraftCfgPath = "Resources/aircraft/mig21.cfg";
	}
	if (!AsyncPreloadManager::GetInstance()->ApplyPreloadedAircraftConfig(aircraftConfig_)) {
		aircraftConfig_.LoadFromCfg(aircraftCfgPath);
	}
	const auto& generalConfig = aircraftConfig_.GetGeneral();

	// ============================
	// 描画オブジェクト・モデル・テクスチャの初期化（初回のみ）
	// ============================
	TextureManager::GetInstance()->LoadTexture("assets/textures/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("assets/textures/circle2.png");
	TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("assets/textures/circle.png");
	groundTextureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");

	// 機体モデル
	ModelManager::GetInstance()->LoadModel(generalConfig.modelPath);
	aircraftObject_ = std::make_unique<Object3d>();
	aircraftObject_->Initialize(Object3dCommon::GetInstance());
	aircraftObject_->SetCamera(CameraManager::GetInstance()->GetActiveCamera());
	aircraftObject_->SetModel(generalConfig.modelPath);
	if (aircraftObject_->GetModel()) {
		aircraftObject_->GetModel()->SetEnvironmentCoefficient(generalConfig.environmentCoefficient);
		aircraftObject_->GetModel()->SetSpecularIntensity(generalConfig.specularIntensity);
		aircraftObject_->GetModel()->SetShininess(generalConfig.shininess);
	}

	// 部位破壊対応マルチパーツビジュアルモデル初期化
	aircraftVisualModel_.Initialize(Object3dCommon::GetInstance(), CameraManager::GetInstance()->GetActiveCamera());
	DebrisManager::GetInstance()->Initialize(Object3dCommon::GetInstance(), CameraManager::GetInstance()->GetActiveCamera());

	// 1つのモデルファイルからノード名に基づいて自動セットアップ
	aircraftVisualModel_.SetupFromSingleModel(generalConfig.modelPath);
	aircraftVisualModel_.SetupCanopyMaterial();
	aircraftVisualModel_.SetMaterialProperties(48.0f, 1.35f, 0.45f);

	if (!aircraftVisualModel_.IsPartVisible(DamagePart::Fuse)) {
		std::vector<DamagePart> parts = {
			DamagePart::Fuse, DamagePart::Engine1, DamagePart::Wing_L, DamagePart::Wing_R,
			DamagePart::Wing1_L, DamagePart::Wing1_R, DamagePart::Wing2_L, DamagePart::Wing2_R,
			DamagePart::Tail, DamagePart::Rudder, DamagePart::Elevator0, DamagePart::Elevator1
		};
		for (auto p : parts) {
			aircraftVisualModel_.SetModelForPart(p, "Resources/models/m21_wg.gltf");
		}
	}

	// スカイボックス
	TextureManager::GetInstance()->LoadTexture("assets/textures/qwantani_dusk_2_puresky_2k.dds");
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(SkyboxCommon::GetInstance());
	skybox_->SetCamera(CameraManager::GetInstance()->GetActiveCamera());
	uint32_t skyboxTexIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/qwantani_dusk_2_puresky_2k.dds");
	skybox_->SetTextureIndex(skyboxTexIndex);
	skybox_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	Object3dCommon::GetInstance()->SetDefaultEnvTextureIndex(skyboxTexIndex);

	// ライティング設定
	environmentManager_.Initialize();
	Object3dCommon::GetInstance()->SetLightType(1);
	Object3dCommon::GetInstance()->SetShadingModel(1);
	Object3dCommon::GetInstance()->SetSpecularModel(2);

	// 戦闘HUD・出撃イントロ・演出スプライト初期化
	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();
	combatBootSequence_.Initialize(spriteCommon);
	hud_.Initialize(spriteCommon);

	seqOverlaySprite_ = std::make_unique<Sprite>();
	seqOverlaySprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
	seqNoiseSprite_ = std::make_unique<Sprite>();
	seqNoiseSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");

	// ポーズメニュー用スプライトプールの初期化（512枚：DAWN_OS デスクトップ完全同期）
	pauseSpritePool_.clear();
	for (int i = 0; i < 512; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		pauseSpritePool_.push_back(std::move(sp));
	}
	pauseSpritePoolIndex_ = 0;

	// パーティクルグループの登録（初回のみ）
	ParticleManager::GetInstance()->CreateParticleGroup("HitSpark", "assets/textures/white1x1.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Vortex", "assets/textures/circle2.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Casing", "assets/textures/white1x1.png");
	ParticleManager::GetInstance()->CreateParticleGroup("ExplosionSpark", "assets/textures/circle.png");
	ParticleManager::GetInstance()->SetBlendMode("ExplosionSpark", BlendMode::kAdd);
	ParticleManager::GetInstance()->CreateParticleGroup("ExplosionFire", "assets/textures/circle2.png");
	ParticleManager::GetInstance()->SetBlendMode("ExplosionFire", BlendMode::kAdd);
	ParticleManager::GetInstance()->CreateParticleGroup("ExplosionSmoke", "assets/textures/circle2.png");
	ParticleManager::GetInstance()->SetBlendMode("ExplosionSmoke", BlendMode::kNormal);

	GPUParticleManager::GetInstance()->SetTexture(TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle.png"));

	// 初回のみテクスチャアップロードを確定
	DirectXCommon::GetInstance()->FlushTextureUploads();
}

void StageScene::ResetGameplayState() {
	// マウスポインターを非表示（自ウィンドウがアクティブな場合のみ）
	HWND hwnd = Input::GetInstance() ? Input::GetInstance()->GetHwnd() : nullptr;
	if (hwnd && ::GetForegroundWindow() == hwnd) {
		while (::ShowCursor(FALSE) >= 0) {}
		::SetCursor(nullptr);
	}

	// 機体設定（CFG）の最新反映
	std::string aircraftCfgPath = aircraftConfig_.GetCurrentFilePath();
	if (aircraftCfgPath.empty()) {
		aircraftCfgPath = "Resources/aircraft/mig21.cfg";
	}
	if (!AsyncPreloadManager::GetInstance()->ApplyPreloadedAircraftConfig(aircraftConfig_)) {
		aircraftConfig_.LoadFromCfg(aircraftCfgPath);
	}

	const auto& airframeData = aircraftConfig_.GetAirframe();
	const auto& engineData = aircraftConfig_.GetEngine();
	const auto& gunpodData = aircraftConfig_.GetGunpod();

	flightModel_.Initialize(airframeData, engineData);
	flightModel_.SetPosition({ 0.0f, 100.0f, 0.0f });

	float initialSpeed = 250.0f / 3.6f;
	flightModel_.SetVelocity(Multiply(initialSpeed, flightModel_.GetForwardDirection()));
	throttle_ = 0.6f;
	flightModel_.SetThrottle(throttle_);

	// 自機パーツをすべて可視状態に復元
	aircraftVisualModel_.ResetAllPartsVisible();

#ifdef _DEBUG
	TerrainSyncManager::GetInstance()->Initialize("localhost", 8765);
#endif

	// カメラ初期位置
	Vector3 initPos = flightModel_.GetPosition();
	Vector3 initForward = flightModel_.GetForwardDirection();
	playerCamera_.Initialize(initPos, initForward);
	CameraManager::GetInstance()->Update();

	// マウスエイムコントローラ
	mouseAimController_.Initialize();
	mouseAimEnabled_ = true;
	Input::GetInstance()->LockCursor();

	// 武装・弾丸
	gunpod_.Initialize(gunpodData);
	bulletManager_.Initialize(512);

	// ミッションデータのロード
	auto& missionManager = MissionManager::GetInstance();
	std::string loadPath = missionManager.GetCurrentFilePath();
	if (loadPath.empty()) {
		loadPath = "Resources/missions/mission01.json";
	}
	if (!AsyncPreloadManager::GetInstance()->ApplyPreloadedMission(missionManager)) {
		missionManager.Load(loadPath);
	}

	const auto& mission = missionManager.GetCurrentMission();
	playerMaxHP_ = mission.playerHP;
	playerHP_ = mission.playerHP;
	flightModel_.SetPosition(mission.playerPosition);

	static const std::vector<EnemySpawnData> kEmptyAirEnemies = {};
	static const std::vector<GroundEnemySpawnData> kEmptyGroundEnemies = {};

	enemyManager_.Initialize(
		enableEnemies_ ? mission.enemies : kEmptyAirEnemies,
		airframeData,
		engineData,
		gunpodData,
		&flightModel_,
		&bulletManager_,
		enableEnemies_ ? mission.groundEnemies : kEmptyGroundEnemies
	);

	// エフェクトマネージャー初期化＆残存破片のクリア
	EffectManager::GetInstance()->Initialize();
	DebrisManager::GetInstance()->Clear();

	// ゲーム状態リセット
	isMissionCleared_ = false;
	isMissionFailed_ = false;
	isGameOver_ = false;
	gameOverTimer_ = 0.0f;
	timeScale_ = 1.0f;
	muzzleFlashTimer_ = 0.0f;
	totalTime_ = 0.0f;
	missionElapsedTime_ = 0.0f;
	isPaused_ = false;
	isPauseAborting_ = false;
	pauseAbortTimer_ = 0.0f;
	pauseMenuIndex_ = 0;
	pauseAnimTimer_ = 0.0f;
	pauseWinPos_ = { (static_cast<float>(WinApp::kClientWidth) - 500.0f) * 0.5f,
	                 (static_cast<float>(WinApp::kClientHeight) - 430.0f) * 0.5f };
	pauseIsDragging_ = false;

	playerBody_.scene_ = this;

	// 制限時間の初期化
	remainingTime_ = mission.timeLimit;
	int initMin = static_cast<int>(remainingTime_) / 60;
	int initSec = static_cast<int>(remainingTime_) % 60;
	char initTimeStr[32];
	snprintf(initTimeStr, sizeof(initTimeStr), "TIME %02d:%02d", initMin, initSec);
	timeText_.Initialize("HackGen", initTimeStr, 32.0f);
	timeText_.SetAnchorPoint({ 0.5f, 0.0f });
	timeText_.SetPosition({ WinApp::kClientWidth * 0.5f, 20.0f });
	timeText_.SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	timeText_.SetCondition("Warning", [this]() { return remainingTime_ <= 30.0f; });
	UITextRegistry::GetInstance()->Register("Stage_Time", &timeText_);

	// 出撃イントロ
	if (GameResult::GetInstance().isRetry) {
		combatBootSequence_.SetQuickMode(true);
		GameResult::GetInstance().isRetry = false;
	} else {
		combatBootSequence_.SetQuickMode(false);
	}
	combatBootSequence_.Start();
	sortieIntroDuration_ = combatBootSequence_.GetCurrentDuration();
	sortieIntroTimer_ = 0.0f;

	// HUDリセット
	hud_.SetWarningColorMode(false);
	hud_.ResetShutdown();
	hud_.ResetDeploy();
	isFiringThisFrame_ = false;

	endSequence_ = StageEndSequence::None;
	endSeqTimer_ = 0.0f;

	missionManager.ResetRuntimeState();
}

void StageScene::Update() {
	// ----------------------------------------------------
	// ポーズメニュー制御（ゲームプレイ中のみ作動）
	// ----------------------------------------------------
	Input* input = Input::GetInstance();
	if (isPaused_) {
		UpdatePauseMenu();
		return;
	}

	// ゲームプレイ中のみポーズ可能（出撃イントロ中・ゲームオーバー演出中・クリア演出中は作動させない）
	if (IsGameplayActive()) {
		if (input && (input->TriggerKey(DIK_ESCAPE) || input->TriggerKey(DIK_P))) {
			SetPaused(true);
			return;
		}
	}

	// ミッション制限時間の更新
	if (!isGameOver_ && !isMissionCleared_) {
		remainingTime_ -= kDeltaTime;
		if (remainingTime_ <= 0.0f) {
			remainingTime_ = 0.0f;
			TriggerGameOver();
		}

		int minutes = static_cast<int>(remainingTime_) / 60;
		int seconds = static_cast<int>(remainingTime_) % 60;
		char timeStr[32];
		snprintf(timeStr, sizeof(timeStr), "TIME %02d:%02d", minutes, seconds);
		timeText_.SetText(timeStr);
	}
	timeText_.Update();

	// デバッグ用キー（VキーでGameOver演出、BキーでGameClear演出を即時テスト可能）
	Input* inputDebug = Input::GetInstance();
	if (inputDebug) {
		if (inputDebug->TriggerKey(DIK_V)) {
			TriggerGameOver();
		}
		if (inputDebug->TriggerKey(DIK_B)) {
			TriggerGameClear();
		}
	}

	// ゲームオーバー / クリア演出シーケンスの更新
	UpdateEndSequence(kDeltaTime);

	float gameDeltaTime = kDeltaTime * timeScale_;
	totalTime_ += gameDeltaTime;

	// 各責務コンポーネント更新
	UpdateFlightControls(gameDeltaTime);
	UpdateCombatFiring(gameDeltaTime);
	UpdateFlightAndEffects(gameDeltaTime);
	UpdateCombatAndMission(gameDeltaTime);
	UpdateCamera(gameDeltaTime);
	UpdateDebugAndStatusUI(gameDeltaTime);
	UpdatePostEffects(gameDeltaTime);
}

void StageScene::Draw() {
	if (skybox_) {
		SkyboxCommon::GetInstance()->SetupCommonState();
		skybox_->Draw();
	}

	// 太陽の見た目を無限遠に描画
	environmentManager_.Draw(CameraManager::GetInstance()->GetActiveCamera());

	// 3Dオブジェクト描画（Object3dパイプライン）
	Object3dCommon::GetInstance()->SetupCommonState();

	// 部位破壊対応ビジュアルモデルの描画
	// 【戦闘モード起動演出（自機専用スキャン波形マテリアライズ・パイプライン）】
	if (!combatBootSequence_.IsFinished() && !combatBootSequence_.IsQuickMode()) {
		float dur = combatBootSequence_.GetCurrentDuration();
		float bootT = std::clamp(combatBootSequence_.GetTimer() / (std::max)(dur, 0.001f), 0.0f, 1.0f);

		if (bootT < 0.12f) {
			// 1. 【無（完全不可視・ステルス）フェーズ】
			// 起動開始前（作戦音声第一声）は完全不可視
		} else if (bootT < 0.85f) {
			// 2. 【スキャン波形マテリアライズフェーズ（機体中心から広がる波紋に沿ってピクセル単位で構築）】
			// 機体中心からスキャン波形が外側へ伝播：[無(不可視) -> ネオンワイヤー＆波形パルス -> 3D実写装甲]
			float scanProg = std::clamp((bootT - 0.12f) / 0.73f, 0.0f, 1.0f);

			combatBootSequence_.BeginAircraftScanDraw(aircraftVisualModel_, scanProg);
			aircraftVisualModel_.Draw();
			combatBootSequence_.EndAircraftScanDraw(aircraftVisualModel_);
		} else {
			// 3. 【起動完了・完全実体装甲フェーズ】
			aircraftVisualModel_.Draw();
		}
	} else {
		// 通常戦闘時およびクイックリトライ時
		aircraftVisualModel_.Draw();
	}
	DebrisManager::GetInstance()->Draw();

	// 敵描画（空中敵＋地上敵）
	Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
	enemyManager_.Draw(cam);

	// 地面描画（PrimitiveModelパイプライン）
	DrawGround();

	// 弾丸描画
	bulletManager_.Draw(cam);

	// エフェクト描画
	EffectManager::GetInstance()->Draw(cam);

	if (!isPaused_) {
		GPUParticleManager::GetInstance()->Update();
	}
	GPUParticleManager::GetInstance()->Draw(cam->GetViewProjectionMatrix(), cam->GetWorldMatrix());

	// ベイパーコーンエフェクト描画
	DrawVaporConeEffect(cam);

	// 追従マズルフラッシュ描画
	DrawMuzzleFlashEffect(cam);
}

void StageScene::DrawUI() {
	// 1. 出撃イントロ用黒フェード背景描画（3Dシーンの上、最前面UIの下）
	combatBootSequence_.DrawBackgroundFade();

	// 2. 戦闘HUD（残弾数・照準・タイマー）描画
	float hudThreshold = combatBootSequence_.IsQuickMode() ? 0.0f : 4.00f;
	if (sortieIntroTimer_ >= hudThreshold) {
		hud_.Draw();
	}

	// 3. AC6スタイル「メインシステム、戦闘モード起動...」ブートUI描画（最前面）
	if (sortieIntroTimer_ < sortieIntroDuration_) {
		combatBootSequence_.DrawBootUI();
	}

	// 4. ゲームオーバー / ステージクリア演出UI描画（最前面）
	DrawEndSequenceUI();

	// 5. ポーズメニューUI描画（最前面）
	if (isPaused_) {
		DrawPauseMenuUI();
	}
}

void StageScene::Finalize() {
#ifdef _DEBUG
	TerrainSyncManager::GetInstance()->Finalize();
#endif
	
	// シーン終了時にカーソルを復帰
	Input::GetInstance()->UnlockCursor();
	PostEffect::GetInstance()->ClearActiveEffects();
	isPaused_ = false;
	isPauseAborting_ = false;
	pauseAbortTimer_ = 0.0f;
}

void StageScene::Restart() {
	ResetGameplayState();
	// リスタート直後の初フレームからカメラを正規追従位置に向ける
	playerCamera_.Update(0.001f, &flightModel_, &mouseAimController_, mouseAimEnabled_);
}

// ============================================================
// ポーズメニュー実装（プレイ中のみ作動）
// ============================================================

bool StageScene::IsGameplayActive() const {
	return (sortieIntroTimer_ >= sortieIntroDuration_ &&
	        !isGameOver_ &&
	        !isMissionFailed_ &&
	        !isMissionCleared_ &&
	        endSequence_ == StageEndSequence::None &&
	        sceneID == SCENE::STAGE);
}

void StageScene::SetPaused(bool paused) {
	isPaused_ = paused;
	HWND hwnd = Input::GetInstance() ? Input::GetInstance()->GetHwnd() : nullptr;
	if (isPaused_) {
		pauseMenuIndex_ = 0;
		pauseAnimTimer_ = 0.0f;
		const float screenW = static_cast<float>(WinApp::kClientWidth);
		const float screenH = static_cast<float>(WinApp::kClientHeight);
		pauseWinPos_ = { (screenW - 500.0f) * 0.5f, (screenH - 430.0f) * 0.5f };
		pauseIsDragging_ = false;
		Input::GetInstance()->UnlockCursor();
		// OSカーソルは非表示にし、自前のカスタムサイバーカーソル（TitleSceneと同一）を使用
		if (hwnd && ::GetForegroundWindow() == hwnd) {
			while (::ShowCursor(FALSE) >= 0) {}
			::SetCursor(nullptr);
		}
		customCursorPos_ = Input::GetInstance()->GetVirtualMousePosition(screenW, screenH);
		cursorRippleTimer_ = 0.0f;
	} else {
		Input::GetInstance()->LockCursor();
		if (hwnd && ::GetForegroundWindow() == hwnd) {
			while (::ShowCursor(FALSE) >= 0) {}
			::SetCursor(nullptr);
		}
	}
}

void StageScene::UpdatePauseMenu() {
	Input* input = Input::GetInstance();
	if (!input) return;

	// カーソルはカスタムスプライトで描画するためOSカーソルは非表示を維持
	HWND hwnd = input->GetHwnd();
	if (hwnd && ::GetForegroundWindow() == hwnd) {
		while (::ShowCursor(FALSE) >= 0) {}
		::SetCursor(nullptr);
	}

	// ポーズからタイトル復帰時のCRTシャットダウン演出（プツッ暗転）
	if (isPauseAborting_) {
		pauseAbortTimer_ += kDeltaTime;

		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();

		// ブラウン管曲面ガラス歪み
		ActivePostEffect dist;
		dist.type = PostEffectType::kLensDistortion;
		dist.intensity = 0.40f;
		postEffect->AddActiveEffect(dist);

		// 背景走査線
		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.intensity = 0.45f;
		scan.dirX = 720.0f;
		scan.dirY = 6.0f;
		postEffect->AddActiveEffect(scan);

		// アナログCRT特有の微細ランダム粒子ノイズ（画像のような質感）
		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.12f;
		postEffect->AddActiveEffect(noise);

		if (pauseAbortTimer_ >= kPauseAbortDuration) {
			postEffect->ClearActiveEffects();
			isPaused_ = false;
			isPauseAborting_ = false;
			pauseAbortTimer_ = 0.0f;

			Input::GetInstance()->UnlockCursor();
			while (::ShowCursor(TRUE) < 0) {}
			::SetCursor(::LoadCursor(nullptr, IDC_ARROW));

			GameResult::GetInstance().titleEntryReason = TitleEntryReason::InitialBoot;
			sceneID = SCENE::TITLE;
		}
		return;
	}

	pauseAnimTimer_ += kDeltaTime;

	// マウス位置取得
	customCursorPos_ = input->GetVirtualMousePosition(
		static_cast<float>(WinApp::kClientWidth),
		static_cast<float>(WinApp::kClientHeight)
	);

	bool isLTrig = input->TriggerMouse(0);
	bool isLDown = input->PushMouse(0);

	if (cursorRippleTimer_ > 0.0f) {
		cursorRippleTimer_ -= kDeltaTime;
	}
	if (isLTrig) {
		cursorRippleTimer_ = 0.30f;
		cursorRipplePos_ = customCursorPos_;
	}

	// ESCキー または Pキーで再開
	if (input->TriggerKey(DIK_ESCAPE) || input->TriggerKey(DIK_P)) {
		SetPaused(false);
		return;
	}

	// キーボードナビゲーション
	if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W)) {
		pauseMenuIndex_ = (pauseMenuIndex_ + 3) % 4;
	}
	if (input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S)) {
		pauseMenuIndex_ = (pauseMenuIndex_ + 1) % 4;
	}

	// 数字キー直接選択
	if (input->TriggerKey(DIK_1)) { ExecutePauseAction(0); return; }
	if (input->TriggerKey(DIK_2)) { ExecutePauseAction(1); return; }
	if (input->TriggerKey(DIK_3)) { ExecutePauseAction(2); return; }
	if (input->TriggerKey(DIK_4)) { ExecutePauseAction(3); return; }

	// 決定キー（ENTER / SPACE / NUMPADENTER）
	if (input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_NUMPADENTER)) {
		ExecutePauseAction(pauseMenuIndex_);
		return;
	}

	const float winW = 500.0f;
	const float winH = 430.0f;

	// ウィンドウドラッグ処理
	if (isLDown) {
		if (!pauseIsDragging_ && isLTrig) {
			if (customCursorPos_.x >= pauseWinPos_.x && customCursorPos_.x <= pauseWinPos_.x + winW &&
			    customCursorPos_.y >= pauseWinPos_.y && customCursorPos_.y <= pauseWinPos_.y + 32.0f) {
				pauseIsDragging_ = true;
				pauseDragOffset_ = { customCursorPos_.x - pauseWinPos_.x, customCursorPos_.y - pauseWinPos_.y };
			}
		}
		if (pauseIsDragging_) {
			pauseWinPos_ = { customCursorPos_.x - pauseDragOffset_.x, customCursorPos_.y - pauseDragOffset_.y };
		}
	} else {
		pauseIsDragging_ = false;
	}

	// メニューボタンのホバー＆クリック判定
	const float buttonWidth = 380.0f;
	const float buttonHeight = 44.0f;
	const float buttonX = pauseWinPos_.x + (winW - buttonWidth) * 0.5f;
	const float buttonStartY = pauseWinPos_.y + 96.0f;
	const float buttonSpacing = 54.0f;

	for (int i = 0; i < 4; ++i) {
		float btnY = buttonStartY + static_cast<float>(i) * buttonSpacing;
		if (customCursorPos_.x >= buttonX && customCursorPos_.x <= buttonX + buttonWidth &&
		    customCursorPos_.y >= btnY && customCursorPos_.y <= btnY + buttonHeight) {
			pauseMenuIndex_ = i;
			if (isLTrig) {
				ExecutePauseAction(i);
				return;
			}
		}
	}
}

void StageScene::ExecutePauseAction(int actionIndex) {
	switch (actionIndex) {
	case 0: // RESUME
		SetPaused(false);
		break;
	case 1: // RESTART
		SetPaused(false);
		GameResult::GetInstance().isRetry = true;
		Restart();
		break;
	case 2: // TITLE（ブラウン管テレビのプツッ暗転演出を開始）
		isPauseAborting_ = true;
		pauseAbortTimer_ = 0.0f;
		pauseIsDragging_ = false;
		break;
	case 3: // QUIT
		::PostQuitMessage(0);
		break;
	default:
		break;
	}
}

Sprite* StageScene::GetPauseSprite() {
	if (pauseSpritePoolIndex_ >= pauseSpritePool_.size()) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(SpriteCommon::GetInstance(), "assets/textures/white1x1.png");
		pauseSpritePool_.push_back(std::move(sp));
	}
	return pauseSpritePool_[pauseSpritePoolIndex_++].get();
}

void StageScene::DrawPauseSolidRect(const Vector2& pos, const Vector2& size, const Vector4& color, BlendMode blend) {
	Sprite* sp = GetPauseSprite();
	if (!sp) return;
	sp->SetBlendMode(blend);
	sp->SetPosition(pos);
	sp->SetAnchorPoint({ 0.0f, 0.0f });
	sp->SetSize(size);
	sp->SetRotation(0.0f);
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

Sprite* StageScene::GetPauseCircleSprite() {
	if (pauseCircleSpritePoolIndex_ >= pauseCircleSpritePool_.size()) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(SpriteCommon::GetInstance(), "assets/textures/circle2.png");
		pauseCircleSpritePool_.push_back(std::move(sp));
	}
	return pauseCircleSpritePool_[pauseCircleSpritePoolIndex_++].get();
}

void StageScene::DrawPauseGlowOval(const Vector2& center, const Vector2& size, const Vector4& color, BlendMode blend) {
	Sprite* sp = GetPauseCircleSprite();
	if (!sp) return;
	sp->SetBlendMode(blend);
	sp->SetPosition(center);
	sp->SetAnchorPoint({ 0.5f, 0.5f });
	sp->SetSize(size);
	sp->SetRotation(0.0f);
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

void StageScene::DrawPauseBorder(const Vector2& pos, const Vector2& size, const Vector4& color, float thickness) {
	DrawPauseSolidRect(pos, { size.x, thickness }, color);
	DrawPauseSolidRect({ pos.x, pos.y + size.y - thickness }, { size.x, thickness }, color);
	DrawPauseSolidRect(pos, { thickness, size.y }, color);
	DrawPauseSolidRect({ pos.x + size.x - thickness, pos.y }, { thickness, size.y }, color);
}

void StageScene::DrawPausePanel(const Vector2& pos, const Vector2& size, const Vector4& bgCol, const Vector4& borderCol, float thickness) {
	DrawPauseSolidRect(pos, size, bgCol);
	DrawPauseBorder(pos, size, borderCol, thickness);
}

void StageScene::DrawPauseCornerBrackets(const Vector2& pos, const Vector2& size, float bracketLen, float thickness, const Vector4& color) {
	// Top-left
	DrawPauseSolidRect(pos, { bracketLen, thickness }, color);
	DrawPauseSolidRect(pos, { thickness, bracketLen }, color);
	// Top-right
	DrawPauseSolidRect({ pos.x + size.x - bracketLen, pos.y }, { bracketLen, thickness }, color);
	DrawPauseSolidRect({ pos.x + size.x - thickness, pos.y }, { thickness, bracketLen }, color);
	// Bottom-left
	DrawPauseSolidRect({ pos.x, pos.y + size.y - thickness }, { bracketLen, thickness }, color);
	DrawPauseSolidRect({ pos.x, pos.y + size.y - bracketLen }, { thickness, bracketLen }, color);
	// Bottom-right
	DrawPauseSolidRect({ pos.x + size.x - bracketLen, pos.y + size.y - thickness }, { bracketLen, thickness }, color);
	DrawPauseSolidRect({ pos.x + size.x - thickness, pos.y + size.y - bracketLen }, { thickness, bracketLen }, color);
}

void StageScene::DrawPauseCustomCursor() {
	float mx = customCursorPos_.x;
	float my = customCursorPos_.y;

	// 1. クリック波紋エフェクト
	if (cursorRippleTimer_ > 0.0f) {
		float rp = 1.0f - (cursorRippleTimer_ / 0.30f);
		float rRad = 4.0f + 24.0f * rp;
		float rAlpha = (1.0f - rp) * 0.90f;
		Vector4 ringCol = { 0.35f, 1.0f, 0.65f, rAlpha };
		DrawPauseSolidRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y - rRad }, { rRad * 2.0f, 1.5f }, ringCol);
		DrawPauseSolidRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y + rRad - 1.5f }, { rRad * 2.0f, 1.5f }, ringCol);
		DrawPauseSolidRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y - rRad }, { 1.5f, rRad * 2.0f }, ringCol);
		DrawPauseSolidRect({ cursorRipplePos_.x + rRad - 1.5f, cursorRipplePos_.y - rRad }, { 1.5f, rRad * 2.0f }, ringCol);
	}

	// 2. ターミナルの色に合わせた緑色のカスタムカーソル（鋭角タクティカル矢印ポインター）
	const Vector4 kBlackEdge = { 0.0f, 0.0f, 0.0f, 0.95f };
	const Vector4 kGreenBody = { 0.22f, 1.0f, 0.55f, 0.98f };
	const Vector4 kGreenCore = { 0.85f, 1.0f, 0.90f, 1.00f };

	// 黒シャドウ・縁
	DrawPauseSolidRect({ mx, my }, { 2.0f, 18.0f }, kBlackEdge);
	DrawPauseSolidRect({ mx + 1.0f, my + 1.0f }, { 13.0f, 2.0f }, kBlackEdge);
	DrawPauseSolidRect({ mx + 2.0f, my + 2.0f }, { 12.0f, 12.0f }, kBlackEdge);

	// 緑本体
	DrawPauseSolidRect({ mx + 1.0f, my + 1.0f }, { 2.0f, 15.0f }, kGreenBody);
	DrawPauseSolidRect({ mx + 2.0f, my + 2.0f }, { 10.0f, 2.0f }, kGreenBody);
	DrawPauseSolidRect({ mx + 3.0f, my + 3.0f }, { 8.0f, 8.0f }, kGreenBody);
	DrawPauseSolidRect({ mx + 1.0f, my + 1.0f }, { 2.0f, 4.0f }, kGreenCore);

	// 3. 先端照準レティクル
	Vector4 reticleCol = { 0.35f, 1.0f, 0.65f, 0.85f };
	DrawPauseSolidRect({ mx - 4.0f, my }, { 3.0f, 1.0f }, reticleCol);
	DrawPauseSolidRect({ mx + 2.0f, my }, { 3.0f, 1.0f }, reticleCol);
	DrawPauseSolidRect({ mx, my - 4.0f }, { 1.0f, 3.0f }, reticleCol);
	DrawPauseSolidRect({ mx, my + 2.0f }, { 1.0f, 3.0f }, reticleCol);
}

void StageScene::DrawPauseMenuUI() {
	ResetPauseSpritePool();
	TextRenderer* tr = TextRenderer::GetInstance();

	const float screenW = static_cast<float>(WinApp::kClientWidth);
	const float screenH = static_cast<float>(WinApp::kClientHeight);

	// 0. 全画面半透明ダークバックドロップ（薄暗くして背後の戦闘シーンを適度に見せる）
	DrawPauseSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.0f, 0.0f, 0.0f, 0.65f });

	// =========================================================================
	// ポーズウィンドウ本体（タイトル画面と同一の【完全な漆黒・純黒ブラック背景】）
	// =========================================================================
	const float winW = 500.0f;
	const float winH = 430.0f;
	Vector2 curPos = pauseWinPos_;
	Vector2 curSize = { winW, winH };

	const Vector4 borderCol   = { 0.18f, 0.82f, 0.45f, 0.85f }; // TitleSceneメインウィンドウのミリタリー枠線色
	const Vector4 titleBarBg  = { 0.03f, 0.10f, 0.06f, 1.0f };  // TitleSceneヘッダー色
	const Vector4 cornerColor = { 0.35f, 1.00f, 0.65f, 1.00f }; // コーナーブラケット色

	// 1. ドロップシャドウ
	DrawPauseSolidRect({ curPos.x + 8.0f, curPos.y + 10.0f }, curSize, { 0.0f, 0.0f, 0.0f, 0.70f });

	// 2. ウィンドウ本体パネル（【重要】TitleSceneのメインウィンドウと完全に同じ純黒ブラック { 0.0f, 0.0f, 0.0f, 1.0f }）
	DrawPausePanel(curPos, curSize, { 0.0f, 0.0f, 0.0f, 1.0f }, borderCol, 2.0f);

	// 3. タイトルバー
	DrawPauseSolidRect(curPos, { curSize.x, 32.0f }, titleBarBg);
	DrawPauseSolidRect(curPos, { curSize.x, 2.0f }, { 0.70f, 1.0f, 0.88f, 1.0f }); // 上端ネオンアクセントライン
	DrawPauseSolidRect({ curPos.x, curPos.y + 30.0f }, { curSize.x, 2.0f }, { 0.20f, 0.85f, 0.50f, 0.80f }); // 下部区切り線
	DrawPauseSolidRect({ curPos.x + 14.0f, curPos.y + 11.0f }, { 8.0f, 8.0f }, { 0.40f, 1.0f, 0.70f, 1.0f }); // 高輝度LED
	tr->Print("HackGen", "[PAUSE] TERMINAL : DAWN_OS [v4.12]", curPos.x + 28.0f, curPos.y + 8.0f, 13.0f, { 0.80f, 1.0f, 0.92f, 1.0f });

	// タイトルバー右側のステータスバッジ
	const float badgeX = curPos.x + curSize.x - 110.0f;
	const float badgeY = curPos.y + 7.0f;
	bool secBlink = (std::fmod(pauseAnimTimer_, 1.2f) < 0.6f);
	Vector4 ledColor = secBlink ? Vector4{ 0.35f, 1.0f, 0.65f, 1.0f } : Vector4{ 0.15f, 0.70f, 0.35f, 0.75f };
	DrawPauseSolidRect({ badgeX + 7.0f, badgeY + 6.0f }, { 6.0f, 6.0f }, ledColor);
	tr->Print("HackGen", "SEC:LV5 [OK]", badgeX + 18.0f, badgeY + 3.0f, 10.5f, { 0.45f, 1.0f, 0.70f, 0.95f });

	// 4. 四隅のコーナーブラケット（太さ 2.5px）
	DrawPauseCornerBrackets(curPos, curSize, 20.0f, 2.5f, cornerColor);

	// 5. サブヘッダー
	tr->Print("HackGenRegular", "/// MISSION PAUSED : PILOT DIRECTIVE ///", curPos.x + curSize.x * 0.5f, curPos.y + 44.0f, 11.5f, { 0.35f, 0.95f, 0.60f, 0.90f }, { 0.5f, 0.0f });

	// 認証ステータス行（水平区切り線 ＆ 黒ベタマスク）
	const float authY = curPos.y + 68.0f;
	DrawPauseSolidRect({ curPos.x + 24.0f, authY }, { curSize.x - 48.0f, 1.0f }, { 0.20f, 0.75f, 0.45f, 0.60f });
	DrawPauseSolidRect({ curPos.x + curSize.x * 0.5f - 160.0f, authY - 8.0f }, { 320.0f, 16.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });
	tr->Print("HackGenRegular", "AUTH LEVEL 5 GRANTED // READY FOR COMMAND", curPos.x + curSize.x * 0.5f, authY - 6.0f, 11.0f, { 0.28f, 0.88f, 0.52f, 0.90f }, { 0.5f, 0.0f });

	// 6. メニューボタン (4つ)
	const float buttonWidth = 380.0f;
	const float buttonHeight = 44.0f;
	const float buttonX = curPos.x + (curSize.x - buttonWidth) * 0.5f;
	const float buttonStartY = curPos.y + 96.0f;
	const float buttonSpacing = 54.0f;

	static const char* kLabels[4] = {
		"[01] RESUME FLIGHT",
		"[02] RESTART MISSION",
		"[03] ABORT TO TITLE",
		"[04] QUIT APPLICATION"
	};

	for (int i = 0; i < 4; ++i) {
		float btnY = buttonStartY + static_cast<float>(i) * buttonSpacing;
		bool isSelected = (pauseMenuIndex_ == i);

		if (isSelected) {
			// 選択中ハイライト（TitleSceneと完全に同一）
			DrawPauseSolidRect({ buttonX, btnY }, { buttonWidth, buttonHeight }, { 0.12f, 0.38f, 0.22f, 0.85f });
			DrawPauseSolidRect({ buttonX, btnY }, { 4.0f, buttonHeight }, { 0.50f, 1.0f, 0.75f, 1.0f });
			DrawPauseBorder({ buttonX, btnY }, { buttonWidth, buttonHeight }, { 0.45f, 1.0f, 0.70f, 0.95f }, 1.5f);
			DrawPauseCornerBrackets({ buttonX, btnY }, { buttonWidth, buttonHeight }, 8.0f, 1.5f, { 0.85f, 1.0f, 0.90f, 1.0f });

			std::string selText = ">> " + std::string(kLabels[i]);
			tr->Print("HackGen", selText, buttonX + 24.0f, btnY + 11.0f, 20.0f, { 0.85f, 1.00f, 0.90f, 1.00f });
		} else {
			// 非選択（完全純黒背景）
			DrawPauseSolidRect({ buttonX, btnY }, { buttonWidth, buttonHeight }, { 0.0f, 0.0f, 0.0f, 1.0f });
			DrawPauseBorder({ buttonX, btnY }, { buttonWidth, buttonHeight }, { 0.18f, 0.50f, 0.30f, 0.55f }, 1.0f);
			DrawPauseCornerBrackets({ buttonX, btnY }, { buttonWidth, buttonHeight }, 8.0f, 1.5f, { 0.25f, 0.70f, 0.40f, 0.70f });
			tr->Print("HackGen", kLabels[i], buttonX + 24.0f, btnY + 11.0f, 20.0f, { 0.35f, 0.92f, 0.55f, 0.95f });
		}
	}

	// 7. 操作ガイド枠（完全純黒背景）
	const float guideY = curPos.y + 322.0f;
	const float guideH = 34.0f;
	DrawPausePanel({ curPos.x + 24.0f, guideY }, { curSize.x - 48.0f, guideH }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.15f, 0.50f, 0.28f, 0.50f }, 1.0f);
	DrawPauseCornerBrackets({ curPos.x + 24.0f, guideY }, { curSize.x - 48.0f, guideH }, 10.0f, 1.5f, { 0.25f, 0.75f, 0.45f, 0.65f });
	tr->Print("HackGen", "[ UP / DOWN ] SELECT     [ ENTER / CLICK ] EXECUTE     [ ESC ] RESUME", curPos.x + curSize.x * 0.5f, guideY + 9.0f, 11.5f, { 0.35f, 0.95f, 0.55f, 0.95f }, { 0.5f, 0.0f });

	// 8. ウィンドウ最下部ステータスバー
	const float sbY = curPos.y + curSize.y - 14.0f;
	tr->Print("HackGenRegular", "ROOT@TACTICAL_OS: ONLINE", curPos.x + 16.0f, sbY, 11.5f, { 0.35f, 0.95f, 0.60f, 0.95f }, { 0.0f, 0.5f });
	tr->Print("HackGenRegular", "PORT: 115200 BAUD // UTF-8", curPos.x + curSize.x - 16.0f, sbY, 11.5f, { 0.30f, 0.90f, 0.55f, 0.90f }, { 1.0f, 0.5f });

	// ウィンドウテキストを描画フラッシュ
	tr->Flush();

	// 9. カスタムサイバーマウスカーソル（最前面）
	if (!isPauseAborting_) {
		DrawPauseCustomCursor();
	}

	// 10. ポーズからタイトル復帰時のCRTシャットダウン演出（最前面）
	if (isPauseAborting_) {
		DrawPauseCRTShutdown(pauseAbortTimer_ / kPauseAbortDuration);
	}
}

void StageScene::DrawPauseCRTShutdown(float progress) {
	float screenW = static_cast<float>(WinApp::kClientWidth);
	float screenH = static_cast<float>(WinApp::kClientHeight);
	float crtT = std::clamp(progress, 0.0f, 1.0f);
	float centerY = screenH * 0.5f;
	float centerX = screenW * 0.5f;

	if (crtT < 0.55f) {
		// ==========================================================
		// Phase 1 (0.00〜0.55): 画面垂直収束 ＆ 中央丸型楕円ブルーム発光
		// ==========================================================
		float p = crtT / 0.55f;
		float collapse = 1.0f - std::pow(1.0f - p, 3.0f);
		float halfH = (screenH * 0.5f) * (1.0f - collapse);
		if (halfH < 2.0f) halfH = 2.0f;

		// 上下の黒マスク
		DrawPauseSolidRect({ 0.0f, 0.0f }, { screenW, centerY - halfH }, { 0.0f, 0.0f, 0.0f, 1.0f });
		DrawPauseSolidRect({ 0.0f, centerY + halfH }, { screenW, screenH - (centerY + halfH) }, { 0.0f, 0.0f, 0.0f, 1.0f });

		float bloomIntensity = (std::min)(1.0f, p / 0.12f);

		// 1. 中央の大きく膨らむ楕円形蛍光体グロー（ユーザー提示画像のような丸みを帯びた発光）
		DrawPauseGlowOval({ centerX, centerY }, { 600.0f, 230.0f }, { 0.85f, 0.82f, 0.65f, 0.40f * bloomIntensity }, BlendMode::kAdd);
		DrawPauseGlowOval({ centerX, centerY }, { 460.0f, 160.0f }, { 0.95f, 0.95f, 0.85f, 0.60f * bloomIntensity }, BlendMode::kAdd);
		DrawPauseGlowOval({ centerX, centerY }, { 320.0f, 100.0f }, { 1.00f, 1.00f, 0.95f, 0.85f * bloomIntensity }, BlendMode::kAdd);
		DrawPauseGlowOval({ centerX, centerY }, { 200.0f, 50.0f },  { 1.00f, 1.00f, 1.00f, 1.00f * bloomIntensity }, BlendMode::kAdd);

		// 2. 画面端まで走る水平ビーム（多層発光ライン）
		DrawPauseSolidRect({ 0.0f, centerY - 6.0f }, { screenW, 12.0f }, { 0.70f, 0.85f, 0.95f, 0.35f * bloomIntensity }, BlendMode::kAdd);
		DrawPauseSolidRect({ 0.0f, centerY - 2.5f }, { screenW, 5.0f },  { 0.90f, 0.95f, 1.00f, 0.70f * bloomIntensity }, BlendMode::kAdd);
		DrawPauseSolidRect({ 0.0f, centerY - 1.0f }, { screenW, 2.0f },  { 1.00f, 1.00f, 1.00f, 1.00f * bloomIntensity }, BlendMode::kAdd);

		// 3. 発光部を横切るCRT特有の黒い走査線スリット（ユーザー提示画像のような横縞スリット）
		for (float y = centerY - 110.0f; y <= centerY + 110.0f; y += 14.0f) {
			if (std::abs(y - centerY) > 2.0f) {
				DrawPauseSolidRect({ 0.0f, y - 2.0f }, { screenW, 4.0f }, { 0.0f, 0.0f, 0.0f, 0.75f * bloomIntensity });
			}
		}
	} else if (crtT < 0.85f) {
		// ==========================================================
		// Phase 2 (0.55〜0.85): 水平ビームと楕円が左右から中央へ収束
		// ==========================================================
		float p = (crtT - 0.55f) / 0.30f;
		float shrink = 1.0f - p;
		float beamW = screenW * shrink;
		float beamX = (screenW - beamW) * 0.5f;

		// 画面全体ブラックアウト
		DrawPauseSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.0f, 0.0f, 0.0f, 1.0f });

		// 収束する中央楕円グロー
		float ovalW = 600.0f * (1.0f - p * 0.85f);
		float ovalH = 230.0f * (1.0f - p * 0.70f);
		DrawPauseGlowOval({ centerX, centerY }, { ovalW, ovalH }, { 0.85f, 0.82f, 0.65f, 0.40f * shrink }, BlendMode::kAdd);
		DrawPauseGlowOval({ centerX, centerY }, { ovalW * 0.75f, ovalH * 0.70f }, { 0.95f, 0.95f, 0.85f, 0.60f * shrink }, BlendMode::kAdd);
		DrawPauseGlowOval({ centerX, centerY }, { ovalW * 0.50f, ovalH * 0.45f }, { 1.00f, 1.00f, 1.00f, 1.00f * shrink }, BlendMode::kAdd);

		// 収束する水平ビーム
		DrawPauseSolidRect({ beamX, centerY - 5.0f }, { beamW, 10.0f }, { 0.70f, 0.85f, 0.95f, 0.35f * shrink }, BlendMode::kAdd);
		DrawPauseSolidRect({ beamX, centerY - 1.5f }, { beamW, 3.0f },  { 1.00f, 1.00f, 1.00f, 1.00f * shrink }, BlendMode::kAdd);

		// 走査線スリット
		for (float y = centerY - ovalH * 0.45f; y <= centerY + ovalH * 0.45f; y += 12.0f) {
			if (std::abs(y - centerY) > 2.0f) {
				DrawPauseSolidRect({ beamX, y - 1.5f }, { beamW, 3.0f }, { 0.0f, 0.0f, 0.0f, 0.70f * shrink });
			}
		}
	} else {
		// ==========================================================
		// Phase 3 (0.85〜1.00): 中央の極小蛍光体光点が「プツッ」と閃光して消滅
		// ==========================================================
		float p = (crtT - 0.85f) / 0.15f;
		DrawPauseSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.0f, 0.0f, 0.0f, 1.0f });

		if (p < 0.50f) {
			float popAlpha = 1.0f - (p / 0.50f);
			DrawPauseGlowOval({ centerX, centerY }, { 40.0f, 30.0f }, { 0.85f, 0.85f, 0.70f, 0.60f * popAlpha }, BlendMode::kAdd);
			DrawPauseGlowOval({ centerX, centerY }, { 16.0f, 10.0f }, { 1.00f, 1.00f, 1.00f, 1.00f * popAlpha }, BlendMode::kAdd);
		}
	}
}


