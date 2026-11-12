#include "Enemy.h"
#include <algorithm>
#include "BehaviorTree/EnemyBehaviorNodes.h"
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "../../engine/base/logger.h"

Enemy::Enemy() = default;
Enemy::~Enemy() = default;

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
	playerFlightModel_ = playerFlightModel;
	bulletManager_ = bulletManager;
	aiType_ = aiType;
	behaviorTreeJson_ = behaviorTreeJson;
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

	// ビヘイビアツリー構築
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
	aimController_.ResetToDirection(targetDir);
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
