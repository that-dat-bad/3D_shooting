#pragma once
#include <memory>
#include <string>
#include "BehaviorTree.h"
#include "../Enemy.h"

namespace AI {

// 巡航・待機アクション
class WaitPatrolAction : public BTNode {
public:
	explicit WaitPatrolAction(
		float cruiseThrottle = 0.72f,
		bool allowOpportunisticFire = true,
		const char* name = "WaitPatrolAction")
		: cruiseThrottle_(cruiseThrottle),
		  allowOpportunisticFire_(allowOpportunisticFire),
		  name_(name) {}

	NodeStatus Tick(float deltaTime, Enemy& enemy) override;
	const char* GetName() const override { return name_; }

	void SetCruiseThrottle(float throttle) { cruiseThrottle_ = throttle; }
	float GetCruiseThrottle() const { return cruiseThrottle_; }
	void SetAllowOpportunisticFire(bool allow) { allowOpportunisticFire_ = allow; }
	bool GetAllowOpportunisticFire() const { return allowOpportunisticFire_; }

private:
	float cruiseThrottle_ = 0.72f;
	bool allowOpportunisticFire_ = true;
	const char* name_ = "WaitPatrolAction";
};

// 攻撃アクション
class AggressionAction : public BTNode {
public:
	explicit AggressionAction(
		float engagementRange = 1200.0f,
		float fireRange = 1000.0f,
		const char* name = "AggressionAction")
		: engagementRange_(engagementRange),
		  fireRange_(fireRange),
		  name_(name) {}

	NodeStatus Tick(float deltaTime, Enemy& enemy) override;
	const char* GetName() const override { return name_; }

	void SetEngagementRange(float range) { engagementRange_ = range; }
	float GetEngagementRange() const { return engagementRange_; }
	void SetFireRange(float range) { fireRange_ = range; }
	float GetFireRange() const { return fireRange_; }

private:
	float engagementRange_ = 1200.0f;
	float fireRange_ = 1000.0f;
	const char* name_ = "AggressionAction";
};

// 撤退アクション
class RetreatAction : public BTNode {
public:
	explicit RetreatAction(
		float safeDistanceMeters = 1400.0f,
		bool useEvadeWobble = true,
		const char* name = "RetreatAction")
		: safeDistanceMeters_(safeDistanceMeters),
		  useEvadeWobble_(useEvadeWobble),
		  name_(name) {}

	NodeStatus Tick(float deltaTime, Enemy& enemy) override;
	void Reset() override;
	const char* GetName() const override { return name_; }

	void SetSafeDistance(float dist) { safeDistanceMeters_ = dist; }
	float GetSafeDistance() const { return safeDistanceMeters_; }
	void SetUseEvadeWobble(bool wobble) { useEvadeWobble_ = wobble; }
	bool GetUseEvadeWobble() const { return useEvadeWobble_; }

private:
	float safeDistanceMeters_ = 1400.0f;
	bool useEvadeWobble_ = true;
	float evadeWobbleTimer_ = 0.0f;
	const char* name_ = "RetreatAction";
};

// ビヘイビアツリー生成クラス
class EnemyBehaviorTreeBuilder {
public:
	// AIタイプに応じたツリー生成
	// aiType: AIタイプ
	static std::unique_ptr<BehaviorTree> CreateEnemyTree(AIType aiType);

	// JSON文字列からツリーを動的構築
	// jsonStr: JSON文字列
	static std::unique_ptr<BehaviorTree> BuildFromJson(const std::string& jsonStr, AIType fallbackType);
};

} // namespace AI

