#include "Missile.h"
#include <algorithm>
#include <cmath>
#include "../Enemy/Enemy.h"
#include "../Enemy/GroundEnemy.h"
#include "../Environment/TerrainChunkManager.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"

using namespace MyMath;

namespace {
	constexpr float kGravity = 9.81f;
	constexpr float kDegToRad = 3.14159265358979f / 180.0f;
	constexpr float kDefaultTerrainFallbackY = -485.0f;
}

void Missile::Spawn(
	const Vector3& position,
	const Vector3& initialVelocity,
	const Vector3& forwardDir,
	Enemy* targetEnemy,
	GroundEnemy* targetGround,
	const MissileSpec& spec
) {
	position_ = position;
	prevPosition_ = position;
	velocity_ = initialVelocity;
	forward_ = (Length(forwardDir) > 0.0001f) ? Normalize(forwardDir) : Vector3{ 0.0f, 0.0f, 1.0f };
	targetEnemy_ = targetEnemy;
	targetGround_ = targetGround;
	spec_ = spec;
	elapsedTime_ = 0.0f;
	isAlive_ = true;
	isMotorBurning_ = false;
}

void Missile::ClearTargets() {
	targetEnemy_ = nullptr;
	targetGround_ = nullptr;
}

void Missile::Update(float dt, TerrainChunkManager* terrain) {
	if (!isAlive_ || dt <= 0.0f) {
		return;
	}

	prevPosition_ = position_;
	elapsedTime_ += dt;

	// 最大飛翔時間で自爆
	if (elapsedTime_ >= spec_.maxLifeTime) {
		Detonate(false);
		return;
	}

	// ロケットモーター燃焼状態
	isMotorBurning_ = (elapsedTime_ >= spec_.initialBoostDelay) &&
	                  (elapsedTime_ < (spec_.initialBoostDelay + spec_.motorBurnTime));

	// 1. 推力・空気抵抗・重力の適用
	if (isMotorBurning_) {
		velocity_ = Add(velocity_, Multiply(spec_.motorThrustAccel * dt, forward_));
	}

	float speed = Length(velocity_);
	if (speed > 0.001f) {
		const float dragScale = spec_.dragCoeff * speed * dt;
		velocity_ = Subtract(velocity_, Multiply(dragScale, velocity_));
	}
	velocity_.y -= kGravity * dt;

	speed = Length(velocity_);
	if (speed > spec_.maxSpeed && speed > 0.001f) {
		velocity_ = Multiply(spec_.maxSpeed / speed, velocity_);
		speed = spec_.maxSpeed;
	}

	// 2. 比例航法（PN）誘導計算
	if (elapsedTime_ >= spec_.guidanceDelay && speed > 1.0f) {
		bool hasValidTarget = false;
		Vector3 targetPos{};
		Vector3 targetVel{};

		if (targetEnemy_) {
			if (targetEnemy_->IsAlive() && targetEnemy_->IsCollisionActive()) {
				targetPos = targetEnemy_->GetPosition();
				targetVel = targetEnemy_->GetVelocity();
				hasValidTarget = true;
			} else {
				targetEnemy_ = nullptr;
			}
		} else if (targetGround_) {
			if (targetGround_->IsAlive() && targetGround_->IsCollisionActive()) {
				targetPos = targetGround_->GetPosition();
				targetVel = { 0.0f, 0.0f, 0.0f };
				hasValidTarget = true;
			} else {
				targetGround_ = nullptr;
			}
		}

		if (hasValidTarget) {
			const Vector3 relPos = Subtract(targetPos, position_);
			const float range = Length(relPos);
			const float cosFovLimit = std::cos(spec_.seekerFovDeg * kDegToRad);

			if (range > spec_.maxTrackRange || range < 0.001f) {
				ClearTargets();
			} else {
				const Vector3 losDir = Multiply(1.0f / range, relPos);
				if (Dot(forward_, losDir) < cosFovLimit) {
					ClearTargets();
				} else if (range > 1.0f) {
					const Vector3 relVel = Subtract(targetVel, velocity_);
					const Vector3 omega = Multiply(1.0f / (range * range), Cross(relPos, relVel));
					const float closingSpeed = (std::max)(speed * 0.5f, -Dot(relPos, relVel) / range);
					const Vector3 velDir = Multiply(1.0f / speed, velocity_);

					// PN加速度指令（＋重力補償）
					Vector3 accelCmd = Multiply(spec_.proportionalNavGain * closingSpeed, Cross(omega, velDir));
					accelCmd.y += kGravity;

					// 進行方向と直交する法線加速度のみ抽出して最大G制限を適用
					Vector3 lateralAccel = Subtract(accelCmd, Multiply(Dot(accelCmd, velDir), velDir));
					const float maxLateralAccel = spec_.maxG * kGravity;
					const float lateralLen = Length(lateralAccel);
					if (lateralLen > maxLateralAccel && lateralLen > 0.0001f) {
						lateralAccel = Multiply(maxLateralAccel / lateralLen, lateralAccel);
					}

					velocity_ = Add(velocity_, Multiply(dt, lateralAccel));
					const float postSpeed = Length(velocity_);
					if (postSpeed > spec_.maxSpeed && postSpeed > 0.001f) {
						velocity_ = Multiply(spec_.maxSpeed / postSpeed, velocity_);
					}
				}
			}
		}
	}

	if (Length(velocity_) > 0.1f) {
		forward_ = Normalize(velocity_);
	}

	// 3. 位置更新
	position_ = Add(position_, Multiply(dt, velocity_));

	// 4. 地形衝突判定
	if (terrain && terrain->IsLoaded()) {
		const float groundY = terrain->GetTerrainHeightAt(position_.x, position_.z, kDefaultTerrainFallbackY);
		if (position_.y <= groundY) {
			position_.y = groundY;
			Detonate(true);
			return;
		}
	}

	// 5. 噴煙トレイル＆噴射炎パーティクル放出
	EmitTrailParticles();
}

void Missile::EmitTrailParticles() {
	ParticleManager* pm = ParticleManager::GetInstance();
	if (!pm) {
		return;
	}

	const Vector3 moveDelta = Subtract(position_, prevPosition_);
	const float moveDist = Length(moveDelta);
	const int steps = std::clamp(static_cast<int>(std::ceil(moveDist / 1.5f)), 1, 12);

	if (isMotorBurning_) {
		ParticleParameters smokeParams{};
		smokeParams.minVelocity = { -1.2f, -0.8f, -1.2f };
		smokeParams.maxVelocity = {  1.2f,  1.4f,  1.2f };
		smokeParams.randomPositionRange = 0.25f;
		smokeParams.minColor = { 0.86f, 0.88f, 0.90f, 0.72f };
		smokeParams.maxColor = { 0.96f, 0.97f, 0.98f, 0.88f };
		smokeParams.minLifeTime = 2.2f;
		smokeParams.maxLifeTime = 3.4f;
		smokeParams.minScale = 0.55f;
		smokeParams.maxScale = 0.95f;
		smokeParams.endScale = 4.2f;
		smokeParams.fadeOut = true;
		smokeParams.scaleEasing = 0.45f;
		smokeParams.minRotation = 0.0f;
		smokeParams.maxRotation = 6.283185f;
		smokeParams.minRotationSpeed = -1.2f;
		smokeParams.maxRotationSpeed = 1.2f;

		for (int i = 0; i < steps; ++i) {
			const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(steps);
			const Vector3 samplePos = Subtract(Lerp(prevPosition_, position_, t), Multiply(1.3f, forward_));
			pm->Emit("ExplosionSmoke", samplePos, smokeParams, 1);
		}

		// ノズル噴射炎
		ParticleParameters fireParams{};
		const Vector3 nozzlePos = Subtract(position_, Multiply(1.45f, forward_));
		fireParams.minVelocity = Multiply(-18.0f, forward_);
		fireParams.maxVelocity = Multiply(-8.0f, forward_);
		fireParams.randomPositionRange = 0.08f;
		fireParams.minColor = { 1.0f, 0.55f, 0.12f, 0.95f };
		fireParams.maxColor = { 1.0f, 0.95f, 0.60f, 1.00f };
		fireParams.minLifeTime = 0.04f;
		fireParams.maxLifeTime = 0.09f;
		fireParams.minScale = 0.45f;
		fireParams.maxScale = 0.75f;
		fireParams.endScale = 0.10f;
		fireParams.fadeOut = true;
		pm->Emit("ExplosionFire", nozzlePos, fireParams, 2);
	} else if (elapsedTime_ > spec_.initialBoostDelay) {
		// 燃焼終了後の慣性滑空時は薄い尾煙のみ残す
		ParticleParameters glideSmoke{};
		glideSmoke.minVelocity = { -0.5f, -0.4f, -0.5f };
		glideSmoke.maxVelocity = {  0.5f,  0.6f,  0.5f };
		glideSmoke.randomPositionRange = 0.15f;
		glideSmoke.minColor = { 0.75f, 0.77f, 0.80f, 0.22f };
		glideSmoke.maxColor = { 0.85f, 0.87f, 0.90f, 0.32f };
		glideSmoke.minLifeTime = 0.8f;
		glideSmoke.maxLifeTime = 1.4f;
		glideSmoke.minScale = 0.35f;
		glideSmoke.maxScale = 0.55f;
		glideSmoke.endScale = 1.6f;
		glideSmoke.fadeOut = true;
		glideSmoke.scaleEasing = 0.5f;

		if (steps >= 1) {
			const Vector3 tailPos = Subtract(position_, Multiply(1.3f, forward_));
			pm->Emit("ExplosionSmoke", tailPos, glideSmoke, 1);
		}
	}
}

void Missile::Detonate(bool hitExplosion) {
	if (!isAlive_) {
		return;
	}
	isAlive_ = false;
	isMotorBurning_ = false;

	if (EffectManager* em = EffectManager::GetInstance()) {
		if (hitExplosion) {
			em->EmitDestroyEffect(position_);
		} else {
			em->EmitHitEffect(position_);
		}
	}
}

void Missile::OnCollision([[maybe_unused]] ICollisionBody3D* other) {
	Detonate(true);
}
