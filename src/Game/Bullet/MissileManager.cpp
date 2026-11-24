#include "MissileManager.h"
#include <algorithm>
#include <cmath>
#include "../Enemy/Enemy.h"
#include "../Enemy/EnemyManager.h"
#include "../Enemy/GroundEnemy.h"
#include "../../engine/Graphics/Camera/Camera.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "../../engine/Graphics/System/BlendMode.h"
#include "../../engine/Graphics/System/TextureManager.h"

using namespace MyMath;

namespace {
	constexpr float kDegToRad = 3.14159265358979f / 180.0f;
	constexpr float kWarmupDurationSec = 0.35f;
	constexpr float kBaseLockTimeSec = 0.85f;
	constexpr float kAirMinLockRange = 2200.0f;
	constexpr float kAirMaxLockRange = 3500.0f;
	constexpr float kDroneLockRange = 2500.0f;
	constexpr float kGroundLockRange = 1800.0f;
	constexpr float kBoresightPreviewDist = 500.0f;

	Matrix4x4 MakeBasisWorldMatrix(
		const Vector3& xAxis,
		const Vector3& yAxis,
		const Vector3& zAxis,
		const Vector3& translate
	) {
		Matrix4x4 mat = Identity4x4();
		mat.m[0][0] = xAxis.x;     mat.m[0][1] = xAxis.y;     mat.m[0][2] = xAxis.z;     mat.m[0][3] = 0.0f;
		mat.m[1][0] = yAxis.x;     mat.m[1][1] = yAxis.y;     mat.m[1][2] = yAxis.z;     mat.m[1][3] = 0.0f;
		mat.m[2][0] = zAxis.x;     mat.m[2][1] = zAxis.y;     mat.m[2][2] = zAxis.z;     mat.m[2][3] = 0.0f;
		mat.m[3][0] = translate.x; mat.m[3][1] = translate.y; mat.m[3][2] = translate.z; mat.m[3][3] = 1.0f;
		return mat;
	}

	void BuildOrthonormalBasis(
		const Vector3& forwardDir,
		Vector3& outRight,
		Vector3& outUp,
		Vector3& outForward
	) {
		outForward = (Length(forwardDir) > 0.0001f) ? Normalize(forwardDir) : Vector3{ 0.0f, 0.0f, 1.0f };
		Vector3 upRef = { 0.0f, 1.0f, 0.0f };
		if (std::fabs(Dot(outForward, upRef)) > 0.99f) {
			upRef = { 0.0f, 0.0f, 1.0f };
		}
		outRight = Normalize(Cross(upRef, outForward));
		outUp = Normalize(Cross(outForward, outRight));
	}

	float DistancePointToSegment(const Vector3& point, const Vector3& segA, const Vector3& segB, Vector3& outClosest) {
		const Vector3 ab = Subtract(segB, segA);
		const float abLenSq = Dot(ab, ab);
		if (abLenSq <= 0.000001f) {
			outClosest = segA;
			return Length(Subtract(point, segA));
		}
		const Vector3 ap = Subtract(point, segA);
		const float t = std::clamp(Dot(ap, ab) / abLenSq, 0.0f, 1.0f);
		outClosest = Add(segA, Multiply(t, ab));
		return Length(Subtract(point, outClosest));
	}

	float ComputeAirTargetIrRange(const Enemy* enemy, const Vector3& toEnemyDir, float& outAspectFactor) {
		if (!enemy) {
			outAspectFactor = 1.0f;
			return 0.0f;
		}
		if (enemy->GetAIType() == AIType::CanyonDrone) {
			outAspectFactor = 1.05f;
			return kDroneLockRange;
		}
		if (enemy->GetAIType() == AIType::BarrageBalloon) {
			outAspectFactor = 1.15f;
			return 3200.0f;
		}
		const Vector3 enemyFwd = (Length(enemy->GetForward()) > 0.0001f)
			? Normalize(enemy->GetForward())
			: Vector3{ 0.0f, 0.0f, 1.0f };
		// 敵機と同じ向きに追いかけている（リアアスペクト）ほど高温排気が正面に入り長距離ロック可能
		const float rearDot = Dot(enemyFwd, toEnemyDir);
		const float t = std::clamp((rearDot + 1.0f) * 0.5f, 0.0f, 1.0f);
		outAspectFactor = 0.85f + 0.45f * t;
		return kAirMinLockRange + (kAirMaxLockRange - kAirMinLockRange) * t;
	}
}

MissileManager::~MissileManager() = default;

void MissileManager::Initialize(uint32_t maxActiveMissiles) {
	maxActiveMissiles_ = maxActiveMissiles;
	missiles_.resize(maxActiveMissiles_);
	Clear();
	ResetSeeker();
}

void MissileManager::Clear() {
	for (auto& missile : missiles_) {
		missile.ClearTargets();
		if (missile.IsAlive()) {
			missile.Detonate(false);
		}
	}
	missiles_.assign(maxActiveMissiles_, Missile{});
}

void MissileManager::ResetSeeker() {
	seekerStatus_ = MissileSeekerStatus{};
	warmupTimer_ = 0.0f;
	aspectLockSpeedMultiplier_ = 1.0f;
}

void MissileManager::SetModelPath(const std::string& modelPath) {
	customModelPath_ = modelPath;
	customModelPool_.clear();
	customModelPoolIndex_ = 0;
	if (!customModelPath_.empty()) {
		ModelManager::GetInstance()->LoadModel(customModelPath_);
	}
}

void MissileManager::UpdateSeeker(
	float dt,
	bool seekerEnabled,
	int remainingMissiles,
	const Vector3& aircraftPos,
	const Vector3& aircraftForward,
	EnemyManager& enemyManager
) {
	const Vector3 fwd = (Length(aircraftForward) > 0.0001f) ? Normalize(aircraftForward) : Vector3{ 0.0f, 0.0f, 1.0f };
	seekerStatus_.boresightWorldPoint = Add(aircraftPos, Multiply(kBoresightPreviewDist, fwd));

	if (!seekerEnabled || remainingMissiles <= 0) {
		seekerStatus_.state = MissileSeekerState::Off;
		seekerStatus_.warmupProgress = 0.0f;
		seekerStatus_.lockProgress = 0.0f;
		seekerStatus_.lockedAirTarget = nullptr;
		seekerStatus_.lockedGroundTarget = nullptr;
		seekerStatus_.hasTarget = false;
		seekerStatus_.targetDistance = 0.0f;
		seekerStatus_.seekerTrackWorldPoint = seekerStatus_.boresightWorldPoint;
		warmupTimer_ = 0.0f;
		return;
	}

	if (seekerStatus_.state == MissileSeekerState::Off) {
		seekerStatus_.state = MissileSeekerState::WarmingUp;
		warmupTimer_ = 0.0f;
		seekerStatus_.warmupProgress = 0.0f;
		seekerStatus_.lockProgress = 0.0f;
		seekerStatus_.lockedAirTarget = nullptr;
		seekerStatus_.lockedGroundTarget = nullptr;
		seekerStatus_.hasTarget = false;
		seekerStatus_.seekerTrackWorldPoint = seekerStatus_.boresightWorldPoint;
	}

	if (seekerStatus_.state == MissileSeekerState::WarmingUp) {
		warmupTimer_ += dt;
		seekerStatus_.warmupProgress = std::clamp(warmupTimer_ / kWarmupDurationSec, 0.0f, 1.0f);
		seekerStatus_.seekerTrackWorldPoint = seekerStatus_.boresightWorldPoint;
		if (warmupTimer_ >= kWarmupDurationSec) {
			seekerStatus_.state = MissileSeekerState::Searching;
			seekerStatus_.warmupProgress = 1.0f;
		}
		return;
	}

	if (seekerStatus_.state == MissileSeekerState::Searching) {
		seekerStatus_.lockProgress = 0.0f;
		seekerStatus_.seekerTrackWorldPoint = seekerStatus_.boresightWorldPoint;
		AcquireBestTarget(aircraftPos, fwd, enemyManager);
		return;
	}

	// Locking / Locked の維持検証
	float effectiveRange = 0.0f;
	float aspectFactor = 1.0f;
	ValidateCurrentTarget(aircraftPos, fwd, effectiveRange, aspectFactor);

	if (!seekerStatus_.hasTarget) {
		seekerStatus_.state = MissileSeekerState::Searching;
		seekerStatus_.lockProgress = 0.0f;
		seekerStatus_.seekerTrackWorldPoint = seekerStatus_.boresightWorldPoint;
		return;
	}

	if (seekerStatus_.state == MissileSeekerState::Locking) {
		const float rangeFactor = (effectiveRange > 1.0f)
			? std::clamp(1.25f - 0.35f * (seekerStatus_.targetDistance / effectiveRange), 0.90f, 1.30f)
			: 1.0f;
		seekerStatus_.lockProgress += (dt / kBaseLockTimeSec) * aspectFactor * rangeFactor;
		if (seekerStatus_.lockProgress >= 1.0f) {
			seekerStatus_.lockProgress = 1.0f;
			seekerStatus_.state = MissileSeekerState::Locked;
		}
	}
}

void MissileManager::ValidateCurrentTarget(
	const Vector3& aircraftPos,
	const Vector3& aircraftForward,
	float& outEffectiveRange,
	float& outAspectFactor
) {
	outEffectiveRange = 0.0f;
	outAspectFactor = 1.0f;
	seekerStatus_.hasTarget = false;

	Vector3 targetPos{};
	bool candidateAlive = false;

	if (seekerStatus_.lockedAirTarget) {
		Enemy* e = seekerStatus_.lockedAirTarget;
		if (e->IsAlive() && e->IsCollisionActive() && !e->IsAlly()) {
			targetPos = e->GetPosition();
			candidateAlive = true;
		}
	} else if (seekerStatus_.lockedGroundTarget) {
		GroundEnemy* g = seekerStatus_.lockedGroundTarget;
		if (g->IsAlive() && g->IsCollisionActive() && g->IsHostile()) {
			targetPos = g->GetPosition();
			candidateAlive = true;
		}
	}

	if (!candidateAlive) {
		seekerStatus_.lockedAirTarget = nullptr;
		seekerStatus_.lockedGroundTarget = nullptr;
		return;
	}

	const Vector3 toTarget = Subtract(targetPos, aircraftPos);
	const float dist = Length(toTarget);
	if (dist < 1.0f) {
		seekerStatus_.lockedAirTarget = nullptr;
		seekerStatus_.lockedGroundTarget = nullptr;
		return;
	}

	const Vector3 toTargetDir = Multiply(1.0f / dist, toTarget);
	if (seekerStatus_.lockedAirTarget) {
		outEffectiveRange = ComputeAirTargetIrRange(seekerStatus_.lockedAirTarget, toTargetDir, outAspectFactor);
	} else {
		outEffectiveRange = kGroundLockRange;
		outAspectFactor = 0.95f;
	}

	const float cosGimbal = std::cos(seekerStatus_.outerGimbalAngleDeg * kDegToRad);
	if (dist > outEffectiveRange || Dot(aircraftForward, toTargetDir) < cosGimbal) {
		seekerStatus_.lockedAirTarget = nullptr;
		seekerStatus_.lockedGroundTarget = nullptr;
		return;
	}

	seekerStatus_.hasTarget = true;
	seekerStatus_.targetDistance = dist;
	seekerStatus_.seekerTrackWorldPoint = targetPos;
}

void MissileManager::AcquireBestTarget(
	const Vector3& aircraftPos,
	const Vector3& aircraftForward,
	EnemyManager& enemyManager
) {
	const float cosInnerAcq = std::cos(seekerStatus_.innerAcqAngleDeg * kDegToRad);
	float bestDot = cosInnerAcq;
	Enemy* bestAir = nullptr;
	GroundEnemy* bestGround = nullptr;
	Vector3 bestPos{};
	float bestDist = 0.0f;

	for (auto* enemy : enemyManager.GetAliveEnemies()) {
		if (!enemy || !enemy->IsAlive() || enemy->IsAlly() || !enemy->IsCollisionActive()) {
			continue;
		}
		const Vector3 pos = enemy->GetPosition();
		const Vector3 toTarget = Subtract(pos, aircraftPos);
		const float dist = Length(toTarget);
		if (dist < 1.0f) {
			continue;
		}
		const Vector3 dir = Multiply(1.0f / dist, toTarget);
		float aspectDummy = 1.0f;
		const float maxIrRange = ComputeAirTargetIrRange(enemy, dir, aspectDummy);
		if (dist > maxIrRange) {
			continue;
		}
		const float d = Dot(aircraftForward, dir);
		if (d >= bestDot) {
			bestDot = d;
			bestAir = enemy;
			bestGround = nullptr;
			bestPos = pos;
			bestDist = dist;
		}
	}

	for (auto* ground : enemyManager.GetAliveGroundEnemies()) {
		if (!ground || !ground->IsAlive() || !ground->IsHostile() || !ground->IsCollisionActive()) {
			continue;
		}
		const Vector3 pos = ground->GetPosition();
		const Vector3 toTarget = Subtract(pos, aircraftPos);
		const float dist = Length(toTarget);
		if (dist < 1.0f || dist > kGroundLockRange) {
			continue;
		}
		const Vector3 dir = Multiply(1.0f / dist, toTarget);
		const float d = Dot(aircraftForward, dir);
		if (d >= bestDot) {
			bestDot = d;
			bestAir = nullptr;
			bestGround = ground;
			bestPos = pos;
			bestDist = dist;
		}
	}

	if (bestAir || bestGround) {
		seekerStatus_.lockedAirTarget = bestAir;
		seekerStatus_.lockedGroundTarget = bestGround;
		seekerStatus_.hasTarget = true;
		seekerStatus_.targetDistance = bestDist;
		seekerStatus_.seekerTrackWorldPoint = bestPos;
		seekerStatus_.lockProgress = 0.0f;
		seekerStatus_.state = MissileSeekerState::Locking;
	}
}

bool MissileManager::LaunchLockedMissile(
	const Vector3& spawnWorldPos,
	const Vector3& aircraftVelocity,
	const Vector3& aircraftForward,
	const MissileSpec& spec
) {
	if (seekerStatus_.state != MissileSeekerState::Locked) {
		return false;
	}
	if (!seekerStatus_.lockedAirTarget && !seekerStatus_.lockedGroundTarget) {
		return false;
	}

	const Vector3 fwd = (Length(aircraftForward) > 0.0001f) ? Normalize(aircraftForward) : Vector3{ 0.0f, 0.0f, 1.0f };
	const Vector3 initialVel = Add(aircraftVelocity, Multiply(25.0f, fwd));

	for (auto& missile : missiles_) {
		if (!missile.IsAlive()) {
			missile.Spawn(
				spawnWorldPos,
				initialVel,
				fwd,
				seekerStatus_.lockedAirTarget,
				seekerStatus_.lockedGroundTarget,
				spec
			);

			// 発射パフ（白煙＋点火火花）
			if (ParticleManager* pm = ParticleManager::GetInstance()) {
				ParticleParameters launchSmoke{};
				launchSmoke.minVelocity = Add(aircraftVelocity, { -4.0f, -4.0f, -4.0f });
				launchSmoke.maxVelocity = Add(aircraftVelocity, {  4.0f,  4.0f,  4.0f });
				launchSmoke.randomPositionRange = 0.4f;
				launchSmoke.minColor = { 0.90f, 0.92f, 0.95f, 0.75f };
				launchSmoke.maxColor = { 1.00f, 1.00f, 1.00f, 0.90f };
				launchSmoke.minLifeTime = 0.6f;
				launchSmoke.maxLifeTime = 1.1f;
				launchSmoke.minScale = 0.8f;
				launchSmoke.maxScale = 1.4f;
				launchSmoke.endScale = 3.6f;
				launchSmoke.fadeOut = true;
				pm->Emit("ExplosionSmoke", spawnWorldPos, launchSmoke, 6);

				ParticleParameters launchSpark{};
				launchSpark.minVelocity = Multiply(-20.0f, fwd);
				launchSpark.maxVelocity = Multiply(-6.0f, fwd);
				launchSpark.randomPositionRange = 0.25f;
				launchSpark.minColor = { 1.0f, 0.65f, 0.15f, 1.0f };
				launchSpark.maxColor = { 1.0f, 0.95f, 0.55f, 1.0f };
				launchSpark.minLifeTime = 0.12f;
				launchSpark.maxLifeTime = 0.24f;
				launchSpark.minScale = 0.35f;
				launchSpark.maxScale = 0.65f;
				launchSpark.endScale = 0.0f;
				launchSpark.fadeOut = true;
				pm->Emit("ExplosionFire", spawnWorldPos, launchSpark, 4);
			}

			// 発射後は即再索敵へ戻す
			seekerStatus_.state = MissileSeekerState::Searching;
			seekerStatus_.lockProgress = 0.0f;
			seekerStatus_.lockedAirTarget = nullptr;
			seekerStatus_.lockedGroundTarget = nullptr;
			seekerStatus_.hasTarget = false;
			return true;
		}
	}
	return false;
}

void MissileManager::Update(float dt, EnemyManager& enemyManager, TerrainChunkManager* terrain) {
	for (auto& missile : missiles_) {
		if (!missile.IsAlive()) {
			continue;
		}
		missile.Update(dt, terrain);
		if (missile.IsAlive() && missile.IsArmed()) {
			CheckProximityFuseAndBlast(missile, enemyManager);
		}
	}
}

void MissileManager::CheckProximityFuseAndBlast(Missile& missile, EnemyManager& enemyManager) {
	const MissileSpec& spec = missile.GetSpec();
	const Vector3 segA = missile.GetPrevPosition();
	const Vector3 segB = missile.GetPosition();

	bool triggered = false;
	Vector3 explodePos = segB;

	for (auto* enemy : enemyManager.GetAliveEnemies()) {
		if (!enemy || !enemy->IsAlive() || enemy->IsAlly() || !enemy->IsCollisionActive()) {
			continue;
		}
		Vector3 closest{};
		const float dist = DistancePointToSegment(enemy->GetPosition(), segA, segB, closest);
		if (dist <= (spec.proximityFuseRadius + enemy->GetCollisionRadius())) {
			triggered = true;
			explodePos = closest;
			break;
		}
	}

	if (!triggered) {
		for (auto* ground : enemyManager.GetAliveGroundEnemies()) {
			if (!ground || !ground->IsAlive() || !ground->IsHostile() || !ground->IsCollisionActive()) {
				continue;
			}
			Vector3 closest{};
			const float dist = DistancePointToSegment(ground->GetPosition(), segA, segB, closest);
			if (dist <= (spec.proximityFuseRadius + ground->GetParam().collisionRadius)) {
				triggered = true;
				explodePos = closest;
				break;
			}
		}
	}

	if (!triggered) {
		return;
	}

	missile.Detonate(true);

	// 爆風範囲内の全敵に弾頭ダメージを適用
	for (auto* enemy : enemyManager.GetAliveEnemies()) {
		if (!enemy || !enemy->IsAlive() || enemy->IsAlly() || !enemy->IsCollisionActive()) {
			continue;
		}
		const float dist = Length(Subtract(enemy->GetPosition(), explodePos));
		if (dist <= (spec.blastRadius + enemy->GetCollisionRadius())) {
			enemy->ApplyMissileDamage(spec.warheadDamage, explodePos);
		}
	}

	for (auto* ground : enemyManager.GetAliveGroundEnemies()) {
		if (!ground || !ground->IsAlive() || !ground->IsHostile() || !ground->IsCollisionActive()) {
			continue;
		}
		const float dist = Length(Subtract(ground->GetPosition(), explodePos));
		if (dist <= (spec.blastRadius + ground->GetParam().collisionRadius)) {
			ground->TakeDamage(spec.warheadDamage);
		}
	}
}

void MissileManager::Draw(Camera* camera) {
	if (!camera) {
		return;
	}
	customModelPoolIndex_ = 0;

	for (const auto& missile : missiles_) {
		if (!missile.IsAlive()) {
			continue;
		}
		Vector3 right{}, up{}, fwd{};
		BuildOrthonormalBasis(missile.GetForward(), right, up, fwd);
		DrawMissileVisual(missile.GetPosition(), right, up, fwd, missile.IsMotorBurning(), camera);
	}
}

void MissileManager::DrawMountedMissile(
	const Vector3& worldPos,
	const Vector3& right,
	const Vector3& up,
	const Vector3& forward,
	Camera* camera
) {
	if (!camera) {
		return;
	}
	DrawMissileVisual(worldPos, right, up, forward, false, camera);
}

void MissileManager::DrawMissileVisual(
	const Vector3& worldPos,
	const Vector3& right,
	const Vector3& up,
	const Vector3& forward,
	bool isBurning,
	Camera* camera
) {
	// カスタム3Dモデルが設定されている場合はObject3dプール経由で描画
	if (!customModelPath_.empty()) {
		if (customModelPoolIndex_ >= customModelPool_.size()) {
			auto obj = std::make_unique<Object3d>();
			obj->Initialize(Object3dCommon::GetInstance());
			obj->SetModel(customModelPath_);
			customModelPool_.push_back(std::move(obj));
		}
		Object3d* obj = customModelPool_[customModelPoolIndex_++].get();
		obj->SetCamera(camera);
		const Matrix4x4 worldMat = MakeBasisWorldMatrix(right, up, forward, worldPos);
		obj->UpdateWithWorldMatrix(worldMat);
		obj->Draw();
		return;
	}

	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim) {
		return;
	}

	const uint32_t whiteTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");
	const Vector3 rNorm = Normalize(right);
	const Vector3 uNorm = Normalize(up);
	const Vector3 fNorm = Normalize(forward);

	// 1. 弾体シリンダー（長さ 2.6m, 半径 0.11m）
	{
		constexpr float kBodyRadius = 0.11f;
		constexpr float kBodyHalfLen = 1.30f;
		const Matrix4x4 bodyMat = MakeBasisWorldMatrix(
			Multiply(kBodyRadius, rNorm),
			Multiply(kBodyHalfLen, fNorm),
			Multiply(-kBodyRadius, uNorm),
			worldPos
		);
		prim->DrawCylinder(bodyMat, { 0.88f, 0.90f, 0.92f, 1.0f }, whiteTex, camera, BlendMode::kOpaque);
	}

	// 2. 先端IRシーカーコーン（長さ 0.55m）
	{
		constexpr float kConeRadius = 0.11f;
		constexpr float kConeHalfLen = 0.275f;
		const Vector3 conePos = Add(worldPos, Multiply(1.30f + kConeHalfLen, fNorm));
		const Matrix4x4 coneMat = MakeBasisWorldMatrix(
			Multiply(kConeRadius, rNorm),
			Multiply(kConeHalfLen, fNorm),
			Multiply(-kConeRadius, uNorm),
			conePos
		);
		prim->DrawCone(coneMat, { 0.18f, 0.20f, 0.24f, 1.0f }, whiteTex, camera, BlendMode::kOpaque);
	}

	// 3. 弾頭識別赤帯
	{
		constexpr float kBandRadius = 0.116f;
		constexpr float kBandHalfLen = 0.08f;
		const Vector3 bandPos = Add(worldPos, Multiply(0.85f, fNorm));
		const Matrix4x4 bandMat = MakeBasisWorldMatrix(
			Multiply(kBandRadius, rNorm),
			Multiply(kBandHalfLen, fNorm),
			Multiply(-kBandRadius, uNorm),
			bandPos
		);
		prim->DrawCylinder(bandMat, { 0.88f, 0.20f, 0.16f, 1.0f }, whiteTex, camera, BlendMode::kOpaque);
	}

	// 4. 後部十字安定翼
	const Vector4 finColor = { 0.28f, 0.31f, 0.36f, 1.0f };
	{
		const Vector3 tailPos = Subtract(worldPos, Multiply(1.05f, fNorm));
		const Matrix4x4 hFinMat = MakeBasisWorldMatrix(
			Multiply(0.32f, rNorm),
			uNorm,
			Multiply(0.24f, fNorm),
			tailPos
		);
		prim->DrawPlane(hFinMat, finColor, whiteTex, camera, BlendMode::kOpaque);

		const Matrix4x4 vFinMat = MakeBasisWorldMatrix(
			Multiply(0.32f, uNorm),
			Multiply(-1.0f, rNorm),
			Multiply(0.24f, fNorm),
			tailPos
		);
		prim->DrawPlane(vFinMat, finColor, whiteTex, camera, BlendMode::kOpaque);
	}

	// 5. 前部カナード翼
	{
		const Vector3 canardPos = Add(worldPos, Multiply(0.80f, fNorm));
		const Matrix4x4 hCanardMat = MakeBasisWorldMatrix(
			Multiply(0.19f, rNorm),
			uNorm,
			Multiply(0.13f, fNorm),
			canardPos
		);
		prim->DrawPlane(hCanardMat, finColor, whiteTex, camera, BlendMode::kOpaque);

		const Matrix4x4 vCanardMat = MakeBasisWorldMatrix(
			Multiply(0.19f, uNorm),
			Multiply(-1.0f, rNorm),
			Multiply(0.13f, fNorm),
			canardPos
		);
		prim->DrawPlane(vCanardMat, finColor, whiteTex, camera, BlendMode::kOpaque);
	}

	// 6. 燃焼中ロケットノズル噴射炎（後方へ伸びる加算コーン）
	if (isBurning) {
		constexpr float kFlameRadius = 0.18f;
		constexpr float kFlameHalfLen = 0.85f;
		const Vector3 flamePos = Subtract(worldPos, Multiply(1.30f + kFlameHalfLen, fNorm));
		const Matrix4x4 flameMat = MakeBasisWorldMatrix(
			Multiply(kFlameRadius, rNorm),
			Multiply(-kFlameHalfLen, fNorm),
			Multiply(kFlameRadius, uNorm),
			flamePos
		);
		prim->DrawCone(flameMat, { 1.0f, 0.72f, 0.25f, 0.92f }, whiteTex, camera, BlendMode::kAdd);
	}
}

uint32_t MissileManager::GetActiveMissileCount() const {
	uint32_t count = 0;
	for (const auto& missile : missiles_) {
		if (missile.IsAlive()) {
			++count;
		}
	}
	return count;
}
