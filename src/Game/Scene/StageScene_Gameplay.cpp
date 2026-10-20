#include "StageSceneInternal.h"

using namespace MyMath;
using namespace StageSceneInternal;

// ============================================================
// プレイヤー入力・操縦更新
// ============================================================
void StageScene::UpdateFlightControls(float gameDeltaTime) {
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
			mouseAimController_.CalculateSteeringInput(
				flightModel_.GetOrientation(),
				kDeltaTime,
				finalPitch, finalRoll, finalYaw
			);
		} else {
			// === 手動操縦モード ===
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
}

// ============================================================
// 射撃・兵装入力更新
// ============================================================
void StageScene::UpdateCombatFiring(float gameDeltaTime) {
	Input* input = Input::GetInstance();
	isFiringThisFrame_ = false;

	if (input && input->PushMouse(0) && !isMissionCleared_ && !isMissionFailed_ && sortieIntroTimer_ >= sortieIntroDuration_) {
		int ammoBefore = gunpod_.GetCurrentAmmo();
		gunpod_.Fire();
		if (gunpod_.GetCurrentAmmo() < ammoBefore) {
			isFiringThisFrame_ = true;
			Vector3 aircraftPos = flightModel_.GetPosition();
			Vector3 aircraftForward = flightModel_.GetForwardDirection();
			Vector3 aircraftVelocity = flightModel_.GetVelocity();

			Vector3 firePos = Add(aircraftPos, Multiply(8.0f, aircraftForward));
			Vector3 fireDir = aircraftForward;

			float muzzleSpeed = 500.0f;
			Vector3 muzzleVelocity = Multiply(muzzleSpeed, fireDir);
			Vector3 bulletVelocity = Add(aircraftVelocity, muzzleVelocity);

			bulletManager_.SpawnBullet(firePos, bulletVelocity, 10.0f);

			// --- 薬莢の放出エフェクト ---
			{
				Vector3 velocity = flightModel_.GetVelocity();
				Vector3 right = flightModel_.GetRightDirection();
				Vector3 up = flightModel_.GetUpDirection();
				Vector3 forward = flightModel_.GetForwardDirection();

				Vector3 casingPos = Add(aircraftPos, Multiply(-2.0f, forward));
				casingPos = Add(casingPos, Multiply(-0.4f, right));
				casingPos = Add(casingPos, Multiply(-0.5f, up));

				ParticleParameters casingParams;
				Vector3 localMinVel = Add(Add(Multiply(-2.0f, right), Multiply(-3.0f, up)), Multiply(-18.0f, forward));
				Vector3 localMaxVel = Add(Add(Multiply(-0.5f, right), Multiply(-1.0f, up)), Multiply(-10.0f, forward));
				
				casingParams.minVelocity = Add(velocity, localMinVel);
				casingParams.maxVelocity = Add(velocity, localMaxVel);
				casingParams.acceleration = { 0.0f, -9.8f, 0.0f };
				casingParams.minLifeTime = 1.2f;
				casingParams.maxLifeTime = 1.8f;
				casingParams.minScale = 0.12f;
				casingParams.maxScale = 0.18f;
				casingParams.endScale = 0.08f;
				casingParams.fadeOut = true;
				casingParams.minColor = { 0.95f, 0.75f, 0.15f, 1.0f };
				casingParams.maxColor = { 1.0f, 0.85f, 0.30f, 1.0f };
				casingParams.minRotation = 0.0f;
				casingParams.maxRotation = 6.283185f;
				casingParams.minRotationSpeed = -15.0f;
				casingParams.maxRotationSpeed = 15.0f;
				casingParams.randomPositionRange = 0.0f;

				ParticleManager::GetInstance()->Emit("Casing", casingPos, casingParams, 1);
			}

			// マズルフラッシュの間引き（2発に1回エフェクトを発生）
			muzzleFlashCount_++;
			if (muzzleFlashCount_ % 2 == 0) {
				EffectManager::GetInstance()->EmitMuzzleFlash(firePos, fireDir);
				muzzleFlashTimer_ = 0.08f;

				if (CameraManager::GetInstance()->GetActiveCamera()) {
					CameraManager::GetInstance()->GetActiveCamera()->Shake(0.1f, 0.5f);
				}

				muzzleFlashRandomScale_ = 0.8f + (rand() % 40) / 100.0f;
				muzzleFlashRandomRoll_ = (rand() % 628) / 100.0f;
				muzzleFlashRandomAlpha_ = 0.8f + (rand() % 20) / 100.0f;

				PointLight* pLight = Object3dCommon::GetInstance()->GetPointLightData();
				if (pLight) {
					pLight->position = firePos;
					pLight->color = { 1.0f, 0.4f, 0.05f, 1.0f };
					pLight->intensity = 30.0f;
					pLight->radius = 30.0f;
					pLight->decay = 2.0f;
				}
			}
		}
	}

	if (muzzleFlashTimer_ > 0.0f) {
		muzzleFlashTimer_ -= gameDeltaTime;
	}

	// 動的ライトの減衰
	PointLight* pLight = Object3dCommon::GetInstance()->GetPointLightData();
	if (pLight && pLight->intensity > 0.0f) {
		pLight->intensity -= 200.0f * gameDeltaTime;
		if (pLight->intensity < 0.0f) { pLight->intensity = 0.0f; }
	}
}

// ============================================================
// 機体物理・翼端エフェクト更新
// ============================================================
void StageScene::UpdateFlightAndEffects(float gameDeltaTime) {
	// 環境の更新
	environmentManager_.Update(CameraManager::GetInstance()->GetActiveCamera(), gameDeltaTime);

	// 飛行モデルの更新
	flightModel_.Update(gameDeltaTime);

	// 部位破壊イベントのチェックと破片放出
	CheckPartDestructionEvents();

	// ビジュアルモデルの更新
	aircraftVisualModel_.Update(flightModel_.GetWorldMatrix(), gameDeltaTime);

	// 破片オブジェクト群の物理シミュレーション更新
	DebrisManager::GetInstance()->Update(gameDeltaTime);

#ifdef _DEBUG
	TerrainSyncManager::GetInstance()->Update();
#endif

	// --- 翼端ボルテックスエフェクトの発生 ---
	float currentG = flightModel_.GetCurrentG();
	Vector3 aircraftPos = flightModel_.GetPosition();
	Vector3 right = flightModel_.GetRightDirection();
	Vector3 forward = flightModel_.GetForwardDirection();

	Vector3 leftWingTip = Add(Add(aircraftPos, Multiply(-6.0f, right)), Multiply(-2.0f, forward));
	Vector3 rightWingTip = Add(Add(aircraftPos, Multiply(6.0f, right)), Multiply(-2.0f, forward));

	if (!isGameOver_ && currentG > 3.0f && flightModel_.GetSpeed() > 50.0f) {
		float gFactor = (currentG - 3.0f) / 3.0f;
		if (gFactor < 0.0f) { gFactor = 0.0f; }
		if (gFactor > 1.0f) { gFactor = 1.0f; }

		Vector4 baseColor = { 0.95f, 0.98f, 1.0f, 0.35f * gFactor };

		ParticleParameters params;
		params.minVelocity = { 0.0f, 0.0f, 0.0f };
		params.maxVelocity = { 0.0f, 0.0f, 0.0f };
		params.randomPositionRange = 0.0f;
		params.minColor = baseColor;
		params.maxColor = baseColor;
		params.minLifeTime = 0.4f;
		params.maxLifeTime = 0.4f;
		params.minScale = 0.08f;
		params.maxScale = 0.08f;
		params.endScale = 0.12f;
		params.fadeOut = true;
		params.minRotation = 0.0f;
		params.maxRotation = 0.0f;
		params.minRotationSpeed = 0.0f;
		params.maxRotationSpeed = 0.0f;
		params.isStretched = true;
		params.stretchFactor = 12.5f;

		if (hasLastWingTips_) {
			Vector3 deltaL = Subtract(leftWingTip, lastLeftWingTip_);
			float distL = Length(deltaL);
			Vector3 deltaR = Subtract(rightWingTip, lastRightWingTip_);

			int steps = static_cast<int>(std::ceil(distL / 0.25f));
			if (steps < 1) { steps = 1; }
			if (steps > 40) { steps = 40; }

			Vector3 stepDeltaL = Multiply(1.0f / static_cast<float>(steps), deltaL);
			Vector3 stepDeltaR = Multiply(1.0f / static_cast<float>(steps), deltaR);

			for (int i = 0; i < steps; ++i) {
				float t = static_cast<float>(i) / static_cast<float>(steps);
				Vector3 interpL = Lerp(lastLeftWingTip_, leftWingTip, t);
				Vector3 interpR = Lerp(lastRightWingTip_, rightWingTip, t);

				params.stretchDir = stepDeltaL;
				ParticleManager::GetInstance()->Emit("Vortex", interpL, params, 1);

				params.stretchDir = stepDeltaR;
				ParticleManager::GetInstance()->Emit("Vortex", interpR, params, 1);
			}
		} else {
			params.isStretched = false;
			ParticleManager::GetInstance()->Emit("Vortex", leftWingTip, params, 1);
			ParticleManager::GetInstance()->Emit("Vortex", rightWingTip, params, 1);
		}

		lastLeftWingTip_ = leftWingTip;
		lastRightWingTip_ = rightWingTip;
		hasLastWingTips_ = true;
	} else {
		hasLastWingTips_ = false;
	}
}

// ============================================================
// 戦闘・敵・ミッション・当たり判定更新
// ============================================================
void StageScene::UpdateCombatAndMission(float gameDeltaTime) {
	if (gameDeltaTime > 0.0001f) {
		gunpod_.Update(gameDeltaTime);
		bulletManager_.Update(gameDeltaTime);
		enemyManager_.Update(gameDeltaTime);
	}
	EffectManager::GetInstance()->Update();

	// ミッション目標 & トリガー評価
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

	// 衝突判定
	if (!isGameOver_) {
		collisionSystem_.ClearAll();
		collisionSystem_.Register(&playerBody_);

		for (auto& bullet : bulletManager_.GetBullets()) {
			collisionSystem_.Register(&bullet);
		}
		for (auto* enemy : enemyManager_.GetAliveEnemies()) {
			collisionSystem_.Register(enemy);
		}
		for (auto* groundEnemy : enemyManager_.GetAliveGroundEnemies()) {
			collisionSystem_.Register(groundEnemy);
		}
		collisionSystem_.UpdateAll();
	}

	// クリア判定
	if (!isMissionCleared_ && !isMissionFailed_ && enemyManager_.IsAllDestroyed()) {
		TriggerGameClear();
	}

	// 機体壊滅判定
	if (!isMissionFailed_ && flightModel_.GetAirframe().IsDestroyed()) {
		TriggerGameOver();
	}
}

// ============================================================
// カメラ・スカイボックス更新
// ============================================================
void StageScene::UpdateCamera(float gameDeltaTime) {
	Vector3 pos = flightModel_.GetPosition();
	Vector3 euler = PlayerCamera::QuaternionToEuler(flightModel_.GetOrientation());

	aircraftObject_->SetTranslate(pos);
	aircraftObject_->SetRotate(euler);
	aircraftObject_->Update();

	if (sortieIntroTimer_ < sortieIntroDuration_) {
		sortieIntroTimer_ += gameDeltaTime;
		combatBootSequence_.Update(gameDeltaTime);
		float introProg = std::clamp(sortieIntroTimer_ / sortieIntroDuration_, 0.0f, 1.0f);

		float boostSpeed = 22.0f + 28.0f * (1.0f - std::exp(-introProg * 2.2f));
		flightModel_.SetVelocity(Multiply(boostSpeed, flightModel_.GetForwardDirection()));

		Vector3 forward = flightModel_.GetForwardDirection();
		Vector3 right = flightModel_.GetRightDirection();
		Vector3 up = flightModel_.GetUpDirection();

		Vector3 camPos{};
		Vector3 lookTarget{};

		if (sortieIntroTimer_ < 2.30f) {
			float sideT = sortieIntroTimer_ / 2.30f;
			float forwardOffset = 6.0f - 2.0f * sideT;
			Vector3 camOffset = Add(Add(Multiply(forwardOffset, forward), Multiply(13.0f, right)), Multiply(-0.6f, up));
			camPos = Add(pos, camOffset);
			lookTarget = Add(pos, Multiply(2.0f, forward));
		} else {
			float orbitDuration = (std::max)(0.1f, sortieIntroDuration_ - 2.30f);
			float orbitProg = std::clamp((sortieIntroTimer_ - 2.30f) / orbitDuration, 0.0f, 1.0f);
			float smoothOrbit = orbitProg * orbitProg * (3.0f - 2.0f * orbitProg);

			float angle = (3.14159265f * 0.50f) + smoothOrbit * (3.14159265f * 0.50f);
			float dist = 14.0f + 8.0f * smoothOrbit;
			float height = -0.6f + 8.6f * smoothOrbit;

			float localX = dist * std::sin(angle);
			float localZ = -dist * std::cos(angle);
			Vector3 camOffset = Add(Add(Multiply(localZ, forward), Multiply(localX, right)), Multiply(height, up));
			camPos = Add(pos, camOffset);

			Vector3 endLookTarget = Add(pos, Add(Multiply(40.0f, forward), Multiply(4.8f, up)));
			Vector3 startLookTarget = Add(pos, Multiply(2.0f, forward));
			lookTarget = Lerp(startLookTarget, endLookTarget, smoothOrbit);
		}

		playerCamera_.SnapTo(camPos, lookTarget);
	} else if (endSequence_ == StageEndSequence::Clear_TacticalMapZoom) {
		float zoomProg = std::clamp((endSeqTimer_ - 2.8f) / 1.5f, 0.0f, 1.0f);
		float easeZoom = zoomProg * zoomProg;
		Vector3 mapCamPos = { pos.x, pos.y + 25.0f + 1600.0f * easeZoom, pos.z };
		Vector3 mapLookTarget = { pos.x, pos.y, pos.z + 0.1f };
		playerCamera_.SnapTo(mapCamPos, mapLookTarget);
	} else {
		playerCamera_.Update(gameDeltaTime, &flightModel_, &mouseAimController_, mouseAimEnabled_);
	}

	if (skybox_) {
		skybox_->Update();
	}
}

// ============================================================
// デバッグUI & レティクルスクリーン投影
// ============================================================
void StageScene::UpdateDebugAndStatusUI(float gameDeltaTime) {
	(void)gameDeltaTime;
#ifdef USE_IMGUI
	Vector3 pos = flightModel_.GetPosition();
	Input* input = Input::GetInstance();

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

	ImGui::Separator();
	ImGui::Text("=== Controls ===");
	ImGui::Text("Mouse: Aim direction (PID)");
	ImGui::Text("LShift/LCtrl: Pitch | A/D: Roll | Q/E: Yaw (override)");
	ImGui::Text("W/S: Throttle");
	ImGui::Text("F: Flaps | B: AirBrake");
	ImGui::Text("M: Mouse Aim [%s]", mouseAimEnabled_ ? "ON" : "OFF");
	ImGui::Text("I: Instructor [%s]", flightInstructor_.IsEnabled() ? "ON" : "OFF");
	ImGui::Text("P: Cursor Lock [%s]", (input && input->IsCursorLocked()) ? "LOCKED" : "FREE");

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

			auto projectToScreen = [&](const Vector3& worldPos, float& outX, float& outY) -> bool {
				float cX = worldPos.x * vpMat.m[0][0] + worldPos.y * vpMat.m[1][0] + worldPos.z * vpMat.m[2][0] + vpMat.m[3][0];
				float cY = worldPos.x * vpMat.m[0][1] + worldPos.y * vpMat.m[1][1] + worldPos.z * vpMat.m[2][1] + vpMat.m[3][1];
				float cW = worldPos.x * vpMat.m[0][3] + worldPos.y * vpMat.m[1][3] + worldPos.z * vpMat.m[2][3] + vpMat.m[3][3];
				if (cW <= 0.001f) { return false; }
				outX = (cX / cW * 0.5f + 0.5f) * screenW;
				outY = (-cY / cW * 0.5f + 0.5f) * screenH;
				return true;
			};

			// マウスエイム目標レティクル
			Vector3 targetDir = mouseAimController_.GetTargetDirection();
			Vector3 aimWorldPos = Add(aircraftPos, Multiply(500.0f, targetDir));
			float cx, cy;
			if (projectToScreen(aimWorldPos, cx, cy)) {
				float crossSize = 15.0f;
				float dotRadius = 3.0f;
				ImU32 reticleColor = IM_COL32(0, 255, 100, 220);
				ImU32 reticleColorDark = IM_COL32(0, 180, 70, 150);

				drawList->AddLine(ImVec2(cx - crossSize, cy), ImVec2(cx - 5.0f, cy), reticleColor, 2.0f);
				drawList->AddLine(ImVec2(cx + 5.0f, cy), ImVec2(cx + crossSize, cy), reticleColor, 2.0f);
				drawList->AddLine(ImVec2(cx, cy - crossSize), ImVec2(cx, cy - 5.0f), reticleColor, 2.0f);
				drawList->AddLine(ImVec2(cx, cy + 5.0f), ImVec2(cx, cy + crossSize), reticleColor, 2.0f);
				drawList->AddCircle(ImVec2(cx, cy), dotRadius, reticleColor, 12, 2.0f);
				drawList->AddCircle(ImVec2(cx, cy), crossSize + 5.0f, reticleColorDark, 24, 1.0f);

				// 機首方向マーカー
				Vector3 forward = flightModel_.GetForwardDirection();
				Vector3 noseWorldPos = Add(aircraftPos, Multiply(500.0f, forward));
				float nx, ny;
				if (projectToScreen(noseWorldPos, nx, ny)) {
					float noseSize = 8.0f;
					ImU32 noseColor = IM_COL32(255, 255, 255, 200);
					drawList->AddLine(ImVec2(nx - noseSize, ny - noseSize), ImVec2(nx + noseSize, ny + noseSize), noseColor, 2.0f);
					drawList->AddLine(ImVec2(nx + noseSize, ny - noseSize), ImVec2(nx - noseSize, ny + noseSize), noseColor, 2.0f);

					ImU32 lineColor = IM_COL32(255, 255, 255, 60);
					drawList->AddLine(ImVec2(cx, cy), ImVec2(nx, ny), lineColor, 1.0f);
				}
			}

			// 1. 空中敵マーカー
			for (auto* enemy : enemyManager_.GetAliveEnemies()) {
				float ex, ey;
				Vector3 enemyPos = enemy->GetPosition();
				if (projectToScreen(enemyPos, ex, ey)) {
					float dist = MyMath::Length(MyMath::Subtract(enemyPos, aircraftPos));
					float diamondSize = 9.0f;
					ImU32 airMarkerColor = IM_COL32(255, 60, 60, 220);

					drawList->AddQuad(
						ImVec2(ex, ey - diamondSize),
						ImVec2(ex + diamondSize, ey),
						ImVec2(ex, ey + diamondSize),
						ImVec2(ex - diamondSize, ey),
						airMarkerColor, 1.8f
					);

					char distBuf[32];
					snprintf(distBuf, sizeof(distBuf), "%dm", static_cast<int>(dist));
					drawList->AddText(ImVec2(ex + diamondSize + 3.0f, ey - 7.0f), airMarkerColor, distBuf);
				}
			}

			// 2. 地上目標マーカー
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

					float corner = 4.0f;
					drawList->AddLine(ImVec2(gx - boxSize, gy - boxSize), ImVec2(gx - boxSize + corner, gy - boxSize), groundColor, 1.8f);
					drawList->AddLine(ImVec2(gx - boxSize, gy - boxSize), ImVec2(gx - boxSize, gy - boxSize + corner), groundColor, 1.8f);
					drawList->AddLine(ImVec2(gx + boxSize, gy - boxSize), ImVec2(gx + boxSize - corner, gy - boxSize), groundColor, 1.8f);
					drawList->AddLine(ImVec2(gx + boxSize, gy - boxSize), ImVec2(gx + boxSize, gy - boxSize + corner), groundColor, 1.8f);
					drawList->AddLine(ImVec2(gx - boxSize, gy + boxSize), ImVec2(gx - boxSize + corner, gy + boxSize), groundColor, 1.8f);
					drawList->AddLine(ImVec2(gx - boxSize, gy + boxSize), ImVec2(gx - boxSize, gy + boxSize - corner), groundColor, 1.8f);
					drawList->AddLine(ImVec2(gx + boxSize, gy + boxSize), ImVec2(gx + boxSize - corner, gy + boxSize), groundColor, 1.8f);
					drawList->AddLine(ImVec2(gx + boxSize, gy + boxSize), ImVec2(gx + boxSize, gy + boxSize - corner), groundColor, 1.8f);

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

	// === ゲーム情報UI ===
	ImGui::Begin("Mission Status");
	
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
}

// ============================================================
// ポストエフェクト & 戦闘HUD更新
// ============================================================
void StageScene::UpdatePostEffects(float deltaTime) {
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->ClearActiveEffects();

	// 1. ナイトビジョン (NVD) モード
	if (isNvdActive_) {
		ActivePostEffect amp;
		amp.type = PostEffectType::kLightAmp;
		amp.intensity = 2.5f;
		postEffect->AddActiveEffect(amp);

		ActivePostEffect tint;
		tint.type = PostEffectType::kColorTint;
		tint.colorR = 0.1f;
		tint.colorG = 0.95f;
		tint.colorB = 0.2f;
		postEffect->AddActiveEffect(tint);

		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = 3.0f;
		bloom.dirX = 1.2f;
		bloom.dirY = 0.75f;
		postEffect->AddActiveEffect(bloom);

		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.12f;
		postEffect->AddActiveEffect(noise);

		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.kernelSize = 0;
		scan.intensity = 0.15f;
		scan.dirX = 600.0f;
		scan.dirY = 5.0f;
		postEffect->AddActiveEffect(scan);

		ActivePostEffect ab;
		ab.type = PostEffectType::kChromaticAberration;
		ab.intensity = 0.015f;
		postEffect->AddActiveEffect(ab);

		ActivePostEffect dist;
		dist.type = PostEffectType::kLensDistortion;
		dist.intensity = 0.18f;
		postEffect->AddActiveEffect(dist);
	} else if (!isGameOver_) {
		float bootBloom = 2.0f;
		float bootScan = 0.030f;
		float bootAberration = 0.004f;
		float bootVignette = 0.40f;

		if (sortieIntroTimer_ < sortieIntroDuration_) {
			bootVignette = 0.55f;

			if (sortieIntroTimer_ < 0.85f) {
				bootBloom = 0.0f;
				bootScan = 0.0f;
				bootAberration = 0.0f;
			} else if (sortieIntroTimer_ < 1.35f) {
				float t = (sortieIntroTimer_ - 0.85f) / 0.50f;
				bootScan = 0.08f * (1.0f - t * 0.4f);
				bootAberration = 0.012f * (1.0f - t * 0.5f);
				bootBloom = 2.4f;
			} else if (sortieIntroTimer_ < 3.40f) {
				bootBloom = 2.2f;
				bootScan = 0.040f;
				bootAberration = 0.006f;
			} else if (sortieIntroTimer_ >= 4.55f && sortieIntroTimer_ <= 4.80f) {
				float flashT = (sortieIntroTimer_ - 4.55f) / 0.25f;
				float pulse = std::sin(flashT * 3.14159f);
				bootBloom = 2.2f + 1.2f * pulse;
				bootAberration = 0.015f * pulse;
			}
		}

		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = bootBloom;
		bloom.dirX = 1.15f;
		bloom.dirY = 0.70f;
		postEffect->AddActiveEffect(bloom);

		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.kernelSize = 0;
		scan.intensity = bootScan;
		scan.dirX = 640.0f;
		scan.dirY = 3.0f;
		postEffect->AddActiveEffect(scan);

		ActivePostEffect ab;
		ab.type = PostEffectType::kChromaticAberration;
		ab.intensity = bootAberration;
		postEffect->AddActiveEffect(ab);

		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = bootVignette;
		postEffect->AddActiveEffect(vig);
	} else if (isGameOver_) {
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

	// 2. 環境の更新
	if (!isNvdActive_) {
		environmentManager_.Update(CameraManager::GetInstance()->GetActiveCamera(), kDeltaTime);
	} else {
		environmentManager_.Update(nullptr, kDeltaTime);
	}

	// 3. Gフォースによるビネット
	float blackout = flightModel_.GetBlackoutFactor();
	float redout = flightModel_.GetRedoutFactor();
	if (blackout > 0.0f || redout > 0.0f) {
		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = blackout * 5.0f;
		vig.dirX = redout * 1.5f;
		vig.colorR = 0.9f;
		vig.colorG = 0.05f;
		vig.colorB = 0.05f;
		postEffect->AddActiveEffect(vig);
	}

	// 4. 戦闘HUDの更新
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

	hud_.Update(deltaTime, hudData);
}
