#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "../../engine/base/Math/MyMath.h"
#include "../Enemy/EnemyManager.h"
#include "../../engine/io/json.hpp"
#include "MissionObjective.h"
#include "MissionTrigger.h"
#include "Waypoint.h"

/// @brief ミッションの構成データ
struct MissionData {
	std::string name = "Default Mission";
	std::string description = "Destroy all enemy aircraft and ground targets.";
	float playerHP = 100.0f;
	MyMath::Vector3 playerPosition = { 0.0f, 100.0f, 0.0f };
	std::vector<EnemySpawnData> enemies;
	std::vector<GroundEnemySpawnData> groundEnemies;

	// --- Phase 1: ミッション目標 & トリガーシステム ---
	float timeLimit = 300.0f;                    ///< ミッション制限時間 (秒, 0 = 無制限)
	std::vector<MissionObjective> objectives;    ///< ミッション目標リスト
	std::vector<MissionTrigger> triggers;        ///< トリガーリスト

	// --- Phase 2: ウェイポイント ---
	std::vector<WaypointPath> waypointPaths;
};

// nlohmann/json 相互変換用の定義
#ifndef MYMATH_VECTOR3_JSON_DEFINED
#define MYMATH_VECTOR3_JSON_DEFINED
namespace MyMath {
	inline void to_json(nlohmann::json& j, const Vector3& v) {
		j = nlohmann::json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
	}
	inline void from_json(const nlohmann::json& j, Vector3& v) {
		j.at("x").get_to(v.x);
		j.at("y").get_to(v.y);
		j.at("z").get_to(v.z);
	}
}
#endif

inline void to_json(nlohmann::json& j, const EnemySpawnData& e) {
	std::string aiStr = "ChaseAttack";
	if (e.aiType == AIType::CruiseEvade) aiStr = "CruiseEvade";
	else if (e.aiType == AIType::FollowWaypoint) aiStr = "FollowWaypoint";

	j = nlohmann::json{
		{"position", e.position},
		{"modelPath", e.modelPath},
		{"health", e.health},
		{"aiType", aiStr},
		{"waypointPathName", e.waypointPathName}
	};
}

inline void from_json(const nlohmann::json& j, EnemySpawnData& e) {
	j.at("position").get_to(e.position);
	j.at("modelPath").get_to(e.modelPath);
	j.at("health").get_to(e.health);
	std::string aiStr = "ChaseAttack";
	if (j.contains("aiType")) {
		j.at("aiType").get_to(aiStr);
	}
	if (aiStr == "CruiseEvade") {
		e.aiType = AIType::CruiseEvade;
	} else if (aiStr == "FollowWaypoint") {
		e.aiType = AIType::FollowWaypoint;
	} else {
		e.aiType = AIType::ChaseAttack;
	}
	if (j.contains("waypointPathName")) j.at("waypointPathName").get_to(e.waypointPathName);
}

inline void to_json(nlohmann::json& j, const GroundEnemyParam& p) {
	j = nlohmann::json{
		{"maxHealth", p.maxHealth},
		{"collisionRadius", p.collisionRadius},
		{"fireRange", p.fireRange},
		{"minElevationDeg", p.minElevationDeg},
		{"maxElevationDeg", p.maxElevationDeg},
		{"turnSpeedDeg", p.turnSpeedDeg},
		{"bulletSpeed", p.bulletSpeed},
		{"bulletDamage", p.bulletDamage},
		{"fireInterval", p.fireInterval},
		{"burstCount", p.burstCount},
		{"burstCooldown", p.burstCooldown},
		{"moveSpeed", p.moveSpeed},
		{"patrolDistance", p.patrolDistance}
	};
}

inline void from_json(const nlohmann::json& j, GroundEnemyParam& p) {
	if (j.contains("maxHealth")) j.at("maxHealth").get_to(p.maxHealth);
	if (j.contains("collisionRadius")) j.at("collisionRadius").get_to(p.collisionRadius);
	if (j.contains("fireRange")) j.at("fireRange").get_to(p.fireRange);
	if (j.contains("minElevationDeg")) j.at("minElevationDeg").get_to(p.minElevationDeg);
	if (j.contains("maxElevationDeg")) j.at("maxElevationDeg").get_to(p.maxElevationDeg);
	if (j.contains("turnSpeedDeg")) j.at("turnSpeedDeg").get_to(p.turnSpeedDeg);
	if (j.contains("bulletSpeed")) j.at("bulletSpeed").get_to(p.bulletSpeed);
	if (j.contains("bulletDamage")) j.at("bulletDamage").get_to(p.bulletDamage);
	if (j.contains("fireInterval")) j.at("fireInterval").get_to(p.fireInterval);
	if (j.contains("burstCount")) j.at("burstCount").get_to(p.burstCount);
	if (j.contains("burstCooldown")) j.at("burstCooldown").get_to(p.burstCooldown);
	if (j.contains("moveSpeed")) j.at("moveSpeed").get_to(p.moveSpeed);
	if (j.contains("patrolDistance")) j.at("patrolDistance").get_to(p.patrolDistance);
}

inline void to_json(nlohmann::json& j, const GroundEnemySpawnData& g) {
	std::string aiStr = "Turret";
	if (g.aiType == GroundAIType::Structure) aiStr = "Structure";
	else if (g.aiType == GroundAIType::PatrolVehicle) aiStr = "PatrolVehicle";

	j = nlohmann::json{
		{"position", g.position},
		{"aiType", aiStr},
		{"health", g.health},
		{"param", g.param},
		{"waypointPathName", g.waypointPathName}
	};
}

inline void from_json(const nlohmann::json& j, GroundEnemySpawnData& g) {
	if (j.contains("position")) j.at("position").get_to(g.position);
	if (j.contains("health")) j.at("health").get_to(g.health);

	std::string aiStr = "Turret";
	if (j.contains("aiType")) j.at("aiType").get_to(aiStr);
	if (aiStr == "Structure") {
		g.aiType = GroundAIType::Structure;
	} else if (aiStr == "PatrolVehicle") {
		g.aiType = GroundAIType::PatrolVehicle;
	} else {
		g.aiType = GroundAIType::Turret;
	}

	if (j.contains("param")) {
		j.at("param").get_to(g.param);
	}
	g.param.maxHealth = g.health;
	if (j.contains("waypointPathName")) j.at("waypointPathName").get_to(g.waypointPathName);
}

inline void to_json(nlohmann::json& j, const MissionData& m) {
	j = nlohmann::json{
		{"name", m.name},
		{"description", m.description},
		{"playerHP", m.playerHP},
		{"playerPosition", m.playerPosition},
		{"enemies", m.enemies},
		{"groundEnemies", m.groundEnemies},
		{"timeLimit", m.timeLimit},
		{"objectives", m.objectives},
		{"triggers", m.triggers},
		{"waypointPaths", m.waypointPaths}
	};
}

inline void from_json(const nlohmann::json& j, MissionData& m) {
	if (j.contains("name")) j.at("name").get_to(m.name);
	if (j.contains("description")) j.at("description").get_to(m.description);
	if (j.contains("playerHP")) j.at("playerHP").get_to(m.playerHP);
	if (j.contains("playerPosition")) j.at("playerPosition").get_to(m.playerPosition);
	if (j.contains("enemies")) j.at("enemies").get_to(m.enemies);
	if (j.contains("groundEnemies")) j.at("groundEnemies").get_to(m.groundEnemies);
	if (j.contains("timeLimit")) j.at("timeLimit").get_to(m.timeLimit);
	if (j.contains("objectives")) j.at("objectives").get_to(m.objectives);
	if (j.contains("triggers")) j.at("triggers").get_to(m.triggers);
	if (j.contains("waypointPaths")) j.at("waypointPaths").get_to(m.waypointPaths);
}

/// @brief ランタイム中に表示するメッセージ情報
struct RuntimeMessage {
	std::string text;
	float remainingTime = 3.0f;
};

/// @brief ミッションの管理とJSONシリアライズ・デシリアライズを担当するクラス
class MissionManager {
public:
	/// @brief シングルトンインスタンスの取得
	static MissionManager& GetInstance();

	/// @brief 指定したJSONファイルからミッションをロード
	bool Load(const std::string& filepath);

	/// @brief 現在のミッションを指定したJSONファイルに保存
	bool Save(const std::string& filepath);

	/// @brief フォルダ内のミッションファイル一覧を取得
	std::vector<std::string> GetMissionList();

	/// @brief デフォルトのミッションデータを構築して保存
	void CreateDefaultMission(const std::string& filepath);

	// === Phase 1: 目標 & トリガー評価 ===

	/// @brief ミッション開始時にランタイム状態をリセット
	void ResetRuntimeState();

	/// @brief 全目標のステータスを評価・更新する
	/// @param playerPos プレイヤーの現在位置
	/// @param playerHP プレイヤーの現在HP
	/// @param destroyedCount 現在の撃破数
	/// @param allDestroyed 全敵が殲滅されたか
	/// @param elapsedTime ミッション経過時間
	void EvaluateObjectives(
		const MyMath::Vector3& playerPos,
		float playerHP,
		int destroyedCount,
		bool allDestroyed,
		float elapsedTime
	);

	/// @brief 全トリガーの条件をチェックし、該当するアクションを実行する
	/// @param playerPos プレイヤーの現在位置
	/// @param playerHP プレイヤーの現在HP
	/// @param destroyedCount 現在の撃破数
	/// @param elapsedTime ミッション経過時間
	void EvaluateTriggers(
		const MyMath::Vector3& playerPos,
		float playerHP,
		int destroyedCount,
		float elapsedTime
	);

	/// @brief 全主目標が達成されたかを返す
	bool AreAllPrimaryObjectivesCompleted() const;

	/// @brief いずれかの主目標が失敗したかを返す
	bool IsAnyPrimaryObjectiveFailed() const;

	/// @brief トリガーによるミッション成功フラグ
	bool IsTriggerMissionComplete() const { return triggerMissionComplete_; }

	/// @brief トリガーによるミッション失敗フラグ
	bool IsTriggerMissionFail() const { return triggerMissionFail_; }

	/// @brief トリガーで出現した追加敵のリストを取得・クリア
	std::vector<EnemySpawnData> PopPendingEnemySpawns();
	std::vector<GroundEnemySpawnData> PopPendingGroundEnemySpawns();

	/// @brief 表示中のメッセージを取得
	const std::vector<RuntimeMessage>& GetActiveMessages() const { return activeMessages_; }

	/// @brief メッセージのタイマーを更新
	void UpdateMessages(float deltaTime);

	// === ゲッター・セッター ===
	const MissionData& GetCurrentMission() const { return currentMission_; }
	MissionData& GetCurrentMission() { return currentMission_; }
	const std::string& GetCurrentFilePath() const { return currentFilePath_; }
	void SetCurrentFilePath(const std::string& path) { currentFilePath_ = path; }

private:
	MissionManager();
	~MissionManager() = default;
	MissionManager(const MissionManager&) = delete;
	MissionManager& operator=(const MissionManager&) = delete;

	/// @brief トリガーアクションを実行する
	void ExecuteAction(const TriggerAction& action);

	MissionData currentMission_;
	std::string currentFilePath_;

	// --- ランタイム状態 ---
	bool triggerMissionComplete_ = false;
	bool triggerMissionFail_ = false;
	std::vector<EnemySpawnData> pendingEnemySpawns_;
	std::vector<GroundEnemySpawnData> pendingGroundEnemySpawns_;
	std::vector<RuntimeMessage> activeMessages_;
};
