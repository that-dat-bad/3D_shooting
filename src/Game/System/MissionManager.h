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

namespace MissionSchemaDefaults {
	inline constexpr int kJsonDumpIndent = 4;
	inline constexpr int kZeroIndex = 0;
	inline constexpr float kZero = 0.0f;
	inline constexpr float kDefaultPlayerHp = 100.0f;
	inline constexpr float kDefaultPlayerYawDeg = 0.0f;
	inline constexpr float kDefaultTimeLimitSeconds = 300.0f;
	inline constexpr float kDefaultMessageDuration = 3.0f;
	inline constexpr MyMath::Vector3 kDefaultPlayerStartPos = { 0.0f, 100.0f, 0.0f };

	// ZADANIE 00 (Obuchenie) 3D空間レイアウト既定座標（Blenderから編集・上書き可能）
	inline constexpr MyMath::Vector3 kDefaultCoraxStartPos = { 244.0f, -278.0f, 935.0f };
	inline constexpr MyMath::Vector3 kDefaultOutpost01Pos = { 338.0f, -455.0f, 1950.0f };
	inline constexpr MyMath::Vector3 kDefaultOutpost02Pos = { 78.0f, -468.0f, 2920.0f };
	inline constexpr MyMath::Vector3 kDefaultOutpost03Pos = { 172.0f, -478.0f, 3880.0f };
	inline constexpr MyMath::Vector3 kDefaultOutpostPos = { -52.0f, -510.1f, 4950.0f };
	inline constexpr MyMath::Vector3 kDefaultDroneSpawnPos = { -220.0f, -328.0f, 5200.0f };
	inline constexpr MyMath::Vector3 kDefaultWesternDiveStart = { -65.0f, -35.0f, 5600.0f };
	inline constexpr MyMath::Vector3 kDefaultWesternDiveMid = { -310.0f, -295.0f, 5520.0f };
	inline constexpr MyMath::Vector3 kDefaultWesternPullUp = { -420.0f, -310.0f, 5880.0f };
	inline constexpr MyMath::Vector3 kDefaultExitBoundaryPos = { -640.0f, -325.0f, 7400.0f };
}

/// @brief ミッション一覧（マニフェスト）エントリデータ（各ミッションの件名・別名・ファイルパス等のメタデータ一元管理）
struct MissionEntry {
	std::string id = "[01]";                           ///< ミッションID（例: "[00]", "[01]"）
	std::string filePath = "";                        ///< ミッションJSONファイルパス
	std::string title = "";                           ///< 作戦件名・主要タイトル（例: "Obuchenie"）
	std::string aliasName = "";                       ///< 作戦別名・コードネーム（例: "ZADANIE 00"）
	std::string subTitle = "";                        ///< 日本語件名・サブタイトル
	bool isLocked = false;                            ///< 解放フラグ
};

inline void to_json(nlohmann::json& j, const MissionEntry& e) {
	j = nlohmann::json{
		{"id", e.id},
		{"filePath", e.filePath},
		{"title", e.title},
		{"aliasName", e.aliasName},
		{"subTitle", e.subTitle},
		{"isLocked", e.isLocked}
	};
}

inline void from_json(const nlohmann::json& j, MissionEntry& e) {
	if (j.contains("id")) j.at("id").get_to(e.id);
	if (j.contains("filePath")) j.at("filePath").get_to(e.filePath);
	if (j.contains("title")) j.at("title").get_to(e.title);
	if (j.contains("aliasName")) j.at("aliasName").get_to(e.aliasName);
	if (j.contains("subTitle")) j.at("subTitle").get_to(e.subTitle);
	if (j.contains("isLocked")) j.at("isLocked").get_to(e.isLocked);
}

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

/// @brief ZADANIE 00（チュートリアル導入任務）の非会話・3D空間オブジェクト配置データ（Blender Mission Editor 対応）
struct TutorialSpatialLayout {
	bool useCustomLayout = false;
	MyMath::Vector3 coraxStartPos = MissionSchemaDefaults::kDefaultCoraxStartPos;
	MyMath::Vector3 outpostPosition = MissionSchemaDefaults::kDefaultOutpostPos;
	std::vector<MyMath::Vector3> outpostPositions = {
		MissionSchemaDefaults::kDefaultOutpost01Pos,
		MissionSchemaDefaults::kDefaultOutpost02Pos,
		MissionSchemaDefaults::kDefaultOutpost03Pos,
		MissionSchemaDefaults::kDefaultOutpostPos
	};
	MyMath::Vector3 droneSpawnPosition = MissionSchemaDefaults::kDefaultDroneSpawnPos;
	MyMath::Vector3 westernDiveStart = MissionSchemaDefaults::kDefaultWesternDiveStart;
	MyMath::Vector3 westernDiveMid = MissionSchemaDefaults::kDefaultWesternDiveMid;
	MyMath::Vector3 westernPullUp = MissionSchemaDefaults::kDefaultWesternPullUp;
	MyMath::Vector3 exitBoundaryPosition = MissionSchemaDefaults::kDefaultExitBoundaryPos;
};

inline void to_json(nlohmann::json& j, const TutorialSpatialLayout& t) {
	j = nlohmann::json{
		{"useCustomLayout", t.useCustomLayout},
		{"coraxStartPos", t.coraxStartPos},
		{"outpostPosition", t.outpostPosition},
		{"outpostPositions", t.outpostPositions},
		{"droneSpawnPosition", t.droneSpawnPosition},
		{"westernDiveStart", t.westernDiveStart},
		{"westernDiveMid", t.westernDiveMid},
		{"westernPullUp", t.westernPullUp},
		{"exitBoundaryPosition", t.exitBoundaryPosition}
	};
}

inline void from_json(const nlohmann::json& j, TutorialSpatialLayout& t) {
	if (j.contains("useCustomLayout")) j.at("useCustomLayout").get_to(t.useCustomLayout);
	if (j.contains("coraxStartPos")) j.at("coraxStartPos").get_to(t.coraxStartPos);
	if (j.contains("outpostPosition")) j.at("outpostPosition").get_to(t.outpostPosition);
	if (j.contains("outpostPositions")) {
		j.at("outpostPositions").get_to(t.outpostPositions);
	}
	if (!t.outpostPositions.empty()) {
		if (!j.contains("outpostPosition")) {
			t.outpostPosition = t.outpostPositions.back();
		}
	} else {
		t.outpostPositions = {
			MissionSchemaDefaults::kDefaultOutpost01Pos,
			MissionSchemaDefaults::kDefaultOutpost02Pos,
			MissionSchemaDefaults::kDefaultOutpost03Pos,
			t.outpostPosition
		};
	}
	if (j.contains("droneSpawnPosition")) j.at("droneSpawnPosition").get_to(t.droneSpawnPosition);
	if (j.contains("westernDiveStart")) j.at("westernDiveStart").get_to(t.westernDiveStart);
	if (j.contains("westernDiveMid")) j.at("westernDiveMid").get_to(t.westernDiveMid);
	if (j.contains("westernPullUp")) j.at("westernPullUp").get_to(t.westernPullUp);
	if (j.contains("exitBoundaryPosition")) j.at("exitBoundaryPosition").get_to(t.exitBoundaryPosition);
}

/// @brief ミッションの構成データ
struct MissionData {
	std::string name = "Default Mission";
	std::string aliasName = "";                  ///< 作戦別名・コードネーム（例: "ZADANIE 00"）
	std::string subTitle = "";                   ///< サブタイトル（例: "オブチェーニエ：訓練・教程 — 極夜の異変 —"）
	std::string difficulty = "NORMAL";           ///< 難易度表示（例: "NORMAL", "HARD", "EXPERT"）
	std::string areaName = "SECTOR 07";          ///< 作戦空域名称
	std::string description = "Destroy all enemy aircraft and ground targets.";
	float playerHP = MissionSchemaDefaults::kDefaultPlayerHp;
	MyMath::Vector3 playerPosition = MissionSchemaDefaults::kDefaultPlayerStartPos;
	float playerYawDeg = MissionSchemaDefaults::kDefaultPlayerYawDeg; ///< 初期方位角（度, 0=+Z北向き, 時計回り）
	std::vector<EnemySpawnData> enemies;
	std::vector<GroundEnemySpawnData> groundEnemies;

	// --- Phase 1: ミッション目標 & トリガーシステム ---
	float timeLimit = MissionSchemaDefaults::kDefaultTimeLimitSeconds; ///< ミッション制限時間 (秒, 0 = 無制限)
	std::vector<MissionObjective> objectives;                          ///< ミッション目標リスト
	std::vector<MissionTrigger> triggers;                              ///< トリガーリスト

	// --- Phase 2: ウェイポイント ---
	std::vector<WaypointPath> waypointPaths;

	// --- ZADANIE 00 3D空間配置オーバーライド ---
	TutorialSpatialLayout tutorialLayout{};
};

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

	if (!e.behaviorTreeJson.empty()) {
		try {
			j["behaviorTree"] = nlohmann::json::parse(e.behaviorTreeJson);
		} catch (...) {
			j["behaviorTree"] = nullptr;
		}
	}
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

	if (j.contains("behaviorTree") && !j.at("behaviorTree").is_null()) {
		try {
			e.behaviorTreeJson = j.at("behaviorTree").dump();
		} catch (...) {
			e.behaviorTreeJson = "";
		}
	} else {
		e.behaviorTreeJson = "";
	}
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
		{"aliasName", m.aliasName},
		{"subTitle", m.subTitle},
		{"difficulty", m.difficulty},
		{"areaName", m.areaName},
		{"description", m.description},
		{"playerHP", m.playerHP},
		{"playerPosition", m.playerPosition},
		{"playerYawDeg", m.playerYawDeg},
		{"enemies", m.enemies},
		{"groundEnemies", m.groundEnemies},
		{"timeLimit", m.timeLimit},
		{"objectives", m.objectives},
		{"triggers", m.triggers},
		{"waypointPaths", m.waypointPaths}
	};
	if (m.tutorialLayout.useCustomLayout) {
		j["tutorialLayout"] = m.tutorialLayout;
	}
}

inline void from_json(const nlohmann::json& j, MissionData& m) {
	if (j.contains("name")) j.at("name").get_to(m.name);
	if (j.contains("aliasName")) j.at("aliasName").get_to(m.aliasName);
	if (j.contains("subTitle")) j.at("subTitle").get_to(m.subTitle);
	if (j.contains("difficulty")) j.at("difficulty").get_to(m.difficulty);
	if (j.contains("areaName")) j.at("areaName").get_to(m.areaName);
	if (j.contains("description")) j.at("description").get_to(m.description);
	if (j.contains("playerHP")) j.at("playerHP").get_to(m.playerHP);
	if (j.contains("playerPosition")) j.at("playerPosition").get_to(m.playerPosition);
	if (j.contains("playerYawDeg")) j.at("playerYawDeg").get_to(m.playerYawDeg);
	if (j.contains("enemies")) j.at("enemies").get_to(m.enemies);
	if (j.contains("groundEnemies")) j.at("groundEnemies").get_to(m.groundEnemies);
	if (j.contains("timeLimit")) j.at("timeLimit").get_to(m.timeLimit);
	if (j.contains("objectives")) j.at("objectives").get_to(m.objectives);
	if (j.contains("triggers")) j.at("triggers").get_to(m.triggers);
	if (j.contains("waypointPaths")) j.at("waypointPaths").get_to(m.waypointPaths);
	if (j.contains("tutorialLayout")) j.at("tutorialLayout").get_to(m.tutorialLayout);
}

/// @brief ランタイム中に表示するメッセージ情報
struct RuntimeMessage {
	std::string text;
	float remainingTime = MissionSchemaDefaults::kDefaultMessageDuration;
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

	/// @brief Blender Live Sync 等から受信したミッションデータを即時適用
	void ApplyLiveMissionData(const MissionData& liveData, const std::string& liveFilePath = "");

	/// @brief フォルダ内のミッションファイル一覧を取得
	std::vector<std::string> GetMissionList();

	/// @brief デフォルトのミッションデータを構築して保存
	void CreateDefaultMission(const std::string& filepath);

	// === ミッション一覧（マニフェスト・別名一元管理） ===

	/// @brief missions.json（ミッション一覧インデックス）をロード
	bool LoadMissionManifest(const std::string& manifestPath = "Resources/missions/missions.json");

	/// @brief missions.json にミッション一覧を保存
	bool SaveMissionManifest(const std::string& manifestPath = "Resources/missions/missions.json");

	/// @brief ミッションエントリ一覧を取得（未ロード時は自動ロード）
	const std::vector<MissionEntry>& GetMissionEntries();

	/// @brief デフォルトの missions.json を生成
	void CreateDefaultMissionManifest(const std::string& manifestPath);

	// === Phase 1: 目標 & トリガー評価 ===

	/// @brief 個別ターゲット撃破判定用に現在の EnemyManager をバインドする
	void BindEnemyManager(const EnemyManager* enemyManager) { boundEnemyManager_ = enemyManager; }

	/// @brief ミッション開始時にランタイム状態をリセット
	void ResetRuntimeState();

	/// @brief 全目標のステータスを評価・更新する
	void EvaluateObjectives(
		const MyMath::Vector3& playerPos,
		float playerHP,
		int destroyedCount,
		bool allDestroyed,
		float elapsedTime,
		const EnemyManager* enemyManager = nullptr
	);

	/// @brief 全トリガーの条件をチェックし、該当するアクションを実行する
	void EvaluateTriggers(
		const MyMath::Vector3& playerPos,
		float playerHP,
		int destroyedCount,
		float elapsedTime,
		const EnemyManager* enemyManager = nullptr
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
	std::vector<RadioMessage> PopPendingRadioMessages();

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

	void CacheInitialObjectiveStatuses();
	void ExecuteAction(const TriggerAction& action);

	MissionData currentMission_;
	std::string currentFilePath_;
	std::vector<ObjectiveStatus> initialObjectiveStatuses_;
	std::vector<bool> initialTriggerEnabled_;

	// --- ミッション一覧（マニフェスト） ---
	std::vector<MissionEntry> missionEntries_;
	bool isManifestLoaded_ = false;

	// --- ランタイム状態 ---
	const EnemyManager* boundEnemyManager_ = nullptr;
	bool triggerMissionComplete_ = false;
	bool triggerMissionFail_ = false;
	std::vector<EnemySpawnData> pendingEnemySpawns_;
	std::vector<GroundEnemySpawnData> pendingGroundEnemySpawns_;
	std::vector<RuntimeMessage> activeMessages_;
	std::vector<RadioMessage> pendingRadioMessages_;
};
