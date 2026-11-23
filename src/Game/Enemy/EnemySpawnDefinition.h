#pragma once
#include <string>
#include <vector>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/io/json.hpp"
#include "Enemy.h"
#include "GroundEnemy.h"

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

// 敵の配置データ
struct EnemySpawnData {
	MyMath::Vector3 position{};
	std::string modelPath = "";
	float health = 50.0f;
	AIType aiType = AIType::ChaseAttack;
	std::string waypointPathName = "";
	std::string behaviorTreeJson = "";
};

// 地上敵の配置データ
struct GroundEnemySpawnData {
	MyMath::Vector3 position{ 0.0f, 0.0f, 0.0f };
	GroundAIType aiType = GroundAIType::Turret;
	float health = 60.0f;
	GroundEnemyParam param{};
	std::string waypointPathName = "";
};

inline void to_json(nlohmann::json& j, const EnemySpawnData& e) {
	std::string aiStr = "ChaseAttack";
	if (e.aiType == AIType::CruiseEvade) aiStr = "CruiseEvade";
	else if (e.aiType == AIType::FollowWaypoint) aiStr = "FollowWaypoint";
	else if (e.aiType == AIType::AllyWingman) aiStr = "AllyWingman";
	else if (e.aiType == AIType::CanyonDrone) aiStr = "CanyonDrone";
	else if (e.aiType == AIType::CanyonWesternFighter) aiStr = "CanyonWesternFighter";

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
	if (j.contains("position")) j.at("position").get_to(e.position);
	if (j.contains("modelPath")) j.at("modelPath").get_to(e.modelPath);
	if (j.contains("health")) j.at("health").get_to(e.health);

	std::string aiStr = "ChaseAttack";
	if (j.contains("aiType")) {
		j.at("aiType").get_to(aiStr);
	}
	if (aiStr == "CruiseEvade") {
		e.aiType = AIType::CruiseEvade;
	} else if (aiStr == "FollowWaypoint") {
		e.aiType = AIType::FollowWaypoint;
	} else if (aiStr == "AllyWingman") {
		e.aiType = AIType::AllyWingman;
	} else if (aiStr == "CanyonDrone") {
		e.aiType = AIType::CanyonDrone;
	} else if (aiStr == "CanyonWesternFighter") {
		e.aiType = AIType::CanyonWesternFighter;
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
	else if (g.aiType == GroundAIType::Watchtower) aiStr = "Watchtower";

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
	} else if (aiStr == "Watchtower") {
		g.aiType = GroundAIType::Watchtower;
	} else {
		g.aiType = GroundAIType::Turret;
	}

	if (j.contains("param")) {
		j.at("param").get_to(g.param);
	}
	g.param.maxHealth = g.health;
	if (j.contains("waypointPathName")) j.at("waypointPathName").get_to(g.waypointPathName);
}
