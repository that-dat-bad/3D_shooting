#include "StageSceneInternal.h"
#include <cmath>

using namespace MyMath;

namespace {
	// 直交正規基底 (Right, Up, Forward) からクォータニオンを計算
	MyMath::Quaternion BasisToQuaternion(const MyMath::Vector3& r, const MyMath::Vector3& u, const MyMath::Vector3& f) {
		float tr = r.x + u.y + f.z;
		MyMath::Quaternion q{};
		if (tr > 0.0f) {
			float s = std::sqrt(tr + 1.0f) * 2.0f;
			q.w = 0.25f * s;
			q.x = (u.z - f.y) / s;
			q.y = (f.x - r.z) / s;
			q.z = (r.y - u.x) / s;
		} else if ((r.x > u.y) && (r.x > f.z)) {
			float s = std::sqrt(1.0f + r.x - u.y - f.z) * 2.0f;
			q.w = (u.z - f.y) / s;
			q.x = 0.25f * s;
			q.y = (r.y + u.x) / s;
			q.z = (f.x + r.z) / s;
		} else if (u.y > f.z) {
			float s = std::sqrt(1.0f + u.y - r.x - f.z) * 2.0f;
			q.w = (f.x - r.z) / s;
			q.x = (r.y + u.x) / s;
			q.y = 0.25f * s;
			q.z = (u.z + f.y) / s;
		} else {
			float s = std::sqrt(1.0f + f.z - r.x - u.y) * 2.0f;
			q.w = (r.y - u.x) / s;
			q.x = (f.x + r.z) / s;
			q.y = (u.z + f.y) / s;
			q.z = 0.25f * s;
		}
		float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
		if (len > 0.0001f) {
			float inv = 1.0f / len;
			q.x *= inv; q.y *= inv; q.z *= inv; q.w *= inv;
		}
		return q;
	}

	// 目標の前方ベクトルと基準上方向からクォータニオンを生成
	MyMath::Quaternion LookRotation(const MyMath::Vector3& forward, const MyMath::Vector3& desiredUp = { 0.0f, 1.0f, 0.0f }) {
		MyMath::Vector3 f = MyMath::Normalize(forward);
		MyMath::Vector3 upRef = desiredUp;
		if (std::fabs(MyMath::Dot(f, upRef)) > 0.99f) {
			upRef = { 0.0f, 0.0f, 1.0f };
		}
		// Right = desiredUp × forward
		MyMath::Vector3 r = MyMath::Normalize(MyMath::Cross(upRef, f));
		// Up = forward × Right
		MyMath::Vector3 u = MyMath::Cross(f, r);
		return BasisToQuaternion(r, u, f);
	}
}

void StageScene::TriggerGameOver() {
	if (isGameOver_ || isMissionCleared_) { return; }
	isGameOver_ = true;
	isMissionFailed_ = true;
	gameOverTimer_ = 0.0f;
	playerHP_ = 0.0f;
	timeScale_ = 1.0f; // Ace Combat演出中は完全停止させず、燃え落ちる機体をフルスピードで追う
	endSequence_ = StageEndSequence::GameOver_Critical;
	endSeqTimer_ = 0.0f;
	gameOverBurnTimer_ = 0.0f;

	// 被弾・墜落ゲームオーバーを設定
	auto& res = GameResult::GetInstance();
	res.gameOverReason = GameOverReason::Destroyed;

	// 非同期処理による軽量化：ゲームオーバー演出中にバックグラウンドで次ステージデータを非同期プリロード！
	AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();

	// テーマカラー赤反転
	hud_.SetWarningColorMode(true);

	// 初撃爆破エフェクト（火球・閃光・破片飛散）
	EffectManager::GetInstance()->EmitDestroyEffect(flightModel_.GetPosition());

	// ベイルアウト時：キャノピー吹き飛ばし＆射出座席の火花演出
	if (isBailout_) {
		ParticleParameters ejectParams;
		Vector3 upDir = flightModel_.GetUpDirection();
		ejectParams.minVelocity = Add(flightModel_.GetVelocity(), Multiply(15.0f, upDir));
		ejectParams.maxVelocity = Add(flightModel_.GetVelocity(), Multiply(25.0f, upDir));
		ejectParams.minColor = { 1.0f, 0.85f, 0.40f, 1.0f };
		ejectParams.maxColor = { 1.0f, 0.95f, 0.70f, 1.0f };
		ejectParams.minLifeTime = 0.8f;
		ejectParams.maxLifeTime = 1.4f;
		ejectParams.minScale = 0.2f;
		ejectParams.maxScale = 0.6f;
		ejectParams.endScale = 0.0f;
		ejectParams.fadeOut = true;
		ejectParams.isStretched = true;
		ejectParams.stretchFactor = 0.08f;
		ParticleManager::GetInstance()->Emit("ExplosionSpark", Add(flightModel_.GetPosition(), Multiply(1.5f, upDir)), ejectParams, 15);
	}

	// カメラシェイクによる被弾・撃墜の強烈な物理的衝撃（指摘1）
	if (CameraManager::GetInstance()->GetActiveCamera()) {
		CameraManager::GetInstance()->GetActiveCamera()->Shake(1.2f, 4.0f);
	}

	// 機体を制御不能・オートパイロットバイパス状態へ
	flightModel_.SetAutopilot(true);
	flightModel_.SetInvulnerable(true);
	flightModel_.SetGEffectsEnabled(false);
	flightModel_.ResetGEffects();

	// 墜落弾道速度の初期化（前進モーメンタムを維持）
	Vector3 curVel = flightModel_.GetVelocity();
	float curSpeed = Length(curVel);
	Vector3 forward = flightModel_.GetForwardDirection();
	if (curSpeed < 40.0f) {
		gameOverVelocity_ = Add(Multiply(70.0f, forward), { 0.0f, -10.0f, 0.0f });
	} else {
		gameOverVelocity_ = curVel;
	}

	// エースコンバット風の制御不能きりもみ回転（タンブル角速度）
	float rollDir = ((rand() % 2) == 0) ? 1.0f : -1.0f;
	float yawDir = ((rand() % 2) == 0) ? 1.0f : -1.0f;
	gameOverTumbleRate_.x = -1.1f - (rand() % 60) * 0.01f; // 機首下げピッチ (-1.1〜-1.7 rad/s)
	gameOverTumbleRate_.y = yawDir * (0.8f + (rand() % 50) * 0.01f); // ヨー旋回 (±0.8〜±1.3 rad/s)
	gameOverTumbleRate_.z = rollDir * (4.2f + (rand() % 120) * 0.01f); // 激しい高速ロール (±4.2〜±5.4 rad/s)
}

void StageScene::TriggerTimeOver() {
	if (isGameOver_ || isMissionCleared_) { return; }
	isGameOver_ = true;
	isMissionFailed_ = true;
	gameOverTimer_ = 0.0f;
	timeScale_ = 1.0f;
	endSequence_ = StageEndSequence::TimeOver_Abort;
	endSeqTimer_ = 0.0f;

	// ゲームオーバーリザルトに時間切れ離脱要因を記録
	auto& res = GameResult::GetInstance();
	res.gameOverReason = GameOverReason::TimeOver;

	// 非同期プリロード開始
	AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();

	// 爆発・破壊はさせず、無敵化＆G効果無効化＆オートパイロット離脱に移行
	flightModel_.SetInvulnerable(true);
	flightModel_.SetGEffectsEnabled(false);
	flightModel_.ResetGEffects();
	flightModel_.SetAutopilot(true);

	// 離脱初期ベクトルを保持
	clearStartOrientation_ = flightModel_.GetOrientation();
	clearStartForward_ = flightModel_.GetForwardDirection();
	clearStartSpeed_ = (std::max)(flightModel_.GetSpeed(), 120.0f);

	Vector3 horiz = { clearStartForward_.x, 0.0f, clearStartForward_.z };
	if (Length(horiz) > 0.001f) {
		clearHorizontalDir_ = Normalize(horiz);
	} else {
		clearHorizontalDir_ = { 0.0f, 0.0f, 1.0f };
	}

	// HUDを警告色モードにしつつオフラインシャットダウン開始
	hud_.SetWarningColorMode(true);
	hud_.StartShutdown();
}

void StageScene::TriggerGameClear() {
	if (isMissionCleared_ || isGameOver_) { return; }
	isMissionCleared_ = true;
	endSequence_ = StageEndSequence::Clear_HudShutdown;
	endSeqTimer_ = 0.0f;

	// 自機を完全無敵化（受けるダメージ0）
	flightModel_.SetInvulnerable(true);

	// G効果（ブラックアウト・レッドアウト）を即座に無効化＆リセット
	flightModel_.SetGEffectsEnabled(false);
	flightModel_.ResetGEffects();

	// オートパイロット移行（物理シミュレーションを直接制御）
	flightModel_.SetAutopilot(true);

	// 離脱機動の初期状態を保持
	clearStartOrientation_ = flightModel_.GetOrientation();
	clearStartForward_ = flightModel_.GetForwardDirection();
	clearStartSpeed_ = (std::max)(flightModel_.GetSpeed(), 120.0f);

	Vector3 horiz = { clearStartForward_.x, 0.0f, clearStartForward_.z };
	if (Length(horiz) > 0.001f) {
		clearHorizontalDir_ = Normalize(horiz);
	} else {
		clearHorizontalDir_ = { 0.0f, 0.0f, 1.0f };
	}

	// 項目5: HUDの順次シャットダウン開始
	hud_.StartShutdown();
}

// ============================================================
// PlayerCollisionBody のコールバック実装
// ============================================================
void StageScene::PlayerCollisionBody::OnCollision(ICollisionBody3D* other) {
	if (!scene_ || scene_->isMissionFailed_ || scene_->isMissionCleared_) { return; }

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

				Model* mainModel = ModelManager::GetInstance()->FindModel("Resources/models/m21.gltf");
				if (!mainModel) {
					mainModel = ModelManager::GetInstance()->FindModel("Resources/models/m21_wg.gltf");
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

	// --- ゲームオーバー系（エースコンバット風：機体炎上きりもみ落下 & 連続火炎・黒煙・火花放出 & 通信シグナル段階的途絶）---
	if (endSequence_ == StageEndSequence::GameOver_Critical ||
		endSequence_ == StageEndSequence::GameOver_SignalLost ||
		endSequence_ == StageEndSequence::GameOver_CrtShutdown) {

		timeScale_ = 1.0f; // 物理更新と炎・煙の動きを止めずに実時間でシミュレーション

		// 1. 落下重力と空気抵抗による弾道計算（緩やかに機首を下げながらきりもみ落下）
		gameOverVelocity_.y -= 9.81f * 1.5f * dt;
		gameOverVelocity_.x *= (1.0f - 0.15f * dt);
		gameOverVelocity_.z *= (1.0f - 0.15f * dt);

		Vector3 curPos = flightModel_.GetPosition();
		Vector3 newPos = Add(curPos, Multiply(dt, gameOverVelocity_));
		if (newPos.y < -5.0f) {
			newPos.y = -5.0f;
			gameOverVelocity_.y = 0.0f;
		}
		flightModel_.SetPosition(newPos);
		flightModel_.SetVelocity(gameOverVelocity_);

		// 2. きりもみ回転（タンブル）クォータニオン更新
		Vector3 right = flightModel_.GetRightDirection();
		Vector3 up = flightModel_.GetUpDirection();
		Vector3 forward = flightModel_.GetForwardDirection();

		float pitchAngle = gameOverTumbleRate_.x * dt;
		float yawAngle   = gameOverTumbleRate_.y * dt;
		float rollAngle  = gameOverTumbleRate_.z * dt;

		auto makeRot = [](const Vector3& axis, float angle) -> Quaternion {
			float half = angle * 0.5f;
			float s = std::sin(half);
			float c = std::cos(half);
			return { axis.x * s, axis.y * s, axis.z * s, c };
		};
		auto qMul = [](const Quaternion& a, const Quaternion& b) -> Quaternion {
			return {
				a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
				a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
				a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
				a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
			};
		};

		Quaternion qP = makeRot(right, pitchAngle);
		Quaternion qY = makeRot(up, yawAngle);
		Quaternion qR = makeRot(forward, rollAngle);
		Quaternion comb = qMul(qR, qMul(qY, qP));
		flightModel_.SetOrientation(qMul(comb, flightModel_.GetOrientation()));

		// 3. 機体からの激しい火炎・濃密黒煙・火花トレイルの連続放出（煙出しながら激しく燃える）
		gameOverBurnTimer_ += dt;
		if (gameOverBurnTimer_ >= 0.020f) {
			gameOverBurnTimer_ = 0.0f;
			ParticleManager* pm = ParticleManager::GetInstance();
			Vector3 planePos = flightModel_.GetPosition();
			Vector3 planeFwd = flightModel_.GetForwardDirection();
			Vector3 planeRight = flightModel_.GetRightDirection();

			// 炎の発生点: エンジン排気部および被弾した主翼付け根
			Vector3 firePos1 = Add(planePos, Multiply(-3.5f, planeFwd));
			Vector3 firePos2 = Add(Add(planePos, Multiply(-1.2f, planeFwd)), Multiply(1.8f, planeRight));

			// 機体後流への気流オフセット
			Vector3 slipstream = Multiply(-0.06f, planeFwd);

			// ========================================================
			// A. 激しく燃え盛る火炎 (ExplosionFire - flame_particle.png)
			//    煙に負けず後方へ大きく広がる高輝度燃焼炎
			// ========================================================
			ParticleParameters fireParams;
			fireParams.minVelocity = Add(slipstream, { -0.04f, -0.01f, -0.04f });
			fireParams.maxVelocity = Add(slipstream, {  0.04f,  0.06f,  0.04f });
			fireParams.minColor = { 1.0f, 0.40f, 0.06f, 0.95f };
			fireParams.maxColor = { 1.0f, 0.95f, 0.35f, 1.0f }; // 高温発光コア
			fireParams.minLifeTime = 0.40f;
			fireParams.maxLifeTime = 0.75f;
			fireParams.minScale = 4.5f;
			fireParams.maxScale = 7.5f;
			fireParams.endScale = 14.0f; // 炎が後方で大きく膨張しながら燃え盛る
			fireParams.fadeOut = true;
			fireParams.scaleEasing = 0.4f;
			fireParams.minRotation = 0.0f;
			fireParams.maxRotation = 6.283f;
			fireParams.minRotationSpeed = -4.0f;
			fireParams.maxRotationSpeed = 4.0f;
			pm->Emit("ExplosionFire", firePos1, fireParams, 4);
			pm->Emit("ExplosionFire", firePos2, fireParams, 3);
			pm->Emit("ExplosionFire", planePos, fireParams, 2);

			// ========================================================
			// B. 内部着火煙（炎から黒煙への過渡熱光レイヤー）
			// ========================================================
			ParticleParameters glowingSmoke;
			glowingSmoke.minVelocity = { -0.035f, 0.02f, -0.035f };
			glowingSmoke.maxVelocity = {  0.035f, 0.05f,  0.035f };
			glowingSmoke.minColor = { 0.50f, 0.18f, 0.05f, 0.75f };
			glowingSmoke.maxColor = { 0.75f, 0.30f, 0.08f, 0.90f };
			glowingSmoke.minLifeTime = 0.6f;
			glowingSmoke.maxLifeTime = 1.0f;
			glowingSmoke.minScale = 5.0f;
			glowingSmoke.maxScale = 8.0f;
			glowingSmoke.endScale = 18.0f;
			glowingSmoke.fadeOut = true;
			glowingSmoke.scaleEasing = 0.45f;
			glowingSmoke.minRotation = 0.0f;
			glowingSmoke.maxRotation = 6.283f;
			glowingSmoke.minRotationSpeed = -1.2f;
			glowingSmoke.maxRotationSpeed = 1.2f;
			pm->Emit("ExplosionSmoke", firePos1, glowingSmoke, 3);
			pm->Emit("ExplosionSmoke", firePos2, glowingSmoke, 2);

			// ========================================================
			// C. 濃密黒煙トレイル (ExplosionSmoke - smoke_particle.png)
			//    空中に滞留し、3D立体陰影を保ちながら巨大に膨張する黒煙雲
			// ========================================================
			ParticleParameters smokeParams;
			smokeParams.minVelocity = { -0.03f, 0.02f, -0.03f };
			smokeParams.maxVelocity = {  0.03f, 0.05f,  0.03f };
			smokeParams.minColor = { 0.06f, 0.06f, 0.07f, 0.85f };
			smokeParams.maxColor = { 0.14f, 0.14f, 0.16f, 0.95f };
			smokeParams.minLifeTime = 1.8f;
			smokeParams.maxLifeTime = 2.8f; // 航跡煙を生成
			smokeParams.minScale = 4.5f;
			smokeParams.maxScale = 7.5f;
			smokeParams.endScale = 22.0f; // 後方で巨大な黒煙の帯に膨張
			smokeParams.fadeOut = true;
			smokeParams.scaleEasing = 0.40f;
			smokeParams.minRotation = 0.0f;
			smokeParams.maxRotation = 6.283f;
			smokeParams.minRotationSpeed = -0.5f;
			smokeParams.maxRotationSpeed = 0.5f;
			pm->Emit("ExplosionSmoke", firePos1, smokeParams, 3);
			pm->Emit("ExplosionSmoke", firePos2, smokeParams, 2);

			// ========================================================
			// D. 飛散する火の粉・燃焼スパーク (ExplosionSpark)
			// ========================================================
			ParticleParameters sparkParams;
			Vector3 sparkBaseVel = Multiply(2.0f, slipstream);
			sparkParams.minVelocity = Add(sparkBaseVel, { -0.12f, -0.06f, -0.12f });
			sparkParams.maxVelocity = Add(sparkBaseVel, {  0.12f,  0.12f,  0.12f });
			sparkParams.minColor = { 1.0f, 0.65f, 0.15f, 1.0f };
			sparkParams.maxColor = { 1.0f, 0.95f, 0.40f, 1.0f };
			sparkParams.minLifeTime = 0.35f;
			sparkParams.maxLifeTime = 0.75f;
			sparkParams.minScale = 0.15f;
			sparkParams.maxScale = 0.45f;
			sparkParams.endScale = 0.0f;
			sparkParams.fadeOut = true;
			sparkParams.isStretched = true;
			sparkParams.stretchFactor = 0.08f;
			pm->Emit("ExplosionSpark", firePos1, sparkParams, 5);
			pm->Emit("ExplosionSpark", firePos2, sparkParams, 3);

			// 炎の動的ライト（明滅）
			PointLight* pLight = Object3dCommon::GetInstance()->GetPointLightData();
			if (pLight) {
				pLight->position = firePos1;
				pLight->color = { 1.0f, 0.55f, 0.12f, 1.0f };
				pLight->intensity = 60.0f + 25.0f * std::sin(endSeqTimer_ * 30.0f);
				pLight->radius = 120.0f;
				pLight->decay = 2.0f;
			}
		}

		// 4. シグナル途絶段階（フェーズ制御）
		if (endSequence_ == StageEndSequence::GameOver_Critical) {
			// Phase 1 (0.0s - 1.4s): 激しい炎上・黒煙噴出 & 警告表示 & ノイズが徐々に混入
			// 最初 (0.0s - 0.35s) はノイズ完全ゼロで機体炎上を鮮明に捉える
			// 0.35s - 1.4s から徐々にノイズと走査線が入り始める (0.00 -> 0.20)
			if (endSeqTimer_ >= 0.35f) {
				float p = (endSeqTimer_ - 0.35f) / 1.05f; // 0.0 -> 1.0
				float easeNoise = p * p; // 二次曲線で穏やかに立ち上がり

				ActivePostEffect noise;
				noise.type = PostEffectType::kRandom;
				noise.intensity = easeNoise * 0.20f;
				postEffect->AddActiveEffect(noise);

				ActivePostEffect scan;
				scan.type = PostEffectType::kScanLine;
				scan.intensity = easeNoise * 0.20f;
				scan.dirX = 600.0f;
				scan.dirY = -8.0f;
				postEffect->AddActiveEffect(scan);

				ActivePostEffect ca;
				ca.type = PostEffectType::kChromaticAberration;
				ca.intensity = easeNoise * 0.025f;
				postEffect->AddActiveEffect(ca);
			}

			if (endSeqTimer_ >= 1.4f) {
				endSequence_ = StageEndSequence::GameOver_SignalLost;
			}
		} else if (endSequence_ == StageEndSequence::GameOver_SignalLost) {
			// Phase 2 (1.4s - 2.2s): ノイズが本格的に激化し、通信完全途絶（SIGNAL LOST）へ
			float degradeT = std::clamp((endSeqTimer_ - 1.4f) / 0.80f, 0.0f, 1.0f);
			// smoothstepでノイズを劇的かつ滑らかに増幅
			float easeT = degradeT * degradeT * (3.0f - 2.0f * degradeT);

			ActivePostEffect scan;
			scan.type = PostEffectType::kScanLine;
			scan.intensity = 0.20f + 0.45f * easeT;
			scan.dirX = 500.0f - 260.0f * easeT;
			scan.dirY = -20.0f - 35.0f * easeT;
			postEffect->AddActiveEffect(scan);

			ActivePostEffect noise;
			noise.type = PostEffectType::kRandom;
			noise.intensity = 0.20f + 0.65f * easeT; // 0.20から0.85へと本格的な砂嵐へ
			postEffect->AddActiveEffect(noise);

			ActivePostEffect ca;
			ca.type = PostEffectType::kChromaticAberration;
			ca.intensity = 0.025f + 0.08f * easeT;
			postEffect->AddActiveEffect(ca);

			ActivePostEffect vig;
			vig.type = PostEffectType::kVignette;
			vig.intensity = 0.4f + 1.2f * easeT;
			postEffect->AddActiveEffect(vig);

			ActivePostEffect dist;
			dist.type = PostEffectType::kLensDistortion;
			dist.intensity = 0.08f + 0.25f * easeT;
			postEffect->AddActiveEffect(dist);

			if (endSeqTimer_ >= 2.2f) {
				endSequence_ = StageEndSequence::GameOver_CrtShutdown;
			}
		} else if (endSequence_ == StageEndSequence::GameOver_CrtShutdown) {
			// Phase 3 (2.2s - 2.55s): 最大砂嵐ノイズが激しく鳴り響く最中にブラウン管プツッ切断（ノイズしながら切る: 0.35秒）
			ActivePostEffect noise;
			noise.type = PostEffectType::kRandom;
			noise.intensity = 0.85f; // 最大砂嵐ノイズを完全維持！ノイズを途切れさせない
			postEffect->AddActiveEffect(noise);

			ActivePostEffect scan;
			scan.type = PostEffectType::kScanLine;
			scan.intensity = 0.60f;
			scan.dirX = 240.0f;
			scan.dirY = -45.0f;
			postEffect->AddActiveEffect(scan);

			ActivePostEffect ca;
			ca.type = PostEffectType::kChromaticAberration;
			ca.intensity = 0.08f;
			postEffect->AddActiveEffect(ca);

			ActivePostEffect vig;
			vig.type = PostEffectType::kVignette;
			vig.intensity = 1.2f;
			postEffect->AddActiveEffect(vig);

			ActivePostEffect dist;
			dist.type = PostEffectType::kLensDistortion;
			dist.intensity = 0.35f;
			postEffect->AddActiveEffect(dist);

			if (endSeqTimer_ >= 2.55f) {
				postEffect->ClearActiveEffects();
				sceneID = SCENE::GAMEOVER;
				return;
			}
		}
	}
	// --- 時間切れ離脱系（爆発なし・作戦離脱） ---
	else if (endSequence_ == StageEndSequence::TimeOver_Abort) {
		// 0.0s - 1.4s: 作戦時間切れ警報 ＆ 自機水平レベリング・離脱加速開始
		timeScale_ = 1.0f;

		// 翼のロールを水平に戻す目標姿勢
		Quaternion qLevel = LookRotation(clearHorizontalDir_, { 0.0f, 1.0f, 0.0f });
		float levelT = std::clamp(endSeqTimer_ / 0.9f, 0.0f, 1.0f);
		float smoothT = levelT * levelT * (3.0f - 2.0f * levelT);
		Quaternion currentOri = Slerp(clearStartOrientation_, qLevel, smoothT);
		flightModel_.SetOrientation(currentOri);

		// 巡航速度へ加速
		float speed = clearStartSpeed_ + smoothT * (180.0f - clearStartSpeed_);
		flightModel_.SetVelocity(Multiply(speed, flightModel_.GetForwardDirection()));

		// 走査線と警告ビネット（アンバー調）
		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.intensity = 0.20f;
		scan.dirX = 650.0f;
		scan.dirY = -2.0f;
		postEffect->AddActiveEffect(scan);

		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = 0.65f + 0.25f * std::sin(endSeqTimer_ * 6.28f);
		postEffect->AddActiveEffect(vig);

		if (endSeqTimer_ >= 1.4f) {
			endSequence_ = StageEndSequence::TimeOver_Withdraw;
		}
	} else if (endSequence_ == StageEndSequence::TimeOver_Withdraw) {
		// 1.4s - 3.4s: 機首を緩やかに上げてアフターバーナー離脱 ＆ 暗転フェード
		timeScale_ = 1.0f;

		Quaternion qLevel = LookRotation(clearHorizontalDir_, { 0.0f, 1.0f, 0.0f });
		constexpr float kClimbAngle = 26.0f * (3.14159265f / 180.0f);
		Vector3 climbForward = Normalize(Add(
			Multiply(std::cos(kClimbAngle), clearHorizontalDir_),
			{ 0.0f, std::sin(kClimbAngle), 0.0f }
		));
		Quaternion qClimb = LookRotation(climbForward, { 0.0f, 1.0f, 0.0f });

		float climbT = std::clamp((endSeqTimer_ - 1.4f) / 1.2f, 0.0f, 1.0f);
		float smoothClimb = climbT * climbT * (3.0f - 2.0f * climbT);
		Quaternion currentOri = Slerp(qLevel, qClimb, smoothClimb);
		flightModel_.SetOrientation(currentOri);

		// 急加速して戦域外へ離脱
		float speed = 180.0f + smoothClimb * 420.0f;
		flightModel_.SetVelocity(Multiply(speed, flightModel_.GetForwardDirection()));

		// 加速のラジアルブラー
		ActivePostEffect blur;
		blur.type = PostEffectType::kRadialBlur;
		blur.kernelSize = 6;
		blur.intensity = smoothClimb * 0.06f;
		postEffect->AddActiveEffect(blur);

		// 離脱完了に向けたフェード暗転
		float fadeT = std::clamp((endSeqTimer_ - 2.5f) / 0.9f, 0.0f, 1.0f);
		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = 0.70f + fadeT * 3.5f;
		postEffect->AddActiveEffect(vig);

		if (endSeqTimer_ >= 3.4f) {
			postEffect->ClearActiveEffects();
			sceneID = SCENE::GAMEOVER;
			return;
		}
	}
	// --- ステージクリア系 ---
	else if (endSequence_ == StageEndSequence::Clear_HudShutdown) {
		// 0.0s - 1.2s: 目標達成ファンファーレ & HUD順次OFFLINE消去
		// 自機は機動姿勢を整え（水平レベリング）、アフターバーナー点火（加速開始）
		timeScale_ = 1.0f;

		// 翼のロールを水平に戻す目標姿勢（水平巡航姿勢）
		Quaternion qLevel = LookRotation(clearHorizontalDir_, { 0.0f, 1.0f, 0.0f });

		// 最初の0.8秒でクリア突入時の姿勢から水平姿勢へ滑らかに復帰
		float levelT = std::clamp(endSeqTimer_ / 0.8f, 0.0f, 1.0f);
		float smoothT = levelT * levelT * (3.0f - 2.0f * levelT);
		Quaternion currentOri = Slerp(clearStartOrientation_, qLevel, smoothT);
		flightModel_.SetOrientation(currentOri);

		// 速度：クリア開始速度から巡航加速（約180m/sへ）
		float speed = clearStartSpeed_ + smoothT * (180.0f - clearStartSpeed_);
		Vector3 curFwd = flightModel_.GetForwardDirection();
		flightModel_.SetVelocity(Multiply(speed, curFwd));

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
		// 1.2s - 2.5s: オートパイロット帰還時の超音速ズームクライム急上昇・急加速離脱機動
		timeScale_ = 1.0f;

		// 水平巡航姿勢と、目標上昇姿勢（ピッチ角 約38度ピッチアップ）
		Quaternion qLevel = LookRotation(clearHorizontalDir_, { 0.0f, 1.0f, 0.0f });

		// ピッチアップ上昇角: 約38度（約0.663ラジアン）
		constexpr float kClimbAngle = 38.0f * (3.14159265f / 180.0f);
		Vector3 climbForward = Normalize(Add(
			Multiply(std::cos(kClimbAngle), clearHorizontalDir_),
			{ 0.0f, std::sin(kClimbAngle), 0.0f }
		));
		Quaternion qClimb = LookRotation(climbForward, { 0.0f, 1.0f, 0.0f });

		// 1.3秒間かけて滑らかに機首を持ち上げ（ピッチアップ）、進行方向と機体姿勢を完全に一致させる
		float climbT = std::clamp((endSeqTimer_ - 1.2f) / 1.3f, 0.0f, 1.0f);
		float smoothClimb = climbT * climbT * (3.0f - 2.0f * climbT);
		Quaternion currentOri = Slerp(qLevel, qClimb, smoothClimb);
		flightModel_.SetOrientation(currentOri);

		// 速度：機首を上げながらアフターバーナー全開で超音速（約660m/s = マッハ2）へ急加速！
		float speed = 180.0f + smoothClimb * 480.0f;
		Vector3 curFwd = flightModel_.GetForwardDirection();
		flightModel_.SetVelocity(Multiply(speed, curFwd));

		// 1. ラジアルブラー（スピード感・ワープ感の画面全体演出）
		ActivePostEffect blur;
		blur.type = PostEffectType::kRadialBlur;
		blur.kernelSize = 8;
		blur.intensity = smoothClimb * 0.08f;
		postEffect->AddActiveEffect(blur);

		// 2. 色収差（光速突入の色ズレ）
		ActivePostEffect ca;
		ca.type = PostEffectType::kChromaticAberration;
		ca.intensity = smoothClimb * 0.04f;
		postEffect->AddActiveEffect(ca);

		if (endSeqTimer_ >= 2.5f) {
			endSequence_ = StageEndSequence::Clear_TacticalMapZoom;
		}
	} else if (endSequence_ == StageEndSequence::Clear_TacticalMapZoom) {
		// 2.5s - 3.8s: 戦術マップ表示（走査線 + ビネット）＆ ターミナル転送接続
		timeScale_ = 1.0f;

		// 機体は最高速のまま空へ離脱を継続
		constexpr float kClimbAngle = 38.0f * (3.14159265f / 180.0f);
		Vector3 climbForward = Normalize(Add(
			Multiply(std::cos(kClimbAngle), clearHorizontalDir_),
			{ 0.0f, std::sin(kClimbAngle), 0.0f }
		));
		Quaternion qClimb = LookRotation(climbForward, { 0.0f, 1.0f, 0.0f });
		flightModel_.SetOrientation(qClimb);
		flightModel_.SetVelocity(Multiply(660.0f, flightModel_.GetForwardDirection()));

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
	// 項目4: 被弾・撃墜時のテーマカラー反転 & 警告バナー & 初期シグナル表示
	// ------------------------------------------------------------
	if (endSequence_ == StageEndSequence::GameOver_Critical) {
		float blinkPhase = std::fmod(endSeqTimer_, 0.12f);
		bool isBlinkOn = (blinkPhase < 0.07f);

		float jitterX = 0.0f;
		if (endSeqTimer_ >= 0.35f) {
			float p = (endSeqTimer_ - 0.35f) / 1.05f;
			jitterX = std::sin(endSeqTimer_ * 90.0f) * (2.5f * p);
		}

		if (isBlinkOn) {
			float bannerW = 740.0f;
			float bannerH = 74.0f;
			float bannerX = (screenW - bannerW) * 0.5f + jitterX;
			float bannerY = 110.0f;

			// 赤色半透明バナー背景
			DrawSolidRect({ bannerX, bannerY }, { bannerW, bannerH }, { 0.45f, 0.02f, 0.02f, 0.85f });
			DrawSolidRect({ bannerX, bannerY }, { bannerW, 2.0f }, { 1.0f, 0.25f, 0.20f, 0.95f });
			DrawSolidRect({ bannerX, bannerY + bannerH - 2.0f }, { bannerW, 2.0f }, { 1.0f, 0.25f, 0.20f, 0.95f });

			const char* mainTitle = isBailout_ ? "[ EMERGENCY BAILOUT // 脱出シーケンス ]" : "! CRITICAL DAMAGE // 操縦不能 !";
			const char* subTitle = isBailout_
				? "PILOT EJECTED // CANOPY JETTISONED // TELEMETRY CUT"
				: "AIRFRAME COMPROMISED // HYDRAULICS OFFLINE // EJECT RECOMMENDED";

			TextRenderer::GetInstance()->Print(
				"HackGen",
				mainTitle,
				screenW * 0.5f + jitterX,
				bannerY + 10.0f,
				32.0f,
				{ 1.0f, 0.40f, 0.35f, 1.0f },
				{ 0.5f, 0.0f }
			);

			TextRenderer::GetInstance()->Print(
				"HackGen",
				subTitle,
				screenW * 0.5f + jitterX,
				bannerY + 46.0f,
				14.0f,
				{ 1.0f, 0.85f, 0.85f, 0.90f },
				{ 0.5f, 0.0f }
			);
		}

		// 画面下部：シグナル健全度（0.35秒までは100%、その後徐々に40%へ低下）
		int sigPct = 100;
		if (endSeqTimer_ >= 0.35f) {
			float p = (endSeqTimer_ - 0.35f) / 1.05f;
			sigPct = std::clamp(static_cast<int>(100.0f - p * 60.0f), 0, 100);
		}
		char sigBuf[64];
		if (sigPct >= 90) {
			snprintf(sigBuf, sizeof(sigBuf), "CARRIER SIGNAL: %d%% [||||||||||] TELEMETRY ACTIVE", sigPct);
		} else {
			snprintf(sigBuf, sizeof(sigBuf), "CARRIER SIGNAL: %d%% [||||||||  ] LINK INTEGRITY DEGRADING", sigPct);
		}
		TextRenderer::GetInstance()->Print(
			"HackGen",
			sigBuf,
			screenW * 0.5f + jitterX,
			screenH - 70.0f,
			15.0f,
			{ 1.0f, 0.70f, 0.30f, 0.85f },
			{ 0.5f, 0.0f }
		);
	}

	// ------------------------------------------------------------
	// 項目1: 通信途絶（シグナル急減衰）& 砂嵐激化 & SIGNAL LOST & ノイズ中のブラウン管プツッ暗転
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::GameOver_SignalLost || endSequence_ == StageEndSequence::GameOver_CrtShutdown) {
		float degradeT = (endSequence_ == StageEndSequence::GameOver_CrtShutdown)
			? 1.0f
			: std::clamp((endSeqTimer_ - 1.4f) / 0.80f, 0.0f, 1.0f);
		int sigPct = (endSequence_ == StageEndSequence::GameOver_CrtShutdown)
			? 0
			: (std::max)(0, static_cast<int>(40.0f * (1.0f - degradeT / 0.45f)));
		float cardAlpha = std::clamp(degradeT / 0.20f, 0.0f, 1.0f);

		// 画面水平ジッター（致命的エラー・同期外れによる激しい横揺れ）
		float jitterX = std::sin(endSeqTimer_ * 110.0f) * (3.0f + 18.0f * degradeT);
		if (std::fmod(endSeqTimer_, 0.09f) < (0.02f + 0.04f * degradeT)) {
			jitterX += ((std::rand() % 24) - 12) * degradeT;
		}

		// 赤黒ノイズベール（シグナル低下に伴い濃縮）
		DrawSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.12f, 0.01f, 0.01f, (0.05f + 0.40f * degradeT) * cardAlpha });

		// 走査線インターレースグリッチ（数本が上下を高速に走る）
		float glitchOffset1 = std::fmod(endSeqTimer_ * 950.0f, screenH);
		DrawSolidRect({ 0.0f, glitchOffset1 }, { screenW, 22.0f }, { 0.95f, 0.95f, 1.0f, (0.08f + 0.32f * degradeT) }, BlendMode::kAdd);
		float glitchOffset2 = std::fmod(endSeqTimer_ * 1400.0f + 240.0f, screenH);
		DrawSolidRect({ 0.0f, glitchOffset2 }, { screenW, 44.0f }, { 1.0f, 0.15f, 0.15f, (0.10f + 0.35f * degradeT) }, BlendMode::kAdd);
		float glitchOffset3 = std::fmod(endSeqTimer_ * 600.0f + 480.0f, screenH);
		DrawSolidRect({ 0.0f, glitchOffset3 }, { screenW, 10.0f }, { 0.2f, 0.8f, 1.0f, (0.08f + 0.25f * degradeT) }, BlendMode::kAdd);

		// 中央警告カード（ジッター振動）
		float cardW = 780.0f;
		float cardH = 165.0f;
		float cardX = (screenW - cardW) * 0.5f + jitterX;
		float cardY = (screenH - cardH) * 0.5f;

		// カード背景と赤枠
		DrawSolidRect({ cardX, cardY }, { cardW, cardH }, { 0.08f, 0.01f, 0.01f, 0.94f * cardAlpha });
		DrawHollowBorder({ cardX, cardY }, { cardW, cardH }, { 1.0f, 0.25f, 0.20f, 0.95f * cardAlpha }, 2.5f);
		DrawSolidRect({ cardX, cardY }, { cardW, 4.0f }, { 1.0f, 0.40f, 0.40f, 1.0f * cardAlpha });

		if (sigPct > 0) {
			// シグナル減衰中
			float pulse = (0.7f + 0.3f * std::sin(endSeqTimer_ * 25.0f)) * cardAlpha;
			TextRenderer::GetInstance()->Print(
				"HackGen",
				"// TELEMETRY LINK CRITICAL // (通信減衰中)",
				screenW * 0.5f + jitterX,
				cardY + 18.0f,
				34.0f,
				{ 1.0f, 0.65f, 0.20f, pulse },
				{ 0.5f, 0.0f }
			);

			// シグナルバー表示
			int barTotal = 16;
			int barFilled = (sigPct * barTotal) / 100;
			std::string barStr = "[" + std::string(barFilled, '#') + std::string(barTotal - barFilled, '.') + "]";
			char buf[128];
			snprintf(buf, sizeof(buf), "SIGNAL STRENGTH: %s %d%%", barStr.c_str(), sigPct);
			TextRenderer::GetInstance()->Print(
				"HackGen",
				buf,
				screenW * 0.5f + jitterX,
				cardY + 68.0f,
				22.0f,
				{ 1.0f, 0.85f, 0.85f, 0.95f * cardAlpha },
				{ 0.5f, 0.0f }
			);

			char lossBuf[128];
			float lossPct = 100.0f - sigPct * 2.0f;
			if (lossPct < 0.0f) lossPct = 0.0f;
			if (lossPct > 99.9f) lossPct = 99.9f;
			snprintf(lossBuf, sizeof(lossBuf), "PACKET LOSS: %.1f%% // RF CARRIER DRIFTING // RETRY LINK...", lossPct);
			TextRenderer::GetInstance()->Print(
				"HackGen",
				lossBuf,
				screenW * 0.5f + jitterX,
				cardY + 112.0f,
				14.0f,
				{ 0.95f, 0.45f, 0.40f, 0.90f * cardAlpha },
				{ 0.5f, 0.0f }
			);
		} else {
			// 完全途絶 (SIGNAL LOST) - 燃え落ちる機体からの通信が途絶した決定打の瞬間！
			float signalPulse = (0.7f + 0.3f * std::sin(endSeqTimer_ * 30.0f)) * cardAlpha;
			const char* signalLostTitle = (std::fmod(endSeqTimer_, 0.12f) < 0.03f) ? "S!GN#L L0ST // ERR" : "// SIGNAL LOST // (通信途絶)";

			// RGBスプリット（シアン影＋赤影）
			TextRenderer::GetInstance()->Print(
				"HackGen",
				signalLostTitle,
				screenW * 0.5f + jitterX + 4.0f,
				cardY + 20.0f,
				44.0f,
				{ 0.10f, 0.85f, 1.0f, signalPulse * 0.6f },
				{ 0.5f, 0.0f }
			);
			TextRenderer::GetInstance()->Print(
				"HackGen",
				signalLostTitle,
				screenW * 0.5f + jitterX - 4.0f,
				cardY + 20.0f,
				44.0f,
				{ 1.0f, 0.15f, 0.15f, signalPulse * 0.8f },
				{ 0.5f, 0.0f }
			);
			TextRenderer::GetInstance()->Print(
				"HackGen",
				signalLostTitle,
				screenW * 0.5f + jitterX,
				cardY + 20.0f,
				44.0f,
				{ 1.0f, 0.95f, 0.95f, signalPulse },
				{ 0.5f, 0.0f }
			);

			TextRenderer::GetInstance()->Print(
				"HackGen",
				"NO CARRIER DETECTED // CONNECTION DROPPED",
				screenW * 0.5f + jitterX,
				cardY + 76.0f,
				22.0f,
				{ 1.0f, 0.85f, 0.85f, 0.95f * cardAlpha },
				{ 0.5f, 0.0f }
			);

			TextRenderer::GetInstance()->Print(
				"HackGen",
				"CARRIER FREQUENCY TERMINATED // SYSTEM FAULT 0x000000FA",
				screenW * 0.5f + jitterX,
				cardY + 114.0f,
				14.0f,
				{ 0.95f, 0.40f, 0.40f, 0.85f * cardAlpha },
				{ 0.5f, 0.0f }
			);
		}

		// ------------------------------------------------------------
		// 項目2: ノイズが激しく鳴り響く最中にブラウン管プツッ暗転（0.35秒で収束）
		//        ノイズを途切れさせず、ノイズとSIGNAL LOSTの真上から遮断マスクが閉じる！
		// ------------------------------------------------------------
		if (endSequence_ == StageEndSequence::GameOver_CrtShutdown) {
			float crtT = std::clamp((endSeqTimer_ - 2.2f) / 0.35f, 0.0f, 1.0f);
			float centerY = screenH * 0.5f;
			float centerX = screenW * 0.5f;

			if (crtT < 0.55f) {
				// Phase 1 (0.00〜0.55): ノイズとSIGNAL LOST画面の上下から黒幕が中央へ急速収束！
				float p = crtT / 0.55f;
				float collapse = 1.0f - std::pow(1.0f - p, 3.0f);
				float halfH = (screenH * 0.5f) * (1.0f - collapse);
				if (halfH < 2.0f) halfH = 2.0f;

				DrawSolidRect({ 0.0f, 0.0f }, { screenW, centerY - halfH }, { 0.0f, 0.0f, 0.0f, 1.0f });
				DrawSolidRect({ 0.0f, centerY + halfH }, { screenW, screenH - (centerY + halfH) }, { 0.0f, 0.0f, 0.0f, 1.0f });

				// 水平高輝度ビーム（砂嵐を圧縮した閃光線）
				float lineAlpha = 0.95f + 0.05f * std::sin(endSeqTimer_ * 90.0f);
				DrawSolidRect({ 0.0f, centerY - 3.0f }, { screenW, 6.0f }, { 0.95f, 0.98f, 1.0f, lineAlpha }, BlendMode::kAdd);
			} else if (crtT < 0.85f) {
				// Phase 2 (0.55〜0.85): 水平線が左右から中心へ急激に収縮！
				float p = (crtT - 0.55f) / 0.30f;
				float shrink = 1.0f - p;
				float halfW = (screenW * 0.5f) * shrink;

				DrawSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.0f, 0.0f, 0.0f, 1.0f });
				DrawSolidRect({ centerX - halfW, centerY - 3.0f }, { halfW * 2.0f, 6.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, BlendMode::kAdd);
			} else {
				// Phase 3 (0.85〜1.00): 「プツッ」と極小の白光点が閃光して即暗転完了（次シーンへ）
				float p = (crtT - 0.85f) / 0.15f;
				DrawSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.0f, 0.0f, 0.0f, 1.0f });

				if (p < 0.60f) {
					float popAlpha = 1.0f - (p / 0.60f);
					DrawSolidRect({ centerX - 5.0f, centerY - 5.0f }, { 10.0f, 10.0f }, { 1.0f, 1.0f, 1.0f, popAlpha }, BlendMode::kAdd);
				}
			}
		}
	}
	// ------------------------------------------------------------
	// 時間切れ離脱演出: TIME OVER & ミッションから離脱します.....
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::TimeOver_Abort || endSequence_ == StageEndSequence::TimeOver_Withdraw) {
		float fadeAlpha = std::clamp(endSeqTimer_ / 0.30f, 0.0f, 1.0f);

		// 点滅ドット計算 (1〜5個)
		int dotCount = 1 + static_cast<int>(endSeqTimer_ * 3.5f) % 5;
		std::string dots = std::string(dotCount, '.');

		// 画面上部〜中央の警告バナー
		float cardW = 760.0f;
		float cardH = 150.0f;
		float cardX = (screenW - cardW) * 0.5f;
		float cardY = 140.0f;

		// 半透明ダークアンバー背景
		DrawSolidRect({ cardX, cardY }, { cardW, cardH }, { 0.04f, 0.02f, 0.01f, 0.90f * fadeAlpha });
		DrawHollowBorder({ cardX, cardY }, { cardW, cardH }, { 1.0f, 0.70f, 0.15f, 0.95f * fadeAlpha }, 2.0f);
		DrawSolidRect({ cardX, cardY }, { cardW, 4.0f }, { 1.0f, 0.85f, 0.30f, 1.0f * fadeAlpha });
		DrawSolidRect({ cardX, cardY + cardH - 2.0f }, { cardW, 2.0f }, { 1.0f, 0.70f, 0.15f, 0.75f * fadeAlpha });

		// 大見出し「Time Over」
		float blink = (std::fmod(endSeqTimer_, 0.5f) < 0.38f) ? 1.0f : 0.65f;
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"Time Over",
			screenW * 0.5f,
			cardY + 16.0f,
			42.0f,
			{ 1.0f, 0.75f, 0.18f, fadeAlpha * blink },
			{ 0.5f, 0.0f }
		);

		// サブメッセージ「ミッションから離脱します.....」
		std::string msgStr = "ミッションから離脱します" + dots;
		TextRenderer::GetInstance()->Print(
			"HackGen",
			msgStr.c_str(),
			screenW * 0.5f,
			cardY + 70.0f,
			22.0f,
			{ 1.0f, 1.0f, 1.0f, fadeAlpha },
			{ 0.5f, 0.0f }
		);

		// ステータスフッター
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"[ OPERATION TIME EXPIRED // AUTOPILOT RTB ENGAGED ]",
			screenW * 0.5f,
			cardY + 114.0f,
			13.0f,
			{ 1.0f, 0.75f, 0.30f, 0.85f * fadeAlpha },
			{ 0.5f, 0.0f }
		);
	}
	// ------------------------------------------------------------
	// ステージクリア時の巨大ファンファーレUI（大きく「OPERATION CLEARED」演出）
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

		// リアルタイムGPS・衛星テレメトリ情報
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

		// 自機アイコン（画面中央）と円形走査レーダー波
		float pX = screenW * 0.5f;
		float pY = screenH * 0.5f;

		// レーダー走査パルス波
		float pulseCycle = std::fmod(endSeqTimer_ * 1.5f, 1.0f);
		float ringRad = 20.0f + pulseCycle * 140.0f;
		float ringAlpha = (1.0f - pulseCycle) * 0.70f;
		DrawHollowBorder({ pX - ringRad, pY - ringRad }, { ringRad * 2.0f, ringRad * 2.0f }, { 0.20f, 0.95f, 0.55f, ringAlpha }, 1.5f);

		// 自機アイコンシンボル
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
			float stampScale = 1.0f + 1.2f * (1.0f - stampT) * (1.0f - stampT);

			float boxW = 540.0f * stampScale;
			float boxH = 130.0f * stampScale;
			float boxX = (screenW - boxW) * 0.5f;
			float boxY = (screenH - boxH) * 0.5f;

			Vector4 stampCol = { 0.95f, 0.25f, 0.18f, 0.95f };
			DrawHollowBorder({ boxX, boxY }, { boxW, boxH }, stampCol, 4.0f * stampScale);
			DrawHollowBorder({ boxX + 6.0f * stampScale, boxY + 6.0f * stampScale }, { boxW - 12.0f * stampScale, boxH - 12.0f * stampScale }, stampCol, 1.5f * stampScale);

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
