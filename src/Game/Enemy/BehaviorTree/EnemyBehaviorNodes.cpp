#include "EnemyBehaviorNodes.h"
#include <cmath>
#include <algorithm>
#include "../../../engine/io/json.hpp"

namespace AI {

// 待機アクション
NodeStatus WaitPatrolAction::Tick(float deltaTime, Enemy& enemy) {
	enemy.SetActiveBehaviorName("WaitPatrol");

	const auto& waypoints = enemy.GetWaypointPoints();
	const bool hasPlayer = enemy.IsPlayerValid();
	const float distToPlayer = hasPlayer ? enemy.GetDistanceToPlayer() : 0.0f;

	// ウェイポイント未設定、またはプレイヤーから1500m以上離れた場合はプレイヤー方向へ接近してエリア外離脱を防ぐ
	if (waypoints.empty() || (hasPlayer && distToPlayer > 1500.0f)) {
		MyMath::Vector3 targetDir = enemy.GetCruiseDirection();
		if (hasPlayer) {
			targetDir = enemy.GetDirectionToPlayer();
			enemy.SetCruiseDirection(targetDir);
		}
		enemy.SetAimTargetDirection(targetDir, deltaTime);
		enemy.SetThrottleInput(hasPlayer ? 0.85f : cruiseThrottle_);
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

	// ウェイポイント到達判定
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

	// 機会射撃
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

// アグレッションアクション
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

	// 偏差射撃の計算：弾速と相手速度から未来位置を2次予測
	const float bulletSpeed = enemy.GetSpeed() + 500.0f;
	float timeToHit = (bulletSpeed > 0.1f) ? (distance / bulletSpeed) : 0.0f;
	MyMath::Vector3 futurePos = MyMath::Add(playerPos, MyMath::Multiply(timeToHit, playerVel));

	const float distance2 = MyMath::Length(MyMath::Subtract(futurePos, myPos));
	timeToHit = (bulletSpeed > 0.1f) ? (distance2 / bulletSpeed) : 0.0f;
	futurePos = MyMath::Add(playerPos, MyMath::Multiply(timeToHit, playerVel));

	const MyMath::Vector3 toFuture = MyMath::Subtract(futurePos, myPos);

	// ワールド水平面上で直交する一定ブレイク方向
	MyMath::Vector3 breakHoriz = { -toPlayer.z, 0.0f, toPlayer.x };
	const float breakHorizLen = MyMath::Length(breakHoriz);
	if (breakHorizLen > EnemyWaypointDefaults::kMinTargetDirectionLength) {
		breakHoriz = MyMath::Multiply(1.0f / breakHorizLen, breakHoriz);
	} else {
		breakHoriz = { 1.0f, 0.0f, 0.0f };
	}
	const MyMath::Vector3 breakDir = MyMath::Normalize(MyMath::Add(
		MyMath::Multiply(0.85f, breakHoriz),
		MyMath::Vector3{ 0.0f, 0.35f, 0.0f }));

	// 距離 110m〜220m でブレイク方向とリード射撃方向を SmoothStep で連続ブレンド
	constexpr float kBreakMinDist = 110.0f;
	constexpr float kBreakMaxDist = 220.0f;
	const float rawT = std::clamp((distance - kBreakMinDist) / (kBreakMaxDist - kBreakMinDist), 0.0f, 1.0f);
	const float leadWeight = rawT * rawT * (3.0f - 2.0f * rawT);

	MyMath::Vector3 aimDir = breakDir;
	if (MyMath::Length(toFuture) > EnemyWaypointDefaults::kMinTargetDirectionLength) {
		const MyMath::Vector3 leadDir = MyMath::Normalize(toFuture);
		const MyMath::Vector3 blended = MyMath::Lerp(breakDir, leadDir, leadWeight);
		if (MyMath::Length(blended) > EnemyWaypointDefaults::kMinTargetDirectionLength) {
			aimDir = MyMath::Normalize(blended);
		} else {
			aimDir = leadDir;
		}
	}
	enemy.SetAimTargetDirection(aimDir, deltaTime);

	// 距離に応じたスロットル調整
	if (leadWeight < 0.5f || distance > engagementRange_ * 0.6f) {
		enemy.SetThrottleInput(1.0f);
	} else if (distance < 320.0f) {
		enemy.SetThrottleInput(0.60f);
	} else {
		enemy.SetThrottleInput(0.85f);
	}

	// 射撃判定：リード追従時かつ機首が未来位置に対して一定角度以内、指定射程内なら射撃
	if (leadWeight > 0.5f && MyMath::Length(toFuture) > EnemyWaypointDefaults::kMinTargetDirectionLength) {
		const MyMath::Vector3 leadDir = MyMath::Normalize(toFuture);
		enemy.TryFireGunpod(leadDir, fireRange_);
	}

	return NodeStatus::Running;
}

// 撤退アクション
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

	// 一定距離を確保したら即座に成功
	if (distance >= safeDistanceMeters_ || distance >= 600.0f) {
		Reset();
		return NodeStatus::Success;
	}

	// プレイヤーに対する横方向ベクトル＋上昇成分を混ぜた戦術的ブレイクターン
	MyMath::Vector3 evadeDir = enemy.GetForward();
	if (distance > EnemyWaypointDefaults::kMinTargetDirectionLength) {
		evadeDir = MyMath::Normalize(awayVector);
	}

	MyMath::Vector3 lateralDir = { -evadeDir.z, 0.0f, evadeDir.x };
	const float lateralLen = MyMath::Length(lateralDir);
	if (lateralLen > EnemyWaypointDefaults::kMinTargetDirectionLength) {
		lateralDir = MyMath::Multiply(1.0f / lateralLen, lateralDir);
	} else {
		lateralDir = { 1.0f, 0.0f, 0.0f };
	}

	const float wobbleY = useEvadeWobble_ ? (std::sin(evadeWobbleTimer_ * 2.2f) * 0.15f) : 0.0f;
	MyMath::Vector3 dynamicEvadeDir = {
		evadeDir.x * 0.45f + lateralDir.x * 0.85f,
		evadeDir.y * 0.35f + 0.32f + wobbleY,
		evadeDir.z * 0.45f + lateralDir.z * 0.85f
	};
	if (MyMath::Length(dynamicEvadeDir) > EnemyWaypointDefaults::kMinTargetDirectionLength) {
		dynamicEvadeDir = MyMath::Normalize(dynamicEvadeDir);
	}

	enemy.SetAimTargetDirection(dynamicEvadeDir, deltaTime);

	// 離脱ターン時は全力加速
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

	// 1. 撤退シーケンス
	auto retreatSeq = std::make_unique<SequenceNode>("RetreatSequence");
	retreatSeq->AddChild(std::make_unique<ConditionNode>("ShouldRetreat", [aiType](Enemy& e) {
		if (!e.IsPlayerValid()) {
			return false;
		}
		const float dist = e.GetDistanceToPlayer();
		// 重度ダメージ時の近距離ブレイクターン
		if (e.GetHealthRatio() < 0.20f && dist < 350.0f) {
			return true;
		}
		// 回避AIタイプの場合、プレイヤーが至近距離に迫ったらブレイクターン
		if (aiType == AIType::CruiseEvade && dist < 300.0f) {
			return true;
		}
		return false;
	}));
	retreatSeq->AddChild(std::make_unique<RetreatAction>());
	rootSelector->AddChild(std::move(retreatSeq));

	// 2. アグレッションシーケンス
	auto aggressionSeq = std::make_unique<SequenceNode>("AggressionSequence");
	aggressionSeq->AddChild(std::make_unique<ConditionNode>("ShouldEngage", [aiType](Enemy& e) {
		if (!e.IsPlayerValid()) {
			return false;
		}
		// 追従攻撃AIなら常時交戦
		if (aiType == AIType::ChaseAttack) {
			return true;
		}
		// 他タイプであっても、プレイヤーが2500m以内にいれば積極迎撃
		const float dist = e.GetDistanceToPlayer();
		if (dist <= 2500.0f) {
			return true;
		}
		return false;
	}));
	aggressionSeq->AddChild(std::make_unique<AggressionAction>());
	rootSelector->AddChild(std::move(aggressionSeq));

	// 3. 待機・経路追従アクション
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
		const float safeDist = (std::min)(nodeJson.value("safeDistance", 450.0f), 500.0f);
		const bool useEvadeWobble = nodeJson.value("useEvadeWobble", true);
		return std::make_unique<RetreatAction>(safeDist, useEvadeWobble);
	}

	if (nodeType == "BTActionAggressionNode") {
		const float engageDist = (std::max)(nodeJson.value("engagementRange", 2500.0f), 2500.0f);
		const float fireRange = (std::max)(nodeJson.value("fireRange", 1200.0f), 1200.0f);
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

		// 1. ノードエディタグラフが存在し、useCustomParamsOnly でない場合はグラフから構築
		const bool useCustomParamsOnly = rootJson.value("useCustomParamsOnly", false);
		if (!useCustomParamsOnly && rootJson.contains("root") && rootJson["root"].is_object()) {
			auto rootNode = ParseNodeRecursive(rootJson["root"]);
			if (rootNode) {
				auto tree = std::make_unique<BehaviorTree>();
				tree->SetRoot(std::move(rootNode));
				return tree;
			}
		}

		// 2. Nパネルパラメータ個別上書きからの動的構築
		nlohmann::json params = nlohmann::json::object();
		if (rootJson.contains("overrideParams") && rootJson["overrideParams"].is_object()) {
			params = rootJson["overrideParams"];
		}

		const float retreatHpPct = (std::min)(params.value("retreatHpPct", 20.0f), 20.0f);
		const float retreatSafeDist = (std::min)(params.value("retreatSafeDist", 450.0f), 500.0f);
		const float engageDist = (std::max)(params.value("engageDist", 2500.0f), 2500.0f);
		const float fireRange = (std::max)(params.value("fireRange", 1200.0f), 1200.0f);
		const float patrolThrottle = params.value("patrolThrottle", 0.85f);

		auto tree = std::make_unique<BehaviorTree>();
		auto rootSelector = std::make_unique<SelectorNode>("DynamicRootSelector");

		// (A) 撤退シーケンス
		auto retreatSeq = std::make_unique<SequenceNode>("RetreatSequence");
		const float retreatRatio = retreatHpPct * 0.01f;
		retreatSeq->AddChild(std::make_unique<ConditionNode>("LowHealthCheck", [retreatRatio, fallbackType](Enemy& e) {
			if (!e.IsPlayerValid()) return false;
			const float dist = e.GetDistanceToPlayer();
			if (e.GetHealthRatio() < retreatRatio && dist < 350.0f) return true;
			if (fallbackType == AIType::CruiseEvade && dist < 300.0f) return true;
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

