#pragma once
#include <memory>
#include <string>
#include <vector>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Physics/ICollisionBody.h"
#include "../../engine/Physics/CollisionConfig.h"
#include "../Bullet/BulletManager.h"
#include "../System/Waypoint.h"

namespace GroundWaypointDefaults {
	inline constexpr size_t kMinPathPoints = 2;
	inline constexpr size_t kInitialIndex = 0;
	inline constexpr int kForwardStep = 1;
	inline constexpr int kBackwardStep = -1;
	inline constexpr float kZero = 0.0f;
	inline constexpr float kHalf = 0.5f;
	inline constexpr float kReachRadiusMeters = 6.0f;
	inline constexpr float kMinDistEpsilon = 0.001f;
}

class FlightModel;
class Camera;

// 地上目標のAIタイプ
enum class GroundAIType {
	Turret,          // 対空砲台
	Structure,       // 固定施設
	PatrolVehicle    // 地上車両
};

// 地上目標のパラメータ
struct GroundEnemyParam {
	float maxHealth = 60.0f;
	float collisionRadius = 8.0f;
	
	// 対空砲火パラメータ
	float fireRange = 1500.0f;       // 射程距離 (m)
	float minElevationDeg = -5.0f;   // 最低仰角 (度)
	float maxElevationDeg = 85.0f;   // 最高仰角 (度)
	float turnSpeedDeg = 60.0f;      // 砲塔旋回速度 (度/秒)
	float bulletSpeed = 600.0f;      // 弾速 (m/s)
	float bulletDamage = 8.0f;       // 弾丸ダメージ
	float fireInterval = 0.08f;      // 連射間隔 (秒)
	int burstCount = 6;              // 連射数
	float burstCooldown = 2.0f;      // クールダウン (秒)

	// 移動パラメータ
	float moveSpeed = 15.0f;         // 移動速度 (m/s)
	float patrolDistance = 200.0f;   // パトロール往復距離 (m)
};

// 地上敵クラス
class GroundEnemy : public ICollisionBody3D {
public:
	GroundEnemy() = default;
	virtual ~GroundEnemy() = default;

	// 初期化
	// position: 初期座標
	// aiType: AIタイプ
	// param: パラメータ
	void Initialize(
		const MyMath::Vector3& position,
		GroundAIType aiType,
		const GroundEnemyParam& param,
		FlightModel* playerFlightModel,
		BulletManager* bulletManager
	);

	// 更新
	// deltaTime: 経過時間
	void Update(float deltaTime);

	// 描画
	// camera: カメラ
	void Draw(Camera* camera);

	// ダメージ処理
	// damage: ダメージ量
	void TakeDamage(float damage);

	// ウェイポイントパス設定
	void SetWaypointPath(const WaypointPath& path);

	// === アクセッサ ===
	bool IsAlive() const { return isAlive_; }
	const MyMath::Vector3& GetPosition() const { return position_; }
	void SetPosition(const MyMath::Vector3& pos) { position_ = pos; initialPosition_ = pos; }
	float GetHealth() const { return health_; }
	float GetMaxHealth() const { return maxHealth_; }
	GroundAIType GetAIType() const { return aiType_; }
	const GroundEnemyParam& GetParam() const { return param_; }
	GroundEnemyParam& GetParam() { return param_; }
	float GetTurretYaw() const { return turretYaw_; }
	float GetTurretPitch() const { return turretPitch_; }
	bool IsTargetInSight() const { return isTargetInSight_; }
	const char* GetAITypeString() const;

	// === ICollisionBody3D 実装 ===
	SphereCollider GetSphereCollider() const override {
		return { position_, param_.collisionRadius };
	}
	uint32_t GetCollisionAttribute() const override { return CollisionAttribute::kEnemy; }
	uint32_t GetCollisionMask() const override { return CollisionMask::kEnemy; }
	void OnCollision(ICollisionBody3D* other) override;
	bool IsCollisionActive() const override { return isAlive_; }

private:
	// AI更新
	void UpdateAI(float deltaTime);

	// 射撃処理
	void Fire();

	// 対空砲台の描画
	void DrawTurret(Camera* camera);

	// 地上施設の描画
	void DrawStructure(Camera* camera);

	// 地上車両の描画
	void DrawVehicle(Camera* camera);

	// 撃破演出の描画
	void DrawDestroyed(Camera* camera);

private:
	// 基本情報
	MyMath::Vector3 position_{ 0.0f, 0.0f, 0.0f };
	MyMath::Vector3 initialPosition_{ 0.0f, 0.0f, 0.0f };
	GroundAIType aiType_ = GroundAIType::Turret;
	GroundEnemyParam param_{};

	float health_ = 60.0f;
	float maxHealth_ = 60.0f;
	bool isAlive_ = true;

	// 参照
	FlightModel* playerFlightModel_ = nullptr;
	BulletManager* bulletManager_ = nullptr;

	// 砲塔の向き（ラジアン）
	float turretYaw_ = 0.0f;         // ヨー角（水平回転 Y軸回り）
	float turretPitch_ = 0.3f;       // ピッチ角（仰角 X軸回り、上向きが正）
	float targetYaw_ = 0.0f;
	float targetPitch_ = 0.3f;
	bool isTargetInSight_ = false;   // プレイヤーを射程・射角内に捉えているか

	// 射撃制御タイマー
	float fireTimer_ = 0.0f;         // 次の弾発射までのタイマー
	int currentBurstShot_ = 0;       // 現在のバースト内で発射した弾数
	float cooldownTimer_ = 0.0f;     // バースト後のリロード/クールダウンタイマー
	int barrelToggle_ = 0;           // 左右砲身交互発射フラグ

	// マズルフラッシュ
	float muzzleFlashTimer_ = 0.0f;
	MyMath::Vector3 muzzleFlashPos_{ 0.0f, 0.0f, 0.0f };
	MyMath::Vector3 muzzleFlashDir_{ 0.0f, 1.0f, 0.0f };

	// 移動制御 (Patrol用)
	float moveDirection_ = 1.0f;     // +1.0 または -1.0
	float currentPatrolOffset_ = 0.0f;
	bool hasCustomBodyYaw_ = false;
	float customBodyYaw_ = GroundWaypointDefaults::kZero;
	std::vector<Waypoint> waypointPoints_;
	bool waypointIsLoop_ = false;
	size_t currentWaypointIndex_ = GroundWaypointDefaults::kInitialIndex;
	int waypointStepDir_ = GroundWaypointDefaults::kForwardStep;

	// アニメーション用タイマー
	float animTime_ = 0.0f;
	float smokeEmitTimer_ = 0.0f;    // 撃破後の発煙タイマー
};
