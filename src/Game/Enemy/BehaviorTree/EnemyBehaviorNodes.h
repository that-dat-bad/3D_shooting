#pragma once
#include <memory>
#include <string>
#include "BehaviorTree.h"
#include "../Enemy.h"

namespace AI {

/// @brief 待機アクション：ミッションエディタで設定した経路（WaypointPath）に沿って飛行・巡航する
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

/// @brief アグレッションアクション：プレイヤーへ向けた接近・追従・偏差射撃（ドッグファイト）を行う
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

/// @brief 撤退アクション：プレイヤーから距離を取って高速離脱・回避運動を行う
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

/// @brief ビヘイビアツリー生成ファクトリ
class EnemyBehaviorTreeBuilder {
public:
	/// @brief 指定したAIタイプに応じた敵ビヘイビアツリーを構築（デフォルト）
	/// @param aiType ミッションエディタ等で設定されたAIType
	/// @return 構築済みビヘイビアツリーのunique_ptr
	static std::unique_ptr<BehaviorTree> CreateEnemyTree(AIType aiType);

	/// @brief JSON文字列（Blender Live Sync またはミッションJSON）から敵ビヘイビアツリーを動的構築
	/// @param jsonStr behaviorTree の JSON文字列
	/// @param fallbackType パース失敗時またはツリー未定義時のフォールバックAI種別
	/// @return 構築済みビヘイビアツリーのunique_ptr
	static std::unique_ptr<BehaviorTree> BuildFromJson(const std::string& jsonStr, AIType fallbackType);
};

} // namespace AI

