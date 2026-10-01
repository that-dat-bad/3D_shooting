#pragma once
#include <string>
#include <vector>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/io/json.hpp"

// 前方宣言または依存解決のため
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

/// @brief 単一のウェイポイント情報
struct Waypoint {
	MyMath::Vector3 position;
	float speed = 300.0f; // 目標通過速度 (km/h 等)
};

inline void to_json(nlohmann::json& j, const Waypoint& wp) {
	j = nlohmann::json{
		{"position", wp.position},
		{"speed", wp.speed}
	};
}

inline void from_json(const nlohmann::json& j, Waypoint& wp) {
	if (j.contains("position")) j.at("position").get_to(wp.position);
	if (j.contains("speed")) j.at("speed").get_to(wp.speed);
}

/// @brief 一連のウェイポイントからなるパス（経路）
struct WaypointPath {
	std::string name;
	std::vector<Waypoint> points;
	bool isLoop = false; // 終点から始点に戻るかどうか
};

inline void to_json(nlohmann::json& j, const WaypointPath& path) {
	j = nlohmann::json{
		{"name", path.name},
		{"points", path.points},
		{"isLoop", path.isLoop}
	};
}

inline void from_json(const nlohmann::json& j, WaypointPath& path) {
	if (j.contains("name")) j.at("name").get_to(path.name);
	if (j.contains("points")) j.at("points").get_to(path.points);
	if (j.contains("isLoop")) j.at("isLoop").get_to(path.isLoop);
}
