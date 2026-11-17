#pragma once
#include <string>
#include <vector>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/io/json.hpp"
#include "../Enemy/EnemySpawnDefinition.h"
#include "../UI/RadioCommunicator.h"

// ============================================================
// ミッショントリガー (Mission Trigger) の型定義
// ============================================================

// トリガーの発火条件
enum class TriggerCondition {
	OnMissionStart,      // ミッション開始時
	OnTimer,             // 経過時間で発火
	OnAreaEnter,         // プレイヤーがエリアに進入
	OnAreaLeave,         // プレイヤーがエリアから離脱
	OnEnemyDestroyed,    // 特定の敵が撃破された
	OnKillCount,         // 撃破数が閾値に達した
	OnPlayerDamaged,     // プレイヤーのHPが閾値以下
	OnObjectiveComplete, // 特定の目標が達成された
};

// トリガー発火時に実行するアクションの種類
enum class TriggerActionType {
	SpawnEnemies,       // 追加の敵を出現させる
	ShowMessage,        // 画面にメッセージ表示
	SetObjective,       // 目標のステータスを変更する
	MissionComplete,    // ミッション成功
	MissionFail,        // ミッション失敗
	ActivateTrigger,    // 別のトリガーを有効化
	PlayRadioDialogue,  // エースコンバット風の無線会話を再生
	PushSystemLog,      // HUD左上へシステムテレメトリログ出力
	IgniteOutpost,      // 監視塔の炎上・爆炎演出を開始
};

// トリガーアクションのデータ
struct TriggerAction {
	TriggerActionType type = TriggerActionType::ShowMessage;

	// --- ShowMessage / PushSystemLog 用 ---
	std::string message = "";
	float messageDuration = 3.0f;
	std::string logCategory = "SYS";
	bool logTimestamp = false;

	// --- SetObjective 用 ---
	int objectiveIndex = -1;         // 対象の目標インデックス
	std::string newStatus = "Active"; // 変更先のステータス ("Active" / "Completed" / "Failed")

	// --- SpawnEnemies 用 ---
	std::vector<EnemySpawnData> spawnEnemies;
	std::vector<GroundEnemySpawnData> spawnGroundEnemies;

	// --- ActivateTrigger 用 ---
	int activateTriggerIndex = -1;   // 有効化する別トリガーのインデックス

	// --- PlayRadioDialogue 用 ---
	std::string speakerName = "";                     // 無線の話者名（自動で <○○> 整形）
	RadioFaction speakerFaction = RadioFaction::Awacs; // 話者の所属・色分け属性
	bool radioInterrupt = false;                      // true の場合は再生中の無線を割り込み更新
};

// ミッショントリガーのデータ
struct MissionTrigger {
	std::string name = "Trigger";
	TriggerCondition condition = TriggerCondition::OnMissionStart;
	bool isEnabled = true;          // 有効/無効（無効なら評価をスキップ）
	bool isFired = false;           // 発火済みフラグ（ランタイム用、JSONには保存しない）
	bool isRepeatable = false;      // true: 条件を満たすたびに何度でも発火
	float delaySeconds = 0.0f;      // 条件成立後の発火ディレイ（秒）
	bool isPendingDelay = false;    // ランタイム用ディレイ待機フラグ
	float delayTimer = 0.0f;        // ランタイム用ディレイ経過時間

	// --- 条件パラメータ ---
	float timerSeconds = 0.0f;             // OnTimer用: 発火までの秒数
	MyMath::Vector3 areaCenter{};          // OnAreaEnter/Leave用
	float areaRadius = 100.0f;             // OnAreaEnter/Leave用
	int enemyIndex = -1;                   // OnEnemyDestroyed用
	int killCount = 0;                     // OnKillCount用
	float hpThreshold = 50.0f;             // OnPlayerDamaged用
	int objectiveIndex = -1;               // OnObjectiveComplete用

	// --- 発火時のアクション（複数設定可能） ---
	std::vector<TriggerAction> actions;
};

// ============================================================
// 文字列変換ユーティリティ
// ============================================================

inline const char* TriggerConditionToString(TriggerCondition c) {
	switch (c) {
	case TriggerCondition::OnMissionStart:      return "OnMissionStart";
	case TriggerCondition::OnTimer:             return "OnTimer";
	case TriggerCondition::OnAreaEnter:         return "OnAreaEnter";
	case TriggerCondition::OnAreaLeave:         return "OnAreaLeave";
	case TriggerCondition::OnEnemyDestroyed:    return "OnEnemyDestroyed";
	case TriggerCondition::OnKillCount:         return "OnKillCount";
	case TriggerCondition::OnPlayerDamaged:     return "OnPlayerDamaged";
	case TriggerCondition::OnObjectiveComplete: return "OnObjectiveComplete";
	default:                                    return "OnMissionStart";
	}
}

inline TriggerCondition StringToTriggerCondition(const std::string& str) {
	if (str == "OnTimer")             return TriggerCondition::OnTimer;
	if (str == "OnAreaEnter")         return TriggerCondition::OnAreaEnter;
	if (str == "OnAreaLeave")         return TriggerCondition::OnAreaLeave;
	if (str == "OnEnemyDestroyed")    return TriggerCondition::OnEnemyDestroyed;
	if (str == "OnKillCount")         return TriggerCondition::OnKillCount;
	if (str == "OnPlayerDamaged")     return TriggerCondition::OnPlayerDamaged;
	if (str == "OnObjectiveComplete") return TriggerCondition::OnObjectiveComplete;
	return TriggerCondition::OnMissionStart;
}

inline const char* TriggerActionTypeToString(TriggerActionType t) {
	switch (t) {
	case TriggerActionType::SpawnEnemies:      return "SpawnEnemies";
	case TriggerActionType::ShowMessage:       return "ShowMessage";
	case TriggerActionType::SetObjective:      return "SetObjective";
	case TriggerActionType::MissionComplete:   return "MissionComplete";
	case TriggerActionType::MissionFail:       return "MissionFail";
	case TriggerActionType::ActivateTrigger:   return "ActivateTrigger";
	case TriggerActionType::PlayRadioDialogue: return "PlayRadioDialogue";
	case TriggerActionType::PushSystemLog:     return "PushSystemLog";
	case TriggerActionType::IgniteOutpost:     return "IgniteOutpost";
	default:                                   return "ShowMessage";
	}
}

inline TriggerActionType StringToTriggerActionType(const std::string& str) {
	if (str == "SpawnEnemies")      return TriggerActionType::SpawnEnemies;
	if (str == "SetObjective")      return TriggerActionType::SetObjective;
	if (str == "MissionComplete")   return TriggerActionType::MissionComplete;
	if (str == "MissionFail")       return TriggerActionType::MissionFail;
	if (str == "ActivateTrigger")   return TriggerActionType::ActivateTrigger;
	if (str == "PlayRadioDialogue") return TriggerActionType::PlayRadioDialogue;
	if (str == "PushSystemLog")     return TriggerActionType::PushSystemLog;
	if (str == "IgniteOutpost")     return TriggerActionType::IgniteOutpost;
	return TriggerActionType::ShowMessage;
}

// ============================================================
// JSON シリアライズ / デシリアライズ
// ============================================================

inline void to_json(nlohmann::json& j, const TriggerAction& a) {
	j = nlohmann::json{
		{"type", TriggerActionTypeToString(a.type)},
		{"message", a.message},
		{"messageDuration", a.messageDuration},
		{"logCategory", a.logCategory},
		{"logTimestamp", a.logTimestamp},
		{"objectiveIndex", a.objectiveIndex},
		{"newStatus", a.newStatus},
		{"activateTriggerIndex", a.activateTriggerIndex},
		{"speakerName", a.speakerName},
		{"speakerFaction", RadioFactionToString(a.speakerFaction)},
		{"radioInterrupt", a.radioInterrupt}
	};
	if (!a.spawnEnemies.empty()) {
		j["spawnEnemies"] = a.spawnEnemies;
	}
	if (!a.spawnGroundEnemies.empty()) {
		j["spawnGroundEnemies"] = a.spawnGroundEnemies;
	}
}

inline void from_json(const nlohmann::json& j, TriggerAction& a) {
	if (j.contains("type")) {
		std::string typeStr;
		j.at("type").get_to(typeStr);
		a.type = StringToTriggerActionType(typeStr);
	}
	if (j.contains("message"))              j.at("message").get_to(a.message);
	if (j.contains("dialogue") && a.message.empty()) j.at("dialogue").get_to(a.message);
	if (j.contains("messageDuration"))      j.at("messageDuration").get_to(a.messageDuration);
	if (j.contains("logCategory"))          j.at("logCategory").get_to(a.logCategory);
	if (j.contains("logTimestamp"))         j.at("logTimestamp").get_to(a.logTimestamp);
	if (j.contains("objectiveIndex"))       j.at("objectiveIndex").get_to(a.objectiveIndex);
	if (j.contains("newStatus"))            j.at("newStatus").get_to(a.newStatus);
	if (j.contains("activateTriggerIndex")) j.at("activateTriggerIndex").get_to(a.activateTriggerIndex);
	if (j.contains("speakerName"))          j.at("speakerName").get_to(a.speakerName);
	if (j.contains("speakerFaction")) {
		std::string factionStr;
		j.at("speakerFaction").get_to(factionStr);
		a.speakerFaction = StringToRadioFaction(factionStr);
	}
	if (j.contains("radioInterrupt"))       j.at("radioInterrupt").get_to(a.radioInterrupt);
	if (j.contains("spawnEnemies")) {
		j.at("spawnEnemies").get_to(a.spawnEnemies);
	}
	if (j.contains("spawnGroundEnemies")) {
		j.at("spawnGroundEnemies").get_to(a.spawnGroundEnemies);
	}
}

inline void to_json(nlohmann::json& j, const MissionTrigger& t) {
	j = nlohmann::json{
		{"name", t.name},
		{"condition", TriggerConditionToString(t.condition)},
		{"isEnabled", t.isEnabled},
		{"isRepeatable", t.isRepeatable},
		{"delaySeconds", t.delaySeconds},
		{"timerSeconds", t.timerSeconds},
		{"areaCenter", t.areaCenter},
		{"areaRadius", t.areaRadius},
		{"enemyIndex", t.enemyIndex},
		{"killCount", t.killCount},
		{"hpThreshold", t.hpThreshold},
		{"objectiveIndex", t.objectiveIndex},
		{"actions", t.actions}
	};
}

inline void from_json(const nlohmann::json& j, MissionTrigger& t) {
	if (j.contains("name")) j.at("name").get_to(t.name);
	if (j.contains("condition")) {
		std::string condStr;
		j.at("condition").get_to(condStr);
		t.condition = StringToTriggerCondition(condStr);
	}
	if (j.contains("isEnabled"))    j.at("isEnabled").get_to(t.isEnabled);
	if (j.contains("isRepeatable")) j.at("isRepeatable").get_to(t.isRepeatable);
	if (j.contains("delaySeconds")) j.at("delaySeconds").get_to(t.delaySeconds);
	if (j.contains("timerSeconds")) j.at("timerSeconds").get_to(t.timerSeconds);
	if (j.contains("areaCenter"))   j.at("areaCenter").get_to(t.areaCenter);
	if (j.contains("areaRadius"))   j.at("areaRadius").get_to(t.areaRadius);
	if (j.contains("enemyIndex"))   j.at("enemyIndex").get_to(t.enemyIndex);
	if (j.contains("killCount"))    j.at("killCount").get_to(t.killCount);
	if (j.contains("hpThreshold"))  j.at("hpThreshold").get_to(t.hpThreshold);
	if (j.contains("objectiveIndex")) j.at("objectiveIndex").get_to(t.objectiveIndex);
	if (j.contains("actions"))      j.at("actions").get_to(t.actions);
	// isFired / isPendingDelay / delayTimer はランタイム専用なのでJSONからは読まない
	t.isFired = false;
	t.isPendingDelay = false;
	t.delayTimer = 0.0f;
}
