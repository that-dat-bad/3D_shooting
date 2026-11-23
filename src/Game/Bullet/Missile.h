#pragma once
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Physics/Collider.h"
#include "../../engine/Physics/ICollisionBody.h"
#include "../../engine/Physics/CollisionConfig.h"

class Enemy;
class GroundEnemy;
class TerrainChunkManager;

// IR誘導ミサイルの性能諸元（War Thunder AIM-9E / R-60 準拠バランス）
struct MissileSpec {
	float initialBoostDelay = 0.12f;   // パイロン離脱・点火遅延 (秒)
	float motorBurnTime = 3.2f;        // ロケットモーター燃焼時間 (秒)
	float maxLifeTime = 11.0f;         // 最大飛翔時間 (秒)
	float motorThrustAccel = 260.0f;   // 燃焼中加速度 (m/s^2)
	float maxSpeed = 680.0f;           // 最大速度 (m/s)
	float dragCoeff = 0.00045f;        // 空気抵抗係数
	float maxG = 18.0f;                // 最大許容旋回G
	float guidanceDelay = 0.25f;       // 誘導開始遅延 (秒)
	float proportionalNavGain = 3.8f;  // 比例航法係数 N
	float seekerFovDeg = 35.0f;        // 飛翔中ジンバル追尾限界角 (度)
	float maxTrackRange = 5000.0f;     // 最大追尾距離 (m)
	float armDelay = 0.35f;            // 信管解除遅延 (秒)
	float proximityFuseRadius = 11.0f; // 近接信管作動半径 (m)
	float blastRadius = 16.0f;         // 爆風ダメージ半径 (m)
	float warheadDamage = 120.0f;      // 弾頭ダメージ
	float collisionRadius = 1.6f;      // 直接衝突判定半径 (m)
};

// IR誘導空対空ミサイル
class Missile : public ICollisionBody3D {
public:
	Missile() = default;
	~Missile() override = default;

	// 発射
	void Spawn(
		const MyMath::Vector3& position,
		const MyMath::Vector3& initialVelocity,
		const MyMath::Vector3& forwardDir,
		Enemy* targetEnemy,
		GroundEnemy* targetGround,
		const MissileSpec& spec = MissileSpec{}
	);

	// 更新
	void Update(float dt, TerrainChunkManager* terrain);

	// 起爆（hitExplosion: 命中・地形衝突による大爆発ならtrue、寿命自爆ならfalse）
	void Detonate(bool hitExplosion = true);

	// 参照ターゲットが一致する場合に解除（敵再初期化時のダングリング防止）
	void ClearTargets();

	// === アクセッサ ===
	bool IsAlive() const { return isAlive_; }
	bool IsMotorBurning() const { return isMotorBurning_; }
	bool IsArmed() const { return isAlive_ && elapsedTime_ >= spec_.armDelay; }
	float GetElapsedTime() const { return elapsedTime_; }
	const MyMath::Vector3& GetPosition() const { return position_; }
	const MyMath::Vector3& GetPrevPosition() const { return prevPosition_; }
	const MyMath::Vector3& GetVelocity() const { return velocity_; }
	const MyMath::Vector3& GetForward() const { return forward_; }
	const MissileSpec& GetSpec() const { return spec_; }
	float GetDamage() const { return spec_.warheadDamage; }
	Enemy* GetTargetEnemy() const { return targetEnemy_; }
	GroundEnemy* GetTargetGround() const { return targetGround_; }

	// === ICollisionBody3D 実装 ===
	SphereCollider GetSphereCollider() const override {
		return { position_, spec_.collisionRadius };
	}
	uint32_t GetCollisionAttribute() const override { return CollisionAttribute::kPlayerMissile; }
	uint32_t GetCollisionMask() const override { return CollisionMask::kPlayerMissile; }
	void OnCollision(ICollisionBody3D* other) override;
	bool IsCollisionActive() const override { return IsArmed(); }

private:
	void EmitTrailParticles();

	MyMath::Vector3 position_{};
	MyMath::Vector3 prevPosition_{};
	MyMath::Vector3 velocity_{};
	MyMath::Vector3 forward_{ 0.0f, 0.0f, 1.0f };

	Enemy* targetEnemy_ = nullptr;
	GroundEnemy* targetGround_ = nullptr;

	MissileSpec spec_{};
	float elapsedTime_ = 0.0f;
	bool isAlive_ = false;
	bool isMotorBurning_ = false;
};
