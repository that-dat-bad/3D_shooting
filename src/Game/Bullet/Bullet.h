#pragma once
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Physics/Collider.h"
#include "../../engine/Physics/ICollisionBody.h"
#include "../../engine/Physics/CollisionConfig.h"

// 弾丸
class Bullet : public ICollisionBody3D {
public:
	Bullet() = default;
	~Bullet() = default;

	// 弾丸を発射
	// position : 初期位置
	// velocity : 初速度ベクトル
	// damage : ダメージ
	// isEnemyBullet : 敵の弾丸フラグ
	void Spawn(const MyMath::Vector3& position, const MyMath::Vector3& velocity, float damage, bool isEnemyBullet = false);

	// 更新
	void Update(float dt);

	// 弾丸を消滅
	void Kill() { isAlive_ = false; }

	// === アクセッサ ===
	bool IsAlive() const { return isAlive_; }
	MyMath::Vector3 GetPosition() const { return position_; }
	MyMath::Vector3 GetVelocity() const { return velocity_; }
	float GetDamage() const { return damage_; }
	bool IsEnemyBullet() const { return isEnemyBullet_; }
	
	static constexpr int kMaxHistory = 4;
	const MyMath::Vector3* GetHistory() const { return history_; }
	int GetHistoryCount() const { return historyCount_; }

	// コライダー取得
	SphereCollider GetCollider() const;

	// === ICollisionBody3D 実装 ===
	SphereCollider GetSphereCollider() const override { return GetCollider(); }
	uint32_t GetCollisionAttribute() const override {
		return isEnemyBullet_ ? CollisionAttribute::kEnemyBullet : CollisionAttribute::kPlayerBullet;
	}
	uint32_t GetCollisionMask() const override {
		return isEnemyBullet_ ? CollisionMask::kEnemyBullet : CollisionMask::kPlayerBullet;
	}
	void OnCollision(ICollisionBody3D* other) override;
	bool IsCollisionActive() const override { return isAlive_; }

private:
	MyMath::Vector3 position_{};
	MyMath::Vector3 velocity_{};
	float damage_ = 0.0f;
	float lifeTime_ = 5.0f;     // 最大生存時間（秒）
	float currentTime_ = 0.0f;
	bool isAlive_ = false;
	bool isEnemyBullet_ = false;

	MyMath::Vector3 history_[kMaxHistory]{};
	int historyCount_ = 0;

	static constexpr float kCollisionRadius = 0.5f;
};
