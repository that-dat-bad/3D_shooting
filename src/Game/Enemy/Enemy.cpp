#include "Enemy.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "BehaviorTree/EnemyBehaviorNodes.h"
#include "../Bullet/Bullet.h"
#include "../Environment/TerrainChunkManager.h"
#include "../Graphics/DebrisManager.h"
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "../../engine/base/logger.h"

namespace {
	constexpr size_t kCanyonControlPointCount = 9;
	constexpr std::array<float, kCanyonControlPointCount> kCanyonControlZ = {
		500.0f, 2500.0f, 4500.0f, 6500.0f, 9000.0f, 11500.0f, 14000.0f, 16500.0f, 18500.0f
	};
	constexpr std::array<float, kCanyonControlPointCount> kCanyonControlX = {
		220.0f, 220.0f, -150.0f, -450.0f, -750.0f, -750.0f, -500.0f, -150.0f, -100.0f
	};

	float SmoothStep01(float t) {
		const float c = std::clamp(t, 0.0f, 1.0f);
		return c * c * (3.0f - 2.0f * c);
	}

	void BuildAircraftBasis(
		const MyMath::Vector3& forwardDir,
		float rollRad,
		MyMath::Vector3& outRight,
		MyMath::Vector3& outUp,
		MyMath::Vector3& outForward) {
		const float fwdLen = MyMath::Length(forwardDir);
		if (fwdLen > 0.0001f) {
			outForward = MyMath::Multiply(1.0f / fwdLen, forwardDir);
		} else {
			outForward = { 0.0f, 0.0f, 1.0f };
		}

		MyMath::Vector3 upRef = { 0.0f, 1.0f, 0.0f };
		if (std::fabs(MyMath::Dot(outForward, upRef)) > 0.99f) {
			upRef = { 0.0f, 0.0f, 1.0f };
		}

		const MyMath::Vector3 baseRight = MyMath::Normalize(MyMath::Cross(upRef, outForward));
		const MyMath::Vector3 baseUp = MyMath::Normalize(MyMath::Cross(outForward, baseRight));

		const float cosR = std::cos(rollRad);
		const float sinR = std::sin(rollRad);

		outRight = MyMath::Add(MyMath::Multiply(cosR, baseRight), MyMath::Multiply(sinR, baseUp));
		outUp = MyMath::Add(MyMath::Multiply(-sinR, baseRight), MyMath::Multiply(cosR, baseUp));
	}

	MyMath::Matrix4x4 BuildWorldMatrixFromBasis(
		const MyMath::Vector3& right,
		const MyMath::Vector3& up,
		const MyMath::Vector3& forward,
		const MyMath::Vector3& position) {
		MyMath::Matrix4x4 mat = MyMath::Identity4x4();
		mat.m[0][0] = right.x;    mat.m[0][1] = right.y;    mat.m[0][2] = right.z;    mat.m[0][3] = 0.0f;
		mat.m[1][0] = up.x;       mat.m[1][1] = up.y;       mat.m[1][2] = up.z;       mat.m[1][3] = 0.0f;
		mat.m[2][0] = forward.x;  mat.m[2][1] = forward.y;  mat.m[2][2] = forward.z;  mat.m[2][3] = 0.0f;
		mat.m[3][0] = position.x; mat.m[3][1] = position.y; mat.m[3][2] = position.z; mat.m[3][3] = 1.0f;
		return mat;
	}
}

Enemy::Enemy() = default;
Enemy::~Enemy() {
	if (damageSmokeEffect_) {
		damageSmokeEffect_->Delete();
		damageSmokeEffect_ = nullptr;
	}
	if (crashPlumeEffect_) {
		crashPlumeEffect_->Delete();
		crashPlumeEffect_ = nullptr;
	}
}

float Enemy::EvaluateCanyonCenterX(float worldZ) {
	if (worldZ <= kCanyonControlZ.front()) {
		return kCanyonControlX.front();
	}
	if (worldZ >= kCanyonControlZ.back()) {
		return kCanyonControlX.back();
	}

	for (size_t i = 0; i + 1 < kCanyonControlPointCount; ++i) {
		const float z0 = kCanyonControlZ[i];
		const float z1 = kCanyonControlZ[i + 1];
		if (worldZ >= z0 && worldZ <= z1) {
			const float span = z1 - z0;
			const float rawT = (span > 0.0001f) ? ((worldZ - z0) / span) : 0.0f;
			const float smoothT = SmoothStep01(rawT);
			const float x0 = kCanyonControlX[i];
			const float x1 = kCanyonControlX[i + 1];
			return x0 + (x1 - x0) * smoothT;
		}
	}
	return kCanyonControlX.back();
}

float Enemy::QueryTerrainY(float worldX, float worldZ) const {
	if (terrain_ && terrain_->IsLoaded()) {
		return terrain_->GetTerrainHeightAt(worldX, worldZ, -485.0f);
	}
	return -485.0f;
}

void Enemy::Initialize(
	const MyMath::Vector3& position,
	const std::string& modelPath,
	float maxHealth,
	const AirframeData& airframeData,
	const EngineData& engineData,
	const GunPodData& gunpodData,
	FlightModel* playerFlightModel,
	BulletManager* bulletManager,
	AIType aiType,
	const std::string& behaviorTreeJson
) {
	maxHealth_ = maxHealth;
	health_ = maxHealth;
	isAlive_ = true;
	hitMarkerTriggered_ = false;
	isInDivePass_ = false;
	isDownedSpiral_ = false;
	isCrashedFloor_ = false;
	useVisualModel_ = false;
	brokenPart_ = 0;
	pendingBrokenPartLog_.clear();
	playerFlightModel_ = playerFlightModel;
	bulletManager_ = bulletManager;
	aiType_ = aiType;
	behaviorTreeJson_ = behaviorTreeJson;
	typeBState_ = TypeBState::Cruising;
	specialAnimTimer_ = 0.0f;
	specialPhaseTimer_ = 0.0f;
	hasLastWingTips_ = false;

	if (damageSmokeEffect_) {
		damageSmokeEffect_->Delete();
		damageSmokeEffect_ = nullptr;
	}
	if (crashPlumeEffect_) {
		crashPlumeEffect_->Delete();
		crashPlumeEffect_ = nullptr;
	}

	// フライトモデルと武装の初期化
	flightModel_.Initialize(airframeData, engineData);
	flightModel_.SetPosition(position);

	cruiseDirection_ = flightModel_.GetForwardDirection();
	if (aiType_ == AIType::CruiseEvade) {
		flightModel_.SetVelocity(MyMath::Multiply(50.0f / 3.6f, cruiseDirection_));
		flightModel_.SetThrottle(0.2f);
	} else {
		flightModel_.SetVelocity(MyMath::Multiply(200.0f / 3.6f, cruiseDirection_));
		flightModel_.SetThrottle(0.8f);
	}

	gunpod_.Initialize(gunpodData);
	aimController_.Initialize();
	aimController_.SetEnabled(true);

	TextureManager::GetInstance()->LoadTexture("assets/textures/circle2.png");
	ringTexIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");

	// モデルの読み込み
	ModelManager::GetInstance()->LoadModel(modelPath);
	Camera* activeCam = CameraManager::GetInstance()->GetActiveCamera();

	if (aiType_ == AIType::AllyWingman) {
		visualModel_.Initialize(Object3dCommon::GetInstance(), activeCam);
		visualModel_.SetupFromSingleModel(modelPath);
		visualModel_.SetMaterialProperties(48.0f, 1.15f, 0.42f);
		visualModel_.SetupCanopyMaterial();
		visualModel_.ResetAllPartsVisible();

		afterburner_ = std::make_unique<Afterburner>();
		afterburner_->Initialize();
		afterburner_->SetTwinEngine(false);
		afterburner_->SetSingleNozzleOffset({ 0.0f, 0.011f, 7.60f });
		afterburner_->SetFlameRadius(0.38f);
		afterburner_->SetFlameLength(5.8f);
		afterburner_->SetShockDiamondCount(4);
		afterburner_->SetFlameColor({ 0.28f, 0.65f, 1.0f });
		afterburner_->SetCoreColor({ 0.88f, 0.95f, 1.0f });
		afterburner_->SetIntensity(0.85f);
		afterburner_->LoadConfig("assets/configs/afterburner_m21.json");

		specialPos_ = position;
		specialForward_ = { 0.0f, 0.0f, 1.0f };
		specialVel_ = { 0.0f, 0.0f, 68.0f };
		specialRollRad_ = 0.0f;
		specialFireTimer_ = 0.0f;
		flightModel_.SetPosition(specialPos_);
		flightModel_.SetVelocity(specialVel_);

		MyMath::Vector3 right{}, up{}, fwd{};
		BuildAircraftBasis(specialForward_, specialRollRad_, right, up, fwd);
		visualModel_.Update(BuildWorldMatrixFromBasis(right, up, fwd, specialPos_), 0.0f);
		return;
	}

	if (aiType_ == AIType::CanyonDrone) {
		collisionRadius_ = 9.5f;
		const MyMath::Vector3 playerPos = GetPlayerPosition();
		droneCruiseZ_ = (position.z >= (playerPos.z + 600.0f)) ? position.z : (playerPos.z + 380.0f);
		const float floorY = QueryTerrainY(position.x, droneCruiseZ_);
		specialPos_ = { position.x, (std::max)(position.y, floorY + 64.0f), droneCruiseZ_ };
		specialVel_ = { 0.0f, 0.0f, 26.0f };
		droneCurrentSpeed_ = 26.0f;
		droneSmoothedGroundY_ = QueryTerrainY(EvaluateCanyonCenterX(droneCruiseZ_), droneCruiseZ_);
		droneRotorAngle_ = 0.0f;
		flightModel_.SetPosition(specialPos_);
		flightModel_.SetVelocity(specialVel_);

		object3d_ = std::make_unique<Object3d>();
		object3d_->Initialize(Object3dCommon::GetInstance());
		object3d_->SetCamera(activeCam);
		object3d_->SetModel(modelPath);
		object3d_->SetScale({ 2.4f, 2.4f, 2.4f });
		const MyMath::Vector4 orangeCol = { 1.00f, 0.46f, 0.06f, 1.00f };
		object3d_->SetMaterialColor(0, orangeCol);
		object3d_->SetMaterialEmissive(0, { orangeCol.x, orangeCol.y, orangeCol.z }, 0.48f);
		object3d_->SetTranslate(specialPos_);
		object3d_->SetRotate({ 0.0f, 0.0f, 0.0f });
		object3d_->Update();
		return;
	}

	if (aiType_ == AIType::CanyonWesternFighter) {
		collisionRadius_ = 15.5f;
		portWingMaxHp_ = 45.0f;
		stbdWingMaxHp_ = 45.0f;
		engineCoreMaxHp_ = 50.0f;
		portWingHp_ = portWingMaxHp_;
		stbdWingHp_ = stbdWingMaxHp_;
		engineCoreHp_ = engineCoreMaxHp_;
		maxHealth_ = 50.0f;
		health_ = 45.0f;

		visualModel_.Initialize(Object3dCommon::GetInstance(), activeCam);
		visualModel_.SetupFromSingleModel(modelPath);
		visualModel_.SetMaterialProperties(42.0f, 0.65f, 0.22f);
		visualModel_.SetupCanopyMaterial();
		visualModel_.ResetAllPartsVisible();
		visualModel_.SetMaterialColor({ 0.56f, 0.61f, 0.68f, 1.00f });

		if (EffectManager::GetInstance()) {
			damageSmokeEffect_ = EffectManager::GetInstance()->CreateHandle("damage_smoke");
			crashPlumeEffect_ = EffectManager::GetInstance()->CreateHandle("crash_plume");
		}

		const MyMath::Vector3 playerPos = GetPlayerPosition();
		diveStartPos_ = { playerPos.x + 195.0f, playerPos.y + 285.0f, playerPos.z + 240.0f };
		diveMidPos_   = { playerPos.x - 32.0f,  playerPos.y + 26.0f,  playerPos.z + 155.0f };
		diveEndPos_   = { playerPos.x - 65.0f,  playerPos.y + 65.0f,  playerPos.z + 360.0f };

		const float midFloorY = QueryTerrainY(diveMidPos_.x, diveMidPos_.z);
		const float endFloorY = QueryTerrainY(diveEndPos_.x, diveEndPos_.z);
		diveMidPos_.y = (std::max)(diveMidPos_.y, midFloorY + 42.0f);
		diveEndPos_.y = (std::max)(diveEndPos_.y, endFloorY + 64.0f);

		specialPos_ = diveStartPos_;
		specialVel_ = { 0.0f, -80.0f, 140.0f };
		isInDivePass_ = true;
		westernStrafeTimer_ = 0.0f;
		westernCruiseZ_ = diveEndPos_.z;
		westernCurrentSpeed_ = 68.0f;
		westernWeavePhaseX_ = 0.0f;
		westernWeavePhaseY_ = 0.0f;
		westernSmoothedGroundY_ = -485.0f;

		flightModel_.SetPosition(specialPos_);
		flightModel_.SetVelocity(specialVel_);

		MyMath::Vector3 right{}, up{}, fwd{};
		BuildAircraftBasis(specialVel_, 0.0f, right, up, fwd);
		visualModel_.Update(BuildWorldMatrixFromBasis(right, up, fwd, specialPos_), 0.0f);
		return;
	}

	// 標準敵3Dオブジェクトの生成
	collisionRadius_ = 8.0f;
	if (modelPath.ends_with(".gltf")) {
		useVisualModel_ = true;
		visualModel_.Initialize(Object3dCommon::GetInstance(), activeCam);
		visualModel_.SetupFromSingleModel(modelPath);
		if (modelPath.find("m15") != std::string::npos) {
			visualModel_.SetMaterialProperties(42.0f, 0.75f, 0.28f);
			visualModel_.SetMaterialColor({ 0.56f, 0.61f, 0.68f, 1.00f });
		} else if (modelPath.find("m21") == std::string::npos) {
			visualModel_.SetMaterialProperties(42.0f, 0.75f, 0.28f);
			visualModel_.SetMaterialColor({ 0.46f, 0.50f, 0.48f, 1.00f });
		} else {
			visualModel_.SetMaterialProperties(48.0f, 1.15f, 0.45f);
		}
		visualModel_.SetupCanopyMaterial();
		visualModel_.ResetAllPartsVisible();
		visualModel_.Update(flightModel_.GetWorldMatrix(), 0.0f);
		object3d_.reset();
	} else {
		object3d_ = std::make_unique<Object3d>();
		object3d_->Initialize(Object3dCommon::GetInstance());
		object3d_->SetCamera(activeCam);
		object3d_->SetModel(modelPath);

		object3d_->GetModel()->SetEnvironmentCoefficient(0.4f);
		object3d_->GetModel()->SetSpecularIntensity(1.5f);
		object3d_->SetScale({ 1.0f, 1.0f, 1.0f });
	}

	RebuildBehaviorTree();
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

	// ロール
	float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
	float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	euler.x = std::atan2(sinr_cosp, cosr_cosp);

	// ピッチ
	float sinp = 2.0f * (q.w * q.y - q.z * q.x);
	if (std::fabs(sinp) >= 1.0f) {
		euler.y = std::copysign(3.14159265f / 2.0f, sinp);
	} else {
		euler.y = std::asin(sinp);
	}

	// ヨー
	float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
	float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	euler.z = std::atan2(siny_cosp, cosy_cosp);

	return euler;
}

void Enemy::SetBehaviorTreeJson(const std::string& json) {
	behaviorTreeJson_ = json;
	RebuildBehaviorTree();
}

void Enemy::RebuildBehaviorTree() {
	if (!behaviorTreeJson_.empty()) {
		behaviorTree_ = AI::EnemyBehaviorTreeBuilder::BuildFromJson(behaviorTreeJson_, aiType_);
	} else {
		behaviorTree_ = AI::EnemyBehaviorTreeBuilder::CreateEnemyTree(aiType_);
	}
}

MyMath::Vector3 Enemy::GetPlayerPosition() const {
	return playerFlightModel_ ? playerFlightModel_->GetPosition() : MyMath::Vector3{};
}

MyMath::Vector3 Enemy::GetPlayerVelocity() const {
	return playerFlightModel_ ? playerFlightModel_->GetVelocity() : MyMath::Vector3{};
}

float Enemy::GetDistanceToPlayer() const {
	if (!playerFlightModel_) return 0.0f;
	return MyMath::Length(MyMath::Subtract(playerFlightModel_->GetPosition(), flightModel_.GetPosition()));
}

MyMath::Vector3 Enemy::GetDirectionToPlayer() const {
	if (!playerFlightModel_) return flightModel_.GetForwardDirection();
	const MyMath::Vector3 toPlayer = MyMath::Subtract(playerFlightModel_->GetPosition(), flightModel_.GetPosition());
	const float len = MyMath::Length(toPlayer);
	return (len > 0.001f) ? MyMath::Multiply(1.0f / len, toPlayer) : flightModel_.GetForwardDirection();
}

void Enemy::SetAimTargetDirection(const MyMath::Vector3& targetDir, float deltaTime) {
	MyMath::Vector3 safeDir = targetDir;
	if (terrain_ && terrain_->IsLoaded()) {
		const MyMath::Vector3 pos = flightModel_.GetPosition();
		const MyMath::Vector3 fwd = flightModel_.GetForwardDirection();
		const auto currHit = terrain_->QuerySurfaceAt(pos.x, pos.z, -485.0f);
		const auto aheadHit = terrain_->QuerySurfaceAt(pos.x + fwd.x * 260.0f, pos.z + fwd.z * 260.0f, -485.0f);
		const float surfaceY = (std::max)({ currHit.landY, currHit.waterY, aheadHit.landY, aheadHit.waterY });
		const float clearance = pos.y - surfaceY;
		if (clearance < 220.0f) {
			const float t = std::clamp((220.0f - clearance) / 140.0f, 0.0f, 1.0f);
			const float minY = 0.08f + 0.48f * t;
			if (safeDir.y < minY) {
				safeDir.y = minY;
				const float len = MyMath::Length(safeDir);
				if (len > 0.001f) {
					safeDir = MyMath::Multiply(1.0f / len, safeDir);
				}
			}
		}
	}
	aimController_.SetTargetDirection(safeDir);
	float finalPitch = 0.0f;
	float finalRoll = 0.0f;
	float finalYaw = 0.0f;
	aimController_.CalculateSteeringInput(flightModel_.GetOrientation(), deltaTime, finalPitch, finalRoll, finalYaw);
	flightModel_.SetControlInput(finalPitch, finalRoll, finalYaw);
}

bool Enemy::TryFireGunpod(const MyMath::Vector3& targetLeadDir, float maxDistance) {
	if (!bulletManager_) return false;
	const float dist = GetDistanceToPlayer();
	if (dist > maxDistance) return false;

	const MyMath::Vector3 forward = flightModel_.GetForwardDirection();
	const float dot = MyMath::Dot(forward, targetLeadDir);
	if (dot <= EnemyWaypointDefaults::kOpportunisticFireDotThreshold) return false;

	const int ammoBefore = gunpod_.GetCurrentAmmo();
	gunpod_.Fire();
	if (gunpod_.GetCurrentAmmo() >= ammoBefore) return false;

	const MyMath::Vector3 firePos = MyMath::Add(
		flightModel_.GetPosition(),
		MyMath::Multiply(EnemyWaypointDefaults::kMuzzleForwardOffset, forward));
	const MyMath::Vector3 muzzleVel = MyMath::Multiply(EnemyWaypointDefaults::kBulletMuzzleSpeed, forward);
	const MyMath::Vector3 bulletVel = MyMath::Add(flightModel_.GetVelocity(), muzzleVel);

	bulletManager_->SpawnBullet(firePos, bulletVel, EnemyWaypointDefaults::kBulletDamage, true);

	muzzleFlashCount_++;
	if ((muzzleFlashCount_ % EnemyWaypointDefaults::kMuzzleFlashSkipModulo) == EnemyWaypointDefaults::kEvenRemainder) {
		EffectManager::GetInstance()->EmitMuzzleFlash(firePos, forward);
		muzzleFlashTimer_ = EnemyWaypointDefaults::kMuzzleFlashDuration;
	}
	return true;
}

void Enemy::AdvanceWaypoint() {
	if (waypointPoints_.empty()) return;
	currentWaypointIndex_ += EnemyWaypointDefaults::kWaypointStep;
	if (currentWaypointIndex_ >= waypointPoints_.size()) {
		if (waypointIsLoop_) {
			currentWaypointIndex_ = EnemyWaypointDefaults::kInitialWaypointIndex;
		} else {
			currentWaypointIndex_ = waypointPoints_.size() - 1;
		}
	}
}

void Enemy::UpdateAI(float deltaTime) {
	if (!isAlive_) return;

	if (behaviorTree_) {
		behaviorTree_->Tick(deltaTime, *this);
	}
}

void Enemy::UpdateAllyWingman(float deltaTime) {
	const float dt = (std::max)(0.0f, deltaTime);
	if (dt <= 0.0001f) {
		return;
	}
	specialAnimTimer_ += dt;

	const MyMath::Vector3 playerPos = GetPlayerPosition();
	const MyMath::Vector3 playerVel = GetPlayerVelocity();
	const float playerSpeed = std::clamp(MyMath::Length(playerVel), 68.0f, 235.0f);

	const bool inActiveDogfight =
		(targetEnemy_ && targetEnemy_->IsAlive() && !targetEnemy_->IsInDivePass());

	MyMath::Vector3 desiredPos{};
	if (inActiveDogfight) {
		const MyMath::Vector3 targetPos = targetEnemy_->GetPosition();
		desiredPos = {
			targetPos.x + (-38.0f) * std::cos(specialAnimTimer_ * 0.55f),
			targetPos.y + 18.0f,
			targetPos.z - 145.0f
		};
	} else {
		const float targetZ = playerPos.z + 135.0f;
		const float canyonCenterX = EvaluateCanyonCenterX(targetZ);
		const float desiredX = canyonCenterX + 24.0f;
		const float floorY = QueryTerrainY(desiredX, targetZ);
		const float safeFormationY = (std::max)(playerPos.y + 14.0f, floorY + 148.0f);
		desiredPos = { desiredX, safeFormationY, targetZ };
	}

	const MyMath::Vector3 toTarget = MyMath::Subtract(desiredPos, specialPos_);
	MyMath::Vector3 desiredVel = MyMath::Multiply(2.6f, toTarget);
	desiredVel.y = std::clamp(desiredVel.y, -20.0f, 25.0f);
	desiredVel.z = (std::max)(desiredVel.z, playerSpeed);
	const float desiredSpeed = MyMath::Length(desiredVel);
	if (desiredSpeed > 235.0f) {
		desiredVel = MyMath::Multiply(235.0f / desiredSpeed, desiredVel);
	}

	const float velBlend = std::clamp(4.2f * dt, 0.0f, 1.0f);
	specialVel_ = MyMath::Lerp(specialVel_, desiredVel, velBlend);
	specialPos_ = MyMath::Add(specialPos_, MyMath::Multiply(dt, specialVel_));

	const float coraxGroundY = QueryTerrainY(specialPos_.x, specialPos_.z);
	const float minSafeY = coraxGroundY + 64.0f;
	if (specialPos_.y < minSafeY) {
		specialPos_.y = minSafeY;
	}

	const float speedNow = MyMath::Length(specialVel_);
	if (speedNow > 0.0001f) {
		MyMath::Vector3 nextFwd = MyMath::Multiply(1.0f / speedNow, specialVel_);
		nextFwd.y = std::clamp(nextFwd.y, -0.26f, 0.35f);
		nextFwd = MyMath::Normalize(nextFwd);
		const float lateralTurn = nextFwd.x - specialForward_.x;
		const float targetRoll = std::clamp(-lateralTurn * 1.6f, -0.95f, 0.95f);
		specialRollRad_ = specialRollRad_ + (targetRoll - specialRollRad_) * std::clamp(5.0f * dt, 0.0f, 1.0f);
		specialForward_ = nextFwd;
	}

	if (inActiveDogfight && bulletManager_) {
		const float cyclePos = std::fmod(specialAnimTimer_, 2.40f);
		specialFireTimer_ -= dt;
		if (cyclePos <= 0.55f && specialFireTimer_ <= 0.0f) {
			specialFireTimer_ = 0.14f;
			const MyMath::Vector3 aimDir = MyMath::Normalize(MyMath::Subtract(targetEnemy_->GetPosition(), specialPos_));
			const MyMath::Vector3 muzzlePos = MyMath::Add(specialPos_, MyMath::Multiply(8.0f, aimDir));
			const MyMath::Vector3 tracerVel = MyMath::Multiply(560.0f, aimDir);
			bulletManager_->SpawnBullet(muzzlePos, tracerVel, 0.0f, false);
		}
	}

	flightModel_.SetPosition(specialPos_);
	flightModel_.SetVelocity(specialVel_);

	Camera* activeCam = CameraManager::GetInstance()->GetActiveCamera();
	if (activeCam) {
		visualModel_.SetCamera(activeCam);
	}
	MyMath::Vector3 right{}, up{}, fwd{};
	BuildAircraftBasis(specialForward_, specialRollRad_, right, up, fwd);
	visualModel_.Update(BuildWorldMatrixFromBasis(right, up, fwd, specialPos_), dt);

	if (afterburner_) {
		const float coraxThrottle = std::clamp(speedNow / 110.0f, 0.70f, 1.0f);
		afterburner_->Update(dt, coraxThrottle);
	}

	const MyMath::Vector3 wingL = MyMath::Add(MyMath::Add(specialPos_, MyMath::Multiply(-6.0f, right)), MyMath::Multiply(-2.0f, fwd));
	const MyMath::Vector3 wingR = MyMath::Add(MyMath::Add(specialPos_, MyMath::Multiply(6.0f, right)), MyMath::Multiply(-2.0f, fwd));

	if (hasLastWingTips_) {
		const MyMath::Vector3 deltaL = MyMath::Subtract(wingL, lastWingTipL_);
		const MyMath::Vector3 deltaR = MyMath::Subtract(wingR, lastWingTipR_);
		const float distL = MyMath::Length(deltaL);

		ParticleParameters params{};
		params.minVelocity = { 0.0f, 0.0f, 0.0f };
		params.maxVelocity = { 0.0f, 0.0f, 0.0f };
		params.randomPositionRange = 0.0f;
		params.minColor = { 0.92f, 0.96f, 1.0f, 0.35f };
		params.maxColor = { 0.92f, 0.96f, 1.0f, 0.35f };
		params.minLifeTime = 0.50f;
		params.maxLifeTime = 0.50f;
		params.minScale = 0.09f;
		params.maxScale = 0.09f;
		params.endScale = 0.14f;
		params.fadeOut = true;
		params.isStretched = true;
		params.stretchFactor = 12.0f;

		int steps = static_cast<int>(std::ceil(distL / 0.35f));
		if (steps < 1) { steps = 1; }
		if (steps > 30) { steps = 30; }

		const MyMath::Vector3 stepDeltaL = MyMath::Multiply(1.0f / static_cast<float>(steps), deltaL);
		const MyMath::Vector3 stepDeltaR = MyMath::Multiply(1.0f / static_cast<float>(steps), deltaR);

		for (int i = 0; i < steps; ++i) {
			const float t = static_cast<float>(i) / static_cast<float>(steps);
			const MyMath::Vector3 interpL = MyMath::Lerp(lastWingTipL_, wingL, t);
			const MyMath::Vector3 interpR = MyMath::Lerp(lastWingTipR_, wingR, t);

			params.stretchDir = stepDeltaL;
			ParticleManager::GetInstance()->Emit("Vortex", interpL, params, 1);

			params.stretchDir = stepDeltaR;
			ParticleManager::GetInstance()->Emit("Vortex", interpR, params, 1);
		}
	}
	lastWingTipL_ = wingL;
	lastWingTipR_ = wingR;
	hasLastWingTips_ = true;
}

void Enemy::UpdateCanyonDrone(float deltaTime) {
	const float dt = (std::max)(0.0f, deltaTime);
	if (dt <= 0.0001f) {
		return;
	}
	specialAnimTimer_ += dt;
	specialPhaseTimer_ += dt;
	droneRotorAngle_ += dt * 52.0f;

	const MyMath::Vector3 playerPos = GetPlayerPosition();
	const float playerSpeed = MyMath::Length(GetPlayerVelocity());
	const float baseMatchSpeed = std::clamp(playerSpeed * 0.35f, 26.0f, 42.0f);
	const float distAheadZ = droneCruiseZ_ - playerPos.z;
	float targetForwardSpeed = baseMatchSpeed;
	if (distAheadZ < 160.0f) {
		const float catchupFactor = std::clamp((160.0f - distAheadZ) / 300.0f, 0.0f, 1.0f);
		const float catchupSpeed = (std::max)(42.0f, playerSpeed * 1.35f);
		const float stayAheadSpeed = (std::max)(baseMatchSpeed, playerSpeed * 1.05f);
		targetForwardSpeed = stayAheadSpeed + (catchupSpeed - stayAheadSpeed) * catchupFactor;
	} else if (distAheadZ > 350.0f) {
		const float holdT = std::clamp((distAheadZ - 350.0f) / 80.0f, 0.0f, 1.0f);
		targetForwardSpeed = baseMatchSpeed * (1.0f - holdT);
		if (distAheadZ > 550.0f) {
			const float returnT = std::clamp((distAheadZ - 550.0f) / 150.0f, 0.0f, 1.0f);
			const float returnSpeed = -std::clamp(playerSpeed * 0.75f, 26.0f, 120.0f);
			targetForwardSpeed += returnSpeed * returnT;
		}
	}
	droneCurrentSpeed_ += (targetForwardSpeed - droneCurrentSpeed_) * std::clamp(dt * 4.0f, 0.0f, 1.0f);

	const MyMath::Vector3 prevPos = specialPos_;
	droneCruiseZ_ += droneCurrentSpeed_ * dt;

	const float canyonCenterX = EvaluateCanyonCenterX(droneCruiseZ_);
	const float weaveOffsetX = std::sin(specialPhaseTimer_ * 0.42f * 6.28318530717958f * 0.25f) * 42.0f;
	const float desiredX = canyonCenterX + weaveOffsetX;
	const float centerFloorY = QueryTerrainY(canyonCenterX, droneCruiseZ_);
	droneSmoothedGroundY_ += (centerFloorY - droneSmoothedGroundY_) * std::clamp(dt * 3.0f, 0.0f, 1.0f);
	const float weaveOffsetY = std::sin(specialPhaseTimer_ * 0.55f) * 12.0f;
	const float desiredY = (std::max)(droneSmoothedGroundY_ + 64.0f, droneSmoothedGroundY_ + 110.0f + weaveOffsetY);

	specialPos_ = { desiredX, desiredY, droneCruiseZ_ };
	const MyMath::Vector3 rawVel = MyMath::Multiply(1.0f / dt, MyMath::Subtract(specialPos_, prevPos));
	specialVel_ = MyMath::Lerp(specialVel_, rawVel, std::clamp(dt * 12.0f, 0.0f, 1.0f));

	flightModel_.SetPosition(specialPos_);
	flightModel_.SetVelocity(specialVel_);

	if (object3d_) {
		Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
		if (cam) {
			object3d_->SetCamera(cam);
		}
		const float yawAngle = std::atan2(specialVel_.x, (std::max)(1.0f, specialVel_.z));
		const float pitchAngle = -std::atan2(specialVel_.y, (std::max)(1.0f, std::hypot(specialVel_.x, specialVel_.z))) * 0.45f;
		const float bankAngle = -specialVel_.x * 0.012f;
		object3d_->SetTranslate(specialPos_);
		object3d_->SetRotate({ pitchAngle, yawAngle, bankAngle });
		object3d_->Update();
	}
}

void Enemy::UpdateCanyonWesternFighter(float deltaTime) {
	const float dt = (std::max)(0.0f, deltaTime);
	if (dt <= 0.0001f) {
		return;
	}
	specialAnimTimer_ += dt;
	specialPhaseTimer_ += dt;

	const MyMath::Vector3 playerPos = GetPlayerPosition();
	const MyMath::Vector3 playerVel = GetPlayerVelocity();

	if (isInDivePass_) {
		const MyMath::Vector3 prevPos = specialPos_;
		const float t = std::clamp(specialPhaseTimer_ / 2.35f, 0.0f, 1.0f);
		const float invT = 1.0f - t;

		const MyMath::Vector3 term0 = MyMath::Multiply(invT * invT, diveStartPos_);
		const MyMath::Vector3 term1 = MyMath::Multiply(2.0f * invT * t, diveMidPos_);
		const MyMath::Vector3 term2 = MyMath::Multiply(t * t, diveEndPos_);
		specialPos_ = MyMath::Add(MyMath::Add(term0, term1), term2);
		specialVel_ = MyMath::Multiply(1.0f / dt, MyMath::Subtract(specialPos_, prevPos));

		westernStrafeTimer_ -= dt;
		if (westernStrafeTimer_ <= 0.0f && t < 0.75f && bulletManager_) {
			westernStrafeTimer_ = 0.055f;
			const MyMath::Vector3 strafeTarget = {
				playerPos.x + 15.0f,
				playerPos.y + 7.5f,
				playerPos.z + 24.0f
			};
			const MyMath::Vector3 strafeDir = MyMath::Normalize(MyMath::Subtract(strafeTarget, specialPos_));
			const MyMath::Vector3 bulletVel = MyMath::Multiply(620.0f, strafeDir);
			bulletManager_->SpawnBullet(specialPos_, bulletVel, 4.0f, true);
		}

		if (t >= 1.0f) {
			isInDivePass_ = false;
			specialPos_ = diveEndPos_;
			westernCruiseZ_ = diveEndPos_.z;
			westernCurrentSpeed_ = (std::max)(26.0f, MyMath::Length(playerVel) * 0.92f);
			westernWeavePhaseX_ = 0.0f;
			westernWeavePhaseY_ = 0.0f;
			westernSmoothedGroundY_ = diveEndPos_.y - 156.0f;

			if (CameraManager::GetInstance()->GetActiveCamera()) {
				CameraManager::GetInstance()->GetActiveCamera()->Shake(0.38f, 0.50f);
			}
		}

		flightModel_.SetPosition(specialPos_);
		flightModel_.SetVelocity(specialVel_);

		Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
		if (cam) {
			visualModel_.SetCamera(cam);
		}
		MyMath::Vector3 right{}, up{}, fwd{};
		BuildAircraftBasis(specialVel_, -specialVel_.x * 0.014f, right, up, fwd);
		visualModel_.Update(BuildWorldMatrixFromBasis(right, up, fwd, specialPos_), dt);
		return;
	}

	const float portRatio = std::clamp(portWingHp_ / portWingMaxHp_, 0.0f, 1.0f);
	const float stbdRatio = std::clamp(stbdWingHp_ / stbdWingMaxHp_, 0.0f, 1.0f);
	const float engRatio = std::clamp(engineCoreHp_ / engineCoreMaxHp_, 0.0f, 1.0f);
	const float minWingRatio = (std::min)(portRatio, stbdRatio);
	const bool isWingDamaged = (minWingRatio < 0.60f);
	const bool isEngineDamaged = (engRatio < 0.60f);

	const float currentWeaveAmpX = isWingDamaged ? 42.0f : 125.0f;
	const float currentWeaveFreq = isWingDamaged ? 0.34f : 0.78f;
	const float engineSpeedFactor = isEngineDamaged ? 0.78f : 1.0f;

	westernWeavePhaseX_ += currentWeaveFreq * dt;
	westernWeavePhaseY_ += 1.05f * dt;

	const float playerSpeed = MyMath::Length(playerVel);
	float desiredForwardSpeed = std::clamp(playerSpeed * 0.92f * engineSpeedFactor, 26.0f, 235.0f);
	const float distAheadZ = westernCruiseZ_ - playerPos.z;
	if (distAheadZ < 240.0f) {
		desiredForwardSpeed = (std::min)(235.0f, playerSpeed * 1.12f);
	} else if (distAheadZ > 460.0f) {
		desiredForwardSpeed = (std::max)(26.0f, playerSpeed * 0.78f);
	}

	westernCurrentSpeed_ += (desiredForwardSpeed - westernCurrentSpeed_) * std::clamp(dt * 3.0f, 0.0f, 1.0f);
	westernCruiseZ_ += westernCurrentSpeed_ * dt;

	const float canyonCenterX = EvaluateCanyonCenterX(westernCruiseZ_);
	const float rawFloorY = QueryTerrainY(canyonCenterX, westernCruiseZ_);
	westernSmoothedGroundY_ += (rawFloorY - westernSmoothedGroundY_) * std::clamp(dt * 2.2f, 0.0f, 1.0f);

	const float effectiveAmpX = (std::min)(currentWeaveAmpX, 42.0f);
	const float targetX = canyonCenterX + std::sin(westernWeavePhaseX_) * effectiveAmpX;
	const float targetY = westernSmoothedGroundY_ + 156.0f + std::sin(westernWeavePhaseY_) * 28.0f;
	const MyMath::Vector3 targetPos = { targetX, targetY, westernCruiseZ_ };

	const MyMath::Vector3 toTarget = MyMath::Subtract(targetPos, specialPos_);
	specialVel_ = { toTarget.x * 4.2f, toTarget.y * 3.2f, westernCurrentSpeed_ };
	specialPos_ = MyMath::Add(specialPos_, MyMath::Multiply(dt, specialVel_));

	flightModel_.SetPosition(specialPos_);
	flightModel_.SetVelocity(specialVel_);

	const float bankRoll = -std::clamp(specialVel_.x * 0.015f, -0.65f, 0.65f);
	MyMath::Vector3 right{}, up{}, fwd{};
	BuildAircraftBasis(specialVel_, bankRoll, right, up, fwd);

	Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
	if (cam) {
		visualModel_.SetCamera(cam);
	}
	visualModel_.Update(BuildWorldMatrixFromBasis(right, up, fwd, specialPos_), dt);

	if ((isWingDamaged || isEngineDamaged) && damageSmokeEffect_) {
		damageSmokeEffect_->emit(specialPos_);
	}
}

void Enemy::UpdateDownedSpiral(float deltaTime) {
	if (!isDownedSpiral_ || isCrashedFloor_) {
		return;
	}
	const float dt = (std::max)(0.0f, deltaTime);
	if (dt <= 0.0001f) {
		return;
	}
	specialPhaseTimer_ += dt;
	specialVel_.y -= 26.0f * dt;
	specialVel_.x *= (std::max)(0.0f, 1.0f - 0.18f * dt);
	specialPos_ = MyMath::Add(specialPos_, MyMath::Multiply(dt, specialVel_));

	const float crashRollDir = (brokenPart_ == 2) ? 1.0f : -1.0f;
	const float spiralRoll = specialPhaseTimer_ * 5.4f * crashRollDir;

	Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
	if (cam) {
		visualModel_.SetCamera(cam);
	}
	MyMath::Vector3 right{}, up{}, fwd{};
	BuildAircraftBasis(specialVel_, spiralRoll, right, up, fwd);
	visualModel_.Update(BuildWorldMatrixFromBasis(right, up, fwd, specialPos_), dt);

	flightModel_.SetPosition(specialPos_);
	flightModel_.SetVelocity(specialVel_);

	if (crashPlumeEffect_) {
		crashPlumeEffect_->emit(specialPos_);
	}

	const float crashGroundY = QueryTerrainY(specialPos_.x, specialPos_.z);
	if (specialPos_.y <= crashGroundY + 2.5f) {
		specialPos_.y = crashGroundY + 2.5f;
		isCrashedFloor_ = true;
		if (crashPlumeEffect_) {
			crashPlumeEffect_->stop();
		}
		if (EffectManager::GetInstance()) {
			EffectManager::GetInstance()->EmitDestroyEffect(specialPos_);
		}
	}
}

void Enemy::TriggerWesternStructuralBreak(int partIndex) {
	brokenPart_ = partIndex;
	isAlive_ = false;
	health_ = 0.0f;
	isDownedSpiral_ = true;
	isCrashedFloor_ = false;
	specialPhaseTimer_ = 0.0f;
	if (damageSmokeEffect_) {
		damageSmokeEffect_->stop();
	}

	Model* westernRawModel = ModelManager::GetInstance()->FindModel("Resources/models/m15.gltf");
	MyMath::Vector3 right{}, up{}, fwd{};
	BuildAircraftBasis(specialVel_, 0.0f, right, up, fwd);

	if (partIndex == 1) {
		const MyMath::Matrix4x4 wingMat = visualModel_.GetPartWorldMatrix(DamagePart::Wing_L);
		visualModel_.SetPartVisible(DamagePart::Wing_L, false);
		visualModel_.SetPartVisible(DamagePart::Wing1_L, false);
		visualModel_.SetPartVisible(DamagePart::Wing2_L, false);
		if (westernRawModel && DebrisManager::GetInstance()) {
			const MyMath::Vector3 eject = MyMath::Multiply(-9.5f, right);
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, specialVel_, eject, "Wing_L");
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, specialVel_, eject, "Wing1_L");
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, specialVel_, eject, "Wing2_L");
		}
		pendingBrokenPartLog_ = "TARGET HIT: PORT WING [STRUCTURAL BREAK] -> DOWNED";
	} else if (partIndex == 2) {
		const MyMath::Matrix4x4 wingMat = visualModel_.GetPartWorldMatrix(DamagePart::Wing_R);
		visualModel_.SetPartVisible(DamagePart::Wing_R, false);
		visualModel_.SetPartVisible(DamagePart::Wing1_R, false);
		visualModel_.SetPartVisible(DamagePart::Wing2_R, false);
		if (westernRawModel && DebrisManager::GetInstance()) {
			const MyMath::Vector3 eject = MyMath::Multiply(9.5f, right);
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, specialVel_, eject, "Wing_R");
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, specialVel_, eject, "Wing1_R");
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, specialVel_, eject, "Wing2_R");
		}
		pendingBrokenPartLog_ = "TARGET HIT: STBD WING [STRUCTURAL BREAK] -> DOWNED";
	} else {
		const MyMath::Matrix4x4 tailMat = visualModel_.GetPartWorldMatrix(DamagePart::Tail);
		visualModel_.SetPartVisible(DamagePart::Tail, false);
		visualModel_.SetPartVisible(DamagePart::Rudder, false);
		if (westernRawModel && DebrisManager::GetInstance()) {
			const MyMath::Vector3 eject = MyMath::Multiply(9.5f, up);
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, tailMat, specialVel_, eject, "Tail");
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, tailMat, specialVel_, eject, "Rudder");
		}
		pendingBrokenPartLog_ = "TARGET HIT: ENGINE CORE [STRUCTURAL BREAK] -> DOWNED";
	}

	if (EffectManager::GetInstance()) {
		EffectManager::GetInstance()->EmitDestroyEffect(specialPos_);
	}
	if (CameraManager::GetInstance()->GetActiveCamera()) {
		CameraManager::GetInstance()->GetActiveCamera()->Shake(0.38f, 0.50f);
	}
}

void Enemy::Update(float deltaTime) {
	if (aiType_ == AIType::AllyWingman) {
		if (isAlive_) {
			UpdateAllyWingman(deltaTime);
		}
		return;
	}
	if (aiType_ == AIType::CanyonDrone) {
		if (isAlive_) {
			UpdateCanyonDrone(deltaTime);
		}
		return;
	}
	if (aiType_ == AIType::CanyonWesternFighter) {
		if (isAlive_) {
			UpdateCanyonWesternFighter(deltaTime);
		} else if (isDownedSpiral_ && !isCrashedFloor_) {
			UpdateDownedSpiral(deltaTime);
		}
		return;
	}

	if (!isAlive_) { return; }

	if (terrain_ && terrain_->IsLoaded()) {
		const MyMath::Vector3 pos = flightModel_.GetPosition();
		const auto hit = terrain_->QuerySurfaceAt(pos.x, pos.z, -485.0f);
		flightModel_.SetGroundElevation((std::max)(hit.landY, hit.waterY));
	}

	UpdateAI(deltaTime);
	flightModel_.Update(deltaTime);

	if (muzzleFlashTimer_ > 0.0f) {
		muzzleFlashTimer_ -= deltaTime;
	}

	// オブジェクトに位置と姿勢を適用
	if (useVisualModel_) {
		if (Camera* cam = CameraManager::GetInstance()->GetActiveCamera()) {
			visualModel_.SetCamera(cam);
		}
		visualModel_.Update(flightModel_.GetWorldMatrix(), deltaTime);
	} else if (object3d_) {
		object3d_->SetTranslate(flightModel_.GetPosition());
		object3d_->SetRotate(QuaternionToEuler(flightModel_.GetOrientation()));
		object3d_->Update();
	}
}

void Enemy::Draw() {
	if (aiType_ == AIType::AllyWingman) {
		if (!isAlive_) { return; }
		Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
		visualModel_.SetCamera(cam);
		visualModel_.Draw();
		if (afterburner_ && cam) {
			MyMath::Vector3 right{}, up{}, fwd{};
			BuildAircraftBasis(specialForward_, specialRollRad_, right, up, fwd);
			const MyMath::Matrix4x4 coraxWorld = BuildWorldMatrixFromBasis(right, up, fwd, specialPos_);
			const MyMath::Matrix4x4 baseRotate = MyMath::MakeAffineMatrix(
				MyMath::Vector3{ 1.0f, 1.0f, 1.0f },
				MyMath::Vector3{ 0.0f, 3.14159265f, 0.0f },
				MyMath::Vector3{ 0.0f, 0.0f, 0.0f });
			const MyMath::Matrix4x4 visualWorld = MyMath::Multiply(baseRotate, coraxWorld);
			afterburner_->Draw(visualWorld, cam);
			Object3dCommon::GetInstance()->SetupCommonState();
			Object3dCommon::GetInstance()->SetLightType(1);
		}
		return;
	}

	if (aiType_ == AIType::CanyonWesternFighter) {
		if (!IsRenderActive()) { return; }
		visualModel_.SetCamera(CameraManager::GetInstance()->GetActiveCamera());
		visualModel_.Draw();
		return;
	}

	if (!isAlive_) { return; }
	if (useVisualModel_) {
		visualModel_.SetCamera(CameraManager::GetInstance()->GetActiveCamera());
		visualModel_.Draw();
		return;
	}
	if (!object3d_) { return; }

	object3d_->Draw();

	if (aiType_ == AIType::CanyonDrone) {
		Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
		PrimitiveModel* prim = PrimitiveModel::GetInstance();
		if (prim && cam) {
			prim->DrawRing(
				{ 4.8f, 4.8f, 1.0f },
				{ 0.0f, 0.0f, specialAnimTimer_ * 2.0f },
				specialPos_,
				{ 1.00f, 0.58f, 0.16f, 0.98f },
				ringTexIndex_,
				cam,
				BlendMode::kAdd);

			static constexpr MyMath::Vector3 kRotorOffsets[4] = {
				{ -1.60f * 2.4f, 0.35f * 2.4f,  1.60f * 2.4f },
				{  1.60f * 2.4f, 0.35f * 2.4f,  1.60f * 2.4f },
				{ -1.60f * 2.4f, 0.35f * 2.4f, -1.60f * 2.4f },
				{  1.60f * 2.4f, 0.35f * 2.4f, -1.60f * 2.4f },
			};
			const MyMath::Vector4 rotorCol = { 0.30f, 0.80f, 1.00f, 0.60f };
			for (int r = 0; r < 4; ++r) {
				const MyMath::Vector3 rotorWorldPos = MyMath::Add(specialPos_, kRotorOffsets[r]);
				prim->DrawRing(
					{ 1.45f, 1.45f, 1.0f },
					{ 1.5707963f, 0.0f, droneRotorAngle_ * ((r % 2 == 0) ? 1.0f : -1.0f) },
					rotorWorldPos,
					rotorCol,
					ringTexIndex_,
					cam,
					BlendMode::kAdd);
			}
			Object3dCommon::GetInstance()->SetupCommonState();
			Object3dCommon::GetInstance()->SetLightType(1);
		}
	}
}

void Enemy::TakeDamage(float damage) {
	if (!isAlive_ || IsAlly()) { return; }

	flightModel_.GetDamageModel().ProcessHit(DamagePart::Fuse, damage);
	if (flightModel_.GetAirframe().IsDestroyed()) {
		health_ = 0.0f;
		isAlive_ = false;
	}
}

void Enemy::OnCollision(ICollisionBody3D* other) {
	if (!isAlive_ || IsAlly()) { return; }

	if (aiType_ == AIType::CanyonWesternFighter) {
		if (isInDivePass_) {
			return;
		}
		if (other->GetCollisionAttribute() & CollisionAttribute::kPlayerBullet) {
			auto* bullet = dynamic_cast<Bullet*>(other);
			const MyMath::Vector3 bulletPos = bullet ? bullet->GetPosition() : specialPos_;
			const float dmg = bullet ? bullet->GetDamage() : 10.0f;
			if (dmg <= 0.001f) {
				EffectManager::GetInstance()->EmitHitEffect(bulletPos);
				return;
			}

			hitMarkerTriggered_ = true;
			EffectManager::GetInstance()->EmitHitEffect(bulletPos);

			const float bankRoll = -std::clamp(specialVel_.x * 0.015f, -0.65f, 0.65f);
			MyMath::Vector3 right{}, up{}, fwd{};
			BuildAircraftBasis(specialVel_, bankRoll, right, up, fwd);

			const MyMath::Vector3 relHit = MyMath::Subtract(bulletPos, specialPos_);
			const float localX = MyMath::Dot(relHit, right);
			const float localZ = MyMath::Dot(relHit, fwd);

			if (localX <= -1.25f) {
				portWingHp_ = (std::max)(0.0f, portWingHp_ - dmg);
			} else if (localX >= 1.25f) {
				stbdWingHp_ = (std::max)(0.0f, stbdWingHp_ - dmg);
			} else if (localZ <= -1.20f) {
				engineCoreHp_ = (std::max)(0.0f, engineCoreHp_ - dmg);
			} else {
				portWingHp_ = (std::max)(0.0f, portWingHp_ - dmg * (1.0f - 0.42f));
				engineCoreHp_ = (std::max)(0.0f, engineCoreHp_ - dmg * 0.42f);
			}
			health_ = (std::min)({ portWingHp_, stbdWingHp_, engineCoreHp_ });

			if (portWingHp_ <= 0.0f) {
				TriggerWesternStructuralBreak(1);
			} else if (stbdWingHp_ <= 0.0f) {
				TriggerWesternStructuralBreak(2);
			} else if (engineCoreHp_ <= 0.0f) {
				TriggerWesternStructuralBreak(3);
			}
		} else if (other->GetCollisionAttribute() & CollisionAttribute::kPlayer) {
			TriggerWesternStructuralBreak(3);
		}
		return;
	}

	// PlayerBullet との衝突 → パーツ被弾処理
	if (other->GetCollisionAttribute() & CollisionAttribute::kPlayerBullet) {
		auto* bullet = dynamic_cast<Bullet*>(other);
		Vector3 bulletPos = bullet ? bullet->GetPosition() : flightModel_.GetPosition();
		float dmg = bullet ? bullet->GetDamage() : 10.0f;
		if (dmg <= 0.001f) {
			return;
		}

		hitMarkerTriggered_ = true;
		health_ = (std::max)(0.0f, health_ - dmg);
		HitResult res = flightModel_.ProcessBulletHit(bulletPos, dmg);
		EffectManager::GetInstance()->EmitHitEffect(bulletPos);

		if (health_ <= 0.0f || (aiType_ != AIType::CanyonDrone && flightModel_.GetAirframe().IsDestroyed())) {
			health_ = 0.0f;
			isAlive_ = false;
			EffectManager::GetInstance()->EmitDestroyEffect(flightModel_.GetPosition());
		}
	}
	// Player との衝突 → 即死して撃破エフェクト発生
	else if (other->GetCollisionAttribute() & CollisionAttribute::kPlayer) {
		health_ = 0.0f;
		isAlive_ = false;
		EffectManager::GetInstance()->EmitDestroyEffect(flightModel_.GetPosition());
	}
}
