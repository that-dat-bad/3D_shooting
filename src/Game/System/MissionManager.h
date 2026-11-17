#pragma once
#include <deque>
#include <string>
#include <vector>
#include <filesystem>
#include "../../engine/base/Math/MyMath.h"
#include "../Enemy/EnemyManager.h"
#include "../Enemy/EnemySpawnDefinition.h"
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
}

// ミッション一覧（マニフェスト）エントリデータ（各ミッションの件名・別名・ファイルパス等のメタデータ一元管理）
struct MissionEntry {
	std::string id = "[01]";                           // ミッションID（例: "[00]", "[01]"）
	std::string filePath = "";                        // ミッションJSONファイルパス
	std::string title = "";                           // 作戦件名・主要タイトル（例: "Obuchenie"）
	std::string aliasName = "";                       // 作戦別名・コードネーム（例: "ZADANIE 00"）
	std::string subTitle = "";                        // 日本語件名・サブタイトル
	bool isLocked = false;                            // 解放フラグ
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

// ミッションの構成データ
struct MissionData {
	std::string name = "Default Mission";
	std::string aliasName = "";                  // 作戦別名・コードネーム（例: "ZADANIE 00"）
	std::string subTitle = "";                   // サブタイトル（例: "オブチェーニエ：訓練・教程 — 極夜の異変 —"）
	std::string difficulty = "NORMAL";           // 難易度表示（例: "NORMAL", "HARD", "EXPERT"）
	std::string areaName = "SECTOR 07";          // 作戦空域名称
	std::string description = "Destroy all enemy aircraft and ground targets.";
	float playerHP = MissionSchemaDefaults::kDefaultPlayerHp;
	MyMath::Vector3 playerPosition = MissionSchemaDefaults::kDefaultPlayerStartPos;
	float playerYawDeg = MissionSchemaDefaults::kDefaultPlayerYawDeg; // 初期方位角（度, 0=+Z北向き, 時計回り）
	std::vector<EnemySpawnData> enemies;
	std::vector<GroundEnemySpawnData> groundEnemies;

	// --- Phase 1: ミッション目標 & トリガーシステム ---
	float timeLimit = MissionSchemaDefaults::kDefaultTimeLimitSeconds; // ミッション制限時間 (秒, 0 = 無制限)
	std::vector<MissionObjective> objectives;                          // ミッション目標リスト
	std::vector<MissionTrigger> triggers;                              // トリガーリスト

	// --- Phase 2: ウェイポイント ---
	std::vector<WaypointPath> waypointPaths;

	// --- 弾薬補給・クリア演出・デブリーフ設定 ---
	int minAmmoReserve = 0;
	bool fadeOutOnClear = false;
	bool isTutorialDebrief = false;
	bool isClearAutoDemo = false;
	bool disableGenericRadio = false;
};

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
		{"waypointPaths", m.waypointPaths},
		{"minAmmoReserve", m.minAmmoReserve},
		{"fadeOutOnClear", m.fadeOutOnClear},
		{"isTutorialDebrief", m.isTutorialDebrief},
		{"isClearAutoDemo", m.isClearAutoDemo},
		{"disableGenericRadio", m.disableGenericRadio}
	};
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
	if (j.contains("minAmmoReserve")) j.at("minAmmoReserve").get_to(m.minAmmoReserve);
	if (j.contains("fadeOutOnClear")) j.at("fadeOutOnClear").get_to(m.fadeOutOnClear);
	if (j.contains("isTutorialDebrief")) j.at("isTutorialDebrief").get_to(m.isTutorialDebrief);
	if (j.contains("isClearAutoDemo")) j.at("isClearAutoDemo").get_to(m.isClearAutoDemo);
	if (j.contains("disableGenericRadio")) j.at("disableGenericRadio").get_to(m.disableGenericRadio);
}

// ランタイム中に表示するメッセージ情報
struct RuntimeMessage {
	std::string text;
	float remainingTime = MissionSchemaDefaults::kDefaultMessageDuration;
};

// HUD左上に表示するシステムテレメトリログ
struct MissionSysLogEntry {
	std::string category = "SYS";
	std::string timestampText;
	std::string message;
	float ageSeconds = 0.0f;
};

// ミッションの管理とJSONシリアライズ・デシリアライズを担当するクラス
class MissionManager {
public:
	// シングルトンインスタンスの取得
	static MissionManager& GetInstance();

	// 指定したJSONファイルからミッションをロード
	bool Load(const std::string& filepath);

	// 現在のミッションを指定したJSONファイルに保存
	bool Save(const std::string& filepath);

	// Blender Live Sync 等から受信したミッションデータを即時適用
	void ApplyLiveMissionData(const MissionData& liveData, const std::string& liveFilePath = "");

	// フォルダ内のミッションファイル一覧を取得
	std::vector<std::string> GetMissionList();

	// デフォルトのミッションデータを構築して保存
	void CreateDefaultMission(const std::string& filepath);

	// === ミッション一覧（マニフェスト・別名一元管理） ===

	// missions.json（ミッション一覧インデックス）をロード
	bool LoadMissionManifest(const std::string& manifestPath = "Resources/missions/missions.json");

	// missions.json にミッション一覧を保存
	bool SaveMissionManifest(const std::string& manifestPath = "Resources/missions/missions.json");

	// ミッションエントリ一覧を取得（未ロード時は自動ロード）
	const std::vector<MissionEntry>& GetMissionEntries();

	// デフォルトの missions.json を生成
	void CreateDefaultMissionManifest(const std::string& manifestPath);

	// === Phase 1: 目標 & トリガー評価 ===

	// 個別ターゲット撃破判定用に現在の EnemyManager をバインドする
	void BindEnemyManager(const EnemyManager* enemyManager) { boundEnemyManager_ = enemyManager; }

	// ミッション開始時にランタイム状態をリセット
	void ResetRuntimeState();

	// 全目標のステータスを評価・更新する
	void EvaluateObjectives(
		const MyMath::Vector3& playerPos,
		float playerHP,
		int destroyedCount,
		bool allDestroyed,
		float elapsedTime,
		const EnemyManager* enemyManager = nullptr,
		float deltaTime = 0.0f,
		float playerThrottle = 0.0f,
		bool aimMovedThisFrame = false,
		bool radioActive = false
	);

	// 全トリガーの条件をチェックし、該当するアクションを実行する
	void EvaluateTriggers(
		const MyMath::Vector3& playerPos,
		float playerHP,
		int destroyedCount,
		float elapsedTime,
		const EnemyManager* enemyManager = nullptr,
		float deltaTime = 0.0f
	);

	// 操作許可・システムログ・HUDガイド照会
	bool IsPlayerSteeringAllowed() const;
	bool IsPlayerThrottleAllowed() const;
	bool IsPlayerWeaponAllowed() const;
	bool ConsumePendingIgniteOutpost();
	void PushSystemLog(const std::string& category, const std::string& message, bool includeTimestamp = false);
	const std::deque<MissionSysLogEntry>& GetActiveSystemLogs() const { return activeSysLogs_; }
	bool HasRadioDialogueTriggers() const;
	bool BuildActiveGuideHUD(
		float playerThrottle,
		float targetDamageRatio,
		std::string& outTitle,
		std::string& outSub,
		bool& outShowProgress,
		float& outProgressRatio
	) const;
	void EvaluateAltitudeAlerts(float currentAgl, float currentY, bool& outPullUp, bool& outHighAlt) const;

	// 全主目標が達成されたかを返す
	bool AreAllPrimaryObjectivesCompleted() const;

	// いずれかの主目標が失敗したかを返す
	bool IsAnyPrimaryObjectiveFailed() const;

	// トリガーによるミッション成功フラグ
	bool IsTriggerMissionComplete() const { return triggerMissionComplete_; }

	// トリガーによるミッション失敗フラグ
	bool IsTriggerMissionFail() const { return triggerMissionFail_; }

	// トリガーで出現した追加敵のリストを取得・クリア
	std::vector<EnemySpawnData> PopPendingEnemySpawns();
	std::vector<GroundEnemySpawnData> PopPendingGroundEnemySpawns();
	std::vector<RadioMessage> PopPendingRadioMessages();

	// 表示中のメッセージを取得
	const std::vector<RuntimeMessage>& GetActiveMessages() const { return activeMessages_; }

	// メッセージのタイマーを更新
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
	bool IsScriptedGuideVisible() const;
	bool HasActiveOrScriptedGuideContext() const;

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
	bool pendingIgniteOutpost_ = false;
	float runtimeMissionClock_ = 0.0f;
	std::vector<EnemySpawnData> pendingEnemySpawns_;
	std::vector<GroundEnemySpawnData> pendingGroundEnemySpawns_;
	std::vector<RuntimeMessage> activeMessages_;
	std::vector<RadioMessage> pendingRadioMessages_;
	std::deque<MissionSysLogEntry> activeSysLogs_;
};
