#pragma once
#include <string>
#include <vector>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/io/json.hpp"

// Vector3 の JSON 変換（複数ヘッダーで必要なためここで定義）
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

// ============================================================
// ミッション目標 (Mission Objective) の型定義
// ============================================================

/// @brief ミッション目標の種類
enum class ObjectiveType {
	DestroyAll,      ///< 全敵を殲滅
	DestroyCount,    ///< N体以上の敵を撃破
	DestroyTarget,   ///< 特定のユニットを撃破（enemyIndex指定）
	ReachArea,       ///< 指定エリアに到達
	Survive,         ///< 制限時間まで生存
	EscortProtect,   ///< 護衛対象の生存
};

/// @brief ミッション目標の状態
enum class ObjectiveStatus {
	Inactive,   ///< 未開始（トリガーで有効化されるまで非表示）
	Active,     ///< 進行中
	Completed,  ///< 達成
	Failed,     ///< 失敗
};

/// @brief ミッション目標のデータ
struct MissionObjective {
	std::string name = "Objective";
	std::string description = "";
	ObjectiveType type = ObjectiveType::DestroyAll;
	ObjectiveStatus status = ObjectiveStatus::Active;
	bool isPrimary = true;       ///< true: 主目標, false: 副目標
	bool isHidden = false;       ///< true: HUDに表示しない（隠し目標）

	// --- 各目標タイプのパラメータ ---
	int requiredKills = 0;              ///< DestroyCount用: 必要撃破数
	int targetIndex = -1;               ///< DestroyTarget / EscortProtect用: 敵インデックス
	MyMath::Vector3 areaCenter{};       ///< ReachArea用: エリア中心
	float areaRadius = 100.0f;          ///< ReachArea用: エリア半径
	float duration = 300.0f;            ///< Survive用: 生存時間 (秒)
	float minHP = 0.0f;                 ///< EscortProtect用: 最低HP
};

// ============================================================
// JSON シリアライズ / デシリアライズ
// ============================================================

/// @brief ObjectiveType → 文字列変換
inline const char* ObjectiveTypeToString(ObjectiveType type) {
	switch (type) {
	case ObjectiveType::DestroyAll:    return "DestroyAll";
	case ObjectiveType::DestroyCount:  return "DestroyCount";
	case ObjectiveType::DestroyTarget: return "DestroyTarget";
	case ObjectiveType::ReachArea:     return "ReachArea";
	case ObjectiveType::Survive:       return "Survive";
	case ObjectiveType::EscortProtect: return "EscortProtect";
	default:                           return "DestroyAll";
	}
}

/// @brief 文字列 → ObjectiveType 変換
inline ObjectiveType StringToObjectiveType(const std::string& str) {
	if (str == "DestroyCount")  return ObjectiveType::DestroyCount;
	if (str == "DestroyTarget") return ObjectiveType::DestroyTarget;
	if (str == "ReachArea")     return ObjectiveType::ReachArea;
	if (str == "Survive")       return ObjectiveType::Survive;
	if (str == "EscortProtect") return ObjectiveType::EscortProtect;
	return ObjectiveType::DestroyAll;
}

/// @brief ObjectiveStatus → 文字列変換
inline const char* ObjectiveStatusToString(ObjectiveStatus status) {
	switch (status) {
	case ObjectiveStatus::Inactive:  return "Inactive";
	case ObjectiveStatus::Active:    return "Active";
	case ObjectiveStatus::Completed: return "Completed";
	case ObjectiveStatus::Failed:    return "Failed";
	default:                         return "Active";
	}
}

/// @brief 文字列 → ObjectiveStatus 変換
inline ObjectiveStatus StringToObjectiveStatus(const std::string& str) {
	if (str == "Inactive")  return ObjectiveStatus::Inactive;
	if (str == "Completed") return ObjectiveStatus::Completed;
	if (str == "Failed")    return ObjectiveStatus::Failed;
	return ObjectiveStatus::Active;
}

inline void to_json(nlohmann::json& j, const MissionObjective& o) {
	j = nlohmann::json{
		{"name", o.name},
		{"description", o.description},
		{"type", ObjectiveTypeToString(o.type)},
		{"status", ObjectiveStatusToString(o.status)},
		{"isPrimary", o.isPrimary},
		{"isHidden", o.isHidden},
		{"requiredKills", o.requiredKills},
		{"targetIndex", o.targetIndex},
		{"areaCenter", o.areaCenter},
		{"areaRadius", o.areaRadius},
		{"duration", o.duration},
		{"minHP", o.minHP}
	};
}

inline void from_json(const nlohmann::json& j, MissionObjective& o) {
	if (j.contains("name"))        j.at("name").get_to(o.name);
	if (j.contains("description")) j.at("description").get_to(o.description);
	if (j.contains("type")) {
		std::string typeStr;
		j.at("type").get_to(typeStr);
		o.type = StringToObjectiveType(typeStr);
	}
	if (j.contains("status")) {
		std::string statusStr;
		j.at("status").get_to(statusStr);
		o.status = StringToObjectiveStatus(statusStr);
	}
	if (j.contains("isPrimary"))     j.at("isPrimary").get_to(o.isPrimary);
	if (j.contains("isHidden"))      j.at("isHidden").get_to(o.isHidden);
	if (j.contains("requiredKills")) j.at("requiredKills").get_to(o.requiredKills);
	if (j.contains("targetIndex"))   j.at("targetIndex").get_to(o.targetIndex);
	if (j.contains("areaCenter"))    j.at("areaCenter").get_to(o.areaCenter);
	if (j.contains("areaRadius"))    j.at("areaRadius").get_to(o.areaRadius);
	if (j.contains("duration"))      j.at("duration").get_to(o.duration);
	if (j.contains("minHP"))         j.at("minHP").get_to(o.minHP);
}
