#include "Enemy.h"
#include <algorithm>
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "../../engine/base/logger.h"

void Enemy::Initialize(
	const MyMath::Vector3& position,
	const std::string& modelPath,
	float maxHealth,
	const AirframeData& airframeData,
	const EngineData& engineData,
	const GunPodData& gunpodData,
	FlightModel* playerFlightModel,
	BulletManager* bulletManager,
	AIType aiType
) {
	maxHealth_ = maxHealth;
	health_ = maxHealth;
	isAlive_ = true;
	playerFlightModel_ = playerFlightModel;
	bulletManager_ = bulletManager;
	aiType_ = aiType;
	typeBState_ = TypeBState::Cruising;

	// フライトモデルと武装の初期化
	flightModel_.Initialize(airframeData, engineData);
	flightModel_.SetPosition(position);

	cruiseDirection_ = flightModel_.GetForwardDirection();
	if (aiType_ == AIType::CruiseEvade) {
		flightModel_.SetVelocity(MyMath::Multiply(50.0f / 3.6f, cruiseDirection_));
		flightModel_.SetThrottle(0.2f);
	} else {
		flightModel_.SetVelocity(MyMath::Multiply(200.0f / 3.6f, cruiseDirection_)); // 初期速度
		flightModel_.SetThrottle(0.8f);
	}

	gunpod_.Initialize(gunpodData);
	
	aimController_.Initialize();
	aimController_.SetEnabled(true);

	// モデルの読み込み
	ModelManager::GetInstance()->LoadModel(modelPath);

	// 3Dオブジェクトの生成
	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(Object3dCommon::GetInstance());
	object3d_->SetCamera(CameraManager::GetInstance()->GetActiveCamera());
	object3d_->SetModel(modelPath);

	// 見た目の設定 (敵機用により光沢を下げるなど適宜変更可能だが、今回は同じモデルを使用)
	object3d_->GetModel()->SetEnvironmentCoefficient(0.4f);
	object3d_->GetModel()->SetSpecularIntensity(1.5f);

	// スケール
	object3d_->SetScale({ 1.0f, 1.0f, 1.0f });
}

void Enemy::SetWaypointPath(const WaypointPath& path) {
	waypointPoints_ = path.points;
	waypointIsLoop_ = path.isLoop;
	currentWaypointIndex_ = EnemyWaypointDefaults::kInitialWaypointIndex;
	if (!waypointPoints_.empty()) {
		const MyMath::Vector3 toFirst = MyMath::Subtract(waypointPoints_.front().position, flightModel_.GetPosition());
		if (MyMath::Length(toFirst) > EnemyWaypointDefaults::kMinTargetDirectionLength) {
			cruiseDirection_ = MyMath::Normalize(toFirst);
			aimController_.ResetToDirection(cruiseDirection_);
		}
	}
}

// クォータニオンからオイラー角への変換ヘルパー
static MyMath::Vector3 QuaternionToEuler(const MyMath::Quaternion& q) {
	MyMath::Vector3 euler{};

	// ロール (X軸回り)
	float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
	float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	euler.x = std::atan2(sinr_cosp, cosr_cosp);

	// ピッチ (Y軸回り)
	float sinp = 2.0f * (q.w * q.y - q.z * q.x);
	if (std::fabs(sinp) >= 1.0f) {
		euler.y = std::copysign(3.14159265f / 2.0f, sinp);
	} else {
		euler.y = std::asin(sinp);
	}

	// ヨー (Z軸回り)
	float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
	float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	euler.z = std::atan2(siny_cosp, cosy_cosp);

	return euler;
}

void Enemy::UpdateAI(float deltaTime) {
	if (!playerFlightModel_) return;

	MyMath::Vector3 playerPos = playerFlightModel_->GetPosition();
	MyMath::Vector3 myPos = flightModel_.GetPosition();
	
	MyMath::Vector3 toPlayer = MyMath::Subtract(playerPos, myPos);
	float distance = MyMath::Length(toPlayer);

	if (aiType_ == AIType::ChaseAttack) {
		// 偏差射撃（リード）の計算：弾速と相手の速度から未来位置を予測する
		float bulletSpeed = flightModel_.GetSpeed() + 500.0f; // 弾速
		MyMath::Vector3 targetVel = playerFlightModel_->GetVelocity();
		
		// 1次予測
		float timeToHit = distance / bulletSpeed;
		MyMath::Vector3 futurePos = MyMath::Add(playerPos, MyMath::Multiply(timeToHit, targetVel));
		
		// 2次予測（より正確な着弾点）
		float distance2 = MyMath::Length(MyMath::Subtract(futurePos, myPos));
		timeToHit = distance2 / bulletSpeed;
		futurePos = MyMath::Add(playerPos, MyMath::Multiply(timeToHit, targetVel));

		MyMath::Vector3 toFuture = MyMath::Subtract(futurePos, myPos);

		// 目標方向を未来位置へセット
		if (distance > 1.0f) {
			MyMath::Vector3 targetDir = MyMath::Normalize(toFuture);
			aimController_.ResetToDirection(targetDir);
		}

		// 操舵入力の計算
		float finalPitch = 0.0f, finalRoll = 0.0f, finalYaw = 0.0f;
		aimController_.CalculateSteeringInput(flightModel_.GetOrientation(), deltaTime, finalPitch, finalRoll, finalYaw);
		flightModel_.SetControlInput(finalPitch, finalRoll, finalYaw);

		// スロットル調整 (遠ければ加速、近ければ減速)
		if (distance > 1000.0f) {
			flightModel_.SetThrottleInput(1.0f);
		} else if (distance < 300.0f) {
			flightModel_.SetThrottleInput(0.4f);
		} else {
			flightModel_.SetThrottleInput(0.7f);
		}

		// 攻撃判定
		MyMath::Vector3 forward = flightModel_.GetForwardDirection();
		// 予測位置(toFuture)に対して正面を向いているかで射撃判定を行う
		float dot = MyMath::Dot(forward, MyMath::Normalize(toFuture));
		// プレイヤー（の未来位置）が正面(約5度以内)におり、かつ距離が1000m以下なら射撃
		if (dot > 0.996f && distance < 1000.0f) {
			int ammoBefore = gunpod_.GetCurrentAmmo();
			gunpod_.Fire();
			
			if (gunpod_.GetCurrentAmmo() < ammoBefore) { // 発射できた場合
				MyMath::Vector3 firePos = flightModel_.GetPosition();
				MyMath::Vector3 aircraftVelocity = flightModel_.GetVelocity();
				
				// 機首から発射
				firePos = MyMath::Add(firePos, MyMath::Multiply(8.0f, forward));
				
				// 弾丸初速ベクトル ＝ 機体速度 ＋ (射出方向 × 射出速度)
				float muzzleSpeed = 500.0f;
				MyMath::Vector3 muzzleVelocity = MyMath::Multiply(muzzleSpeed, forward);
				MyMath::Vector3 bulletVelocity = MyMath::Add(aircraftVelocity, muzzleVelocity);

				if (bulletManager_) {
					bulletManager_->SpawnBullet(firePos, bulletVelocity, 10.0f, true);
				}

				// マズルフラッシュ
				muzzleFlashCount_++;
				if (muzzleFlashCount_ % 2 == 0) {
					EffectManager::GetInstance()->EmitMuzzleFlash(firePos, forward);
					muzzleFlashTimer_ = 0.08f;
				}
			}
		}
	}
	else if (aiType_ == AIType::CruiseEvade) {
		// ステートの切り替え条件
		if (typeBState_ == TypeBState::Cruising) {
			// プレイヤーが接近した時（例：500m以内）
			if (distance < 500.0f) {
				typeBState_ = TypeBState::Evading;
				logger::Log("[Enemy AI Type B] Switched to EVADING state. Distance to player: " + std::to_string(distance) + "\n");
			}
		} else if (typeBState_ == TypeBState::Evading) {
			// プレイヤーから離れた時（例：800m以上）
			if (distance > 800.0f) {
				typeBState_ = TypeBState::Cruising;
				logger::Log("[Enemy AI Type B] Switched to CRUISING state. Distance to player: " + std::to_string(distance) + "\n");
			}
		}

		MyMath::Vector3 targetDir = cruiseDirection_;
		float targetThrottle = 0.2f;

		if (typeBState_ == TypeBState::Evading) {
			// プレイヤーから離れるように進路を変更（回避行動）
			MyMath::Vector3 awayFromPlayer = MyMath::Subtract(myPos, playerPos);
			if (MyMath::Length(awayFromPlayer) > 0.001f) {
				targetDir = MyMath::Normalize(awayFromPlayer);
			}
			targetThrottle = 1.0f; // 回避時は加速（全力）
		}

		aimController_.ResetToDirection(targetDir);

		// 操舵入力の計算
		float finalPitch = 0.0f, finalRoll = 0.0f, finalYaw = 0.0f;
		aimController_.CalculateSteeringInput(flightModel_.GetOrientation(), deltaTime, finalPitch, finalRoll, finalYaw);
		flightModel_.SetControlInput(finalPitch, finalRoll, finalYaw);
		flightModel_.SetThrottleInput(targetThrottle);
	}
	else if (aiType_ == AIType::FollowWaypoint) {
		using namespace EnemyWaypointDefaults;
		if (waypointPoints_.empty()) {
			aimController_.ResetToDirection(cruiseDirection_);
			float finalPitch = kZero;
			float finalRoll = kZero;
			float finalYaw = kZero;
			aimController_.CalculateSteeringInput(flightModel_.GetOrientation(), deltaTime, finalPitch, finalRoll, finalYaw);
			flightModel_.SetControlInput(finalPitch, finalRoll, finalYaw);
			flightModel_.SetThrottleInput(kBaseCruiseThrottle);
			return;
		}

		if (currentWaypointIndex_ >= waypointPoints_.size()) {
			currentWaypointIndex_ = kInitialWaypointIndex;
		}

		const Waypoint& targetWp = waypointPoints_[currentWaypointIndex_];
		MyMath::Vector3 toWp = MyMath::Subtract(targetWp.position, myPos);
		const float distToWp = MyMath::Length(toWp);

		if (distToWp <= kWaypointCaptureRadiusMeters) {
			currentWaypointIndex_ += kWaypointStep;
			if (currentWaypointIndex_ >= waypointPoints_.size()) {
				if (waypointIsLoop_) {
					currentWaypointIndex_ = kInitialWaypointIndex;
				} else {
					aiType_ = AIType::ChaseAttack;
					return;
				}
			}
		}

		const Waypoint& activeWp = waypointPoints_[currentWaypointIndex_];
		const MyMath::Vector3 activeToWp = MyMath::Subtract(activeWp.position, myPos);
		MyMath::Vector3 targetDir = cruiseDirection_;
		if (MyMath::Length(activeToWp) > kMinTargetDirectionLength) {
			targetDir = MyMath::Normalize(activeToWp);
			cruiseDirection_ = targetDir;
		}

		aimController_.ResetToDirection(targetDir);
		float finalPitch = kZero;
		float finalRoll = kZero;
		float finalYaw = kZero;
		aimController_.CalculateSteeringInput(flightModel_.GetOrientation(), deltaTime, finalPitch, finalRoll, finalYaw);
		flightModel_.SetControlInput(finalPitch, finalRoll, finalYaw);

		const float desiredSpeedKmh = (activeWp.speed > kZero) ? activeWp.speed : kDefaultWaypointSpeedKmh;
		const float desiredSpeedMps = desiredSpeedKmh / kKmhPerMeterPerSec;
		const float speedError = desiredSpeedMps - flightModel_.GetSpeed();
		const float desiredThrottle = std::clamp(
			kBaseCruiseThrottle + speedError * kThrottleSpeedErrorGain,
			kMinThrottle,
			kMaxThrottle);
		flightModel_.SetThrottleInput(desiredThrottle);

		if (distance > kMinTargetDirectionLength && distance < kOpportunisticFireRangeMeters) {
			const MyMath::Vector3 forward = flightModel_.GetForwardDirection();
			const float dotToPlayer = MyMath::Dot(forward, MyMath::Normalize(toPlayer));
			if (dotToPlayer > kOpportunisticFireDotThreshold) {
				const int ammoBefore = gunpod_.GetCurrentAmmo();
				gunpod_.Fire();
				if (gunpod_.GetCurrentAmmo() < ammoBefore) {
					const MyMath::Vector3 firePos = MyMath::Add(
						flightModel_.GetPosition(),
						MyMath::Multiply(kMuzzleForwardOffset, forward));
					const MyMath::Vector3 muzzleVel = MyMath::Multiply(kBulletMuzzleSpeed, forward);
					const MyMath::Vector3 bulletVel = MyMath::Add(flightModel_.GetVelocity(), muzzleVel);
					if (bulletManager_) {
						bulletManager_->SpawnBullet(firePos, bulletVel, kBulletDamage, true);
					}
					muzzleFlashCount_++;
					if ((muzzleFlashCount_ % kMuzzleFlashSkipModulo) == kEvenRemainder) {
						EffectManager::GetInstance()->EmitMuzzleFlash(firePos, forward);
						muzzleFlashTimer_ = kMuzzleFlashDuration;
					}
				}
			}
		}
	}
}

void Enemy::Update(float deltaTime) {
	if (!isAlive_) { return; }

	UpdateAI(deltaTime);
	flightModel_.Update(deltaTime);

	if (muzzleFlashTimer_ > 0.0f) {
		muzzleFlashTimer_ -= deltaTime;
	}

	// オブジェクトに位置と姿勢を適用
	object3d_->SetTranslate(flightModel_.GetPosition());
	object3d_->SetRotate(QuaternionToEuler(flightModel_.GetOrientation()));
	object3d_->Update();
}

void Enemy::Draw() {
	if (!isAlive_) { return; }
	if (!object3d_) { return; }

	object3d_->Draw();
}

#include "../Bullet/Bullet.h"

void Enemy::TakeDamage(float damage) {
	if (!isAlive_) { return; }

	flightModel_.GetDamageModel().ProcessHit(DamagePart::Fuse, damage);
	if (flightModel_.GetAirframe().IsDestroyed()) {
		health_ = 0.0f;
		isAlive_ = false;
	}
}

void Enemy::OnCollision(ICollisionBody3D* other) {
	if (!isAlive_) { return; }

	// PlayerBullet との衝突 → パーツ被弾処理
	if (other->GetCollisionAttribute() & CollisionAttribute::kPlayerBullet) {
		auto* bullet = dynamic_cast<Bullet*>(other);
		Vector3 bulletPos = bullet ? bullet->GetPosition() : flightModel_.GetPosition();
		float dmg = bullet ? bullet->GetDamage() : 10.0f;

		HitResult res = flightModel_.ProcessBulletHit(bulletPos, dmg);
		EffectManager::GetInstance()->EmitHitEffect(bulletPos);

		if (flightModel_.GetAirframe().IsDestroyed()) {
			isAlive_ = false;
			EffectManager::GetInstance()->EmitDestroyEffect(flightModel_.GetPosition());
		}
	}
	// Player との衝突 → 即死して撃破エフェクト発生
	else if (other->GetCollisionAttribute() & CollisionAttribute::kPlayer) {
		isAlive_ = false;
		EffectManager::GetInstance()->EmitDestroyEffect(flightModel_.GetPosition());
	}
}
