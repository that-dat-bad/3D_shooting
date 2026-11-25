#include "StageSceneInternal.h"

using namespace MyMath;
using namespace StageSceneInternal;

// ============================================================
// プレイヤー入力・操縦更新
// ============================================================
void StageScene::UpdateFlightControls(float gameDeltaTime) {
	// ============================
	// マウスポインター制御
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
	// プレイヤー入力 → FlightModel
	// ============================
	Input* input = Input::GetInstance();
	float finalPitch = 0.0f;
	float finalRoll = 0.0f;
	float finalYaw = 0.0f;

	if (sortieIntroTimer_ < sortieIntroDuration_) {
		flightModel_.SetControlInput(0.0f, 0.0f, 0.0f);
		if (isFreeFlightMode_) {
			flightModel_.SetThrottle(throttle_);
			flightModel_.SetThrottleInput(throttle_);
		} else {
			flightModel_.SetThrottleInput(0.0f);
		}
		return;
	}

	// ゲームオーバー中・クリア演出中・終了シーケンス中は操作遮断
	if (isGameOver_ || isMissionCleared_ || endSequence_ != StageEndSequence::None) {
		flightModel_.SetControlInput(0.0f, 0.0f, 0.0f);
		return;
	}

	if (!isGameOver_) {
		if (isParticleDemoMode_ && particleDemo_.IsActive()) {
			ParticleDemoController::DemoContext ctx{};
			ctx.dt = gameDeltaTime;
			ctx.position = flightModel_.GetPosition();
			ctx.forward = flightModel_.GetForwardDirection();
			ctx.speed = flightModel_.GetSpeed();
			ctx.normalThrottleLimit = (std::max)(0.1f, flightModel_.GetNormalThrottleLimit());
			ctx.wepThrottleLimit = (std::max)(ctx.normalThrottleLimit, flightModel_.GetWepThrottleLimit());
			ctx.enemyManager = &enemyManager_;
			ctx.missileLocked = missileManager_.IsLocked();
			ctx.missileReadyToFire = (missileLauncher_ && missileLauncher_->IsReadyToFire());
			ctx.remainingMissiles = missileLauncher_ ? missileLauncher_->GetCurrentMissiles() : 0;

			particleDemo_.Update(ctx);
			const auto& demoOut = particleDemo_.GetOutput();
			throttle_ = std::clamp(demoOut.throttle, 0.0f, ctx.wepThrottleLimit);
			flightModel_.SetThrottleInput(throttle_);
			mouseAimEnabled_ = true;
			mouseAimController_.SetEnabled(true);
			mouseAimController_.SetTargetDirection(demoOut.desiredDirection);
			mouseAimController_.CalculateSteeringInput(
				flightModel_.GetOrientation(),
				kDeltaTime,
				finalPitch, finalRoll, finalYaw
			);
			flightModel_.SetControlInput(finalPitch, finalRoll, finalYaw);
			flightModel_.SetFlapInput(demoOut.flapInput);
			flightModel_.SetAirBrakeInput(demoOut.airBrakeInput);
			if (demoOut.triggerFlare) {
				countermeasures_.TriggerSalvo();
			}
			if (input && input->TriggerKey(DIK_P)) {
				SetPaused(true);
			}
			return;
		}

		const auto& missionManager = MissionManager::GetInstance();
		const bool allowSteering = missionManager.IsPlayerSteeringAllowed();
		const bool allowThrottle = missionManager.IsPlayerThrottleAllowed();

		// --- スロットル (War Thunder風 100%ディテント + WEP制御) ---
		if (allowThrottle) {
			const float normalLimit = (std::max)(0.1f, flightModel_.GetNormalThrottleLimit());
			const float wepLimit = (std::max)(normalLimit, flightModel_.GetWepThrottleLimit());
			const bool hasWep = flightModel_.HasWep();

			constexpr float kDryThrottleRate = 0.8f;
			constexpr float kWepDetentDuration = 0.22f;
			constexpr float kWepEpsilon = 0.005f;

			const bool pushW = input->PushKey(DIK_W);
			const bool triggerW = input->TriggerKey(DIK_W);
			const bool pushS = input->PushKey(DIK_S);
			const bool triggerS = input->TriggerKey(DIK_S);

			if (pushS) {
				wepDetentTimer_ = 0.0f;
				if (triggerS && throttle_ > normalLimit + kWepEpsilon) {
					throttle_ = normalLimit;
				} else {
					throttle_ -= kDryThrottleRate * gameDeltaTime;
				}
			} else if (pushW) {
				if (throttle_ < normalLimit - 0.001f) {
					throttle_ = (std::min)(throttle_ + kDryThrottleRate * gameDeltaTime, normalLimit);
					wepDetentTimer_ = 0.0f;
				} else if (hasWep) {
					const float wepRate = (std::max)(kDryThrottleRate, (wepLimit - normalLimit) * 2.0f);
					if (throttle_ <= normalLimit + kWepEpsilon) {
						if (triggerW) {
							wepDetentTimer_ = kWepDetentDuration;
						} else {
							wepDetentTimer_ += gameDeltaTime;
						}
						if (wepDetentTimer_ >= kWepDetentDuration) {
							throttle_ += wepRate * gameDeltaTime;
						}
					} else {
						throttle_ += wepRate * gameDeltaTime;
					}
				}
			} else {
				wepDetentTimer_ = 0.0f;
				if (holdWForWep_ && throttle_ > normalLimit) {
					throttle_ = normalLimit;
				}
			}

			throttle_ = std::clamp(throttle_, 0.0f, hasWep ? wepLimit : normalLimit);
		} else {
			wepDetentTimer_ = 0.0f;
			throttle_ += (0.65f - throttle_) * std::clamp(gameDeltaTime * 2.5f, 0.0f, 1.0f);
		}
		flightModel_.SetThrottleInput(throttle_);

		// --- マウスエイム ON/OFF トグル ---
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

		// --- ポーズメニュー呼び出し ---
		if (input->TriggerKey(DIK_P)) {
			SetPaused(true);
			return;
		}

		// --- ナイトビジョン ON/OFF トグル ---
		if (input->TriggerKey(DIK_N)) {
			isNvdActive_ = !isNvdActive_;
		}

		// --- チャフ＆フレア（カウンターメジャー）展開 ---
		if (input->TriggerKey(DIK_X)) {
			countermeasures_.TriggerSalvo();
		}

		// --- ベイルアウト / 緊急脱出 ---
		if (input->TriggerKey(DIK_J)) {
			const bool isClearDemo =
				MissionManager::GetInstance().GetCurrentMission().name.find("CLEAR DEMO") != std::string::npos;
			if (isClearDemo) {
				TriggerGameClear();
			} else {
				isBailout_ = true;
				TriggerGameOver();
			}
			return;
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

		// フリーカメラ時の画面外エイム追従を停止
		bool pauseMouseAim = false;
		if (mouseAimEnabled_ && playerCamera_.GetFreeViewActive()) {
			Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
			if (cam) {
				const Matrix4x4& mat = cam->GetWorldMatrix();
				Vector3 camForward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
				Vector3 targetDir = mouseAimController_.GetTargetDirection();
				
				// 視界から一定以上外れているか判定
				if (MyMath::Dot(camForward, targetDir) < 0.8f) {
					pauseMouseAim = true;
				}
			}
		}

		// --- 操舵入力の決定 ---
		finalPitch = 0.0f;
		finalRoll = 0.0f;
		finalYaw = 0.0f;

		if (allowSteering) {
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

			// 失速中はエイム目標を機首方向へ戻す
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

				// インストラクター ON/OFF
				if (input->TriggerKey(DIK_I)) {
					flightInstructor_.ToggleEnabled();
				}

				// インストラクターによる補正
				flightInstructor_.ApplyCorrection(
					manualPitch, manualRoll, manualYaw,
					flightModel_.GetOrientation(),
					finalPitch, finalRoll, finalYaw
				);
			}
		} else {
			if (mouseAimEnabled_) {
				mouseAimController_.ResetToDirection(flightModel_.GetForwardDirection());
			}
			flightInstructor_.ApplyCorrection(
				0.0f, 0.0f, 0.0f,
				flightModel_.GetOrientation(),
				finalPitch, finalRoll, finalYaw
			);
		}

		flightModel_.SetControlInput(finalPitch, finalRoll, finalYaw);

		// --- フラップ / エアブレーキ ---
		flightModel_.SetFlapInput(allowThrottle && input->PushKey(DIK_F));
		flightModel_.SetAirBrakeInput(allowThrottle && input->PushKey(DIK_B));
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
	const bool allowWeapon = MissionManager::GetInstance().IsPlayerWeaponAllowed();

	if (weaponSwitchCooldown_ > 0.0f) {
		weaponSwitchCooldown_ = (std::max)(0.0f, weaponSwitchCooldown_ - gameDeltaTime);
	}

	// デモ制御時またはマウススクロールホイールで機関砲 / ミサイルを切り替え
	const bool demoControlled = (isParticleDemoMode_ && particleDemo_.IsActive());
	const auto& demoOut = particleDemo_.GetOutput();
	if (demoControlled) {
		activeWeaponMode_ = demoOut.useMissileMode ? WeaponMode::Missile : WeaponMode::Gun;
	} else if (allowWeapon && IsGameplayActive() && !isPaused_ && !combatBootSequence_.IsWaitingForSpace() && input) {
		const Input::MouseMove mouseMove = input->GetMouseMove();
		if (mouseMove.lZ != 0 && weaponSwitchCooldown_ <= 0.0f) {
			activeWeaponMode_ = (activeWeaponMode_ == WeaponMode::Gun) ? WeaponMode::Missile : WeaponMode::Gun;
			weaponSwitchCooldown_ = 0.12f;
		}
	}

	const bool canFireWeapon = allowWeapon && !isMissionCleared_ && !isMissionFailed_ &&
		sortieIntroTimer_ >= sortieIntroDuration_ && (input != nullptr || demoControlled);
	const bool triggerGun = demoControlled ? demoOut.fireGun : (input && input->PushMouse(0));
	const bool triggerMissile = demoControlled ? demoOut.fireMissile : (input && input->TriggerMouse(0));
	if (canFireWeapon && activeWeaponMode_ == WeaponMode::Gun && triggerGun) {
		int ammoBefore = gunpod_.GetCurrentAmmo();
		gunpod_.Fire();
		if (gunpod_.GetCurrentAmmo() < ammoBefore) {
			isFiringThisFrame_ = true;
			Vector3 aircraftPos = flightModel_.GetPosition();
			Vector3 aircraftRight = flightModel_.GetRightDirection();
			Vector3 aircraftUp = flightModel_.GetUpDirection();
			Vector3 aircraftForward = flightModel_.GetForwardDirection();
			Vector3 aircraftVelocity = flightModel_.GetVelocity();

			lastMuzzleLocalOffset_ = gunpod_.GetLastFiredMuzzleOffset();
			Vector3 firePos = Add(
				aircraftPos,
				Add(
					Add(Multiply(lastMuzzleLocalOffset_.x, aircraftRight), Multiply(lastMuzzleLocalOffset_.y, aircraftUp)),
					Multiply(lastMuzzleLocalOffset_.z, aircraftForward)
				)
			);
			Vector3 fireDir = aircraftForward;

			float muzzleSpeed = 500.0f;
			Vector3 muzzleVelocity = Multiply(muzzleSpeed, fireDir);
			Vector3 bulletVelocity = Add(aircraftVelocity, muzzleVelocity);

			bulletManager_.SpawnBullet(firePos, bulletVelocity, 10.0f);
			GunCombatEffectEmitter::EmitMuzzleGas(firePos, fireDir, aircraftVelocity, aircraftUp);

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

			// マズルフラッシュの間引き
			muzzleFlashCount_++;
			if (muzzleFlashCount_ % 2 == 0) {
				EffectManager::GetInstance()->EmitMuzzleFlash(firePos, fireDir);
				muzzleFlashTimer_ = 0.08f;

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
	} else if (canFireWeapon && activeWeaponMode_ == WeaponMode::Missile && triggerMissile) {
		if (missileLauncher_ && missileLauncher_->IsReadyToFire() && missileManager_.IsLocked()) {
			Vector3 localPylonOffset{};
			if (missileLauncher_->TryConsumeMissile(localPylonOffset)) {
				const Vector3 acPos = flightModel_.GetPosition();
				const Vector3 acRight = flightModel_.GetRightDirection();
				const Vector3 acUp = flightModel_.GetUpDirection();
				const Vector3 acFwd = flightModel_.GetForwardDirection();
				const Vector3 launchPos = Add(
					acPos,
					Add(Add(Multiply(localPylonOffset.x, acRight), Multiply(localPylonOffset.y, acUp)), Multiply(localPylonOffset.z, acFwd))
				);

				if (missileManager_.LaunchLockedMissile(launchPos, flightModel_.GetVelocity(), acFwd)) {
					if (CameraManager::GetInstance()->GetActiveCamera()) {
						CameraManager::GetInstance()->GetActiveCamera()->Shake(0.18f, 1.2f);
					}
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

	// クリア画面用の実飛行軌跡を記録
	const Vector3 curPos = flightModel_.GetPosition();
	if (!isMissionCleared_ && !isGameOver_) {
		auto& traj = GameResult::GetInstance().flightTrajectory;
		if (traj.empty() || (traj.size() < 6000 && (missionElapsedTime_ - traj.back().timeSec) >= 0.05f)) {
			TrajectorySample sample;
			sample.pos = curPos;
			sample.forward = flightModel_.GetForwardDirection();
			sample.up = flightModel_.GetUpDirection();
			sample.timeSec = missionElapsedTime_;
			traj.push_back(sample);
		}
	}

	// 飛行モデルの更新
	if (isFreeFlightMode_) {
		flightModel_.SetGroundElevation(0.0f);
		flightModel_.SetWaterSurface(false, 0.0f, 0.0f);
	} else if (terrainChunkManager_ && terrainChunkManager_->IsLoaded()) {
		const auto surf = terrainChunkManager_->QuerySurfaceAt(curPos.x, curPos.z, -485.0f);
		flightModel_.SetGroundElevation(surf.landY);
		flightModel_.SetWaterSurface(
			surf.type == TerrainChunkManager::SurfaceType::Water,
			surf.waterY,
			surf.waterDepth);

		// War Thunder 風: 超低空水面飛行のウォータースプレー & 着水柱エフェクト
		if (!isGameOver_ && surf.type == TerrainChunkManager::SurfaceType::Water) {
			const float altAboveWater = curPos.y - surf.waterY;
			const float acSpeed = flightModel_.GetSpeed();
			if (flightModel_.IsInWaterContact()) {
				ParticleParameters splash;
				splash.minVelocity = { -18.0f, 12.0f, -18.0f };
				splash.maxVelocity = { 18.0f, 45.0f, 18.0f };
				splash.randomPositionRange = 3.5f;
				splash.minColor = { 0.85f, 0.93f, 1.0f, 0.75f };
				splash.maxColor = { 0.96f, 0.99f, 1.0f, 0.90f };
				splash.minLifeTime = 0.6f;
				splash.maxLifeTime = 1.4f;
				splash.minScale = 1.5f;
				splash.maxScale = 4.2f;
				splash.endScale = 7.5f;
				splash.fadeOut = true;
				ParticleManager::GetInstance()->Emit("Vortex", { curPos.x, surf.waterY + 0.5f, curPos.z }, splash, 12);
			} else if (altAboveWater > 0.2f && altAboveWater < 15.0f && acSpeed > 40.0f) {
				float intensity = (1.0f - altAboveWater / 15.0f) * (std::min)(1.0f, acSpeed / 120.0f);
				ParticleParameters spray;
				spray.minVelocity = { -6.0f, 2.0f, -6.0f };
				spray.maxVelocity = { 6.0f, 9.0f * intensity + 2.0f, 6.0f };
				spray.randomPositionRange = 2.0f;
				spray.minColor = { 0.90f, 0.96f, 1.0f, 0.30f * intensity };
				spray.maxColor = { 0.98f, 1.00f, 1.0f, 0.48f * intensity };
				spray.minLifeTime = 0.35f;
				spray.maxLifeTime = 0.65f;
				spray.minScale = 0.8f;
				spray.maxScale = 1.8f;
				spray.endScale = 3.6f;
				spray.fadeOut = true;
				ParticleManager::GetInstance()->Emit("Vortex", { curPos.x, surf.waterY + 0.3f, curPos.z }, spray, 3);
			}
		}
	}
	if (wasInSortieIntro_ && sortieIntroTimer_ >= sortieIntroDuration_) {
		wasInSortieIntro_ = false;
		if (isFreeFlightMode_) {
			constexpr float kFreeFlightInitialSpeed = 400.0f / 3.6f;
			flightModel_.SetVelocity(Multiply(kFreeFlightInitialSpeed, flightModel_.GetForwardDirection()));
			flightModel_.SetThrottle(throttle_);
			flightModel_.SetThrottleInput(throttle_);
		}
		if (!isGameOver_ && !isMissionCleared_) {
			flightModel_.SetInvulnerable(false);
		}
	}

	if (!combatBootSequence_.IsWaitingForSpace()) {
		flightModel_.Update(gameDeltaTime);
	}

	// 部位破壊イベントのチェックと破片放出
	CheckPartDestructionEvents();

	// ビジュアルモデルの更新
	aircraftVisualModel_.Update(flightModel_.GetWorldMatrix(), gameDeltaTime);

	// 自機アフターバーナーの更新 (WEP時のみ点火)
	if (afterburner_) {
		const float abIntensity = flightModel_.GetAfterburnerIntensity();
		afterburner_->Update(gameDeltaTime, abIntensity);
		if (!isGameOver_ && (combatBootSequence_.IsFinished() || sortieIntroTimer_ >= 4.50f)) {
			afterburnerParticles_.Update(
				gameDeltaTime,
				*afterburner_,
				flightModel_.GetWorldMatrix(),
				flightModel_.GetVelocity(),
				abIntensity);
		} else {
			afterburnerParticles_.Reset();
		}
	}

	// 破片オブジェクト群の物理シミュレーション更新
	DebrisManager::GetInstance()->Update(gameDeltaTime);

	// Blender MCP リアルタイム同期の更新
	TerrainSyncManager::GetInstance()->Update();

	const float currentG = flightModel_.GetCurrentG();
	const Vector3 aircraftPos = flightModel_.GetPosition();
	const Vector3 right = flightModel_.GetRightDirection();
	const Vector3 forward = flightModel_.GetForwardDirection();

	// --- 高速飛行時のスピード感演出 ---
	float curSpeed = flightModel_.GetSpeed();
	if (curSpeed > 135.0f && !isGameOver_) { // 約480km/h以上
		float shakeFactor = std::clamp((curSpeed - 135.0f) / 100.0f, 0.0f, 1.0f);

		// 自機前方の空間を高速で後方へ通り過ぎるスピードダスト
		if ((rand() % 100) < static_cast<int>(40 * shakeFactor)) {
			Vector3 streamSpawn = Add(aircraftPos, Multiply(16.0f + (rand() % 14), forward));
			streamSpawn = Add(streamSpawn, Multiply((rand() % 24 - 12) * 1.0f, right));
			streamSpawn = Add(streamSpawn, Multiply((rand() % 16 - 8) * 1.0f, flightModel_.GetUpDirection()));

			ParticleParameters streamParams;
			streamParams.minVelocity = Multiply(-0.95f * curSpeed, forward);
			streamParams.maxVelocity = Multiply(-1.05f * curSpeed, forward);
			streamParams.minColor = { 0.85f, 0.95f, 1.0f, 0.35f };
			streamParams.maxColor = { 0.95f, 1.0f, 1.0f, 0.65f };
			streamParams.minLifeTime = 0.20f;
			streamParams.maxLifeTime = 0.35f;
			streamParams.minScale = 0.06f;
			streamParams.maxScale = 0.12f;
			streamParams.endScale = 0.02f;
			streamParams.fadeOut = true;
			streamParams.isStretched = true;
			streamParams.stretchFactor = 16.0f;
			streamParams.stretchDir = Multiply(-1.0f, forward);
			ParticleManager::GetInstance()->Emit("Vortex", streamSpawn, streamParams, 1);
		}
	}

	// --- GPUパーティクル連携エフェクト群の更新 ---
	{
		const Vector3 acUp = flightModel_.GetUpDirection();
		const Vector3 acVel = flightModel_.GetVelocity();

		// 1. 航空機ウェイク場（翼端渦＆ジェット排気）＋極地吹雪／氷晶ダスト更新
		AmbientWeatherSystem::WakeSource wake{};
		wake.position = aircraftPos;
		wake.velocity = acVel;
		wake.forward = forward;
		wake.right = right;
		wake.halfWingspan = 4.50f;
		wake.currentG = currentG;
		wake.throttle = flightModel_.GetCurrentThrottle();
		wake.afterburnerIntensity = flightModel_.GetAfterburnerIntensity();
		wake.active = !isGameOver_;
		ambientWeather_.SubmitWakeSource(0u, wake);

		Camera* activeCam = CameraManager::GetInstance()->GetActiveCamera();
		const Vector3 weatherCenter = activeCam ? activeCam->GetTranslate() : aircraftPos;
		ambientWeather_.Update(gameDeltaTime, weatherCenter);

		// 2. カウンターメジャー（チャフ＆フレア）更新
		CountermeasureSystem::AircraftEmitterState cmState{};
		cmState.position = aircraftPos;
		cmState.velocity = acVel;
		cmState.forward = forward;
		cmState.right = right;
		cmState.up = acUp;
		cmState.floorY = flightModel_.GetGroundElevation();
		countermeasures_.Update(gameDeltaTime, cmState);

		// 3. 高G主翼上面ベイパークラウド・遷音速ベイパー・音速突破ソニックブーム更新
		if (!isGameOver_) {
			AeroVaporSystem::AircraftAeroState aero{};
			aero.position = aircraftPos;
			aero.velocity = acVel;
			aero.forward = forward;
			aero.right = right;
			aero.up = acUp;
			aero.speed = curSpeed;
			aero.currentG = currentG;
			aero.aoaRad = flightModel_.GetCurrentAoA();
			aero.criticalAoARad = flightModel_.GetAirframe().GetCriticalAoA();
			aero.halfWingspan = 4.50f;
			aero.isStalling = flightModel_.IsStalling();
			aeroVapor_.Update(gameDeltaTime, aero);

			if (aeroVapor_.ConsumeSonicBoomTriggered() && activeCam) {
				activeCam->Shake(0.35f, 1.8f);
			}
		}
	}
}

// ============================================================
// 戦闘・敵・ミッション・当たり判定更新
// ============================================================
void StageScene::ApplyMissionLiveSyncUpdate() {
	if (MissionManager::GetInstance().GetCurrentMission().name.find("CLEAR DEMO") != std::string::npos) {
		return;
	}
	MissionData liveData{};
	std::string livePath;
	bool restartRequested = false;
	if (!MissionSyncManager::GetInstance()->ConsumePendingUpdate(liveData, livePath, restartRequested)) {
		return;
	}

	auto& missionManager = MissionManager::GetInstance();
	missionManager.ApplyLiveMissionData(liveData, livePath);

	if (restartRequested) {
		GameResult::GetInstance().isRetry = true;
		Restart();
		return;
	}

	if (combatBootSequence_.IsWaitingForSpace()) {
		flightModel_.SetPosition(liveData.playerPosition);
		const float yawRad = liveData.playerYawDeg * StageRadioDefaults::kDegToRad;
		const float halfYaw = yawRad * StageRadioDefaults::kHalfAngleFactor;
		flightModel_.SetOrientation({
			StageRadioDefaults::kZeroTimeLimit,
			std::sin(halfYaw),
			StageRadioDefaults::kZeroTimeLimit,
			std::cos(halfYaw)
		});
		mouseAimController_.ResetToDirection(flightModel_.GetForwardDirection());
	}

	static const std::vector<EnemySpawnData> kEmptyAir = {};
	static const std::vector<GroundEnemySpawnData> kEmptyGround = {};
	missileManager_.Clear();
	missileManager_.ResetSeeker();
	enemyManager_.SetWaypointPaths(liveData.waypointPaths);
	enemyManager_.SetTerrainChunkManager(terrainChunkManager_);
	enemyManager_.Initialize(
		enableEnemies_ ? liveData.enemies : kEmptyAir,
		aircraftConfig_.GetAirframe(),
		aircraftConfig_.GetEngine(),
		aircraftConfig_.GetGunpod(),
		&flightModel_,
		&bulletManager_,
		enableEnemies_ ? liveData.groundEnemies : kEmptyGround
	);
	missionManager.BindEnemyManager(&enemyManager_);
	missionManager.ResetRuntimeState();
}

void StageScene::UpdateCombatAndMission(float gameDeltaTime) {
	// ブリーフィング中のSPACEキー入力待機中は戦闘処理・敵更新・目標評価を完全停止
	if (combatBootSequence_.IsWaitingForSpace()) {
		return;
	}

	if (hitMarkerTimer_ > 0.0f) {
		hitMarkerTimer_ = (std::max)(0.0f, hitMarkerTimer_ - gameDeltaTime);
	}

	if (gameDeltaTime > 0.0001f) {
		gunpod_.Update(gameDeltaTime);
		bulletManager_.Update(gameDeltaTime);
		enemyManager_.Update(gameDeltaTime);

		const bool seekerEnabled =
			(activeWeaponMode_ == WeaponMode::Missile) &&
			MissionManager::GetInstance().IsPlayerWeaponAllowed() &&
			IsGameplayActive() &&
			!isPaused_;
		const int remainingMissiles = missileLauncher_ ? missileLauncher_->GetCurrentMissiles() : 0;
		missileManager_.UpdateSeeker(
			gameDeltaTime,
			seekerEnabled,
			remainingMissiles,
			flightModel_.GetPosition(),
			flightModel_.GetForwardDirection(),
			enemyManager_
		);
		missileManager_.Update(gameDeltaTime, enemyManager_, isFreeFlightMode_ ? nullptr : terrainChunkManager_);
	}

	if (enemyManager_.GetLastHitMarkerTriggered()) {
		hitMarkerTimer_ = 0.22f;
	}

	auto& missionManager = MissionManager::GetInstance();
	for (const auto& enemy : enemyManager_.GetEnemies()) {
		if (!enemy) { continue; }
		const std::string brokenLog = enemy->ConsumeBrokenPartLog();
		if (!brokenLog.empty()) {
			missionManager.PushSystemLog("SYS", brokenLog, false);
		}
	}

	// ミッション目標 & トリガー評価
	const auto& activeMission = missionManager.GetCurrentMission();
	if (IsGameplayActive()) {
		if (activeMission.minAmmoReserve > 0 && gunpod_.GetCurrentAmmo() < activeMission.minAmmoReserve) {
			gunpod_.ReplenishAmmo();
		}

		bool aimMovedThisFrame = false;
		Input* input = Input::GetInstance();
		if (input && input->IsCursorLocked()) {
			const Input::MouseMove rawMove = input->GetMouseMove();
			if (std::abs(rawMove.lX) >= 1 || std::abs(rawMove.lY) >= 1) {
				aimMovedThisFrame = true;
			}
		}
		const Vector3 curAimDir = mouseAimController_.GetTargetDirection();
		if (!hasPrevAimDir_) {
			prevAimDir_ = curAimDir;
			hasPrevAimDir_ = true;
		} else {
			if (MyMath::Length(Subtract(curAimDir, prevAimDir_)) >= 0.0025f) {
				aimMovedThisFrame = true;
			}
			prevAimDir_ = curAimDir;
		}

		missionManager.EvaluateObjectives(
			flightModel_.GetPosition(),
			playerHP_,
			enemyManager_.GetDestroyedCount(),
			enemyManager_.IsAllDestroyed(),
			missionElapsedTime_,
			&enemyManager_,
			gameDeltaTime,
			throttle_,
			aimMovedThisFrame,
			radioCommunicator_.IsActive()
		);
		missionManager.EvaluateTriggers(
			flightModel_.GetPosition(),
			playerHP_,
			enemyManager_.GetDestroyedCount(),
			missionElapsedTime_,
			&enemyManager_,
			gameDeltaTime
		);
	}
	missionManager.UpdateMessages(gameDeltaTime);
	if (missionManager.ConsumePendingIgniteOutpost()) {
		enemyManager_.IgniteLastWatchtower();
	}
	if (enableEnemies_) {
		const std::vector<EnemySpawnData> pendingAir = missionManager.PopPendingEnemySpawns();
		if (!pendingAir.empty()) {
			enemyManager_.SpawnAirEnemies(pendingAir);
		}
		const std::vector<GroundEnemySpawnData> pendingGround = missionManager.PopPendingGroundEnemySpawns();
		if (!pendingGround.empty()) {
			enemyManager_.SpawnGroundEnemies(pendingGround);
		}
	} else {
		(void)missionManager.PopPendingEnemySpawns();
		(void)missionManager.PopPendingGroundEnemySpawns();
	}
	UpdateRadioCommunications(gameDeltaTime);

	// 衝突判定
	if (!isGameOver_) {
		collisionSystem_.ClearAll();
		collisionSystem_.Register(&playerBody_);

		for (auto& bullet : bulletManager_.GetBullets()) {
			collisionSystem_.Register(&bullet);
		}
		for (auto& missile : missileManager_.GetMissiles()) {
			collisionSystem_.Register(&missile);
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
	if (!isFreeFlightMode_ && !isMissionCleared_ && !isMissionFailed_) {
		if (missionManager.IsTriggerMissionFail() || missionManager.IsAnyPrimaryObjectiveFailed()) {
			TriggerGameOver();
		} else if (missionManager.IsTriggerMissionComplete() || missionManager.AreAllPrimaryObjectivesCompleted()) {
			if (activeMission.fadeOutOnClear) {
				clearFadeOutActive_ = true;
				clearFadeOutTimer_ = 0.0f;
				isMissionCleared_ = true;
			} else {
				TriggerGameClear();
			}
		} else if (activeMission.objectives.empty() && enemyManager_.IsAllDestroyed()) {
			TriggerGameClear();
		}
	}
	if (false) {
		TriggerGameClear();
	}

	if (isParticleDemoMode_ && !isGameOver_ && !isMissionCleared_ && particleDemo_.ConsumeFinaleDeathRequested()) {
		isBailout_ = false;
		TriggerGameOver();
		return;
	}

	// 機体壊滅判定
	if (!isMissionFailed_ && flightModel_.GetAirframe().IsDestroyed()) {
		TriggerGameOver();
	}
}

// ============================================================
// カメラ・スカイボックス更新
// ============================================================
void StageScene::UpdateRadioCommunications(float gameDeltaTime) {
	auto& missionManager = MissionManager::GetInstance();
	std::vector<RadioMessage> pendingRadio = missionManager.PopPendingRadioMessages();
	for (const auto& msg : pendingRadio) {
		radioCommunicator_.Enqueue(msg);
	}

	if (sortieIntroTimer_ < sortieIntroDuration_ ||
	    isGameOver_ ||
	    isMissionCleared_ ||
	    endSequence_ != StageEndSequence::None) {
		return;
	}

	const bool suppressGenericRadio =
		missionManager.HasRadioDialogueTriggers() ||
		missionManager.GetCurrentMission().disableGenericRadio;

	if (!hasSentSortieRadio_) {
		hasSentSortieRadio_ = true;
		if (!suppressGenericRadio && !radioCommunicator_.IsActive()) {
			radioCommunicator_.Enqueue(
				StageRadioDefaults::kAwacsName,
				StageRadioDefaults::kSortieDialogue,
				RadioFaction::Awacs);
		}
	}

	const int destroyedCount = enemyManager_.GetDestroyedCount();
	if (destroyedCount > lastRadioKillCount_) {
		lastRadioKillCount_ = destroyedCount;
		if (!suppressGenericRadio && pendingRadio.empty() && !enemyManager_.IsAllDestroyed()) {
			const bool isWingmanLine =
				((destroyedCount % StageRadioDefaults::kAlternateModulo) == StageRadioDefaults::kEvenRemainder);
			if (isWingmanLine) {
				radioCommunicator_.Enqueue(
					StageRadioDefaults::kWingmanName,
					StageRadioDefaults::kKillWingmanDialogue,
					RadioFaction::Ally);
			} else {
				radioCommunicator_.Enqueue(
					StageRadioDefaults::kAwacsName,
					StageRadioDefaults::kKillAwacsDialogue,
					RadioFaction::Awacs);
			}
		}
	}

	if (!suppressGenericRadio && !hasSentDamageWarningRadio_ && playerMaxHP_ > RadioDefaults::kZero) {
		const float hpRatio = playerHP_ / playerMaxHP_;
		if (hpRatio > RadioDefaults::kZero && hpRatio <= StageRadioDefaults::kCriticalHpRatioThreshold) {
			hasSentDamageWarningRadio_ = true;
			radioCommunicator_.Interrupt(
				StageRadioDefaults::kAwacsName,
				StageRadioDefaults::kCriticalDamageDialogue,
				RadioFaction::Awacs);
		}
	}

	radioCommunicator_.Update(gameDeltaTime);
}

void StageScene::UpdateCamera(float gameDeltaTime) {
	Vector3 pos = flightModel_.GetPosition();
	Vector3 euler = PlayerCamera::QuaternionToEuler(flightModel_.GetOrientation());

	aircraftObject_->SetTranslate(pos);
	aircraftObject_->SetRotate(euler);
	aircraftObject_->Update();

	if (sortieIntroTimer_ < sortieIntroDuration_) {
		combatBootSequence_.Update(gameDeltaTime);

		if (combatBootSequence_.IsWaitingForSpace()) {
			sortieIntroTimer_ = 0.0f;
			flightModel_.SetVelocity({ 0.0f, 0.0f, 0.0f });
		} else {
			sortieIntroTimer_ = combatBootSequence_.GetTimer();
			float introProg = std::clamp(sortieIntroTimer_ / sortieIntroDuration_, 0.0f, 1.0f);

			float boostSpeed = isFreeFlightMode_
				? (400.0f / 3.6f)
				: (22.0f + 28.0f * (1.0f - std::exp(-introProg * 2.2f)));
			flightModel_.SetVelocity(Multiply(boostSpeed, flightModel_.GetForwardDirection()));
		}

		Vector3 forward = flightModel_.GetForwardDirection();
		Vector3 right = flightModel_.GetRightDirection();
		Vector3 up = flightModel_.GetUpDirection();

		if (combatBootSequence_.IsQuickMode()) {
			if (isParticleDemoMode_ && particleDemo_.IsActive()) {
				Vector3 camPos{};
				Vector3 lookTarget{};
				ParticleDemoCamera::CameraContext camCtx{};
				camCtx.dt = gameDeltaTime;
				camCtx.elapsed = particleDemo_.GetElapsedTime();
				camCtx.playerPos = pos;
				camCtx.playerForward = forward;
				camCtx.playerRight = right;
				camCtx.playerUp = up;
				camCtx.hasTarget = particleDemo_.HasActiveTarget();
				camCtx.targetPos = particleDemo_.GetActiveTargetPosition();
				camCtx.isExplosionHold = particleDemo_.IsExplosionHoldActive();
				camCtx.isFlareFocus = particleDemo_.IsFlareFocusActive();
				particleDemoCamera_.Update(camCtx, camPos, lookTarget);
				playerCamera_.SnapTo(camPos, lookTarget);
			} else {
				// クイックリトライ・リスタート時は横からのドローン追従カメラではなく、即座に通常自機追従カメラ視点にする
				playerCamera_.Update(gameDeltaTime, &flightModel_, &mouseAimController_, mouseAimEnabled_);
			}
		} else {
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
				float dist = 14.0f + 16.0f * smoothOrbit;
				float height = -0.6f + 3.8f * smoothOrbit;

				float localX = dist * std::sin(angle);
				float localZ = -dist * std::cos(angle);
				Vector3 camOffset = Add(Add(Multiply(localZ, forward), Multiply(localX, right)), Multiply(height, up));
				camPos = Add(pos, camOffset);

				Vector3 endLookTarget = Add(pos, Add(Multiply(90.0f, forward), Multiply(3.2f, up)));
				Vector3 startLookTarget = Add(pos, Multiply(2.0f, forward));
				lookTarget = Lerp(startLookTarget, endLookTarget, smoothOrbit);
			}

			playerCamera_.SnapTo(camPos, lookTarget);
		}
	} else if (endSequence_ == StageEndSequence::Clear_TacticalMapZoom) {
		float zoomProg = std::clamp((endSeqTimer_ - 2.8f) / 1.5f, 0.0f, 1.0f);
		float easeZoom = zoomProg * zoomProg;
		Vector3 mapCamPos = { pos.x, pos.y + 25.0f + 1600.0f * easeZoom, pos.z };
		Vector3 mapLookTarget = { pos.x, pos.y, pos.z + 0.1f };
		playerCamera_.SnapTo(mapCamPos, mapLookTarget);
	} else if (isGameOver_ && endSequence_ != StageEndSequence::TimeOver_Abort && endSequence_ != StageEndSequence::TimeOver_Withdraw) {
		// エースコンバット風 撃墜・炎上追尾カメラ
		Vector3 velDir = gameOverVelocity_;
		Vector3 horizDir = { velDir.x, 0.0f, velDir.z };
		if (Length(horizDir) > 0.1f) {
			horizDir = Normalize(horizDir);
		} else {
			horizDir = { 0.0f, 0.0f, 1.0f };
		}
		Vector3 horizRight = { horizDir.z, 0.0f, -horizDir.x };

		// 時間経過とともに燃え落ちる機体が遠ざかる演出
		float pullProg = std::clamp(endSeqTimer_ / 2.2f, 0.0f, 1.0f);
		float camDist = 28.0f + 12.0f * pullProg;
		float camHeight = 6.5f + 2.5f * (1.0f - pullProg);
		float camSide = 5.5f + 1.5f * std::sin(endSeqTimer_ * 1.6f);

		Vector3 targetCamPos = Add(Add(pos, Multiply(-camDist, horizDir)), Add(Multiply(camSide, horizRight), { 0.0f, camHeight, 0.0f }));
		Vector3 lookTarget = Add(pos, Multiply(0.5f, flightModel_.GetForwardDirection()));

		playerCamera_.SnapTo(targetCamPos, lookTarget);
	} else if (isParticleDemoMode_ && particleDemo_.IsActive()) {
		Vector3 camPos{};
		Vector3 lookTarget{};
		ParticleDemoCamera::CameraContext camCtx{};
		camCtx.dt = gameDeltaTime;
		camCtx.elapsed = particleDemo_.GetElapsedTime();
		camCtx.playerPos = pos;
		camCtx.playerForward = flightModel_.GetForwardDirection();
		camCtx.playerRight = flightModel_.GetRightDirection();
		camCtx.playerUp = flightModel_.GetUpDirection();
		camCtx.hasTarget = particleDemo_.HasActiveTarget();
		camCtx.targetPos = particleDemo_.GetActiveTargetPosition();
		camCtx.isExplosionHold = particleDemo_.IsExplosionHoldActive();
		camCtx.isFlareFocus = particleDemo_.IsFlareFocusActive();
		particleDemoCamera_.Update(camCtx, camPos, lookTarget);
		playerCamera_.SnapTo(camPos, lookTarget);
	} else {
		playerCamera_.Update(gameDeltaTime, &flightModel_, &mouseAimController_, mouseAimEnabled_);
	}

	// 最新フレームのカメラViewProjection行列を自機3DモデルのWVP定数バッファへ即時反映
	aircraftVisualModel_.Update(flightModel_.GetWorldMatrix(), 0.0f);

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
	const bool isThrottleWep = throttle_ > flightModel_.GetNormalThrottleLimit() + 0.005f;
	if (isThrottleWep) {
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.20f, 1.0f), "Throttle: %.0f%% [WEP]", throttle_ * 100.0f);
	} else {
		ImGui::Text("Throttle: %.0f%%", throttle_ * 100.0f);
	}
	ImGui::Text("Engine Output: %.0f%%", flightModel_.GetCurrentThrottle() * 100.0f);
	const float abIntensityDbg = flightModel_.GetAfterburnerIntensity();
	if (abIntensityDbg > 0.001f) {
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.20f, 1.0f), "Afterburner: ON (%.0f%%)", abIntensityDbg * 100.0f);
	} else {
		ImGui::TextDisabled("Afterburner: OFF (0%%)");
	}
	ImGui::Checkbox("Hold W for WEP (Release -> 100%)", &holdWForWep_);
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
	ImGui::Text("W/S: Throttle (Hold/Tap W at 100% -> WEP & A/B)");
	ImGui::Text("F: Flaps | B: AirBrake | X: Chaff & Flares (%d/%d)",
		countermeasures_.GetRemainingAmmo(), countermeasures_.GetMaxAmmo());
	ImGui::Text("M: Mouse Aim [%s]", mouseAimEnabled_ ? "ON" : "OFF");

	ImGui::Separator();
	if (ImGui::CollapsingHeader("GPU Particle Effects (Contest)")) {
		ImGui::Text("Mach: %.2f | Wing: %.0f%% | Stall: %.0f%% | Flares: %d/%d",
			aeroVapor_.GetCurrentMach(),
			aeroVapor_.GetCurrentWingVaporIntensity() * 100.0f,
			aeroVapor_.GetCurrentStallVaporIntensity() * 100.0f,
			countermeasures_.GetRemainingAmmo(),
			countermeasures_.GetMaxAmmo());
		ImGui::Text("Targets Alive -> Air: %d | Ground: %d",
			enemyManager_.GetAliveAirCount(),
			enemyManager_.GetAliveGroundCount());

		int weatherIdx = static_cast<int>(ambientWeather_.GetPreset());
		const char* weatherLabels[] = {
			AmbientWeatherSystem::GetPresetName(AmbientWeatherSystem::WeatherPreset::Off),
			AmbientWeatherSystem::GetPresetName(AmbientWeatherSystem::WeatherPreset::IceDust),
			AmbientWeatherSystem::GetPresetName(AmbientWeatherSystem::WeatherPreset::PolarBlizzard),
			AmbientWeatherSystem::GetPresetName(AmbientWeatherSystem::WeatherPreset::ArcticStorm)
		};
		if (ImGui::Combo("Polar Weather", &weatherIdx, weatherLabels, 4)) {
			ambientWeather_.SetPreset(static_cast<AmbientWeatherSystem::WeatherPreset>(weatherIdx));
		}

		if (ImGui::Button("Deploy Chaff & Flares [X]")) {
			countermeasures_.TriggerSalvo();
		}
		ImGui::SameLine();
		if (ImGui::Button("Replenish Missiles & Flares")) {
			countermeasures_.Replenish();
			if (missileLauncher_) {
				missileLauncher_->ReplenishMissiles();
			}
		}
		if (ImGui::Button("Test Sonic Boom")) {
			aeroVapor_.TriggerSonicBoom(
				flightModel_.GetPosition(),
				flightModel_.GetForwardDirection(),
				flightModel_.GetVelocity(),
				1.0f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Test GPU Explosion Ahead")) {
			const Vector3 ahead = Add(flightModel_.GetPosition(), Multiply(65.0f, flightModel_.GetForwardDirection()));
			EffectManager::GetInstance()->EmitDestroyEffect(ahead);
		}
		if (ImGui::Button("Spawn Barrage Balloon Ahead")) {
			Vector3 spawnPos = Add(flightModel_.GetPosition(), Multiply(900.0f, flightModel_.GetForwardDirection()));
			spawnPos.y = (std::max)(140.0f, flightModel_.GetPosition().y + 30.0f);
			EnemySpawnData balloon{};
			balloon.position = spawnPos;
			balloon.modelPath = "Resources/models/barrage_balloon.gltf";
			balloon.health = 80.0f;
			balloon.aiType = AIType::BarrageBalloon;
			enemyManager_.SpawnAirEnemies({ balloon });
		}
		if (ImGui::Button("Spawn Air Target Ahead")) {
			Vector3 spawnPos = Add(flightModel_.GetPosition(), Multiply(1050.0f, flightModel_.GetForwardDirection()));
			spawnPos.y = (std::max)(180.0f, flightModel_.GetPosition().y + 40.0f);
			EnemySpawnData target{};
			target.position = spawnPos;
			target.modelPath = "Resources/models/m15.gltf";
			target.health = 80.0f;
			target.aiType = AIType::FollowWaypoint;
			target.behaviorTreeJson = R"({"useCustomParamsOnly":true,"overrideParams":{"engageDist":0.0,"fireRange":0.0,"patrolThrottle":0.62}})";
			enemyManager_.SpawnAirEnemies({ target });
		}
		ImGui::SameLine();
		if (ImGui::Button("Spawn Ground Target Ahead")) {
			Vector3 spawnPos = Add(flightModel_.GetPosition(), Multiply(850.0f, flightModel_.GetForwardDirection()));
			if (!isFreeFlightMode_ && terrainChunkManager_ && terrainChunkManager_->IsLoaded()) {
				const auto surf = terrainChunkManager_->QuerySurfaceAt(spawnPos.x, spawnPos.z, 0.0f);
				spawnPos.y = (surf.type == TerrainChunkManager::SurfaceType::Water)
					? (std::max)(surf.landY, surf.waterY)
					: surf.landY;
			} else {
				spawnPos.y = 0.0f;
			}
			GroundEnemySpawnData groundTarget{};
			groundTarget.position = spawnPos;
			groundTarget.aiType = GroundAIType::Structure;
			groundTarget.health = 80.0f;
			enemyManager_.SpawnGroundEnemies({ groundTarget });
		}
	}
	ImGui::Text("I: Instructor [%s]", flightInstructor_.IsEnabled() ? "ON" : "OFF");
	ImGui::Text("P: Cursor Lock [%s]", (input && input->IsCursorLocked()) ? "LOCKED" : "FREE");

	ImGui::End();

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
	ImGui::Text("Weapon Mode: %s (Scroll to Switch)", activeWeaponMode_ == WeaponMode::Missile ? "MISSILE" : "GUN");
	if (missileLauncher_) {
		const auto& seeker = missileManager_.GetSeekerStatus();
		const char* seekerLabels[] = { "OFF", "WARMING", "SEARCH", "LOCKING", "LOCKED" };
		const int stIdx = std::clamp(static_cast<int>(seeker.state), 0, 4);
		ImGui::Text("Missiles: %d / %d | Active: %u | Seeker: %s (%.0f%%)",
			missileLauncher_->GetCurrentMissiles(),
			missileLauncher_->GetMaxMissiles(),
			missileManager_.GetActiveMissileCount(),
			seekerLabels[stIdx],
			seeker.lockProgress * 100.0f);
		if (ImGui::Button("Replenish Missiles")) {
			missileLauncher_->ReplenishMissiles();
		}
	}
	
	ImGui::Separator();
	ImGui::Text("=== Alive Air Enemies (%d) ===", enemyManager_.GetAliveAirCount());
	int enemyIndex = 0;
	for (auto* enemy : enemyManager_.GetAliveEnemies()) {
		float dist = MyMath::Length(MyMath::Subtract(enemy->GetPosition(), flightModel_.GetPosition()));
		const char* aiLabel = "Patrol (FollowWaypoint)";
		if (enemy->GetAIType() == AIType::ChaseAttack) {
			aiLabel = "Type A (ChaseAttack)";
		} else if (enemy->GetAIType() == AIType::CruiseEvade) {
			aiLabel = "Type B (CruiseEvade)";
		}
		ImGui::Text("[%d] %s | BT: %s", enemyIndex, aiLabel, enemy->GetActiveBehaviorName());
		ImGui::Text("   HP: %.1f / %.1f | Dist: %.1f m", enemy->GetHealth(), enemy->GetMaxHealth(), dist);
		ImGui::Text("   Pos: (%.1f, %.1f, %.1f)", enemy->GetPosition().x, enemy->GetPosition().y, enemy->GetPosition().z);
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

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Radio Communicator (Ace Combat Style)")) {
		static char testSpeaker[StageRadioDefaults::kImGuiSpeakerBufferSize] = "Joker 2 Coster";
		static char testDialogue[StageRadioDefaults::kImGuiDialogueBufferSize] = "今はやめだ";
		static int testFactionIndex = static_cast<int>(RadioFaction::Ally);
		static float testHoldSeconds = RadioDefaults::kDefaultHoldSeconds;

		const char* factionLabels[] = { "Ally", "Awacs", "Enemy", "System" };
		constexpr int kFactionLabelCount = static_cast<int>(std::size(factionLabels));

		ImGui::InputText("Speaker", testSpeaker, sizeof(testSpeaker));
		ImGui::Combo("Faction", &testFactionIndex, factionLabels, kFactionLabelCount);
		ImGui::InputTextMultiline("Dialogue << >>", testDialogue, sizeof(testDialogue));
		ImGui::SliderFloat(
			"Hold (sec)",
			&testHoldSeconds,
			StageRadioDefaults::kImGuiMinHoldSeconds,
			StageRadioDefaults::kImGuiMaxHoldSeconds);

		const RadioFaction selectedFaction = static_cast<RadioFaction>(testFactionIndex);
		if (ImGui::Button("Enqueue Radio")) {
			radioCommunicator_.Enqueue(testSpeaker, testDialogue, selectedFaction, testHoldSeconds);
		}
		ImGui::SameLine();
		if (ImGui::Button("Interrupt Radio")) {
			radioCommunicator_.Interrupt(testSpeaker, testDialogue, selectedFaction, testHoldSeconds);
		}
		ImGui::SameLine();
		if (ImGui::Button("Skip")) {
			radioCommunicator_.SkipCurrent();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			radioCommunicator_.Clear();
		}

		if (ImGui::Button("Preset: AWACS")) {
			radioCommunicator_.Enqueue(
				StageRadioDefaults::kAwacsName,
				StageRadioDefaults::kSortieDialogue,
				RadioFaction::Awacs);
		}
		ImGui::SameLine();
		if (ImGui::Button("Preset: Wingman")) {
			radioCommunicator_.Enqueue(
				StageRadioDefaults::kWingmanName,
				StageRadioDefaults::kKillWingmanDialogue,
				RadioFaction::Ally);
		}
		ImGui::SameLine();
		if (ImGui::Button("Preset: Enemy Ace")) {
			radioCommunicator_.Enqueue(
				StageRadioDefaults::kEnemyAceName,
				StageRadioDefaults::kPresetEnemyDialogue,
				RadioFaction::Enemy);
		}
	}

	if (isMissionCleared_) {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "*** MISSION CLEAR ***");
	}
	if (isMissionFailed_) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "*** MISSION FAILED ***");
	}
	ImGui::End();

	DrawAircraftTuningEditor();
	if (terrainChunkManager_) {
		terrainChunkManager_->DrawImGui();
	}
#endif
}

// ============================================================
// ポストエフェクト & 戦闘HUD更新
// ============================================================
void StageScene::UpdatePostEffects(float deltaTime) {
	// ゲームオーバー演出やクリア演出のポストエフェクトは UpdateEndSequence で一元管理するためここでは干渉しない
	if (isGameOver_ || endSequence_ != StageEndSequence::None) {
		return;
	}

	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->ClearActiveEffects();

	// 1. ナイトビジョン モード
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
		float bootBloom = 1.0f;
		float bootScan = 0.030f;
		float bootAberration = 0.004f;
		float bootVignette = 0.40f;
		float bloomStrength = 0.50f;  // 通常戦闘飛行時の適正ブルーム強度
		float bloomThreshold = 0.90f; // 機体ボディが白飛び・まぶしく発光しない適正なしきい値

		if (sortieIntroTimer_ < sortieIntroDuration_) {
			bootVignette = 0.55f;

			if (sortieIntroTimer_ < 0.85f) {
				bootBloom = 0.0f;
				bootScan = 0.0f;
				bootAberration = 0.0f;
				bloomStrength = 0.0f;
			} else if (sortieIntroTimer_ < 1.35f) {
				float t = (sortieIntroTimer_ - 0.85f) / 0.50f;
				bootScan = 0.08f * (1.0f - t * 0.4f);
				bootAberration = 0.012f * (1.0f - t * 0.5f);
				bootBloom = 2.0f;
				bloomStrength = 1.0f;
				bloomThreshold = 0.75f;
			} else if (sortieIntroTimer_ < 3.40f) {
				bootBloom = 1.8f;
				bootScan = 0.040f;
				bootAberration = 0.006f;
				bloomStrength = 0.90f;
				bloomThreshold = 0.75f;
			} else if (sortieIntroTimer_ >= 4.55f && sortieIntroTimer_ <= 4.80f) {
				float flashT = (sortieIntroTimer_ - 4.55f) / 0.25f;
				float pulse = std::sin(flashT * 3.14159f);
				bootBloom = 1.8f + 0.8f * pulse;
				bootAberration = 0.012f * pulse;
				bloomStrength = 0.90f;
				bloomThreshold = 0.75f;
			}
		}

		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = bootBloom;
		bloom.dirX = bloomStrength;
		bloom.dirY = bloomThreshold;
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

		// 作戦ブリーフィング中：奥の3D景観に被写界深度をかけて手前のUIを際立たせる
		if (combatBootSequence_.IsWaitingForSpace()) {
			ActivePostEffect bgBlur;
			bgBlur.type = PostEffectType::kKawaseBlur;
			bgBlur.intensity = 2.4f;
			bgBlur.kernelSize = 2;
			postEffect->AddActiveEffect(bgBlur);
		}
	}

	// 2. 環境の更新
	if (!isNvdActive_) {
		environmentManager_.Update(CameraManager::GetInstance()->GetActiveCamera(), kDeltaTime);
	} else {
		environmentManager_.Update(nullptr, kDeltaTime);
	}

	// 3. Gフォースによるビネット
	if (!isMissionCleared_ && endSequence_ == StageEndSequence::None) {
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
	}

	// 4. 戦闘HUDの更新
	Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
	StageHUDData hudData{};
	hudData.remainingTime = remainingTime_;
	hudData.elapsedTime = missionElapsedTime_;
	hudData.isElapsedTimerMode =
		(MissionManager::GetInstance().GetCurrentMission().timeLimit <= StageRadioDefaults::kZeroTimeLimit);
	hudData.currentAmmo = gunpod_.GetCurrentAmmo();
	hudData.maxAmmo = gunpod_.GetMaxAmmo();
	hudData.isFiring = isFiringThisFrame_;
	hudData.aircraftPosition = flightModel_.GetPosition();
	hudData.aircraftVelocity = flightModel_.GetVelocity();
	hudData.aircraftForward = flightModel_.GetForwardDirection();
	hudData.mouseAimTargetDirection = mouseAimController_.GetTargetDirection();
	const bool showGameplayReticles =
		IsGameplayActive() &&
		!isPaused_ &&
		!isMissionCleared_ &&
		!isMissionFailed_ &&
		!isGameOver_ &&
		endSequence_ == StageEndSequence::None &&
		!combatBootSequence_.IsWaitingForSpace();
	hudData.showReticles = showGameplayReticles;
	hudData.mouseAimEnabled = mouseAimEnabled_ && showGameplayReticles;
	if (cam) {
		hudData.viewProjectionMatrix = cam->GetViewProjectionMatrix();
	}
	hudData.screenWidth = static_cast<float>(WinApp::kClientWidth);
	hudData.screenHeight = static_cast<float>(WinApp::kClientHeight);

	const auto& currentMission = MissionManager::GetInstance().GetCurrentMission();
	const bool isClearDemo = currentMission.name.find("CLEAR DEMO") != std::string::npos;
	hudData.isClearDemoMode = isClearDemo;
	if (isClearDemo) {
		for (const auto& obj : currentMission.objectives) {
			if (obj.type == ObjectiveType::Survive) {
				hudData.clearCountdownSec = (std::max)(0.0f, obj.duration - missionElapsedTime_);
				break;
			}
		}
	}

	const float groundY = (!isFreeFlightMode_ && terrainChunkManager_ && terrainChunkManager_->IsLoaded())
		? terrainChunkManager_->GetTerrainHeightAt(flightModel_.GetPosition().x, flightModel_.GetPosition().z, 0.0f)
		: flightModel_.GetGroundElevation();
	const float currentAgl = (std::max)(0.0f, flightModel_.GetPosition().y - groundY);

	if (!isGameOver_ &&
	    !combatBootSequence_.IsWaitingForSpace()) {
		auto& missionManager = MissionManager::GetInstance();
		const auto& objectives = missionManager.GetCurrentMission().objectives;

		float targetDamageRatio = 0.0f;
		for (const auto& obj : objectives) {
			if (obj.status == ObjectiveStatus::Active && obj.type == ObjectiveType::DestroyTarget) {
				const auto& allEnemies = enemyManager_.GetEnemies();
				if (obj.targetIndex >= 0 && obj.targetIndex < static_cast<int>(allEnemies.size())) {
					const auto* target = allEnemies[static_cast<size_t>(obj.targetIndex)].get();
					if (target && target->IsAlive() && target->GetAIType() == AIType::CanyonDrone) {
						targetDamageRatio = 1.0f - target->GetHealthRatio();
					}
				}
				break;
			}
		}

		for (const auto& enemyPtr : enemyManager_.GetEnemies()) {
			const Enemy* enemy = enemyPtr.get();
			if (!enemy) { continue; }
			if (enemy->GetAIType() == AIType::CanyonWesternFighter && !enemy->IsInDivePass()) {
				hudData.structuralTelemetry.visible = true;
				hudData.structuralTelemetry.portWingRatio = enemy->GetPortWingHpRatio();
				hudData.structuralTelemetry.stbdWingRatio = enemy->GetStbdWingHpRatio();
				hudData.structuralTelemetry.engineRatio = enemy->GetEngineHpRatio();
				hudData.structuralTelemetry.brokenPartIndex = enemy->GetBrokenPartIndex();
			}
			if (!enemy->IsAlive()) {
				continue;
			}
			if (enemy->GetAIType() == AIType::CanyonWesternFighter && enemy->IsInDivePass()) {
				hudData.showRwrAlert = true;
				continue;
			}
			StageHUDTargetMarker marker{};
			marker.worldPos = enemy->GetPosition();
			marker.velocity = enemy->GetVelocity();
			switch (enemy->GetAIType()) {
			case AIType::AllyWingman:
				marker.label = "ALLY // CORAX [M-21A]";
				marker.color = { 0.68f, 0.98f, 0.84f, 0.96f };
				marker.showLeadSight = false;
				break;
			case AIType::CanyonDrone:
				marker.label = "TRAINING DRONE [ORANGE]";
				marker.color = { 1.00f, 0.58f, 0.16f, 0.98f };
				marker.showLeadSight = true;
				break;
			case AIType::CanyonWesternFighter:
				marker.label = "UNKNOWN FIGHTER [WESTERN]";
				marker.color = { 1.00f, 0.28f, 0.26f, 0.98f };
				marker.showLeadSight = true;
				break;
			default:
				if (enemy->GetAIType() == AIType::ChaseAttack) {
					marker.label = "HOSTILE [INTERCEPTOR]";
				} else if (enemy->GetAIType() == AIType::CruiseEvade) {
					marker.label = "HOSTILE [ACE]";
				} else {
					marker.label = "HOSTILE [PATROL]";
				}
				marker.color = { 1.00f, 0.28f, 0.26f, 0.98f };
				marker.showLeadSight = true;
				break;
			}
			hudData.targetMarkers.push_back(std::move(marker));
		}

		const GroundEnemy* lastWatchtower = nullptr;
		for (const auto& groundPtr : enemyManager_.GetGroundEnemies()) {
			if (groundPtr && groundPtr->IsAlive() && groundPtr->GetAIType() == GroundAIType::Watchtower) {
				lastWatchtower = groundPtr.get();
			}
		}
		bool hasOutpostObjectiveActive = false;
		bool hasCombatStageStarted = false;
		for (const auto& obj : objectives) {
			if (obj.status == ObjectiveStatus::Active &&
			    obj.type == ObjectiveType::ReachArea &&
			    obj.startDelaySeconds > 0.0f) {
				hasOutpostObjectiveActive = true;
			}
			if (obj.type == ObjectiveType::DestroyTarget && obj.status != ObjectiveStatus::Inactive) {
				hasCombatStageStarted = true;
			}
		}

		for (auto* ground : enemyManager_.GetAliveGroundEnemies()) {
			if (!ground) { continue; }
			if (ground->GetAIType() == GroundAIType::Watchtower) {
				if (ground != lastWatchtower || hasCombatStageStarted || (!hasOutpostObjectiveActive && !ground->IsIgnited())) {
					continue;
				}
				StageHUDTargetMarker marker{};
				const Vector3 gPos = ground->GetPosition();
				marker.worldPos = { gPos.x, gPos.y + ground->GetWatchtowerTopOffsetY(), gPos.z };
				marker.velocity = { 0.0f, 0.0f, 0.0f };
				marker.label = ground->IsIgnited() ? "OUTPOST 04 // CARRIER LOST" : "OUTPOST 04";
				marker.color = ground->IsIgnited()
					? Vector4{ 1.00f, 0.84f, 0.28f, 0.98f }
					: Vector4{ 0.42f, 0.82f, 0.70f, 0.70f };
				marker.showLeadSight = false;
				hudData.targetMarkers.push_back(std::move(marker));
				continue;
			}
			StageHUDTargetMarker marker{};
			const Vector3 gPos = ground->GetPosition();
			marker.worldPos = { gPos.x, gPos.y + 2.0f, gPos.z };
			marker.velocity = { 0.0f, 0.0f, 0.0f };
			switch (ground->GetAIType()) {
			case GroundAIType::Turret:
				marker.label = "AAA [TURRET]";
				marker.color = { 1.00f, 0.48f, 0.16f, 0.96f };
				break;
			case GroundAIType::Structure:
				marker.label = "FACILITY";
				marker.color = { 1.00f, 0.84f, 0.24f, 0.96f };
				break;
			default:
				marker.label = "VEHICLE";
				marker.color = { 0.35f, 0.82f, 1.00f, 0.96f };
				break;
			}
			marker.showLeadSight = false;
			hudData.targetMarkers.push_back(std::move(marker));
		}
		for (const auto& obj : objectives) {
			if (obj.status != ObjectiveStatus::Active || obj.type != ObjectiveType::ReachArea) {
				continue;
			}
			if (obj.enableBoundaryLineCheck) {
				if (obj.boundaryCrossed) {
					continue;
				}
				StageHUDTargetMarker marker{};
				marker.worldPos = obj.areaCenter;
				marker.velocity = { 0.0f, 0.0f, 0.0f };
				marker.label = "RTB SECTOR BOUNDARY";
				marker.color = { 0.28f, 0.92f, 1.00f, 0.90f };
				marker.showLeadSight = false;
				hudData.targetMarkers.push_back(std::move(marker));
			} else if (obj.gateTotalCount > 0) {
				StageHUDTargetMarker marker{};
				marker.worldPos = obj.areaCenter;
				marker.velocity = { 0.0f, 0.0f, 0.0f };
				const int stepNo = obj.gateStepIndex + 1;
				marker.label = (stepNo < 10 ? "WAYPOINT 0" : "WAYPOINT ") + std::to_string(stepNo);
				marker.color = { 0.30f, 1.00f, 0.65f, 1.00f };
				marker.showLeadSight = false;
				hudData.targetMarkers.push_back(std::move(marker));
			} else if (obj.startDelaySeconds <= 0.0f) {
				StageHUDTargetMarker marker{};
				marker.worldPos = obj.areaCenter;
				marker.velocity = { 0.0f, 0.0f, 0.0f };
				marker.label = "WAYPOINT";
				marker.color = { 0.35f, 0.95f, 0.45f, 0.96f };
				marker.showLeadSight = false;
				hudData.targetMarkers.push_back(std::move(marker));
			}
		}

		for (const auto& log : missionManager.GetActiveSystemLogs()) {
			StageHUDSysLogEntry hudLog{};
			hudLog.category = log.category;
			hudLog.timestampText = log.timestampText;
			hudLog.message = log.message;
			hudLog.ageSeconds = log.ageSeconds;
			hudData.systemLogs.push_back(std::move(hudLog));
		}

		hudData.guide.visible = missionManager.BuildActiveGuideHUD(
			throttle_,
			targetDamageRatio,
			hudData.guide.title,
			hudData.guide.sub,
			hudData.guide.showProgressBar,
			hudData.guide.progressRatio);

		missionManager.EvaluateAltitudeAlerts(
			currentAgl,
			flightModel_.GetPosition().y,
			hudData.showPullUpAlert,
			hudData.showHighAltCaution);
	}

	hudData.hitMarkerTimer = hitMarkerTimer_;
	hudData.fadeOutAlpha = clearFadeOutActive_ ? std::clamp(clearFadeOutTimer_ / 1.65f, 0.0f, 1.0f) : 0.0f;

	{
		const auto& seeker = missileManager_.GetSeekerStatus();
		hudData.missileSeeker.isMissileSelected = (activeWeaponMode_ == WeaponMode::Missile);
		hudData.missileSeeker.seekerState = static_cast<int>(seeker.state);
		hudData.missileSeeker.warmupProgress = seeker.warmupProgress;
		hudData.missileSeeker.lockProgress = seeker.lockProgress;
		hudData.missileSeeker.boresightWorldPoint = seeker.boresightWorldPoint;
		hudData.missileSeeker.seekerTrackWorldPoint = seeker.seekerTrackWorldPoint;
		hudData.missileSeeker.aircraftRight = flightModel_.GetRightDirection();
		hudData.missileSeeker.outerGimbalAngleDeg = seeker.outerGimbalAngleDeg;
		hudData.missileSeeker.innerAcqAngleDeg = seeker.innerAcqAngleDeg;
		hudData.missileSeeker.currentMissiles = missileLauncher_ ? missileLauncher_->GetCurrentMissiles() : 0;
		hudData.missileSeeker.maxMissiles = missileLauncher_ ? missileLauncher_->GetMaxMissiles() : 0;
		hudData.missileSeeker.cooldownRatio = missileLauncher_ ? missileLauncher_->GetCooldownRatio() : 0.0f;
		hudData.missileSeeker.hasTarget = seeker.hasTarget;
		hudData.missileSeeker.targetDistanceMeters = seeker.targetDistance;
		for (const auto& m : missileManager_.GetMissiles()) {
			if (m.IsAlive()) {
				hudData.missileSeeker.activeMissilePositions.push_back(m.GetPosition());
			}
		}
	}

	hud_.Update(deltaTime, hudData);

	// 5. War Thunder風フライト計器UIの更新
	FlightInfoUIData infoData{};
	infoData.throttle = throttle_;
	infoData.normalThrottleLimit = flightModel_.GetNormalThrottleLimit();
	infoData.isWepActive = (throttle_ > infoData.normalThrottleLimit + 0.005f);
	infoData.speed = flightModel_.GetSpeed();
	infoData.currentAmmo = gunpod_.GetCurrentAmmo();
	infoData.maxAmmo = gunpod_.GetMaxAmmo();
	infoData.currentMissiles = missileLauncher_ ? missileLauncher_->GetCurrentMissiles() : 0;
	infoData.maxMissiles = missileLauncher_ ? missileLauncher_->GetMaxMissiles() : 0;
	infoData.isMissileSelected = (activeWeaponMode_ == WeaponMode::Missile);
	infoData.missileSeekerState = static_cast<int>(missileManager_.GetSeekerStatus().state);
	infoData.absoluteAltitude = flightModel_.GetPosition().y;
	infoData.relativeAltitude = currentAgl;
	infoData.screenWidth = static_cast<float>(WinApp::kClientWidth);
	infoData.screenHeight = static_cast<float>(WinApp::kClientHeight);
	infoData.alpha = 1.0f;
	infoUI_.Update(deltaTime, infoData);
}
