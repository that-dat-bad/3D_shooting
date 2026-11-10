#pragma once
#include <memory>
#include <string>
#include "BehaviorTree/BehaviorTree.h"
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/Physics/ICollisionBody.h"
#include "../../engine/Physics/CollisionConfig.h"
#include "../FlightModel/FlightModel.h"
#include "../FlightModel/Payload/Gunpod.h"
#include "../Bullet/BulletManager.h"
#include "../FlightModel/MouseAimController.h"
#include "../System/Waypoint.h"

namespace EnemyWaypointDefaults {
	inline constexpr size_t kInitialWaypointIndex = 0;
	inline constexpr size_t kWaypointStep = 1;
	inline constexpr int kMuzzleFlashSkipModulo = 2;
	inline constexpr int kEvenRemainder = 0;
	inline constexpr float kZero = 0.0f;
	inline constexpr float kUnit = 1.0f;
	inline constexpr float kKmhPerMeterPerSec = 3.6f;
	inline constexpr float kWaypointCaptureRadiusMeters = 85.0f;
	inline constexpr float kMinTargetDirectionLength = 0.001f;
	inline constexpr float kMinThrottle = 0.25f;
	inline constexpr float kMaxThrottle = 1.00f;
	inline constexpr float kBaseCruiseThrottle = 0.72f;
	inline constexpr float kThrottleSpeedErrorGain = 0.025f;
	inline constexpr float kDefaultWaypointSpeedKmh = 280.0f;
	inline constexpr float kOpportunisticFireRangeMeters = 850.0f;
	inline constexpr float kOpportunisticFireDotThreshold = 0.995f;
	inline constexpr float kMuzzleForwardOffset = 8.0f;
	inline constexpr float kBulletMuzzleSpeed = 500.0f;
	inline constexpr float kBulletDamage = 10.0f;
	inline constexpr float kMuzzleFlashDuration = 0.08f;
}

class Object3dCommon;
class Camera;

namespace AI {
	class BehaviorTree;
}

enum class AIType {
	ChaseAttack,
	CruiseEvade,
	FollowWaypoint
};

enum class TypeBState {
	Cruising,
	Evading
};

/// <summary>
/// 空中ターゲット（敵AI）
/// </summary>
class Enemy : public ICollisionBody3D {
public:
	Enemy();
	~Enemy();

	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="position">配置座標</param>
	/// <param name="modelPath">モデルファイルパス</param>
	/// <param name="maxHealth">最大HP</param>
	/// <param name="airframeData">機体パラメータ</param>
	/// <param name="engineData">エンジンパラメータ</param>
	/// <param name="gunpodData">ガンポッドパラメータ</param>
	/// <param name="playerFlightModel">自機のフライトモデルへのポインタ</param>
	/// <param name="bulletManager">弾丸マネージャーへのポインタ</param>
	/// <param name="aiType">AI挙動タイプ</param>
	/// <param name="behaviorTreeJson">ビヘイビアツリー定義JSON（任意）</param>
	void Initialize(
		const MyMath::Vector3& position, 
		const std::string& modelPath, 
		float maxHealth,
		const AirframeData& airframeData,
		const EngineData& engineData,
		const GunPodData& gunpodData,
		FlightModel* playerFlightModel,
		BulletManager* bulletManager,
		AIType aiType = AIType::ChaseAttack,
		const std::string& behaviorTreeJson = ""
	);

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update(float deltaTime);

	/// <summary>
	/// 描画処理
	/// </summary>
	void Draw();

	/// <summary>
	/// ダメージを受ける
	/// </summary>
	/// <param name="damage">ダメージ量</param>
	void TakeDamage(float damage);

	/// @brief 追従するウェイポイントパスを設定
	void SetWaypointPath(const WaypointPath& path);

	// === アクセッサ ===
	bool IsAlive() const { return isAlive_; }
	MyMath::Vector3 GetPosition() const { return flightModel_.GetPosition(); }
	float GetCollisionRadius() const { return collisionRadius_; }
	float GetHealth() const { return health_; }
	float GetMaxHealth() const { return maxHealth_; }
	FlightModel& GetFlightModel() { return flightModel_; }
	const FlightModel& GetFlightModel() const { return flightModel_; }
	AIType GetAIType() const { return aiType_; }
	TypeBState GetTypeBState() const { return typeBState_; }
	const char* GetTypeBStateString() const {
		return typeBState_ == TypeBState::Cruising ? "Cruising" : "Evading";
	}

	// === ビヘイビアツリー連携・状況取得ヘルパー ===
	float GetHealthRatio() const { return (maxHealth_ > 0.0f) ? (health_ / maxHealth_) : 0.0f; }
	bool IsPlayerValid() const { return playerFlightModel_ != nullptr; }
	MyMath::Vector3 GetPlayerPosition() const;
	MyMath::Vector3 GetPlayerVelocity() const;
	float GetDistanceToPlayer() const;
	MyMath::Vector3 GetDirectionToPlayer() const;
	MyMath::Vector3 GetVelocity() const { return flightModel_.GetVelocity(); }
	MyMath::Vector3 GetForward() const { return flightModel_.GetForwardDirection(); }
	float GetSpeed() const { return flightModel_.GetSpeed(); }

	// === ビヘイビアツリー操縦・射撃インターフェース ===
	void SetAimTargetDirection(const MyMath::Vector3& targetDir, float deltaTime);
	void SetThrottleInput(float throttle) { flightModel_.SetThrottleInput(throttle); }
	bool TryFireGunpod(const MyMath::Vector3& targetLeadDir, float maxDistance = 1000.0f);

	// === ウェイポイント・巡航関連 ===
	const std::vector<Waypoint>& GetWaypointPoints() const { return waypointPoints_; }
	bool GetWaypointIsLoop() const { return waypointIsLoop_; }
	size_t GetCurrentWaypointIndex() const { return currentWaypointIndex_; }
	void SetCurrentWaypointIndex(size_t index) { currentWaypointIndex_ = index; }
	void AdvanceWaypoint();
	const MyMath::Vector3& GetCruiseDirection() const { return cruiseDirection_; }
	void SetCruiseDirection(const MyMath::Vector3& dir) { cruiseDirection_ = dir; }

	// === 実行中ビヘイビア名 ===
	const char* GetActiveBehaviorName() const { return activeBehaviorName_; }
	void SetActiveBehaviorName(const char* name) { activeBehaviorName_ = name; }
	AI::BehaviorTree* GetBehaviorTree() { return behaviorTree_.get(); }
	const std::string& GetBehaviorTreeJson() const { return behaviorTreeJson_; }
	void SetBehaviorTreeJson(const std::string& json);
	void RebuildBehaviorTree();

	// === ICollisionBody3D 実装 ===
	SphereCollider GetSphereCollider() const override {
		return { flightModel_.GetPosition(), collisionRadius_ };
	}
	uint32_t GetCollisionAttribute() const override { return CollisionAttribute::kEnemy; }
	uint32_t GetCollisionMask() const override { return CollisionMask::kEnemy; }
	void OnCollision(ICollisionBody3D* other) override;
	bool IsCollisionActive() const override { return isAlive_; }

private:
	/// @brief AI思考・操縦入力の更新
	void UpdateAI(float deltaTime);

	// 物理モデルとAIコントローラー
	FlightModel flightModel_;
	MouseAimController aimController_;
	GunPod gunpod_;

	// 参照ポインタ
	FlightModel* playerFlightModel_ = nullptr;
	BulletManager* bulletManager_ = nullptr;

	float health_ = 0.0f;
	float maxHealth_ = 0.0f;
	bool isAlive_ = true;

	// 衝突判定用の半径
	float collisionRadius_ = 5.0f;

	// エフェクト関連
	float muzzleFlashTimer_ = 0.0f;
	int muzzleFlashCount_ = 0;

	// 描画用3Dオブジェクト
	std::unique_ptr<Object3d> object3d_ = nullptr;

	// AI設定・ステート
	AIType aiType_ = AIType::ChaseAttack;
	TypeBState typeBState_ = TypeBState::Cruising;
	MyMath::Vector3 cruiseDirection_ = { 0.0f, 0.0f, 1.0f };
	std::vector<Waypoint> waypointPoints_;
	bool waypointIsLoop_ = false;
	size_t currentWaypointIndex_ = EnemyWaypointDefaults::kInitialWaypointIndex;

	// ビヘイビアツリー管理
	std::string behaviorTreeJson_ = "";
	std::unique_ptr<AI::BehaviorTree> behaviorTree_;
	const char* activeBehaviorName_ = "WaitPatrol";
};
