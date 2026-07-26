#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "../../engine/base/Math/MyMath.h"
#include "../Enemy/EnemyManager.h"
#include "../../engine/io/json.hpp"

/// @brief ミッションの構成データ
struct MissionData {
	std::string name = "Default Mission";
	std::string description = "Destroy all enemy aircraft.";
	float playerHP = 100.0f;
	MyMath::Vector3 playerPosition = { 0.0f, 100.0f, 0.0f };
	std::vector<EnemySpawnData> enemies;
};

// nlohmann/json 相互変換用の定義
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

inline void to_json(nlohmann::json& j, const EnemySpawnData& e) {
	std::string aiStr = (e.aiType == AIType::ChaseAttack) ? "ChaseAttack" : "CruiseEvade";
	j = nlohmann::json{
		{"position", e.position},
		{"modelPath", e.modelPath},
		{"health", e.health},
		{"aiType", aiStr}
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
	} else {
		e.aiType = AIType::ChaseAttack;
	}
}

inline void to_json(nlohmann::json& j, const MissionData& m) {
	j = nlohmann::json{
		{"name", m.name},
		{"description", m.description},
		{"playerHP", m.playerHP},
		{"playerPosition", m.playerPosition},
		{"enemies", m.enemies}
	};
}

inline void from_json(const nlohmann::json& j, MissionData& m) {
	if (j.contains("name")) j.at("name").get_to(m.name);
	if (j.contains("description")) j.at("description").get_to(m.description);
	if (j.contains("playerHP")) j.at("playerHP").get_to(m.playerHP);
	if (j.contains("playerPosition")) j.at("playerPosition").get_to(m.playerPosition);
	if (j.contains("enemies")) j.at("enemies").get_to(m.enemies);
}

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

	MissionData currentMission_;
	std::string currentFilePath_;
};
