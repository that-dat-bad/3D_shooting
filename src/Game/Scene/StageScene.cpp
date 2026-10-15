#include "StageSceneInternal.h"

using namespace MyMath;
using namespace StageSceneInternal;

void StageScene::Initialize() {
	auto tStart = std::chrono::high_resolution_clock::now();
	sceneID = SCENE::STAGE;

	double assetsMs = 0.0;
	bool ranAssets = false;
	if (!isAssetsInitialized_) {
		auto aStart = std::chrono::high_resolution_clock::now();
		InitializeAssets();
		isAssetsInitialized_ = true;
		auto aEnd = std::chrono::high_resolution_clock::now();
		assetsMs = std::chrono::duration<double, std::milli>(aEnd - aStart).count();
		ranAssets = true;
	}

	auto rStart = std::chrono::high_resolution_clock::now();
	ResetGameplayState();
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
}

void StageScene::PrewarmAssets() {
	if (!isAssetsInitialized_) {
		auto aStart = std::chrono::high_resolution_clock::now();
		InitializeAssets();
		isAssetsInitialized_ = true;
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

	// 部位破壊対応ビジュアルモデルと飛散破片の描画
	aircraftVisualModel_.Draw();
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

	GPUParticleManager::GetInstance()->Update();
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
	float hudThreshold = combatBootSequence_.IsQuickMode() ? 0.40f : 3.60f;
	if (sortieIntroTimer_ >= hudThreshold) {
		hud_.Draw();
	}

	// 3. AC6スタイル「メインシステム、戦闘モード起動...」ブートUI描画（最前面）
	if (sortieIntroTimer_ < sortieIntroDuration_) {
		combatBootSequence_.DrawBootUI();
	}

	// 4. ゲームオーバー / ステージクリア演出UI描画（最前面）
	DrawEndSequenceUI();
}

void StageScene::Finalize() {
	combatBootSequence_.Finalize();
#ifdef _DEBUG
	TerrainSyncManager::GetInstance()->Finalize();
#endif
	
	// シーン終了時にカーソルを復帰
	Input::GetInstance()->UnlockCursor();
}

void StageScene::Restart() {
	ResetGameplayState();
}
