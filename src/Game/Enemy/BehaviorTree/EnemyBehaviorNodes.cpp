#include "EnemyBehaviorNodes.h"
#include <cmath>
#include <algorithm>
#include "../../../engine/io/json.hpp"

namespace AI {

// ============================================================================
// 待機アクション
// ミッションエディタで設定した WaypointPath に沿って飛行
// ============================================================================
NodeStatus WaitPatrolAction::Tick(float deltaTime, Enemy& enemy) {
	enemy.SetActiveBehaviorName("WaitPatrol");

	const auto& waypoints = enemy.GetWaypointPoints();

	// ウェイポイントが設定されていない場合は、現在の巡航方向へ安定飛行
	if (waypoints.empty()) {
		enemy.SetAimTargetDirection(enemy.GetCruiseDirection(), deltaTime);
		enemy.SetThrottleInput(cruiseThrottle_);
		return NodeStatus::Running;
	}

	size_t currentIdx = enemy.GetCurrentWaypointIndex();
	if (currentIdx >= waypoints.size()) {
		currentIdx = EnemyWaypointDefaults::kInitialWaypointIndex;
		enemy.SetCurrentWaypointIndex(currentIdx);
	}

	const Waypoint& activeWp = waypoints[currentIdx];
	const MyMath::Vector3 myPos = enemy.GetPosition();
	const MyMath::Vector3 toWp = MyMath::Subtract(activeWp.position, myPos);
	const float distToWp = MyMath::Length(toWp);

	// ウェイポイント到達判定（85m以内）
	if (distToWp <= EnemyWaypointDefaults::kWaypointCaptureRadiusMeters) {
		enemy.AdvanceWaypoint();
		currentIdx = enemy.GetCurrentWaypointIndex();
	}

	// 目標ウェイポイントへの機首誘導
	const Waypoint& targetWp = waypoints[enemy.GetCurrentWaypointIndex()];
	const MyMath::Vector3 toTargetWp = MyMath::Subtract(targetWp.position, myPos);
	MyMath::Vector3 targetDir = enemy.GetCruiseDirection();
	if (MyMath::Length(toTargetWp) > EnemyWaypointDefaults::kMinTargetDirectionLength) {
		targetDir = MyMath::Normalize(toTargetWp);
		enemy.SetCruiseDirection(targetDir);
	}

	enemy.SetAimTargetDirection(targetDir, deltaTime);

	// ウェイポイントの目標速度に合わせたスロットル調整
	const float desiredSpeedKmh = (targetWp.speed > EnemyWaypointDefaults::kZero)
		? targetWp.speed
		: EnemyWaypointDefaults::kDefaultWaypointSpeedKmh;
	const float desiredSpeedMps = desiredSpeedKmh / EnemyWaypointDefaults::kKmhPerMeterPerSec;
	const float speedError = desiredSpeedMps - enemy.GetSpeed();
	const float desiredThrottle = std::clamp(
		cruiseThrottle_ + speedError * EnemyWaypointDefaults::kThrottleSpeedErrorGain,
		EnemyWaypointDefaults::kMinThrottle,
		EnemyWaypointDefaults::kMaxThrottle);
	enemy.SetThrottleInput(desiredThrottle);

	// 機会射撃（正面約5度以内、850m以内に自機がいれば牽制発射）
	if (allowOpportunisticFire_ && enemy.IsPlayerValid()) {
		const float distToPlayer = enemy.GetDistanceToPlayer();
		if (distToPlayer > EnemyWaypointDefaults::kMinTargetDirectionLength &&
			distToPlayer < EnemyWaypointDefaults::kOpportunisticFireRangeMeters) {
			const MyMath::Vector3 dirToPlayer = enemy.GetDirectionToPlayer();
			const MyMath::Vector3 forward = enemy.GetForward();
			const float dot = MyMath::Dot(forward, dirToPlayer);
			if (dot > EnemyWaypointDefaults::kOpportunisticFireDotThreshold) {
				enemy.TryFireGunpod(dirToPlayer, EnemyWaypointDefaults::kOpportunisticFireRangeMeters);
			}
		}
	}

	return NodeStatus::Running;
}

// ============================================================================
// アグレッションアクション (AggressionAction)
// プレイヤーへ向けた接近・追従・偏差射撃（ドッグファイト）
// ============================================================================
NodeStatus AggressionAction::Tick(float deltaTime, Enemy& enemy) {
	if (!enemy.IsPlayerValid()) {
		return NodeStatus::Failure;
	}

	enemy.SetActiveBehaviorName("Aggression");

	const MyMath::Vector3 playerPos = enemy.GetPlayerPosition();
	const MyMath::Vector3 playerVel = enemy.GetPlayerVelocity();
	const MyMath::Vector3 myPos = enemy.GetPosition();
	const MyMath::Vector3 toPlayer = MyMath::Subtract(playerPos, myPos);
	const float distance = MyMath::Length(toPlayer);

	// 偏差射撃（リード予測）の計算：弾速と相手速度から未来位置を2次予測
	const float bulletSpeed = enemy.GetSpeed() + 500.0f;
	float timeToHit = (bulletSpeed > 0.1f) ? (distance / bulletSpeed) : 0.0f;
	MyMath::Vector3 futurePos = MyMath::Add(playerPos, MyMath::Multiply(timeToHit, playerVel));

	const float distance2 = MyMath::Length(MyMath::Subtract(futurePos, myPos));
	timeToHit = (bulletSpeed > 0.1f) ? (distance2 / bulletSpeed) : 0.0f;
	futurePos = MyMath::Add(playerPos, MyMath::Multiply(timeToHit, playerVel));

	const MyMath::Vector3 toFuture = MyMath::Subtract(futurePos, myPos);
	if (MyMath::Length(toFuture) > EnemyWaypointDefaults::kMinTargetDirectionLength) {
		const MyMath::Vector3 targetDir = MyMath::Normalize(toFuture);
		enemy.SetAimTargetDirection(targetDir, deltaTime);
	}

	// 距離に応じたスロットル調整（交戦距離に応じて追従・減速）
	if (distance > engagementRange_ * 0.8f) {
		enemy.SetThrottleInput(1.0f);
	} else if (distance < 300.0f) {
		enemy.SetThrottleInput(0.4f);
	} else {
		enemy.SetThrottleInput(0.7f);
	}

	// 射撃判定：機首が未来位置に対して約5度以内、かつ指定射程内なら射撃
	if (MyMath::Length(toFuture) > EnemyWaypointDefaults::kMinTargetDirectionLength) {
		const MyMath::Vector3 leadDir = MyMath::Normalize(toFuture);
		enemy.TryFireGunpod(leadDir, fireRange_);
	}

	return NodeStatus::Running;
}

// ============================================================================
// 撤退アクション (RetreatAction)
// プレイヤーから距離を取って高速離脱・回避運動
// ============================================================================
NodeStatus RetreatAction::Tick(float deltaTime, Enemy& enemy) {
	if (!enemy.IsPlayerValid()) {
		return NodeStatus::Failure;
	}

	enemy.SetActiveBehaviorName("Retreat");
	evadeWobbleTimer_ += deltaTime;

	const MyMath::Vector3 myPos = enemy.GetPosition();
	const MyMath::Vector3 playerPos = enemy.GetPlayerPosition();
	const MyMath::Vector3 awayVector = MyMath::Subtract(myPos, playerPos);
	const float distance = MyMath::Length(awayVector);

	// 安全距離まで十分に離脱できたら成功（再評価サイクルへ移行）
	if (distance >= safeDistanceMeters_) {
		Reset();
		return NodeStatus::Success;
	}

	// プレイヤーとは反対方向を基本離脱ベクトルとする
	MyMath::Vector3 evadeDir = enemy.GetForward();
	if (distance > EnemyWaypointDefaults::kMinTargetDirectionLength) {
		evadeDir = MyMath::Normalize(awayVector);
	}

	// 回避機動（蛇行・上昇）
	const float wobbleX = useEvadeWobble_ ? (std::sin(evadeWobbleTimer_ * 1.8f) * 0.22f) : 0.0f;
	const float climbY = useEvadeWobble_ ? 0.20f : 0.0f;
	MyMath::Vector3 dynamicEvadeDir = {
		evadeDir.x + wobbleX,
		evadeDir.y + climbY,
		evadeDir.z
	};
	if (MyMath::Length(dynamicEvadeDir) > EnemyWaypointDefaults::kMinTargetDirectionLength) {
		dynamicEvadeDir = MyMath::Normalize(dynamicEvadeDir);
	}

	enemy.SetAimTargetDirection(dynamicEvadeDir, deltaTime);

	// 離脱時は全力加速（スロットル100% / WEP）
	enemy.SetThrottleInput(1.0f);

	return NodeStatus::Running;
}

void RetreatAction::Reset() {
	evadeWobbleTimer_ = 0.0f;
}

// ============================================================================
// ビヘイビアツリー構築ファクトリ
// ============================================================================
std::unique_ptr<BehaviorTree> EnemyBehaviorTreeBuilder::CreateEnemyTree(AIType aiType) {
	auto tree = std::make_unique<BehaviorTree>();
	auto rootSelector = std::make_unique<SelectorNode>("EnemyRootSelector");

	// ------------------------------------------------------------------------
	// 1. 撤退シーケンス (優先度：高)
	// HPが35%未満の危機的状況、またはCruiseEvadeで接近された場合に発動
	// ------------------------------------------------------------------------
	auto retreatSeq = std::make_unique<SequenceNode>("RetreatSequence");
	retreatSeq->AddChild(std::make_unique<ConditionNode>("ShouldRetreat", [aiType](Enemy& e) {
		if (!e.IsPlayerValid()) {
			return false;
		}
		// 重度ダメージ時の緊急撤退
		if (e.GetHealthRatio() < 0.35f) {
			return true;
		}
		// 回避AIタイプの場合、プレイヤーが接近（600m以内）したら撤退
		if (aiType == AIType::CruiseEvade && e.GetDistanceToPlayer() < 600.0f) {
			return true;
		}
		return false;
	}));
	retreatSeq->AddChild(std::make_unique<RetreatAction>());
	rootSelector->AddChild(std::move(retreatSeq));

	// ------------------------------------------------------------------------
	// 2. アグレッションシーケンス (優先度：中)
	// ChaseAttack設定、または索敵・交戦距離内に自機がいる場合に発動
	// ------------------------------------------------------------------------
	auto aggressionSeq = std::make_unique<SequenceNode>("AggressionSequence");
	aggressionSeq->AddChild(std::make_unique<ConditionNode>("ShouldEngage", [aiType](Enemy& e) {
		if (!e.IsPlayerValid()) {
			return false;
		}
		// 追従攻撃AIなら常時交戦
		if (aiType == AIType::ChaseAttack) {
			return true;
		}
		// ウェイポイント巡航または回避AIであっても、プレイヤーが1200m以内に接近したら迎撃
		const float dist = e.GetDistanceToPlayer();
		if (dist < 1200.0f) {
			return true;
		}
		return false;
	}));
	aggressionSeq->AddChild(std::make_unique<AggressionAction>());
	rootSelector->AddChild(std::move(aggressionSeq));

	// ------------------------------------------------------------------------
	// 3. 待機・ミッションエディタ経路追従アクション (優先度：低 / デフォルト)
	// 上記条件に該当しない場合は、エディタで設定したWaypointPathに沿って巡航
	// ------------------------------------------------------------------------
	rootSelector->AddChild(std::make_unique<WaitPatrolAction>());

	tree->SetRoot(std::move(rootSelector));
	return tree;
}

// 内部ヘルパー：Blender Node Editor JSON ノードの再帰パース
static std::unique_ptr<BTNode> ParseNodeRecursive(const nlohmann::json& nodeJson) {
	if (!nodeJson.is_object()) {
		return nullptr;
	}

	const std::string nodeType = nodeJson.value("type", "");

	if (nodeType == "BTRootNode") {
		if (nodeJson.contains("children") && nodeJson["children"].is_array() && !nodeJson["children"].empty()) {
			return ParseNodeRecursive(nodeJson["children"].front());
		}
		return nullptr;
	}

	if (nodeType == "BTSelectorNode") {
		auto selector = std::make_unique<SelectorNode>("Selector");
		if (nodeJson.contains("children") && nodeJson["children"].is_array()) {
			for (const auto& childJson : nodeJson["children"]) {
				auto childNode = ParseNodeRecursive(childJson);
				if (childNode) {
					selector->AddChild(std::move(childNode));
				}
			}
		}
		return selector;
	}

	if (nodeType == "BTSequenceNode") {
		auto seq = std::make_unique<SequenceNode>("Sequence");
		if (nodeJson.contains("children") && nodeJson["children"].is_array()) {
			for (const auto& childJson : nodeJson["children"]) {
				auto childNode = ParseNodeRecursive(childJson);
				if (childNode) {
					seq->AddChild(std::move(childNode));
				}
			}
		}
		return seq;
	}

	if (nodeType == "BTConditionHealthNode") {
		const float threshPct = nodeJson.value("thresholdPct", 35.0f);
		const float ratio = threshPct * 0.01f;
		return std::make_unique<ConditionNode>("ConditionHealth", [ratio](Enemy& e) {
			return e.IsPlayerValid() && (e.GetHealthRatio() < ratio);
		});
	}

	if (nodeType == "BTConditionDistanceNode") {
		const float distMeters = nodeJson.value("distanceMeters", 1200.0f);
		return std::make_unique<ConditionNode>("ConditionDistance", [distMeters](Enemy& e) {
			return e.IsPlayerValid() && (e.GetDistanceToPlayer() <= distMeters);
		});
	}

	if (nodeType == "BTActionRetreatNode") {
		const float safeDist = nodeJson.value("safeDistance", 1400.0f);
		const bool useEvadeWobble = nodeJson.value("useEvadeWobble", true);
		return std::make_unique<RetreatAction>(safeDist, useEvadeWobble);
	}

	if (nodeType == "BTActionAggressionNode") {
		const float engageDist = nodeJson.value("engagementRange", 1200.0f);
		const float fireRange = nodeJson.value("fireRange", 1000.0f);
		return std::make_unique<AggressionAction>(engageDist, fireRange);
	}

	if (nodeType == "BTActionWaitPatrolNode") {
		const float cruiseThrottle = nodeJson.value("cruiseThrottle", 0.72f);
		const bool oppFire = nodeJson.value("allowOpportunisticFire", true);
		return std::make_unique<WaitPatrolAction>(cruiseThrottle, oppFire);
	}

	return nullptr;
}

std::unique_ptr<BehaviorTree> EnemyBehaviorTreeBuilder::BuildFromJson(
	const std::string& jsonStr,
	AIType fallbackType
) {
	if (jsonStr.empty()) {
		return CreateEnemyTree(fallbackType);
	}

	try {
		const nlohmann::json rootJson = nlohmann::json::parse(jsonStr);
		if (!rootJson.is_object()) {
			return CreateEnemyTree(fallbackType);
		}

		// 1. ノードエディタグラフ（rootノード）が存在し、useCustomParamsOnly でない場合はグラフから構築
		const bool useCustomParamsOnly = rootJson.value("useCustomParamsOnly", false);
		if (!useCustomParamsOnly && rootJson.contains("root") && rootJson["root"].is_object()) {
			auto rootNode = ParseNodeRecursive(rootJson["root"]);
			if (rootNode) {
				auto tree = std::make_unique<BehaviorTree>();
				tree->SetRoot(std::move(rootNode));
				return tree;
			}
		}

		// 2. Nパネルパラメータ個別上書き（overrideParams）からの動的構築
		nlohmann::json params = nlohmann::json::object();
		if (rootJson.contains("overrideParams") && rootJson["overrideParams"].is_object()) {
			params = rootJson["overrideParams"];
		}

		const float retreatHpPct = params.value("retreatHpPct", 35.0f);
		const float retreatSafeDist = params.value("retreatSafeDist", 1400.0f);
		const float engageDist = params.value("engageDist", 1200.0f);
		const float fireRange = params.value("fireRange", 1000.0f);
		const float patrolThrottle = params.value("patrolThrottle", 0.72f);

		auto tree = std::make_unique<BehaviorTree>();
		auto rootSelector = std::make_unique<SelectorNode>("DynamicRootSelector");

		// (A) 撤退シーケンス
		auto retreatSeq = std::make_unique<SequenceNode>("RetreatSequence");
		const float retreatRatio = retreatHpPct * 0.01f;
		retreatSeq->AddChild(std::make_unique<ConditionNode>("LowHealthCheck", [retreatRatio, fallbackType](Enemy& e) {
			if (!e.IsPlayerValid()) return false;
			if (e.GetHealthRatio() < retreatRatio) return true;
			if (fallbackType == AIType::CruiseEvade && e.GetDistanceToPlayer() < 600.0f) return true;
			return false;
		}));
		retreatSeq->AddChild(std::make_unique<RetreatAction>(retreatSafeDist, true));
		rootSelector->AddChild(std::move(retreatSeq));

		// (B) アグレッションシーケンス
		auto aggressionSeq = std::make_unique<SequenceNode>("AggressionSequence");
		aggressionSeq->AddChild(std::make_unique<ConditionNode>("EngageCheck", [engageDist, fallbackType](Enemy& e) {
			if (!e.IsPlayerValid()) return false;
			if (fallbackType == AIType::ChaseAttack) return true;
			return e.GetDistanceToPlayer() <= engageDist;
		}));
		aggressionSeq->AddChild(std::make_unique<AggressionAction>(engageDist, fireRange));
		rootSelector->AddChild(std::move(aggressionSeq));

		// (C) 待機・経路追従アクション
		rootSelector->AddChild(std::make_unique<WaitPatrolAction>(patrolThrottle, true));

		tree->SetRoot(std::move(rootSelector));
		return tree;
	}
	catch (...) {
		// パース例外時は安全にフォールバックツリーを生成
		return CreateEnemyTree(fallbackType);
	}
}

} // namespace AI

