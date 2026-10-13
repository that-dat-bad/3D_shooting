#include "StageScene.h"
#include "../System/GameResult.h"
#include "../System/AsyncPreloadManager.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/SkyboxCommon.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "WinApp.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Physics/Collision3DManager.h"
#include "../Environment/TerrainSyncManager.h"
#include "../../engine/Graphics/UI/UITextRegistry.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include <cmath>
#include <algorithm>
#include <filesystem>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

using namespace MyMath;

// 固定フレームレートの deltaTime (60FPS想定)
static constexpr float kDeltaTime = 1.0f / 60.0f;


void StageScene::Initialize() {
	sceneID = SCENE::STAGE;

	if (!isAssetsInitialized_) {
		InitializeAssets();
		isAssetsInitialized_ = true;
	}

	ResetGameplayState();
}

void StageScene::PrewarmAssets() {
	if (!isAssetsInitialized_) {
		InitializeAssets();
		isAssetsInitialized_ = true;
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
	// TerrainSyncManager の初期化（通信は自動開始せず、デバッグImGuiからの明示的な開始時のみ機能）
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

void StageScene::TriggerGameOver() {
	if (isGameOver_ || isMissionCleared_) { return; }
	isGameOver_ = true;
	isMissionFailed_ = true;
	gameOverTimer_ = 0.0f;
	playerHP_ = 0.0f;
	timeScale_ = 0.35f; // 被弾スローモーション
	endSequence_ = StageEndSequence::GameOver_Critical;
	endSeqTimer_ = 0.0f;

	// 非同期処理による軽量化：ゲームオーバー演出中にバックグラウンドで次ステージデータを非同期プリロード！
	AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();

	// 項目4: テーマカラー赤反転
	hud_.SetWarningColorMode(true);

	EffectManager::GetInstance()->EmitDestroyEffect(flightModel_.GetPosition());

	// カメラシェイクによる被弾・撃墜の強烈な物理的衝撃（指摘1）
	if (CameraManager::GetInstance()->GetActiveCamera()) {
		CameraManager::GetInstance()->GetActiveCamera()->Shake(1.2f, 4.0f);
	}
}

void StageScene::TriggerGameClear() {
	if (isMissionCleared_ || isGameOver_) { return; }
	isMissionCleared_ = true;
	endSequence_ = StageEndSequence::Clear_HudShutdown;
	endSeqTimer_ = 0.0f;

	// 項目5: HUDの順次シャットダウン開始
	hud_.StartShutdown();
}


// ============================================================
// PlayerCollisionBody のコールバック実装
// ============================================================
void StageScene::PlayerCollisionBody::OnCollision(ICollisionBody3D* other) {
	if (!scene_ || scene_->isMissionFailed_) { return; }

	// EnemyBullet との衝突 → サブコライダー被弾処理
	if (other->GetCollisionAttribute() & CollisionAttribute::kEnemyBullet) {
		auto* bullet = dynamic_cast<Bullet*>(other);
		Vector3 bulletPos = bullet ? bullet->GetPosition() : scene_->flightModel_.GetPosition();
		float damage = bullet ? bullet->GetDamage() : 10.0f;

		HitResult res = scene_->flightModel_.ProcessBulletHit(bulletPos, damage);
		EffectManager::GetInstance()->EmitHitEffect(bulletPos);

		// カメラシェイク（被弾）
		if (CameraManager::GetInstance()->GetActiveCamera()) {
			CameraManager::GetInstance()->GetActiveCamera()->Shake(0.5f, 2.0f);
		}

		if (res.fireStarted) {
			EffectManager::GetInstance()->EmitDestroyEffect(bulletPos);
		}

		// 致命的ダメージでゲームオーバー
		if (scene_->flightModel_.GetAirframe().IsDestroyed()) {
			scene_->TriggerGameOver();
		}
	}
	// Enemy との衝突 → 胴体に大ダメージ
	else if (other->GetCollisionAttribute() & CollisionAttribute::kEnemy) {
		scene_->flightModel_.GetDamageModel().ProcessHit(DamagePart::Fuse, 50.0f);
		EffectManager::GetInstance()->EmitHitEffect(scene_->flightModel_.GetPosition());

		// カメラシェイク（激突）
		if (CameraManager::GetInstance()->GetActiveCamera()) {
			CameraManager::GetInstance()->GetActiveCamera()->Shake(0.8f, 5.0f);
		}

		if (scene_->flightModel_.GetAirframe().IsDestroyed()) {
			scene_->TriggerGameOver();
		}
	}
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

		// 残り時間に応じたスタイル切り替えは timeText_ の Warning 条件（ラムダ）で自動処理
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

	// 経過時間を更新
	totalTime_ += gameDeltaTime;

	// ============================
	// マウスポインター制御（自アプリがフォアグラウンドの時のみ非表示にし、他アプリ操作時は標準カーソルを復元）
	// ============================
	HWND hwnd = Input::GetInstance() ? Input::GetInstance()->GetHwnd() : nullptr;
	bool isForeground = (hwnd && ::GetForegroundWindow() == hwnd);
	if (isForeground) {
#ifdef USE_IMGUI
		ImGui::SetMouseCursor(ImGuiMouseCursor_None);
#endif
		while (::ShowCursor(FALSE) >= 0) {}
		::SetCursor(nullptr);
	} else {
		while (::ShowCursor(TRUE) < 0) {}
		::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
	}

	// ============================
	// プレイヤー入力 → FlightModel（ゲームオーバー中は操作遮断）
	// ============================
	Input* input = Input::GetInstance();
	float finalPitch = 0.0f;
	float finalRoll = 0.0f;
	float finalYaw = 0.0f;

	if (!isGameOver_) {
		// --- スロットル ---
		if (input->PushKey(DIK_W)) {
			throttle_ += 0.8f * gameDeltaTime;
		}
		if (input->PushKey(DIK_S)) {
			throttle_ -= 0.8f * gameDeltaTime;
		}
		if (throttle_ < 0.0f) { throttle_ = 0.0f; }
		if (throttle_ > 1.0f) { throttle_ = 1.0f; }
		flightModel_.SetThrottleInput(throttle_);

	// --- マウスエイム ON/OFF トグル (M キー) ---
	if (input->TriggerKey(DIK_M)) {
		mouseAimEnabled_ = !mouseAimEnabled_;
		mouseAimController_.SetEnabled(mouseAimEnabled_);
		if (mouseAimEnabled_) {
			mouseAimController_.ResetToDirection(flightModel_.GetForwardDirection());
			input->LockCursor();
		} else {
			input->UnlockCursor();
		}
	}

	// --- カーソルロック ON/OFF トグル (P キー) ---
	if (input->TriggerKey(DIK_P)) {
		if (input->IsCursorLocked()) {
			input->UnlockCursor();
		} else {
			input->LockCursor();
		}
	}

	// --- ナイトビジョン (NVD) ON/OFF トグル (N キー) ---
	if (input->TriggerKey(DIK_N)) {
		isNvdActive_ = !isNvdActive_;
	}

	// --- WASD 手動操縦入力 ---
	float manualPitch = 0.0f;
	float manualRoll = 0.0f;
	float manualYaw = 0.0f;

	if (input->PushKey(DIK_LSHIFT)) { manualPitch -= 1.0f; }
	if (input->PushKey(DIK_LCONTROL)) { manualPitch += 1.0f; }
	if (input->PushKey(DIK_A)) { manualRoll += 1.0f; }
	if (input->PushKey(DIK_D)) { manualRoll -= 1.0f; }
	if (input->PushKey(DIK_Q)) { manualYaw -= 1.0f; }
	if (input->PushKey(DIK_E)) { manualYaw += 1.0f; }

	bool hasManualInput = (std::fabs(manualPitch) > 0.01f ||
	                       std::fabs(manualRoll) > 0.01f ||
	                       std::fabs(manualYaw) > 0.01f);

	// フリーカメラ時、マウスエイムの目標が画面外（カメラの向いている方向から外れている）場合は
	// マウスエイムによる追従を一時停止し、機体がそちらに向かわないようにする
	bool pauseMouseAim = false;
	if (mouseAimEnabled_ && playerCamera_.GetFreeViewActive()) {
		Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
		if (cam) {
			const Matrix4x4& mat = cam->GetWorldMatrix();
			Vector3 camForward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
			Vector3 targetDir = mouseAimController_.GetTargetDirection();
			
			// 視界（カメラ前方）から一定以上外れているか判定（約36度: cos 0.8）
			if (MyMath::Dot(camForward, targetDir) < 0.8f) {
				pauseMouseAim = true;
			}
		}
	}

	// --- 操舵入力の決定 ---
	finalPitch = 0.0f;
	finalRoll = 0.0f;
	finalYaw = 0.0f;

	// --- マウス入力による目標方向の更新 ---
	// カーソルロック中かつ自由視点カメラ(Cキー)でない場合のみ、マウスで目標方向を更新する
	// ※WASD操作中でもマウスエイム目標の更新は止めない（置いてけぼりになるのを防ぐため）
	if (mouseAimEnabled_ && input->IsCursorLocked() && !playerCamera_.GetFreeViewActive()) {
		Input::MouseMove mouseMove = input->GetMouseMove();
		Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
		if (cam) {
			const Matrix4x4& mat = cam->GetWorldMatrix();
			Vector3 camRight = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
			Vector3 camUp = { mat.m[1][0], mat.m[1][1], mat.m[1][2] };
			mouseAimController_.UpdateTargetDirection(mouseMove.lX, mouseMove.lY, camRight, camUp);
		}
	}

	// 失速して機首が強制的に落ちている間は、エイム目標が空に取り残されないよう
	// 機首の方向へ徐々に引き戻す（ドラッグする）※フリーカメラ時限定
	if (mouseAimEnabled_ && playerCamera_.GetFreeViewActive() && flightModel_.IsStalling()) {
		Vector3 currentTarget = mouseAimController_.GetTargetDirection();
		Vector3 forward = flightModel_.GetForwardDirection();
		Vector3 newTarget = MyMath::Normalize(MyMath::Lerp(currentTarget, forward, 5.0f * kDeltaTime));
		mouseAimController_.ResetToDirection(newTarget);
	}

	if (mouseAimEnabled_ && !hasManualInput && !pauseMouseAim) {
		// === マウスエイムモード ===
		// マウスエイムコントローラーがPID制御で操舵入力を生成
		mouseAimController_.CalculateSteeringInput(
			flightModel_.GetOrientation(),
			kDeltaTime,
			finalPitch, finalRoll, finalYaw
		);
	} else {
		// === 手動操縦モード（WASDオーバーライド or マウスエイムOFF or 目標が画面外）===

		// フリーカメラ(Cキー)中に手動でWASD操作をした場合は、
		// 目標に到達しているかどうかに関わらずエイム目標を現在の機首方向に上書きする
		if (mouseAimEnabled_ && playerCamera_.GetFreeViewActive() && hasManualInput) {
			mouseAimController_.ResetToDirection(flightModel_.GetForwardDirection());
		}

		// インストラクター ON/OFF (I キー)
		if (input->TriggerKey(DIK_I)) {
			flightInstructor_.ToggleEnabled();
		}

		// インストラクターによる補正（無入力時に自動水平復帰）
		flightInstructor_.ApplyCorrection(
			manualPitch, manualRoll, manualYaw,
			flightModel_.GetOrientation(),
			finalPitch, finalRoll, finalYaw
		);
	}

	flightModel_.SetControlInput(finalPitch, finalRoll, finalYaw);

		// --- フラップ / エアブレーキ ---
		flightModel_.SetFlapInput(input->PushKey(DIK_F));
		flightModel_.SetAirBrakeInput(input->PushKey(DIK_B));
	} else {
		flightModel_.SetControlInput(0.0f, 0.0f, 0.0f);
		flightModel_.SetThrottleInput(0.0f);
	}

	// --- 射撃入力（マウス左クリック） ---
	isFiringThisFrame_ = false;
	if (input->PushMouse(0) && !isMissionCleared_ && !isMissionFailed_ && sortieIntroTimer_ >= sortieIntroDuration_) {
		int ammoBefore = gunpod_.GetCurrentAmmo();
		gunpod_.Fire();
		if (gunpod_.GetCurrentAmmo() < ammoBefore) {
			isFiringThisFrame_ = true;
			Vector3 aircraftPos = flightModel_.GetPosition();
			Vector3 aircraftForward = flightModel_.GetForwardDirection();
			Vector3 aircraftVelocity = flightModel_.GetVelocity();

			// 機首先端から発射
			Vector3 firePos = Add(aircraftPos, Multiply(8.0f, aircraftForward));

			// まっすぐ発射 (ガンコンバージェンスを廃止し、機首方向固定)
			Vector3 fireDir = aircraftForward;

			// 弾丸の初速ベクトル ＝ 機体速度 ＋ (射出方向 × 射出速度)
			// (自機が弾を追い抜く問題を防止し、物理法則に従う)
			float muzzleSpeed = 500.0f;
			Vector3 muzzleVelocity = Multiply(muzzleSpeed, fireDir);
			Vector3 bulletVelocity = Add(aircraftVelocity, muzzleVelocity);

			bulletManager_.SpawnBullet(firePos, bulletVelocity, 10.0f);

			// --- 薬莢の放出エフェクト ---
			{
				Vector3 aircraftPos = flightModel_.GetPosition();
				Vector3 velocity = flightModel_.GetVelocity(); // 機体の現在の速度
				Vector3 right = flightModel_.GetRightDirection();
				Vector3 up = flightModel_.GetUpDirection();
				Vector3 forward = flightModel_.GetForwardDirection();

				// 薬莢の排出口位置（コックピットより少し後ろの左下あたりにオフセット）
				Vector3 casingPos = Add(aircraftPos, Multiply(-2.0f, forward));
				casingPos = Add(casingPos, Multiply(-0.4f, right)); // 排出口に近い本来の左下オフセットに戻す
				casingPos = Add(casingPos, Multiply(-0.5f, up));

				ParticleParameters casingParams;
				// 機体速度をベースに、後方（-forward）へ勢いよくはじき出す速度を加算（後ろ側に吹き飛ぶように設定）
				Vector3 localMinVel = Add(Add(Multiply(-2.0f, right), Multiply(-3.0f, up)), Multiply(-18.0f, forward));
				Vector3 localMaxVel = Add(Add(Multiply(-0.5f, right), Multiply(-1.0f, up)), Multiply(-10.0f, forward));
				
				casingParams.minVelocity = Add(velocity, localMinVel);
				casingParams.maxVelocity = Add(velocity, localMaxVel);
				
				// 加速度：重力（-Y方向に 9.8m/s^2）
				casingParams.acceleration = { 0.0f, -9.8f, 0.0f };

				// 寿命：1.5秒程度で消える
				casingParams.minLifeTime = 1.2f;
				casingParams.maxLifeTime = 1.8f;

				// サイズ：カメラからでも黄銅色の破片がはっきり見えるようにサイズを大きくする
				casingParams.minScale = 0.12f;
				casingParams.maxScale = 0.18f;
				casingParams.endScale = 0.08f;
				casingParams.fadeOut = true;

				// 色：より明るく目立つゴールド/真鍮色
				casingParams.minColor = { 0.95f, 0.75f, 0.15f, 1.0f };
				casingParams.maxColor = { 1.0f, 0.85f, 0.30f, 1.0f };

				// 向き（回転）を完全にランダムに
				casingParams.minRotation = 0.0f;
				casingParams.maxRotation = 6.283185f;
				casingParams.minRotationSpeed = -15.0f;
				casingParams.maxRotationSpeed = 15.0f;

				// 発生位置のランダムばらつきはゼロ (排出口から直接)
				casingParams.randomPositionRange = 0.0f;

				// エミット（1発につき1個）
				ParticleManager::GetInstance()->Emit("Casing", casingPos, casingParams, 1);
			}

			// マズルフラッシュの間引き（2発に1回エフェクトを発生）
			muzzleFlashCount_++;
			if (muzzleFlashCount_ % 2 == 0) {
				EffectManager::GetInstance()->EmitMuzzleFlash(firePos, fireDir);
				muzzleFlashTimer_ = 0.08f; // 表示時間

				// カメラシェイク（発砲時）
				if (CameraManager::GetInstance()->GetActiveCamera()) {
					CameraManager::GetInstance()->GetActiveCamera()->Shake(0.1f, 0.5f);
				}

				// ランダム化パラメータの設定
				muzzleFlashRandomScale_ = 0.8f + (rand() % 40) / 100.0f; // 0.8 ~ 1.2
				muzzleFlashRandomRoll_ = (rand() % 628) / 100.0f;        // 0 ~ 2PI
				muzzleFlashRandomAlpha_ = 0.8f + (rand() % 20) / 100.0f; // 0.8 ~ 1.0

				// 動的ライト (PointLight) の点灯
				PointLight* pLight = Object3dCommon::GetInstance()->GetPointLightData();
				if (pLight) {
					pLight->position = firePos;
					pLight->color = { 1.0f, 0.4f, 0.05f, 1.0f }; // 濃いオレンジ
					pLight->intensity = 30.0f; // 強く光らせる
					pLight->radius = 30.0f;    // 届く範囲
					pLight->decay = 2.0f;      // 減衰
				}
			}
		}
	}

	if (muzzleFlashTimer_ > 0.0f) {
		muzzleFlashTimer_ -= gameDeltaTime;
	}

	// 動的ライトの減衰（毎フレーム少しずつ暗くする）
	PointLight* pLight = Object3dCommon::GetInstance()->GetPointLightData();
	if (pLight && pLight->intensity > 0.0f) {
		pLight->intensity -= 200.0f * gameDeltaTime; // 0.15秒程度で消えるペース
		if (pLight->intensity < 0.0f) { pLight->intensity = 0.0f; }
	}

	// 環境（太陽の位置・ライティング・ブルーム）の更新
	environmentManager_.Update(CameraManager::GetInstance()->GetActiveCamera(), gameDeltaTime);

	// ============================
	// 飛行モデルの更新
	// ============================
	flightModel_.Update(gameDeltaTime);

	// --- 部位破壊イベントのチェックと破片放出 ---
	CheckPartDestructionEvents();

	// ビジュアルモデルの更新（自機のワールド変換行列を親として階層合成）
	aircraftVisualModel_.Update(flightModel_.GetWorldMatrix(), gameDeltaTime);

	// 破片オブジェクト群の物理シミュレーション更新
	DebrisManager::GetInstance()->Update(gameDeltaTime);

#ifdef _DEBUG
	// 地形パラメータのMCP同期更新（デバッグ時のみ）
	TerrainSyncManager::GetInstance()->Update();
#endif

	// --- 翼端ボルテックスエフェクトの発生 ---
	float currentG = flightModel_.GetCurrentG();
	Vector3 aircraftPos = flightModel_.GetPosition();
	Vector3 right = flightModel_.GetRightDirection();
	Vector3 forward = flightModel_.GetForwardDirection();

	// 左右の翼端位置の計算 (翼幅を約12mと仮定し、左右に6.0mオフセット)
	// 翼端は重心より少し後方（-2.0m）にあると仮定
	Vector3 leftWingTip = Add(Add(aircraftPos, Multiply(-6.0f, right)), Multiply(-2.0f, forward));
	Vector3 rightWingTip = Add(Add(aircraftPos, Multiply(6.0f, right)), Multiply(-2.0f, forward));

	if (!isGameOver_ && currentG > 3.0f && flightModel_.GetSpeed() > 50.0f) {
		float gFactor = (currentG - 3.0f) / 3.0f; // 0.0 ~ 1.0
		if (gFactor < 0.0f) { gFactor = 0.0f; }
		if (gFactor > 1.0f) { gFactor = 1.0f; }

		Vector4 baseColor = { 0.95f, 0.98f, 1.0f, 0.35f * gFactor }; // アルファ値を適度に見える濃さに調整

		ParticleParameters params;
		params.minVelocity = { 0.0f, 0.0f, 0.0f }; // 完全にブレをゼロにして直線を保つ
		params.maxVelocity = { 0.0f, 0.0f, 0.0f };
		params.randomPositionRange = 0.0f;         // 発生位置の強制的なランダム散らしを無効化する
		params.minColor = baseColor;
		params.maxColor = baseColor;
		params.minLifeTime = 0.4f; // 寿命は少し長めで安定させる
		params.maxLifeTime = 0.4f;
		params.minScale = 0.08f;   // 初期サイズを非常に細くする (直径約16cm)
		params.maxScale = 0.08f;
		params.endScale = 0.12f;   // 消え際もあまり膨らませない (直径約24cm)
		params.fadeOut = true;
		params.minRotation = 0.0f; // 回転を完全にゼロにして繋がりを均一にする
		params.maxRotation = 0.0f;
		params.minRotationSpeed = 0.0f;
		params.maxRotationSpeed = 0.0f;

		// ストレッチビルボード（トレイル化）設定
		params.isStretched = true;
		params.stretchFactor = 12.5f; // scale 0.08f の逆数（1/0.08 = 12.5）で移動長さにフィットさせる

		if (hasLastWingTips_) {
			// 前フレームからの移動距離に応じて、隙間を埋めるように補間
			Vector3 deltaL = Subtract(leftWingTip, lastLeftWingTip_);
			float distL = Length(deltaL);
			Vector3 deltaR = Subtract(rightWingTip, lastRightWingTip_);

			// 0.25m（25cm）間隔で配置するための補間数（ストレッチビルボード化によりステップ数を激減）
			int steps = static_cast<int>(std::ceil(distL / 0.25f));
			if (steps < 1) { steps = 1; }
			if (steps > 40) { steps = 40; }

			// 各ステップの移動ベクトルを計算
			Vector3 stepDeltaL = Multiply(1.0f / static_cast<float>(steps), deltaL);
			Vector3 stepDeltaR = Multiply(1.0f / static_cast<float>(steps), deltaR);

			for (int i = 0; i < steps; ++i) {
				float t = static_cast<float>(i) / static_cast<float>(steps);
				Vector3 interpL = Lerp(lastLeftWingTip_, leftWingTip, t);
				Vector3 interpR = Lerp(lastRightWingTip_, rightWingTip, t);

				// 左翼ボルテックスのストレッチ方向を設定してエミット
				params.stretchDir = stepDeltaL;
				ParticleManager::GetInstance()->Emit("Vortex", interpL, params, 1);

				// 右翼ボルテックスのストレッチ方向を設定してエミット
				params.stretchDir = stepDeltaR;
				ParticleManager::GetInstance()->Emit("Vortex", interpR, params, 1);
			}
		} else {
			// 最初の1フレームは補間なしで単一生成（ストレッチは無効化）
			params.isStretched = false;
			ParticleManager::GetInstance()->Emit("Vortex", leftWingTip, params, 1);
			ParticleManager::GetInstance()->Emit("Vortex", rightWingTip, params, 1);
		}

		lastLeftWingTip_ = leftWingTip;
		lastRightWingTip_ = rightWingTip;
		hasLastWingTips_ = true;
	} else {
		// ボルテックスが発生していないときは前フレーム履歴をリセット
		hasLastWingTips_ = false;
	}

	// ============================
	// 戦闘システム更新（ゲームオーバー等の時間停止中はCPU負荷を完全バイパス）
	// ============================
	if (gameDeltaTime > 0.0001f) {
		gunpod_.Update(gameDeltaTime);
		bulletManager_.Update(gameDeltaTime);
		enemyManager_.Update(gameDeltaTime);
	}
	EffectManager::GetInstance()->Update();

	// ============================
	// ミッション目標 & トリガー評価 (Phase 1)
	// ============================
	auto& missionManager = MissionManager::GetInstance();
	missionManager.EvaluateObjectives(
		flightModel_.GetPosition(),
		playerHP_,
		enemyManager_.GetDestroyedCount(),
		enemyManager_.GetAliveEnemies().empty(),
		totalTime_
	);
	missionManager.EvaluateTriggers(
		flightModel_.GetPosition(),
		playerHP_,
		enemyManager_.GetDestroyedCount(),
		totalTime_
	);
	missionManager.UpdateMessages(gameDeltaTime);



	// ============================
	// 衝突判定（ゲームオーバー中は不要な当たり判定処理をスキップ）
	// ============================
	if (!isGameOver_) {
		// 毎フレーム登録をクリアして再登録
		collisionSystem_.ClearAll();

		// プレイヤーを登録
		collisionSystem_.Register(&playerBody_);

		// 全弾丸を登録
		for (auto& bullet : bulletManager_.GetBullets()) {
			collisionSystem_.Register(&bullet);
		}

		// 全敵（空中敵）を登録
		for (auto* enemy : enemyManager_.GetAliveEnemies()) {
			collisionSystem_.Register(enemy);
		}

		// 全地上敵を登録
		for (auto* groundEnemy : enemyManager_.GetAliveGroundEnemies()) {
			collisionSystem_.Register(groundEnemy);
		}

		// 全判定実行（コールバックが自動で呼ばれる）
		collisionSystem_.UpdateAll();
	}

	// --- クリア判定 ---
	if (!isMissionCleared_ && !isMissionFailed_ && enemyManager_.IsAllDestroyed()) {
		TriggerGameClear();
	}

	// --- 機体全般の壊滅判定 (パーツ破壊・パイロット死亡等) ---
	if (!isMissionFailed_ && flightModel_.GetAirframe().IsDestroyed()) {
		TriggerGameOver();
	}

	// ============================
	// 描画オブジェクトに位置・姿勢を反映
	// ============================
	Vector3 pos = flightModel_.GetPosition();
	Vector3 euler = PlayerCamera::QuaternionToEuler(flightModel_.GetOrientation());

	aircraftObject_->SetTranslate(pos);
	aircraftObject_->SetRotate(euler);
	aircraftObject_->Update();

	// ============================
	// 追従カメラ ＆ 出撃イントロカメラワーク（項目4: シームレス円弧旋回移行）
	// ============================
	if (sortieIntroTimer_ < sortieIntroDuration_) {
		sortieIntroTimer_ += gameDeltaTime;
		combatBootSequence_.Update(gameDeltaTime);
		float introProg = std::clamp(sortieIntroTimer_ / sortieIntroDuration_, 0.0f, 1.0f);

		// 重厚なカタパルト射出加速（初速約80km/hから徐々に180km/hへ滑走）
		float boostSpeed = 22.0f + 28.0f * (1.0f - std::exp(-introProg * 2.2f));
		flightModel_.SetVelocity(Multiply(boostSpeed, flightModel_.GetForwardDirection()));

		Vector3 forward = flightModel_.GetForwardDirection();
		Vector3 right = flightModel_.GetRightDirection();
		Vector3 up = flightModel_.GetUpDirection();

		Vector3 camPos{};
		Vector3 lookTarget{};

		if (sortieIntroTimer_ < 2.30f) {
			// Phase 1〜2: 機体の斜め前方〜右横ローアングルパス（重厚な滑走カット）
			float sideT = sortieIntroTimer_ / 2.30f;
			float forwardOffset = 6.0f - 2.0f * sideT;
			Vector3 camOffset = Add(Add(Multiply(forwardOffset, forward), Multiply(13.0f, right)), Multiply(-0.6f, up));
			camPos = Add(pos, camOffset);
			lookTarget = Add(pos, Multiply(2.0f, forward));
		} else {
			// Phase 3〜4: 機体の右側面から後方へのシネマティック円弧旋回（項目4）
			// 2.30s〜4.80s（2.5秒間）をかけて、機体の横から真後ろへ回り込み、通常の追従視線へシームレス接続！
			float orbitProg = std::clamp((sortieIntroTimer_ - 2.30f) / 2.50f, 0.0f, 1.0f);
			float smoothOrbit = orbitProg * orbitProg * (3.0f - 2.0f * orbitProg);

			// 旋回角: 右横（pi/2 = 90度） -> 真後ろ（pi = 180度）
			float angle = (3.14159265f * 0.50f) + smoothOrbit * (3.14159265f * 0.50f);
			float dist = 14.0f + 8.0f * smoothOrbit; // 14m -> 22m (通常追従距離)
			float height = -0.6f + 8.6f * smoothOrbit; // -0.6m -> +8.0m (通常追従高さ)

			// 機体のローカル水平面での円弧位置計算
			// angle = 90度 -> right=1.0, forward=0.0
			// angle = 180度 -> right=0.0, forward=-1.0
			float localX = dist * std::sin(angle);
			float localZ = -dist * std::cos(angle);
			Vector3 camOffset = Add(Add(Multiply(localZ, forward), Multiply(localX, right)), Multiply(height, up));
			camPos = Add(pos, camOffset);

			// 注視点も機体中心から、前方の追従注視点（前方40m + 上方4.8m）へ滑らかに補間
			Vector3 endLookTarget = Add(pos, Add(Multiply(40.0f, forward), Multiply(4.8f, up)));
			Vector3 startLookTarget = Add(pos, Multiply(2.0f, forward));
			lookTarget = Lerp(startLookTarget, endLookTarget, smoothOrbit);
		}

		// 毎フレーム PlayerCamera の内部状態と同期（ハードカットの完全根絶：項目4）
		playerCamera_.SnapTo(camPos, lookTarget);

	} else if (endSequence_ == StageEndSequence::Clear_TacticalMapZoom) {
		// 項目7: 戦闘空間（緑のグリッド）のカメラが急激にズームアウトし、ワイヤーフレームの戦術マップ画面に切り替わる
		float zoomProg = std::clamp((endSeqTimer_ - 2.8f) / 1.5f, 0.0f, 1.0f);
		float easeZoom = zoomProg * zoomProg;
		// 高度 100m -> 1800m へ真上から見下ろす垂直急ズームアウト！
		Vector3 mapCamPos = { pos.x, pos.y + 25.0f + 1600.0f * easeZoom, pos.z };
		Vector3 mapLookTarget = { pos.x, pos.y, pos.z + 0.1f };
		playerCamera_.SnapTo(mapCamPos, mapLookTarget);
	} else {
		// 通常追従カメラ（イントロの終了地点と注視点から100%シームレスに継続）
		playerCamera_.Update(gameDeltaTime, &flightModel_, &mouseAimController_, mouseAimEnabled_);
	}

	// カメラ更新直後にスカイボックスの位置を同期（項目7）
	if (skybox_) {
		skybox_->Update();
	}

	// ============================
	// ============================
#ifdef USE_IMGUI
	ImGui::Begin("Flight Model Debug");

	ImGui::Text("=== Status ===");
	ImGui::Text("Position: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);

	Vector3 vel = flightModel_.GetVelocity();
	ImGui::Text("Velocity: (%.1f, %.1f, %.1f)", vel.x, vel.y, vel.z);
	ImGui::Text("Speed: %.1f m/s (%.1f km/h)", flightModel_.GetSpeed(), flightModel_.GetSpeed() * 3.6f);
	ImGui::Text("Altitude: %.1f m", pos.y);

	ImGui::Separator();
	ImGui::Text("=== Flight Data ===");
	ImGui::Text("AoA: %.1f deg", flightModel_.GetCurrentAoA() * 57.2958f);
	ImGui::Text("G-Force: %.1f G", flightModel_.GetCurrentG());
	if (flightModel_.IsStalling()) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "*** STALL ***");
	}
	if (flightModel_.GetBlackoutFactor() > 0.01f) {
		ImGui::TextColored(ImVec4(0.3f, 0.3f, 0.3f, 1), "Blackout: %.0f%%", flightModel_.GetBlackoutFactor() * 100.0f);
	}
	if (flightModel_.GetRedoutFactor() > 0.01f) {
		ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "Redout: %.0f%%", flightModel_.GetRedoutFactor() * 100.0f);
	}

	ImGui::Separator();
	ImGui::Text("=== Engine ===");
	ImGui::Text("Throttle: %.0f%%", throttle_ * 100.0f);
	ImGui::Text("Engine Output: %.0f%%", flightModel_.GetCurrentThrottle() * 100.0f);
	ImGui::Text("Fuel: %.1f kg", flightModel_.GetCurrentFuel());

	ImGui::Separator();
	ImGui::Text("=== Aero Devices ===");
	ImGui::Text("Flap: %.0f%%", flightModel_.GetFlapPosition() * 100.0f);
	ImGui::Text("AirBrake: %.0f%%", flightModel_.GetAirBrakePosition() * 100.0f);

	ImGui::Separator();
	ImGui::Text("=== Damage Model ===");
	const auto& dm = flightModel_.GetDamageModel();
	for (int i = 0; i < DamageModel::kPartCount; ++i) {
		DamagePart part = static_cast<DamagePart>(i);
		const auto& st = dm.GetPartState(part);
		float hpRatio = st.GetHPRatio();

		ImVec4 color = ImVec4(1.0f - hpRatio, hpRatio, 0.2f, 1.0f);
		if (st.HasFlag(PartStatus::OnFire)) color = ImVec4(1.0f, 0.3f, 0.0f, 1.0f);
		if (st.HasFlag(PartStatus::Destroyed)) color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

		std::string statusStr = "";
		if (st.HasFlag(PartStatus::OnFire)) statusStr += " [FIRE]";
		if (st.HasFlag(PartStatus::Leaking)) statusStr += " [LEAK]";
		if (st.HasFlag(PartStatus::Destroyed)) statusStr += " [DESTROYED]";

		ImGui::TextColored(color, "%-18s: %3.0f%% HP%s", DamageModel::GetPartName(part), hpRatio * 100.0f, statusStr.c_str());
	}

	if (ImGui::TreeNode("Destruction Test (War Thunder)")) {
		if (ImGui::Button("Break Left Wing")) {
			flightModel_.GetDamageModel().ProcessHit(DamagePart::Wing_L, 999.0f);
			flightModel_.GetDamageModel().ProcessHit(DamagePart::Wing1_L, 999.0f);
			flightModel_.GetDamageModel().ProcessHit(DamagePart::Wing2_L, 999.0f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Break Right Wing")) {
			flightModel_.GetDamageModel().ProcessHit(DamagePart::Wing_R, 999.0f);
			flightModel_.GetDamageModel().ProcessHit(DamagePart::Wing1_R, 999.0f);
			flightModel_.GetDamageModel().ProcessHit(DamagePart::Wing2_R, 999.0f);
		}
		if (ImGui::Button("Destroy Engine")) {
			flightModel_.GetDamageModel().ProcessHit(DamagePart::Engine1, 999.0f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Break Rudder/Tail")) {
			flightModel_.GetDamageModel().ProcessHit(DamagePart::Tail, 999.0f);
			flightModel_.GetDamageModel().ProcessHit(DamagePart::Rudder, 999.0f);
		}
		ImGui::TreePop();
	}

	ImGui::Separator();
	ImGui::Text("=== Mass ===");
	ImGui::Text("Total Mass: %.1f kg", flightModel_.GetTotalMass());

	ImGui::Separator();
	ImGui::Text("=== Camera ===");
	ImGui::DragFloat("Distance", &*playerCamera_.GetCameraDistancePtr(), 0.5f, 5.0f, 100.0f);
	ImGui::DragFloat("Height", &*playerCamera_.GetCameraHeightPtr(), 0.5f, 0.0f, 30.0f);
	ImGui::DragFloat("Pos Lag", &*playerCamera_.GetCameraPosLagPtr(), 0.1f, 0.5f, 20.0f);
	ImGui::DragFloat("Look Lag", &*playerCamera_.GetCameraLookLagPtr(), 0.1f, 0.5f, 30.0f);

	ImGui::Separator();
	ImGui::Text("=== Mouse Aim ===");
	ImGui::Text("Mode: %s", mouseAimEnabled_ ? "MOUSE AIM (PID)" : "MANUAL");
	if (mouseAimEnabled_) {
		// マウス感度調整スライダー
		float sens = mouseAimController_.GetSensitivity();
		if (ImGui::SliderFloat("Sensitivity", &sens, 0.01f, 1.0f, "%.3f")) {
			mouseAimController_.SetSensitivity(sens);
		}
		Vector3 tDir = mouseAimController_.GetTargetDirection();
		ImGui::Text("Target Dir: (%.2f, %.2f, %.2f)", tDir.x, tDir.y, tDir.z);

		if (ImGui::TreeNode("PID Tuning")) {
			auto pidSliders = [](const char* label, PIDController& pid) {
				if (ImGui::TreeNode(label)) {
					ImGui::SliderFloat("P", &pid.kP, 0.0f, 5.0f);
					ImGui::SliderFloat("I", &pid.kI, 0.0f, 2.0f);
					ImGui::SliderFloat("D", &pid.kD, 0.0f, 3.0f);
					ImGui::Text("Integral: %.3f", pid.integral);
					ImGui::TreePop();
				}
			};
			pidSliders("Pitch PID", mouseAimController_.GetPitchPID());
			pidSliders("Roll PID", mouseAimController_.GetRollPID());
			pidSliders("Yaw PID", mouseAimController_.GetYawPID());
			pidSliders("Bank Level PID", mouseAimController_.GetBankPID());
			ImGui::TreePop();
		}
	}
	ImGui::Text("Steering: P=%.2f R=%.2f Y=%.2f", finalPitch, finalRoll, finalYaw);

	ImGui::Separator();
	ImGui::Text("=== Controls ===");
	ImGui::Text("Mouse: Aim direction (PID)");
	ImGui::Text("LShift/LCtrl: Pitch | A/D: Roll | Q/E: Yaw (override)");
	ImGui::Text("W/S: Throttle");
	ImGui::Text("F: Flaps | B: AirBrake");
	ImGui::Text("M: Mouse Aim [%s]", mouseAimEnabled_ ? "ON" : "OFF");
	ImGui::Text("I: Instructor [%s]", flightInstructor_.IsEnabled() ? "ON" : "OFF");
	ImGui::Text("P: Cursor Lock [%s]", input->IsCursorLocked() ? "LOCKED" : "FREE");

	ImGui::End();

	// === レティクルの描画（ワールド空間→スクリーン投影）===
	if (mouseAimEnabled_) {
		Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
		if (cam) {
			ImDrawList* drawList = ImGui::GetForegroundDrawList();
			const Matrix4x4& vpMat = cam->GetViewProjectionMatrix();
			float screenW = static_cast<float>(WinApp::kClientWidth);
			float screenH = static_cast<float>(WinApp::kClientHeight);
			Vector3 aircraftPos = flightModel_.GetPosition();

			// --- ワールド方向をスクリーン座標に投影するラムダ ---
			auto projectToScreen = [&](const Vector3& worldPos, float& outX, float& outY) -> bool {
				float cX = worldPos.x * vpMat.m[0][0] + worldPos.y * vpMat.m[1][0] + worldPos.z * vpMat.m[2][0] + vpMat.m[3][0];
				float cY = worldPos.x * vpMat.m[0][1] + worldPos.y * vpMat.m[1][1] + worldPos.z * vpMat.m[2][1] + vpMat.m[3][1];
				float cW = worldPos.x * vpMat.m[0][3] + worldPos.y * vpMat.m[1][3] + worldPos.z * vpMat.m[2][3] + vpMat.m[3][3];
				if (cW <= 0.001f) { return false; }
				outX = (cX / cW * 0.5f + 0.5f) * screenW;
				outY = (-cY / cW * 0.5f + 0.5f) * screenH;
				return true;
			};

			// --- マウスエイム目標レティクル（緑の十字線）---
			Vector3 targetDir = mouseAimController_.GetTargetDirection();
			Vector3 aimWorldPos = Add(aircraftPos, Multiply(500.0f, targetDir));
			float cx, cy;
			if (projectToScreen(aimWorldPos, cx, cy)) {
				float crossSize = 15.0f;
				float dotRadius = 3.0f;
				ImU32 reticleColor = IM_COL32(0, 255, 100, 220);
				ImU32 reticleColorDark = IM_COL32(0, 180, 70, 150);

				// 十字線
				drawList->AddLine(ImVec2(cx - crossSize, cy), ImVec2(cx - 5.0f, cy), reticleColor, 2.0f);
				drawList->AddLine(ImVec2(cx + 5.0f, cy), ImVec2(cx + crossSize, cy), reticleColor, 2.0f);
				drawList->AddLine(ImVec2(cx, cy - crossSize), ImVec2(cx, cy - 5.0f), reticleColor, 2.0f);
				drawList->AddLine(ImVec2(cx, cy + 5.0f), ImVec2(cx, cy + crossSize), reticleColor, 2.0f);
				drawList->AddCircle(ImVec2(cx, cy), dotRadius, reticleColor, 12, 2.0f);
				drawList->AddCircle(ImVec2(cx, cy), crossSize + 5.0f, reticleColorDark, 24, 1.0f);

				// --- 機首方向マーカー（白い×印）---
				Vector3 forward = flightModel_.GetForwardDirection();
				Vector3 noseWorldPos = Add(aircraftPos, Multiply(500.0f, forward));
				float nx, ny;
				if (projectToScreen(noseWorldPos, nx, ny)) {
					float noseSize = 8.0f;
					ImU32 noseColor = IM_COL32(255, 255, 255, 200);
					drawList->AddLine(ImVec2(nx - noseSize, ny - noseSize), ImVec2(nx + noseSize, ny + noseSize), noseColor, 2.0f);
					drawList->AddLine(ImVec2(nx + noseSize, ny - noseSize), ImVec2(nx - noseSize, ny + noseSize), noseColor, 2.0f);

					// 目標と機首をつなぐ薄いライン
					ImU32 lineColor = IM_COL32(255, 255, 255, 60);
					drawList->AddLine(ImVec2(cx, cy), ImVec2(nx, ny), lineColor, 1.0f);
				}
			}

			// ============================================
			// 敵目標HUDマーカー（空中・地上）のスクリーン描画
			// ============================================
			// 1. 空中敵マーカー (赤いひし形)
			for (auto* enemy : enemyManager_.GetAliveEnemies()) {
				float ex, ey;
				Vector3 enemyPos = enemy->GetPosition();
				if (projectToScreen(enemyPos, ex, ey)) {
					float dist = MyMath::Length(MyMath::Subtract(enemyPos, aircraftPos));
					float diamondSize = 9.0f;
					ImU32 airMarkerColor = IM_COL32(255, 60, 60, 220);

					// ひし形アイコン
					drawList->AddQuad(
						ImVec2(ex, ey - diamondSize),
						ImVec2(ex + diamondSize, ey),
						ImVec2(ex, ey + diamondSize),
						ImVec2(ex - diamondSize, ey),
						airMarkerColor, 1.8f
					);

					// 距離表示
					char distBuf[32];
					snprintf(distBuf, sizeof(distBuf), "%dm", static_cast<int>(dist));
					drawList->AddText(ImVec2(ex + diamondSize + 3.0f, ey - 7.0f), airMarkerColor, distBuf);
				}
			}

			// 2. 地上目標マーカー (オレンジ/黄色のブラケット [TGT])
			for (auto* ground : enemyManager_.GetAliveGroundEnemies()) {
				float gx, gy;
				Vector3 groundPos = ground->GetPosition();
				Vector3 markerPos = { groundPos.x, groundPos.y + 2.0f, groundPos.z };
				if (projectToScreen(markerPos, gx, gy)) {
					float dist = MyMath::Length(MyMath::Subtract(groundPos, aircraftPos));
					float boxSize = 10.0f;
					
					ImU32 groundColor = (ground->GetAIType() == GroundAIType::Turret) 
						? IM_COL32(255, 120, 30, 220) 
						: (ground->GetAIType() == GroundAIType::Structure)
							? IM_COL32(255, 210, 40, 220)
							: IM_COL32(80, 200, 255, 220);

					// 角括弧 [ ] マーカー
					float corner = 4.0f;
					// 左上
					drawList->AddLine(ImVec2(gx - boxSize, gy - boxSize), ImVec2(gx - boxSize + corner, gy - boxSize), groundColor, 1.8f);
					drawList->AddLine(ImVec2(gx - boxSize, gy - boxSize), ImVec2(gx - boxSize, gy - boxSize + corner), groundColor, 1.8f);
					// 右上
					drawList->AddLine(ImVec2(gx + boxSize, gy - boxSize), ImVec2(gx + boxSize - corner, gy - boxSize), groundColor, 1.8f);
					drawList->AddLine(ImVec2(gx + boxSize, gy - boxSize), ImVec2(gx + boxSize, gy - boxSize + corner), groundColor, 1.8f);
					// 左下
					drawList->AddLine(ImVec2(gx - boxSize, gy + boxSize), ImVec2(gx - boxSize + corner, gy + boxSize), groundColor, 1.8f);
					drawList->AddLine(ImVec2(gx - boxSize, gy + boxSize), ImVec2(gx - boxSize, gy + boxSize - corner), groundColor, 1.8f);
					// 右下
					drawList->AddLine(ImVec2(gx + boxSize, gy + boxSize), ImVec2(gx + boxSize - corner, gy + boxSize), groundColor, 1.8f);
					drawList->AddLine(ImVec2(gx + boxSize, gy + boxSize), ImVec2(gx + boxSize, gy + boxSize - corner), groundColor, 1.8f);

					// 目標ラベル & 距離表示
					char labelBuf[48];
					snprintf(labelBuf, sizeof(labelBuf), "%s %dm", 
						(ground->GetAIType() == GroundAIType::Turret) ? "AAA" :
						(ground->GetAIType() == GroundAIType::Structure) ? "FACILITY" : "VEHICLE",
						static_cast<int>(dist));
					drawList->AddText(ImVec2(gx + boxSize + 4.0f, gy - 7.0f), groundColor, labelBuf);
				}
			}
		}
	}
#endif

	// === ゲーム情報UI（ImGui仮実装） ===
#ifdef USE_IMGUI
	ImGui::Begin("Mission Status");
	
	// プレイヤーHPを色を変えて表示
	ImVec4 hpColor = (playerHP_ > 30.0f) ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
	ImGui::TextColored(hpColor, "PLAYER HP: %.1f / %.1f", playerHP_, playerMaxHP_);
	ImGui::Separator();

	if (ImGui::Checkbox("Enable Enemies (敵出現 ON/OFF)", &enableEnemies_)) {
		Restart();
	}

	ImGui::Text("Total Enemies: %d / %d (Air: %d, Ground: %d)", 
		enemyManager_.GetDestroyedCount(), enemyManager_.GetTotalCount(),
		enemyManager_.GetAliveAirCount(), enemyManager_.GetAliveGroundCount());
	ImGui::Text("Ammo: %d / %d", gunpod_.GetCurrentAmmo(), gunpod_.GetMaxAmmo());
	ImGui::Text("Bullets Active: %u", bulletManager_.GetActiveBulletCount());
	
	ImGui::Separator();
	ImGui::Text("=== Alive Air Enemies (%d) ===", enemyManager_.GetAliveAirCount());
	int enemyIndex = 0;
	for (auto* enemy : enemyManager_.GetAliveEnemies()) {
		float dist = MyMath::Length(MyMath::Subtract(enemy->GetPosition(), flightModel_.GetPosition()));
		if (enemy->GetAIType() == AIType::ChaseAttack) {
			ImGui::Text("[%d] Type A (Chase/Attack)", enemyIndex);
		} else {
			ImVec4 stateColor = (enemy->GetTypeBState() == TypeBState::Evading) ? ImVec4(1.0f, 0.5f, 0.0f, 1.0f) : ImVec4(0.0f, 0.8f, 1.0f, 1.0f);
			ImGui::Text("[%d] Type B (Cruise/Evade)", enemyIndex);
			ImGui::TextColored(stateColor, "   State: %s", enemy->GetTypeBStateString());
		}
		ImGui::Text("   Pos: (%.1f, %.1f, %.1f) | Dist: %.1f m", enemy->GetPosition().x, enemy->GetPosition().y, enemy->GetPosition().z, dist);
		enemyIndex++;
	}

	ImGui::Separator();
	ImGui::Text("=== Alive Ground Targets (%d) ===", enemyManager_.GetAliveGroundCount());
	int groundIndex = 0;
	for (auto* ground : enemyManager_.GetAliveGroundEnemies()) {
		float dist = MyMath::Length(MyMath::Subtract(ground->GetPosition(), flightModel_.GetPosition()));
		ImVec4 typeColor = (ground->GetAIType() == GroundAIType::Turret) ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) :
			(ground->GetAIType() == GroundAIType::Structure) ? ImVec4(0.9f, 0.9f, 0.3f, 1.0f) : ImVec4(0.4f, 0.85f, 1.0f, 1.0f);
		ImGui::TextColored(typeColor, "[%d] %s", groundIndex, ground->GetAITypeString());
		ImGui::Text("   HP: %.1f/%.1f | Dist: %.1f m | InSight: %s", 
			ground->GetHealth(), ground->GetMaxHealth(), dist, ground->IsTargetInSight() ? "YES" : "NO");
		ImGui::Text("   Pos: (%.1f, %.1f, %.1f)", ground->GetPosition().x, ground->GetPosition().y, ground->GetPosition().z);
		groundIndex++;
	}

	if (isMissionCleared_) {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "*** MISSION CLEAR ***");
	}
	if (isMissionFailed_) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "*** MISSION FAILED ***");
	}
	ImGui::End();

	DrawAircraftTuningEditor();
#endif

	// ============================
	// ポストエフェクトの動的設定
	// ============================
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->ClearActiveEffects();

	// 1. ナイトビジョン (NVD) モード
	if (isNvdActive_) {
		// 光量増幅 (LightAmp)
		ActivePostEffect amp;
		amp.type = PostEffectType::kLightAmp;
		amp.intensity = 2.5f;
		postEffect->AddActiveEffect(amp);

		// カラーティント (ColorTint - 緑)
		ActivePostEffect tint;
		tint.type = PostEffectType::kColorTint;
		tint.colorR = 0.1f;
		tint.colorG = 0.95f;
		tint.colorB = 0.2f;
		postEffect->AddActiveEffect(tint);

		// ブルーム (Bloom)
		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = 3.0f;
		bloom.dirX = 1.2f;
		bloom.dirY = 0.75f;
		postEffect->AddActiveEffect(bloom);

		// ノイズ (Noise)
		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.12f;
		postEffect->AddActiveEffect(noise);

		// 走査線 (ScanLine)
		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.kernelSize = 0;
		scan.intensity = 0.15f;
		scan.dirX = 600.0f;
		scan.dirY = 5.0f;
		postEffect->AddActiveEffect(scan);

		// 色収差 (Chromatic Aberration)
		ActivePostEffect ab;
		ab.type = PostEffectType::kChromaticAberration;
		ab.intensity = 0.015f;
		postEffect->AddActiveEffect(ab);

		// レンズ歪み (LensDistortion)
		ActivePostEffect dist;
		dist.type = PostEffectType::kLensDistortion;
		dist.intensity = 0.18f;
		postEffect->AddActiveEffect(dist);
	} else if (!isGameOver_) {
		// 出撃イントロ（AC6メインシステム戦闘モード起動）演出連動ポストエフェクト
		float bootBloom = 2.0f;
		float bootScan = 0.030f;
		float bootAberration = 0.004f;
		float bootVignette = 0.40f;

		if (sortieIntroTimer_ < sortieIntroDuration_) {
			bootVignette = 0.55f; // 演出中は外周を少し暗くして中央バナーを強調

			if (sortieIntroTimer_ < 0.85f) {
				// 項目1: 完全暗転（ノイズ・ブルームも抑えて静寂のタメ）
				bootBloom = 0.0f;
				bootScan = 0.0f;
				bootAberration = 0.0f;
			} else if (sortieIntroTimer_ < 1.35f) {
				// 走査レーザー時のサイバースキャンライン
				float t = (sortieIntroTimer_ - 0.85f) / 0.50f;
				bootScan = 0.08f * (1.0f - t * 0.4f);
				bootAberration = 0.012f * (1.0f - t * 0.5f);
				bootBloom = 2.4f;
			} else if (sortieIntroTimer_ < 3.40f) {
				// タイピング〜戦闘モード確立（適正ブルーム：過剰発光による横引き残像を防止：項目5）
				bootBloom = 2.2f;
				bootScan = 0.040f;
				bootAberration = 0.006f;
			} else if (sortieIntroTimer_ >= 4.55f && sortieIntroTimer_ <= 4.80f) {
				// 項目4: プレイ視点移行瞬間の微小ホワイトロックオングリッチ
				float flashT = (sortieIntroTimer_ - 4.55f) / 0.25f;
				float pulse = std::sin(flashT * 3.14159f);
				bootBloom = 2.2f + 1.2f * pulse;
				bootAberration = 0.015f * pulse;
			}
		}

		// ブルーム (Bloom)
		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = bootBloom;
		bloom.dirX = 1.15f;
		bloom.dirY = 0.70f;
		postEffect->AddActiveEffect(bloom);

		// 上品な走査線 (ScanLine)
		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.kernelSize = 0;
		scan.intensity = bootScan;
		scan.dirX = 640.0f;
		scan.dirY = 3.0f;
		postEffect->AddActiveEffect(scan);

		// 微小色収差 (Chromatic Aberration)
		ActivePostEffect ab;
		ab.type = PostEffectType::kChromaticAberration;
		ab.intensity = bootAberration;
		postEffect->AddActiveEffect(ab);

		// 周辺減光 (Vignette)
		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = bootVignette;
		postEffect->AddActiveEffect(vig);
	} else if (isGameOver_) {
		// 項目1, 4: ゲームオーバー時のポストエフェクト（マルチパスピンポンを廃止し、超軽量な1パスに最適化）
		if (endSequence_ == StageEndSequence::GameOver_Critical) {
			ActivePostEffect ab;
			ab.type = PostEffectType::kChromaticAberration;
			ab.intensity = 0.025f;
			postEffect->AddActiveEffect(ab);
		} else if (endSequence_ == StageEndSequence::GameOver_SignalLost) {
			ActivePostEffect scan;
			scan.type = PostEffectType::kScanLine;
			scan.intensity = 0.25f;
			scan.dirX = 640.0f;
			scan.dirY = 4.0f;
			postEffect->AddActiveEffect(scan);
		}
	}

	// 2. 環境の更新（太陽の回転、ライティング、太陽ブルーム処理の委譲）
	if (!isNvdActive_) {
		environmentManager_.Update(CameraManager::GetInstance()->GetActiveCamera(), kDeltaTime);
	} else {
		// ナイトビジョン中は太陽ブルームをかけないが、時間は進めたいので camera を nullptr にして更新
		environmentManager_.Update(nullptr, kDeltaTime);
	}

	// 3. Gフォースによるビネット（ブラックアウト / レッドアウト）
	float blackout = flightModel_.GetBlackoutFactor();
	float redout = flightModel_.GetRedoutFactor();
	if (blackout > 0.0f || redout > 0.0f) {
		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = blackout * 5.0f;
		vig.dirX = redout * 1.5f;
		// レッドアウト用の色を明示的に指定
		vig.colorR = 0.9f;
		vig.colorG = 0.05f;
		vig.colorB = 0.05f;
		postEffect->AddActiveEffect(vig);
	}


	// ============================
	// 戦闘HUDの更新
	// ============================
	Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
	StageHUDData hudData{};
	hudData.remainingTime = remainingTime_;
	hudData.currentAmmo = gunpod_.GetCurrentAmmo();
	hudData.maxAmmo = gunpod_.GetMaxAmmo();
	hudData.isFiring = isFiringThisFrame_;
	hudData.aircraftPosition = flightModel_.GetPosition();
	hudData.aircraftForward = flightModel_.GetForwardDirection();
	hudData.mouseAimTargetDirection = mouseAimController_.GetTargetDirection();
	hudData.mouseAimEnabled = mouseAimEnabled_;
	if (cam) {
		hudData.viewProjectionMatrix = cam->GetViewProjectionMatrix();
	}
	hudData.screenWidth = static_cast<float>(WinApp::kClientWidth);
	hudData.screenHeight = static_cast<float>(WinApp::kClientHeight);

	hud_.Update(gameDeltaTime, hudData);
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
	// (レガシーの全結合単一モデル描画を無効化し、マルチパーツモデルのみ描画)
	// if (aircraftObject_) {
	// 	aircraftObject_->Draw();
	// }

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

	// --- ベイパーコーン（プラントル・グロワート・シンギュラリティ）エフェクトの描画 ---
	float speed = flightModel_.GetSpeed();
	float speedKmH = speed * 3.6f; // m/s から km/h に変換
	if (speedKmH >= 1000.0f && speedKmH <= 1200.0f) {
		// 1000km/hで0.0、1100km/hで最大(1.0)、1200km/hで0.0になるフェードイン・アウト係数
		float speedFactor = 0.0f;
		if (speedKmH < 1100.0f) {
			speedFactor = (speedKmH - 1000.0f) / 100.0f;
		} else {
			speedFactor = (1200.0f - speedKmH) / 100.0f;
		}

		// 経過時間を用いたゆらぎの計算
		float t = totalTime_ * 40.0f; // 高速で揺らす

		// 基本位置・姿勢
		Vector3 aircraftPos = flightModel_.GetPosition();
		Vector3 forward = flightModel_.GetForwardDirection();
		MyMath::Quaternion aircraftQ = flightModel_.GetOrientation();

		// クォータニオンの乗算ヘルパー
		auto qMul = [](const MyMath::Quaternion& a, const MyMath::Quaternion& b) -> MyMath::Quaternion {
			return {
				a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
				a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
				a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
				a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
			};
		};

		// 軸と角度からクォータニオンを作成するヘルパー
		auto makeRotation = [](const MyMath::Vector3& axis, float angle) -> MyMath::Quaternion {
			float halfAngle = angle * 0.5f;
			float sinHalf = sinf(halfAngle);
			Vector3 normAxis = Normalize(axis);
			return {
				normAxis.x * sinHalf,
				normAxis.y * sinHalf,
				normAxis.z * sinHalf,
				cosf(halfAngle)
			};
		};

		// 描画用のテクスチャ取得
		uint32_t coneTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");

		// -------------------------------------------------------------
		// コーン1 (アウターシェル: 大きくて薄い)
		// -------------------------------------------------------------
		{
			// ゆらぎパラメータ
			float scaleNoise = 1.0f + 0.08f * sinf(t * 1.3f);
			float lengthNoise = 1.0f + 0.12f * cosf(t * 0.9f);
			float offsetNoise = 0.15f * sinf(t * 1.7f);

			// スケール: Y軸が長さ、X/Z軸が半径。
			// アウターシェルは少し大きめに。
			float radius = 5.0f * scaleNoise;
			float length = 4.0f * lengthNoise;
			Vector3 scale = { radius, length, radius };

			// 位置: 機体の中央付近に配置。オフセットも揺らす。
			Vector3 pos = Add(aircraftPos, Multiply(-0.8f + offsetNoise, forward));

			// 回転: 機体のクォータニオン * ローカル回転（X軸+90度 + ゆらぎ）
			MyMath::Quaternion localPitch = makeRotation({1.0f, 0.0f, 0.0f}, 3.14159265f / 2.0f);
			MyMath::Quaternion localYaw = makeRotation({0.0f, 1.0f, 0.0f}, 0.03f * sinf(t * 0.8f));
			MyMath::Quaternion localRoll = makeRotation({0.0f, 0.0f, 1.0f}, t * 0.2f);
			MyMath::Quaternion localRot = qMul(localRoll, qMul(localYaw, localPitch));
			MyMath::Quaternion finalRot = qMul(aircraftQ, localRot);

			// 色: 少し青みがかった白、フェードイン
			float alpha = 0.22f * speedFactor * (0.8f + 0.2f * sinf(t * 2.1f));
			Vector4 color = { 0.92f, 0.96f, 1.0f, alpha };

			PrimitiveModel::GetInstance()->DrawCone(scale, finalRot, pos, color, coneTex, cam, BlendMode::kNormal);
		}

		// -------------------------------------------------------------
		// コーン2 (インナーシェル: 小さくて濃い)
		// -------------------------------------------------------------
		{
			// ゆらぎパラメータ (アウターシェルと異なる周期で)
			float scaleNoise = 1.0f + 0.06f * sinf(t * 1.5f + 1.0f);
			float lengthNoise = 1.0f + 0.10f * cosf(t * 1.1f + 0.5f);
			float offsetNoise = 0.10f * sinf(t * 2.0f + 2.0f);

			// スケール: インナーシェルは少し小さく、長めに。
			float radius = 3.5f * scaleNoise;
			float length = 5.0f * lengthNoise;
			Vector3 scale = { radius, length, radius };

			// 位置: アウターシェルより少し前寄り（-0.4f）に。
			Vector3 pos = Add(aircraftPos, Multiply(-0.4f + offsetNoise, forward));

			// 回転: 機体のクォータニオン * ローカル回転（X軸+90度 + ゆらぎ）
			MyMath::Quaternion localPitch = makeRotation({1.0f, 0.0f, 0.0f}, 3.14159265f / 2.0f);
			MyMath::Quaternion localYaw = makeRotation({0.0f, 1.0f, 0.0f}, 0.02f * sinf(t * 1.2f + 0.3f));
			MyMath::Quaternion localRoll = makeRotation({0.0f, 0.0f, 1.0f}, -t * 0.15f);
			MyMath::Quaternion localRot = qMul(localRoll, qMul(localYaw, localPitch));
			MyMath::Quaternion finalRot = qMul(aircraftQ, localRot);

			// 色: より白っぽく、少し濃い
			float alpha = 0.28f * speedFactor * (0.85f + 0.15f * cosf(t * 1.9f));
			Vector4 color = { 0.95f, 0.98f, 1.0f, alpha };

			PrimitiveModel::GetInstance()->DrawCone(scale, finalRot, pos, color, coneTex, cam, BlendMode::kNormal);
		}
	}

	// 追従マズルフラッシュ（板ポリ・リング）の描画
	if (muzzleFlashTimer_ > 0.0f) {
		Vector3 aircraftPos = flightModel_.GetPosition();
		Vector3 forward = flightModel_.GetForwardDirection();
		Vector3 muzzlePos = Add(aircraftPos, Multiply(8.0f, forward));
		
		float t = 1.0f - (muzzleFlashTimer_ / 0.08f); // 0.0(開始) -> 1.0(終了)
		if (t < 0.0f) { t = 0.0f; }
		if (t > 1.0f) { t = 1.0f; }
		
		// 機体の回転を取得（オイラー角）
		Vector3 aircraftRot = PlayerCamera::QuaternionToEuler(flightModel_.GetOrientation());
		
		float randomAlpha = muzzleFlashRandomAlpha_ * (1.0f - t);
		
		// ============================
		// 1. Flash (中心の閃光)
		// ============================
		// 銃口から前方（Z軸方向）に長く伸びる十字の板ポリゴン
		uint32_t flashTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");
		Vector4 flashColor = { 1.0f, 0.7f, 0.2f, randomAlpha }; // 燃えるようなオレンジイエロー
		
		float flashLength = 4.0f * muzzleFlashRandomScale_ * (1.0f - t);
		float flashWidth = 1.0f * muzzleFlashRandomScale_ * (1.0f - t);
		Vector3 flashScale = { flashWidth, flashLength, 1.0f }; // Y軸方向に長くする（あとでX軸回転でZ軸方向に向ける）
		
		// 板ポリの中心が銃口になるため、前方に長さの半分だけオフセットする
		Vector3 flashPos = Add(muzzlePos, Multiply(flashLength * 0.5f, forward));
		
		for (int i = 0; i < 2; i++) {
			Vector3 rot = aircraftRot;
			// 1. X軸で90度回転し、Y軸方向の伸びをZ軸方向（前方）へ向ける
			rot.x -= 3.14159265f / 2.0f;
			// 2. 進行方向（Z軸）を中心に十字になるように回転
			rot.z += muzzleFlashRandomRoll_ + (i * 3.14159f / 2.0f);
			
			PrimitiveModel::GetInstance()->DrawPlane(flashScale, rot, flashPos, flashColor, flashTex, cam, BlendMode::kAdd);
		}

		// ============================
		// 2. Petals (星型の広がり)
		// ============================
		// 銃口の横（XY平面）に広がるガス炎
		uint32_t petalTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");
		Vector4 petalColor = { 1.0f, 0.4f, 0.05f, randomAlpha }; // 濃いオレンジ色
		
		// 星の「トゲ」を表現するため、細長い楕円を複数枚重ねる
		float petalLength = 3.0f * muzzleFlashRandomScale_ * (1.0f - t);
		float petalWidth = 0.5f * muzzleFlashRandomScale_ * (1.0f - t);
		Vector3 petalScale = { petalWidth, petalLength, 1.0f };
		
		for (int i = 0; i < 4; i++) {
			Vector3 rot = aircraftRot;
			// ペタルは前方に伸びるのではなく、銃口の正面（XY平面）に広がるため、X軸の回転は不要！
			// 十字/星型になるようにZ軸のみ回転させる
			rot.z += muzzleFlashRandomRoll_ + (i * 3.14159f / 4.0f);
			
			PrimitiveModel::GetInstance()->DrawPlane(petalScale, rot, muzzlePos, petalColor, petalTex, cam, BlendMode::kAdd);
		}
	}
}


void StageScene::DrawUI() {
	// 1. 出撃イントロ用黒フェード背景描画（3Dシーンの上、最前面UIの下）
	combatBootSequence_.DrawBackgroundFade();

	// 2. 戦闘HUD（残弾数・照準・タイマー）描画
	// 高速リトライ時は0.4sで即座に展開、初回通常時は3.6s以降に順次展開
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
	while (::ShowCursor(TRUE) < 0) {}
	::SetCursor(::LoadCursor(nullptr, IDC_ARROW));

	// エフェクトマネージャーの後片付け
	EffectManager::GetInstance()->Finalize();
	DebrisManager::GetInstance()->Clear();

	// ポストエフェクトをクリア
	PostEffect::GetInstance()->ClearActiveEffects();

	UITextRegistry::GetInstance()->Clear();
}


// ===========================================================
// 地面描画（タイル式グリッド ＆ シミュレータ空間Rez構築演出：項目7）
// ===========================================================

void StageScene::DrawGround() {
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (!camera) { return; }

	Vector3 aircraftPos = flightModel_.GetPosition();

	// --- 地面タイル（項目7: 4.5km四方の広大な大地） ---
	const float tileSize = 160.0f;
	const int tileRange = 14;

	// 機体位置をタイルサイズでスナップ（タイルの中心座標に丸める）
	float snapX = std::floor(aircraftPos.x / tileSize) * tileSize;
	float snapZ = std::floor(aircraftPos.z / tileSize) * tileSize;

	// タイルを敷き詰めて描画
	for (int ix = -tileRange; ix <= tileRange; ix++) {
		for (int iz = -tileRange; iz <= tileRange; iz++) {
			float cx = snapX + ix * tileSize;
			float cz = snapZ + iz * tileSize;

			// チェッカーボードパターンで色を交互に変える
			int gridX = static_cast<int>(std::floor(cx / tileSize));
			int gridZ = static_cast<int>(std::floor(cz / tileSize));
			bool isDark = ((gridX + gridZ) % 2 != 0);

			Vector4 color = isDark
				? Vector4{ 0.18f, 0.26f, 0.18f, 1.0f }   // 落ち着いた軍用ダークオリーブグリーン
				: Vector4{ 0.24f, 0.35f, 0.22f, 1.0f };   // 自然なミリタリーグリーン

			Vector3 scale = { tileSize, 1.0f, tileSize };
			Vector3 rotate = { 0.0f, 0.0f, 0.0f };
			Vector3 translate = { cx + tileSize * 0.5f, 0.0f, cz + tileSize * 0.5f };

			PrimitiveModel::GetInstance()->DrawPlane(
				scale, rotate, translate, color,
				groundTextureIndex_, camera
			);
		}
	}

	// --- グリッドライン（160m間隔） ---
	// 仮想戦闘空間のタクティカルネオングリッド
	const float lineExtent = tileRange * tileSize;
	const Vector4 lineCol = { 0.18f, 0.55f, 0.32f, 0.70f };

	for (int i = -tileRange; i <= tileRange; i++) {
		float x = snapX + i * tileSize;
		PrimitiveModel::GetInstance()->DrawLine3D(
			{ x, 0.15f, snapZ - lineExtent },
			{ x, 0.15f, snapZ + lineExtent },
			lineCol, camera
		);
	}

	for (int i = -tileRange; i <= tileRange; i++) {
		float z = snapZ + i * tileSize;
		PrimitiveModel::GetInstance()->DrawLine3D(
			{ snapX - lineExtent, 0.15f, z },
			{ snapX + lineExtent, 0.15f, z },
			lineCol, camera
		);
	}
}

void StageScene::CheckPartDestructionEvents() {
	const auto& dm = flightModel_.GetDamageModel();

	for (int i = 0; i < DamageModel::kPartCount; ++i) {
		DamagePart part = static_cast<DamagePart>(i);

		if (dm.IsPartDestroyed(part)) {
			if (aircraftVisualModel_.IsPartVisible(part)) {
				// 1. ビジュアル側を非表示化
				aircraftVisualModel_.SetPartVisible(part, false);

				// 2. 切断破片（Debris）を生成しワールドへ放出して物理運動
				Matrix4x4 partWorldMatrix = aircraftVisualModel_.GetPartWorldMatrix(part);
				Vector3 baseVelocity = flightModel_.GetVelocity();
				Vector3 ejectForce = Multiply(6.0f, flightModel_.GetRightDirection());
				if (part == DamagePart::Wing_L || part == DamagePart::Wing1_L || part == DamagePart::Wing2_L) {
					ejectForce = Multiply(-6.0f, flightModel_.GetRightDirection());
				}

				Model* mainModel = ModelManager::GetInstance()->FindModel("Resources/models/m21_wg.gltf");
				if (!mainModel) {
					mainModel = ModelManager::GetInstance()->FindModel("Resources/models/m21.gltf");
				}
				static const std::unordered_map<DamagePart, std::string> kPartNodeNameMap = {
					{ DamagePart::Fuse, "Fuse" },
					{ DamagePart::Wing_L, "Wing_L" },
					{ DamagePart::Wing1_L, "Wing1_L" },
					{ DamagePart::Wing2_L, "Wing2_L" },
					{ DamagePart::Wing_R, "Wing_R" },
					{ DamagePart::Wing1_R, "Wing1_R" },
					{ DamagePart::Wing2_R, "Wing2_R" },
					{ DamagePart::Tail, "Tail" },
				};

				std::string nodeName = "";
				auto itMap = kPartNodeNameMap.find(part);
				if (itMap != kPartNodeNameMap.end()) {
					nodeName = itMap->second;
				}

				DebrisManager::GetInstance()->SpawnDebris(mainModel, partWorldMatrix, baseVelocity, ejectForce, nodeName);

				// 3. 切断面からの破片発煙エフェクト
				Vector3 partPos = aircraftVisualModel_.GetPartWorldPosition(part);
				if (EffectManager::GetInstance()) {
					EffectManager::GetInstance()->EmitDestroyEffect(partPos);
				}
			}
		}
	}

	// エンジン損傷時のプロペラ回転制御
	if (dm.IsPartDestroyed(DamagePart::Engine1) || dm.GetPartState(DamagePart::Engine1).HasFlag(PartStatus::Damaged)) {
		float powerFactor = dm.GetEnginePowerFactor();
		aircraftVisualModel_.SetPropellerRpm(2000.0f * powerFactor);
	}
}


// ===========================================================
// ===========================================================
//
// ・機体の後方 + 上方にカメラの「理想位置」を計算
// ・現在のカメラ位置を Lerp で滑らかに理想位置へ追従（遅延あり）
// ・注視点も機体の少し前方を Lerp で滑らかに追従
// ・カメラの回転は LookAt で常に機体を見つめる
//



// ===========================================================
// クォータニオン → オイラー角 変換
// ===========================================================



// ===========================================================
// カメラ位置(from) → 注視点(to) を向くオイラー角を返す
// そのrotateに合致するオイラー角(XYZ順)を逆算する
// ジンバルロック対策: 真上・真下付近ではヨーを前フレームの値で維持
// ===========================================================
#ifdef USE_IMGUI
void StageScene::DrawAircraftTuningEditor() {
	if (ImGui::Begin("Aircraft Tuning (CFG)", &isAircraftTuningOpen_)) {
		// 1. CFGファイル管理
		if (ImGui::CollapsingHeader("CFG File Management", ImGuiTreeNodeFlags_DefaultOpen)) {
			std::vector<std::string> cfgFiles = AircraftConfig::GetAvailableAircraftList("Resources/aircraft");
			std::vector<std::string> fileNames;
			std::vector<const char*> fileNamePtrs;
			for (const auto& f : cfgFiles) {
				std::filesystem::path p(f);
				fileNames.push_back(p.filename().string());
			}
			for (const auto& fn : fileNames) {
				fileNamePtrs.push_back(fn.c_str());
			}

			if (ImGui::Combo("Select Aircraft CFG", &selectedAircraftConfigIndex_, fileNamePtrs.data(), static_cast<int>(fileNamePtrs.size()))) {
			}

			if (ImGui::Button("Load Selected CFG")) {
				if (selectedAircraftConfigIndex_ >= 0 && selectedAircraftConfigIndex_ < cfgFiles.size()) {
					aircraftConfig_.LoadFromCfg(cfgFiles[selectedAircraftConfigIndex_]);
					strcpy_s(tempAircraftConfigName_, sizeof(tempAircraftConfigName_), std::filesystem::path(cfgFiles[selectedAircraftConfigIndex_]).filename().string().c_str());
					Restart();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Save (Overwrite)")) {
				std::string savePath = aircraftConfig_.GetCurrentFilePath();
				if (savePath.empty()) {
					savePath = "Resources/aircraft/" + std::string(tempAircraftConfigName_);
				}
				aircraftConfig_.SaveToCfg(savePath);
			}

			ImGui::Separator();
			ImGui::InputText("New CFG Name", tempAircraftConfigName_, sizeof(tempAircraftConfigName_));
			if (ImGui::Button("Save As New CFG")) {
				std::string newPath = "Resources/aircraft/" + std::string(tempAircraftConfigName_);
				if (newPath.find(".cfg") == std::string::npos) {
					newPath += ".cfg";
				}
				aircraftConfig_.SaveToCfg(newPath);
				aircraftConfig_.SetCurrentFilePath(newPath);
			}
		}

		auto& general = aircraftConfig_.GetGeneral();
		auto& airframe = aircraftConfig_.GetAirframe();
		auto& engine = aircraftConfig_.GetEngine();
		auto& gunpod = aircraftConfig_.GetGunpod();

		// 2. [General & Visual]
		if (ImGui::CollapsingHeader("General & Visual", ImGuiTreeNodeFlags_DefaultOpen)) {
			static char nameBuf[128];
			strcpy_s(nameBuf, sizeof(nameBuf), general.name.c_str());
			if (ImGui::InputText("Aircraft Name", nameBuf, sizeof(nameBuf))) {
				general.name = nameBuf;
			}

			static char modelBuf[256];
			strcpy_s(modelBuf, sizeof(modelBuf), general.modelPath.c_str());
			if (ImGui::InputText("Model Path", modelBuf, sizeof(modelBuf))) {
				general.modelPath = modelBuf;
			}

			ImGui::SliderFloat("Env Reflection", &general.environmentCoefficient, 0.0f, 1.0f);
			ImGui::SliderFloat("Specular Intensity", &general.specularIntensity, 0.0f, 5.0f);
			ImGui::SliderFloat("Shininess", &general.shininess, 1.0f, 200.0f);
		}

		// 3. [Airframe]
		if (ImGui::CollapsingHeader("Airframe & Aerodynamics", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat("Empty Mass (kg)", &airframe.emptyFrameMass, 50.0f, 500.0f, 30000.0f);
			ImGui::DragFloat("Max Fuel (kg)", &airframe.maxInternalFuel, 10.0f, 0.0f, 10000.0f);
			ImGui::SliderFloat("Base Drag (Cd0)", &airframe.baseDrag, 0.001f, 0.2f, "%.4f");
			ImGui::SliderFloat("Lift Coeff (CL0)", &airframe.liftCoefficient, 0.0f, 1.5f);
			ImGui::DragFloat("Wing Area (m^2)", &airframe.wingArea, 0.5f, 5.0f, 200.0f);
			ImGui::DragFloat("Max Health (HP)", &airframe.maxHealth, 5.0f, 10.0f, 2000.0f);

			ImGui::Separator();
			ImGui::Text("--- Lift & Stall Characteristics ---");
			ImGui::SliderFloat("Critical AoA (rad)", &airframe.criticalAoA, 0.1f, 1.0f);
			ImGui::SliderFloat("Max Lift Coeff (CLmax)", &airframe.maxLiftCoefficient, 0.5f, 3.5f);
			ImGui::SliderFloat("Stall Lift Coeff", &airframe.stallLiftCoefficient, 0.05f, 1.0f);

			ImGui::Separator();
			ImGui::Text("--- Induced Drag & G Limits ---");
			ImGui::SliderFloat("Aspect Ratio", &airframe.aspectRatio, 1.0f, 20.0f);
			ImGui::SliderFloat("Oswald Efficiency", &airframe.oswaldEfficiency, 0.3f, 1.0f);
			ImGui::SliderFloat("Positive G Limit", &airframe.positiveGLimit, 2.0f, 20.0f);
			ImGui::SliderFloat("Negative G Limit", &airframe.negativeGLimit, -10.0f, 0.0f);

			ImGui::Separator();
			ImGui::Text("--- Flaps & Airbrake ---");
			ImGui::SliderFloat("Flap Lift Bonus", &airframe.flapLiftBonus, 0.0f, 2.0f);
			ImGui::SliderFloat("Flap Drag Bonus", &airframe.flapDragBonus, 0.0f, 0.5f);
			ImGui::DragFloat("Flap Max Speed (m/s)", &airframe.flapMaxSpeed, 1.0f, 30.0f, 300.0f);
			ImGui::SliderFloat("Flap Deploy Speed", &airframe.flapDeploySpeed, 0.5f, 10.0f);

			ImGui::SliderFloat("Airbrake Drag Bonus", &airframe.airBrakeDragBonus, 0.0f, 1.0f);
			ImGui::SliderFloat("Airbrake Deploy Speed", &airframe.airBrakeDeploySpeed, 0.5f, 10.0f);
		}

		// 4. [Engine]
		if (ImGui::CollapsingHeader("Engine & Propulsion", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat("Engine Mass (kg)", &engine.mass, 10.0f, 50.0f, 5000.0f);
			ImGui::DragFloat("Base Thrust (N)", &engine.baseThrust, 1000.0f, 1000.0f, 500000.0f, "%.0f N");
			ImGui::SliderFloat("Normal Throttle Limit", &engine.normalThrottleLimit, 0.5f, 1.0f);
			ImGui::SliderFloat("WEP Throttle Limit", &engine.wepThrottleLimit, 1.0f, 2.0f);
			ImGui::SliderFloat("Spool Speed", &engine.physicalSpoolSpeed, 0.05f, 5.0f);
			ImGui::SliderFloat("Fuel Flow Rate (kg/s)", &engine.baseFuelFlowRate, 0.1f, 20.0f);
			ImGui::SliderFloat("Altitude Thrust Factor", &engine.altitudeThrottleFactor, 0.0f, 0.0002f, "%.6f");
		}

		// 5. [Gunpod]
		if (ImGui::CollapsingHeader("Gunpod & Armament", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat("Gunpod Mass (kg)", &gunpod.baseMass, 5.0f, 10.0f, 500.0f);
			ImGui::SliderFloat("Gunpod Drag (Cd)", &gunpod.drag, 0.0f, 0.05f, "%.4f");
			ImGui::SliderFloat("Ammo Weight (kg/rd)", &gunpod.ammoWeight, 0.01f, 1.0f);
			ImGui::DragInt("Max Ammo", &gunpod.maxAmmo, 10, 50, 5000);
			ImGui::SliderFloat("Fire Rate (rds/s)", &gunpod.fireRate, 5.0f, 100.0f);
		}

		ImGui::Separator();

		// 即座に適用
		if (ImGui::Button("Apply to Current Aircraft", ImVec2(-1, 35))) {
			Vector3 curPos = flightModel_.GetPosition();
			Vector3 curVel = flightModel_.GetVelocity();
			Quaternion curOri = flightModel_.GetOrientation();

			flightModel_.Initialize(airframe, engine);
			flightModel_.SetPosition(curPos);
			flightModel_.SetVelocity(curVel);
			flightModel_.SetOrientation(curOri);

			gunpod_.Initialize(gunpod);
		}

		if (ImGui::Button("Save & Restart Stage", ImVec2(-1, 35))) {
			std::string savePath = aircraftConfig_.GetCurrentFilePath();
			if (savePath.empty()) {
				savePath = "Resources/aircraft/mig21.cfg";
			}
			aircraftConfig_.SaveToCfg(savePath);
			Restart();
		}
	}
	ImGui::End();
}
#endif

// ============================================================
// Phase 1: ミッション目標のHUD表示（ゲーム中の進捗表示）
// ============================================================
void StageScene::DrawMissionObjectivesHUD() {
	auto& mm = MissionManager::GetInstance();
	const auto& objectives = mm.GetCurrentMission().objectives;

	if (objectives.empty()) return;

	// 画面右上に目標リストを表示
	float startX = WinApp::kClientWidth - 350.0f;
	float startY = 60.0f;

#ifdef USE_IMGUI
	ImGui::SetNextWindowPos(ImVec2(startX, startY), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.5f);

	ImGui::Begin("Mission Objectives", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs);

	ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "MISSION OBJECTIVES");
	ImGui::Separator();

	for (const auto& obj : objectives) {
		if (obj.isHidden && obj.status != ObjectiveStatus::Completed) continue;

		ImVec4 color;
		const char* statusIcon;
		switch (obj.status) {
		case ObjectiveStatus::Completed: color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); statusIcon = "[OK]"; break;
		case ObjectiveStatus::Failed:    color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); statusIcon = "[X]"; break;
		case ObjectiveStatus::Inactive:  continue; // 非表示
		default:                         color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); statusIcon = "[ ]"; break;
		}

		std::string prefix = obj.isPrimary ? "" : "  ";
		ImGui::TextColored(color, "%s%s %s", prefix.c_str(), statusIcon, obj.name.c_str());
	}

	ImGui::End();
#endif
}

// ============================================================
// Phase 1: トリガーメッセージのHUD表示
// ============================================================
void StageScene::DrawMissionMessages() {
	auto& mm = MissionManager::GetInstance();
	const auto& messages = mm.GetActiveMessages();

	if (messages.empty()) return;

#ifdef USE_IMGUI
	// 画面中央上部にメッセージを表示
	float centerX = WinApp::kClientWidth * 0.5f;
	float msgY = 80.0f;

	ImGui::SetNextWindowPos(ImVec2(centerX - 250.0f, msgY), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.7f);

	ImGui::Begin("MissionMessages", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs);

	for (const auto& msg : messages) {
		// フェードアウト効果
		float alpha = (std::min)(msg.remainingTime, 1.0f);
		ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.5f, alpha), "%s", msg.text.c_str());
	}

	ImGui::End();
#endif
}

void StageScene::Restart() {
	ResetGameplayState();
}

// ============================================================
// ゲームオーバー / クリア演出シーケンス更新（項目1〜7）
// ============================================================
void StageScene::UpdateEndSequence(float dt) {
	if (endSequence_ == StageEndSequence::None) {
		timeScale_ = 1.0f;
		return;
	}

	endSeqTimer_ += dt;

	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->ClearActiveEffects();

	// --- ゲームオーバー系（被弾・撃墜のシステム破壊演出をシェーダーで画面全体に展開：指摘1, 5）---
	if (endSequence_ == StageEndSequence::GameOver_Critical) {
		// 0.0s - 0.25s: 撃墜瞬間の被弾スローモーション & 激しい画面破壊シェーダー
		timeScale_ = 0.20f;

		// 1. 赤色警告ティントストロボ点滅
		ActivePostEffect tint;
		tint.type = PostEffectType::kColorTint;
		tint.colorR = 1.0f;
		tint.colorG = (std::fmod(endSeqTimer_, 0.08f) < 0.04f) ? 0.15f : 0.40f;
		tint.colorB = 0.10f;
		postEffect->AddActiveEffect(tint);

		// 2. 衝撃レンズ歪み
		ActivePostEffect dist;
		dist.type = PostEffectType::kLensDistortion;
		dist.intensity = 0.25f;
		postEffect->AddActiveEffect(dist);

		// 3. 激しい色収差 (RGB分離)
		ActivePostEffect ca;
		ca.type = PostEffectType::kChromaticAberration;
		ca.intensity = 0.08f;
		postEffect->AddActiveEffect(ca);

		// 4. ランダム破壊ノイズ
		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.35f;
		postEffect->AddActiveEffect(noise);

		if (endSeqTimer_ >= 0.25f) {
			endSequence_ = StageEndSequence::GameOver_SignalLost;
		}
	} else if (endSequence_ == StageEndSequence::GameOver_SignalLost) {
		// 0.25s - 0.70s: 致命的通信途絶・フリーズ & 激しい走査線同期乱れ & SIGNAL LOST
		timeScale_ = 0.0f;

		// 1. 走査線の乱れ
		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.kernelSize = 0;
		scan.intensity = 0.45f;
		scan.dirX = 250.0f;
		scan.dirY = -30.0f;
		postEffect->AddActiveEffect(scan);

		// 2. デジタルグリッチノイズ
		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.45f;
		postEffect->AddActiveEffect(noise);

		// 3. 色収差
		ActivePostEffect ca;
		ca.type = PostEffectType::kChromaticAberration;
		ca.intensity = 0.06f;
		postEffect->AddActiveEffect(ca);

		// 4. 赤暗化ビネット
		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = 1.4f;
		postEffect->AddActiveEffect(vig);

		if (endSeqTimer_ >= 0.70f) {
			endSequence_ = StageEndSequence::GameOver_CrtShutdown;
		}
	} else if (endSequence_ == StageEndSequence::GameOver_CrtShutdown) {
		// 0.70s - 0.95s: ブラウン管テレビの電源遮断演出（水平収束・プツッ消滅）
		timeScale_ = 0.0f;

		ActivePostEffect dist;
		dist.type = PostEffectType::kLensDistortion;
		dist.intensity = 0.35f;
		postEffect->AddActiveEffect(dist);

		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.intensity = 0.5f;
		scan.dirX = 800.0f;
		scan.dirY = 5.0f;
		postEffect->AddActiveEffect(scan);

		if (endSeqTimer_ >= 0.95f) {
			postEffect->ClearActiveEffects();
			sceneID = SCENE::GAMEOVER;
			return;
		}
	}
	// --- ステージクリア系 ---
	else if (endSequence_ == StageEndSequence::Clear_HudShutdown) {
		// 0.0s - 1.2s: 目標達成ファンファーレ & 発光
		timeScale_ = 1.0f;

		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = 2.5f;
		bloom.dirX = 1.2f;
		bloom.dirY = 0.7f;
		postEffect->AddActiveEffect(bloom);

		if (endSeqTimer_ >= 1.2f) {
			endSequence_ = StageEndSequence::Clear_AutopilotReturn;
		}
	} else if (endSequence_ == StageEndSequence::Clear_AutopilotReturn) {
		// 1.2s - 2.5s: オートパイロット帰還時の超音速ワープ急加速演出（指摘2: ラジアルブラー画面全体シェーダー）
		timeScale_ = 1.0f;
		float climbT = std::clamp((endSeqTimer_ - 1.2f) / 1.3f, 0.0f, 1.0f);
		float speed = 120.0f + climbT * 600.0f; // 圧倒的最高速
		Vector3 curFwd = flightModel_.GetForwardDirection();
		Vector3 climbDir = Normalize(Add(curFwd, { 0.0f, 0.65f * climbT, 0.0f }));
		flightModel_.SetVelocity(Multiply(speed, climbDir));

		// 1. ラジアルブラー（スピード感・ワープ感の画面全体演出）
		ActivePostEffect blur;
		blur.type = PostEffectType::kRadialBlur;
		blur.kernelSize = 8;
		blur.intensity = climbT * 0.08f;
		postEffect->AddActiveEffect(blur);

		// 2. 色収差（光速突入の色ズレ）
		ActivePostEffect ca;
		ca.type = PostEffectType::kChromaticAberration;
		ca.intensity = climbT * 0.04f;
		postEffect->AddActiveEffect(ca);

		if (endSeqTimer_ >= 2.5f) {
			endSequence_ = StageEndSequence::Clear_TacticalMapZoom;
		}
	} else if (endSequence_ == StageEndSequence::Clear_TacticalMapZoom) {
		// 2.5s - 3.8s: 戦術マップ表示（走査線 + ビネット）＆ ターミナル転送接続（指摘3, 4）
		timeScale_ = 1.0f;

		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.intensity = 0.25f;
		scan.dirX = 500.0f;
		scan.dirY = -3.0f;
		postEffect->AddActiveEffect(scan);

		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = 1.1f;
		postEffect->AddActiveEffect(vig);

		if (endSeqTimer_ >= 3.8f) {
			postEffect->ClearActiveEffects();
			// クリアリザルトへ記録を保存して遷移
			auto& res = GameResult::GetInstance();
			res.missionCleared = true;
			res.clearTimeSeconds = missionElapsedTime_;
			res.playerHPRemaining = playerHP_;
			res.playerMaxHP = playerMaxHP_;
			sceneID = SCENE::CLEAR;
			return;
		}
	}
}

// ============================================================
// ゲームオーバー / クリア演出UI描画（項目1〜7）
// ============================================================
void StageScene::DrawEndSequenceUI() {
	if (endSequence_ == StageEndSequence::None) {
		return;
	}

	float screenW = static_cast<float>(WinApp::kClientWidth);
	float screenH = static_cast<float>(WinApp::kClientHeight);

	// 軽量描画ヘルパーラムダ
	auto DrawSolidRect = [this](const Vector2& pos, const Vector2& size, const Vector4& color, BlendMode blend = BlendMode::kNormal) {
		if (!seqOverlaySprite_) return;
		seqOverlaySprite_->SetBlendMode(blend);
		seqOverlaySprite_->SetPosition(pos);
		seqOverlaySprite_->SetAnchorPoint({ 0.0f, 0.0f });
		seqOverlaySprite_->SetSize(size);
		seqOverlaySprite_->SetRotation(0.0f);
		seqOverlaySprite_->SetColor(color);
		seqOverlaySprite_->Update();
		seqOverlaySprite_->Draw();
	};

	auto DrawHollowBorder = [&DrawSolidRect](const Vector2& pos, const Vector2& size, const Vector4& color, float thickness) {
		DrawSolidRect(pos, { size.x, thickness }, color);
		DrawSolidRect({ pos.x, pos.y + size.y - thickness }, { size.x, thickness }, color);
		DrawSolidRect(pos, { thickness, size.y }, color);
		DrawSolidRect({ pos.x + size.x - thickness, pos.y }, { thickness, size.y }, color);
	};

	// ------------------------------------------------------------
	// 項目4: 被弾・撃墜時のテーマカラー反転 & CRITICAL DAMAGE 激しい点滅
	// ------------------------------------------------------------
	if (endSequence_ == StageEndSequence::GameOver_Critical) {
		// 0.08s 周期の激しい赤色警告フラッシュ
		float blinkPhase = std::fmod(endSeqTimer_, 0.10f);
		bool isBlinkOn = (blinkPhase < 0.06f);

		if (isBlinkOn) {
			float bannerW = 680.0f;
			float bannerH = 70.0f;
			float bannerX = (screenW - bannerW) * 0.5f;
			float bannerY = 120.0f;

			// 赤色半透明バナー背景
			DrawSolidRect({ bannerX, bannerY }, { bannerW, bannerH }, { 0.50f, 0.02f, 0.02f, 0.85f });
			DrawSolidRect({ bannerX, bannerY }, { bannerW, 2.0f }, { 1.0f, 0.25f, 0.20f, 0.95f });
			DrawSolidRect({ bannerX, bannerY + bannerH - 2.0f }, { bannerW, 2.0f }, { 1.0f, 0.25f, 0.20f, 0.95f });

			TextRenderer::GetInstance()->Print(
				"HackGen",
				"! CRITICAL DAMAGE !",
				screenW * 0.5f,
				bannerY + 10.0f,
				34.0f,
				{ 1.0f, 0.35f, 0.30f, 1.0f },
				{ 0.5f, 0.0f }
			);

			TextRenderer::GetInstance()->Print(
				"HackGen",
				"HULL INTEGRITY COMPROMISED // EJECTION RECOMMENDED",
				screenW * 0.5f,
				bannerY + 44.0f,
				14.0f,
				{ 1.0f, 0.85f, 0.85f, 0.90f },
				{ 0.5f, 0.0f }
			);
		}
	}

	// ------------------------------------------------------------
	// 項目1: 通信途絶（シグナルロスト）フリーズ & 軽量グリッチ帯 & テキスト
	// （※GPUストールを引き起こしていた30回スプライト描画ループを完全撤廃し軽量化）
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::GameOver_SignalLost) {
		// 薄暗い赤黒ノイズベール（1枚のみ）
		// 項目1: 致命的通信途絶・フリーズ & 激しい画面の乱れ（ジッター・RGBスプリット・グリッチ走査）
		// 画面水平ジッター（致命的エラーによる同期ズレ）
		float jitterX = std::sin(endSeqTimer_ * 110.0f) * 8.0f;
		if (std::fmod(endSeqTimer_, 0.08f) < 0.03f) {
			jitterX += ((std::rand() % 20) - 10) * 1.5f;
		}

		// 赤黒ノイズベール
		DrawSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.20f, 0.02f, 0.02f, 0.40f });

		// 走査線インターレースグリッチ（数本が激しく上下を走る）
		float glitchOffset1 = std::fmod(endSeqTimer_ * 950.0f, screenH);
		DrawSolidRect({ 0.0f, glitchOffset1 }, { screenW, 20.0f }, { 0.95f, 0.95f, 1.0f, 0.25f }, BlendMode::kAdd);
		float glitchOffset2 = std::fmod(endSeqTimer_ * 1400.0f + 240.0f, screenH);
		DrawSolidRect({ 0.0f, glitchOffset2 }, { screenW, 40.0f }, { 1.0f, 0.15f, 0.15f, 0.35f }, BlendMode::kAdd);
		float glitchOffset3 = std::fmod(endSeqTimer_ * 600.0f + 480.0f, screenH);
		DrawSolidRect({ 0.0f, glitchOffset3 }, { screenW, 8.0f }, { 0.2f, 0.8f, 1.0f, 0.30f }, BlendMode::kAdd);

		// 中央警告カード（ジッター振動）
		float cardW = 760.0f;
		float cardH = 155.0f;
		float cardX = (screenW - cardW) * 0.5f + jitterX;
		float cardY = (screenH - cardH) * 0.5f;

		// カード背景と赤枠
		DrawSolidRect({ cardX, cardY }, { cardW, cardH }, { 0.08f, 0.01f, 0.01f, 0.94f });
		DrawHollowBorder({ cardX, cardY }, { cardW, cardH }, { 1.0f, 0.25f, 0.20f, 0.95f }, 2.5f);
		DrawSolidRect({ cardX, cardY }, { cardW, 4.0f }, { 1.0f, 0.40f, 0.40f, 1.0f });

		// 点滅＆RGB色ズレする「SIGNAL LOST」
		float signalPulse = 0.7f + 0.3f * std::sin(endSeqTimer_ * 30.0f);
		
		// グリッチ文字化（周期的に一瞬化ける）
		const char* signalLostTitle = (std::fmod(endSeqTimer_, 0.12f) < 0.03f) ? "S!GN#L L0ST // ERR" : "SIGNAL LOST";

		// RGBスプリット（シアン影＋赤影で致命的画面乱れ感を演出）
		TextRenderer::GetInstance()->Print(
			"HackGen",
			signalLostTitle,
			screenW * 0.5f + jitterX + 3.0f,
			cardY + 20.0f,
			46.0f,
			{ 0.10f, 0.85f, 1.0f, signalPulse * 0.6f },
			{ 0.5f, 0.0f }
		);
		TextRenderer::GetInstance()->Print(
			"HackGen",
			signalLostTitle,
			screenW * 0.5f + jitterX - 3.0f,
			cardY + 20.0f,
			46.0f,
			{ 1.0f, 0.15f, 0.15f, signalPulse * 0.8f },
			{ 0.5f, 0.0f }
		);
		TextRenderer::GetInstance()->Print(
			"HackGen",
			signalLostTitle,
			screenW * 0.5f + jitterX,
			cardY + 20.0f,
			46.0f,
			{ 1.0f, 0.95f, 0.95f, signalPulse },
			{ 0.5f, 0.0f }
		);

		// 「TELEMETRY DISCONNECTED (遠隔測定切断)」
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"TELEMETRY DISCONNECTED (遠隔測定切断)",
			screenW * 0.5f + jitterX,
			cardY + 76.0f,
			22.0f,
			{ 1.0f, 0.85f, 0.85f, 0.95f },
			{ 0.5f, 0.0f }
		);

		TextRenderer::GetInstance()->Print(
			"HackGen",
			"CARRIER FREQUENCY TERMINATED // SYSTEM FAULT 0x000000FA",
			screenW * 0.5f + jitterX,
			cardY + 110.0f,
			14.0f,
			{ 0.95f, 0.40f, 0.40f, 0.85f },
			{ 0.5f, 0.0f }
		);
	}

	// ------------------------------------------------------------
	// 項目2: システム強制シャットダウン（ブラウン管テレビのプツッ暗転: 0.25秒で鋭く閉鎖）
	// 空白時間を作らず、暗転直後（0.95s）に即座にGameOverSceneへバトンタッチ！
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::GameOver_CrtShutdown) {
		float crtT = std::clamp((endSeqTimer_ - 0.70f) / 0.25f, 0.0f, 1.0f);
		float centerY = screenH * 0.5f;
		float centerX = screenW * 0.5f;

		if (crtT < 0.60f) {
			// Phase 1 (0.00〜0.60): 上下から黒マスクが中央水平線に向かって急激に収束！
			float p = crtT / 0.60f;
			float collapse = 1.0f - std::pow(1.0f - p, 3.0f);
			float halfH = (screenH * 0.5f) * (1.0f - collapse);
			if (halfH < 2.0f) halfH = 2.0f;

			DrawSolidRect({ 0.0f, 0.0f }, { screenW, centerY - halfH }, { 0.0f, 0.0f, 0.0f, 1.0f });
			DrawSolidRect({ 0.0f, centerY + halfH }, { screenW, screenH - (centerY + halfH) }, { 0.0f, 0.0f, 0.0f, 1.0f });

			// 中央水平発光線
			float lineAlpha = 0.9f + 0.1f * std::sin(endSeqTimer_ * 60.0f);
			DrawSolidRect({ 0.0f, centerY - 2.0f }, { screenW, 4.0f }, { 0.90f, 0.95f, 1.0f, lineAlpha }, BlendMode::kAdd);
		} else if (crtT < 0.88f) {
			// Phase 2 (0.60〜0.88): 水平線が左右から急速に収縮！
			float p = (crtT - 0.60f) / 0.28f;
			float shrink = 1.0f - p;
			float halfW = (screenW * 0.5f) * shrink;

			DrawSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.0f, 0.0f, 0.0f, 1.0f });
			DrawSolidRect({ centerX - halfW, centerY - 2.5f }, { halfW * 2.0f, 5.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, BlendMode::kAdd);
		} else {
			// Phase 3 (0.88〜1.00): 「プツッ」と極小の光点が光って暗転完了（即時次シーンへ）
			float p = (crtT - 0.88f) / 0.12f;
			DrawSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.0f, 0.0f, 0.0f, 1.0f });

			if (p < 0.50f) {
				float popAlpha = 1.0f - (p / 0.50f);
				DrawSolidRect({ centerX - 4.0f, centerY - 4.0f }, { 8.0f, 8.0f }, { 1.0f, 1.0f, 1.0f, popAlpha }, BlendMode::kAdd);
			}
		}
	}

	// ------------------------------------------------------------
	// ステージクリア時の巨大ファンファーレUI（大きく「OPERATION CLEARED」演出：指摘1）
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::Clear_HudShutdown) {
		float bannerW = 880.0f;
		float bannerH = 120.0f;
		float bannerX = (screenW - bannerW) * 0.5f;
		float bannerY = screenH * 0.5f - 60.0f;

		// 発光バックグラウンド
		float pulseAlpha = 0.88f + 0.12f * std::sin(endSeqTimer_ * 15.0f);
		DrawSolidRect({ bannerX, bannerY }, { bannerW, bannerH }, { 0.02f, 0.12f, 0.06f, 0.92f });
		DrawHollowBorder({ bannerX, bannerY }, { bannerW, bannerH }, { 0.30f, 1.0f, 0.60f, pulseAlpha }, 3.0f);
		DrawSolidRect({ bannerX, bannerY }, { bannerW, 4.0f }, { 0.60f, 1.0f, 0.80f, 1.0f });

		// 巨大な "OPERATION CLEARED" スタンプ
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"// OPERATION CLEARED //",
			screenW * 0.5f,
			bannerY + 14.0f,
			48.0f,
			{ 0.35f, 1.0f, 0.60f, 1.0f },
			{ 0.5f, 0.0f }
		);

		TextRenderer::GetInstance()->Print(
			"HackGen",
			"ALL PRIMARY OBJECTIVES ACHIEVED // AIRSPACE SECURED (作戦空域完全制圧)",
			screenW * 0.5f,
			bannerY + 74.0f,
			20.0f,
			{ 0.80f, 1.0f, 0.90f, 0.95f },
			{ 0.5f, 0.0f }
		);
	}

	// ------------------------------------------------------------
	// 項目6: 帰還シーケンス（オートパイロット）
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::Clear_AutopilotReturn) {
		float bannerW = 760.0f;
		float bannerH = 80.0f;
		float bannerX = (screenW - bannerW) * 0.5f;
		float bannerY = screenH * 0.5f + 80.0f;

		DrawSolidRect({ bannerX, bannerY }, { bannerW, bannerH }, { 0.02f, 0.08f, 0.04f, 0.85f });
		DrawHollowBorder({ bannerX, bannerY }, { bannerW, bannerH }, { 0.25f, 0.95f, 0.55f, 0.90f }, 1.5f);

		TextRenderer::GetInstance()->Print(
			"HackGen",
			"ENTERING AUTOPILOT MODE... RETURNING TO BASE",
			screenW * 0.5f,
			bannerY + 14.0f,
			24.0f,
			{ 0.40f, 1.0f, 0.70f, 1.0f },
			{ 0.5f, 0.0f }
		);

		TextRenderer::GetInstance()->Print(
			"HackGen",
			"(自動操縦移行... 帰還中)",
			screenW * 0.5f,
			bannerY + 46.0f,
			18.0f,
			{ 0.80f, 0.95f, 0.85f, 0.90f },
			{ 0.5f, 0.0f }
		);
	}

	// ------------------------------------------------------------
	// 項目10, 11: タクティカルマップ移行（デジタル急ズームワイプ＆稼働する動的戦術マップ）
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::Clear_TacticalMapZoom) {
		float mapTimer = endSeqTimer_ - 2.8f; // 0.0s 〜 1.7s

		// 項目11: 動的にスクロールする背景グリッド（稼働感の演出）
		const float gridSpacing = 64.0f;
		float gridScrollX = std::fmod(endSeqTimer_ * 32.0f, gridSpacing);
		float gridScrollY = std::fmod(endSeqTimer_ * 20.0f, gridSpacing);
		Vector4 gridLineCol = { 0.15f, 0.45f, 0.30f, 0.22f };

		for (float gx = -gridSpacing + gridScrollX; gx < screenW + gridSpacing; gx += gridSpacing) {
			DrawSolidRect({ gx, 0.0f }, { 1.0f, screenH }, gridLineCol);
		}
		for (float gy = -gridSpacing + gridScrollY; gy < screenH + gridSpacing; gy += gridSpacing) {
			DrawSolidRect({ 0.0f, gy }, { screenW, 1.0f }, gridLineCol);
		}

		// 戦術マップ走査ライングリッド風オーバーレイ
		float scanPulse = 0.18f + 0.06f * std::sin(endSeqTimer_ * 12.0f);
		DrawSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.01f, 0.08f, 0.04f, scanPulse });

		// 四隅の戦術座標ブラケット
		float bracketLen = 50.0f;
		float margin = 40.0f;
		Vector4 tactCol = { 0.25f, 0.90f, 0.55f, 0.85f };
		DrawSolidRect({ margin, margin }, { bracketLen, 2.0f }, tactCol);
		DrawSolidRect({ margin, margin }, { 2.0f, bracketLen }, tactCol);
		DrawSolidRect({ screenW - margin - bracketLen, margin }, { bracketLen, 2.0f }, tactCol);
		DrawSolidRect({ screenW - margin, margin }, { 2.0f, bracketLen }, tactCol);
		DrawSolidRect({ margin, screenH - margin }, { bracketLen, 2.0f }, tactCol);
		DrawSolidRect({ margin, screenH - margin - bracketLen }, { 2.0f, bracketLen }, tactCol);
		DrawSolidRect({ screenW - margin - bracketLen, screenH - margin }, { bracketLen, 2.0f }, tactCol);
		DrawSolidRect({ screenW - margin, screenH - margin - bracketLen }, { 2.0f, bracketLen }, tactCol);

		// 項目11: リアルタイムGPS・衛星テレメトリ情報（システム稼働感）
		int latNoise = static_cast<int>(std::abs(std::sin(endSeqTimer_ * 14.0f)) * 99.0f);
		int lonNoise = static_cast<int>(std::abs(std::cos(endSeqTimer_ * 18.0f)) * 99.0f);
		char coordBuf[128];
		snprintf(coordBuf, sizeof(coordBuf), "SAT_ORBIT: 420.4KM | LAT: 35.68%02d N | LON: 139.76%02d E | RATE: 10.4 Gbps", latNoise, lonNoise);
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"TACTICAL MAP // SATELLITE ORBIT VIEW [GRID: 44-ALPHA]",
			margin + 10.0f,
			margin + 8.0f,
			16.0f,
			tactCol
		);
		TextRenderer::GetInstance()->Print(
			"HackGen",
			coordBuf,
			margin + 10.0f,
			margin + 28.0f,
			12.0f,
			{ 0.35f, 0.85f, 0.60f, 0.75f }
		);

		// 項目11: 自機アイコン（画面中央）と円形走査レーダー波
		float pX = screenW * 0.5f;
		float pY = screenH * 0.5f;

		// レーダー走査パルス波（外側に広がるリング）
		float pulseCycle = std::fmod(endSeqTimer_ * 1.5f, 1.0f);
		float ringRad = 20.0f + pulseCycle * 140.0f;
		float ringAlpha = (1.0f - pulseCycle) * 0.70f;
		DrawHollowBorder({ pX - ringRad, pY - ringRad }, { ringRad * 2.0f, ringRad * 2.0f }, { 0.20f, 0.95f, 0.55f, ringAlpha }, 1.5f);

		// 自機アイコンシンボル（デルタ翼・十字レティクル）
		DrawSolidRect({ pX - 12.0f, pY - 1.0f }, { 24.0f, 2.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
		DrawSolidRect({ pX - 1.0f, pY - 12.0f }, { 2.0f, 24.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
		DrawHollowBorder({ pX - 8.0f, pY - 8.0f }, { 16.0f, 16.0f }, { 0.40f, 1.0f, 0.70f, 0.95f }, 1.5f);

		// 自機コールサイン
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"▲ PLAYER [MiG-21] (RTB)",
			pX + 16.0f,
			pY - 14.0f,
			12.5f,
			{ 0.40f, 1.0f, 0.70f, 0.90f }
		);

		// 項目10: 移行直後（0.0s〜0.26s）の鋭利な垂直急ズーム＋走査デジタルワイプ
		if (mapTimer < 0.26f) {
			float wipeProg = mapTimer / 0.26f;
			float bandH = screenH * 0.25f;
			float bandY = wipeProg * (screenH + bandH) - bandH;
			DrawSolidRect({ 0.0f, bandY }, { screenW, bandH }, { 0.25f, 1.0f, 0.60f, (1.0f - wipeProg) * 0.75f }, BlendMode::kAdd);
			DrawSolidRect({ 0.0f, bandY + bandH * 0.5f }, { screenW, 4.0f }, { 0.90f, 1.0f, 0.95f, (1.0f - wipeProg) * 0.95f }, BlendMode::kAdd);
		}

		// 3.5s に達した瞬間、SECTOR CLEARED のスタンプがドンッと押される！
		if (endSeqTimer_ >= 3.5f) {
			float stampT = std::clamp((endSeqTimer_ - 3.5f) / 0.14f, 0.0f, 1.0f);
			// スケール 2.2 -> 1.0 への急激な衝撃落下
			float stampScale = 1.0f + 1.2f * (1.0f - stampT) * (1.0f - stampT);

			float boxW = 540.0f * stampScale;
			float boxH = 130.0f * stampScale;
			float boxX = (screenW - boxW) * 0.5f;
			float boxY = (screenH - boxH) * 0.5f;

			// スタンプ外枠（重厚な2重四角枠）
			Vector4 stampCol = { 0.95f, 0.25f, 0.18f, 0.95f };
			DrawHollowBorder({ boxX, boxY }, { boxW, boxH }, stampCol, 4.0f * stampScale);
			DrawHollowBorder({ boxX + 6.0f * stampScale, boxY + 6.0f * stampScale }, { boxW - 12.0f * stampScale, boxH - 12.0f * stampScale }, stampCol, 1.5f * stampScale);

			// スタンプ文字
			TextRenderer::GetInstance()->Print(
				"HackGen",
				"SECTOR CLEARED",
				screenW * 0.5f,
				boxY + 22.0f * stampScale,
				44.0f * stampScale,
				stampCol,
				{ 0.5f, 0.0f }
			);

			TextRenderer::GetInstance()->Print(
				"HackGen",
				"(セクター制圧)",
				screenW * 0.5f,
				boxY + 74.0f * stampScale,
				22.0f * stampScale,
				{ 1.0f, 0.85f, 0.80f, 0.95f },
				{ 0.5f, 0.0f }
			);
		}

		// 項目4: 戦術マップとリザルト画面（デスクトップ）の視覚的繋がり（転送ブリッジ）
		if (mapTimer >= 0.60f) {
			float transBannerW = 740.0f;
			float transBannerH = 52.0f;
			float transBannerX = (screenW - transBannerW) * 0.5f;
			float transBannerY = screenH - 100.0f;
			DrawSolidRect({ transBannerX, transBannerY }, { transBannerW, transBannerH }, { 0.02f, 0.08f, 0.16f, 0.92f });
			DrawHollowBorder({ transBannerX, transBannerY }, { transBannerW, transBannerH }, { 0.25f, 0.85f, 1.0f, 0.95f }, 1.5f);
			TextRenderer::GetInstance()->Print(
				"HackGen",
				"TRANSFERRING TELEMETRY TO TACTICAL TERMINAL // DEBRIEF READY...",
				screenW * 0.5f,
				transBannerY + 16.0f,
				18.0f,
				{ 0.40f, 0.95f, 1.0f, 1.0f },
				{ 0.5f, 0.0f }
			);
		}
	}
}


