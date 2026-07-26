#include "MissionManager.h"
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

MissionManager& MissionManager::GetInstance() {
	static MissionManager instance;
	return instance;
}

MissionManager::MissionManager() {
	currentFilePath_ = "Resources/missions/mission01.json";
}

bool MissionManager::Load(const std::string& filepath) {
	if (!fs::exists(filepath)) {
		// ファイルが存在しない場合、デフォルトミッションを作成してロードする
		CreateDefaultMission(filepath);
	}

	std::ifstream file(filepath);
	if (!file.is_open()) {
		return false;
	}

	try {
		nlohmann::json j;
		file >> j;
		currentMission_ = j.get<MissionData>();
		currentFilePath_ = filepath;
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to parse mission JSON: " << e.what() << std::endl;
		return false;
	}
}

bool MissionManager::Save(const std::string& filepath) {
	try {
		fs::path path(filepath);
		fs::path parent = path.parent_path();
		if (!parent.empty() && !fs::exists(parent)) {
			fs::create_directories(parent);
		}

		std::ofstream file(filepath);
		if (!file.is_open()) {
			return false;
		}

		nlohmann::json j = currentMission_;
		file << j.dump(4); // インデント幅4で整形出力
		currentFilePath_ = filepath;
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to save mission JSON: " << e.what() << std::endl;
		return false;
	}
}

std::vector<std::string> MissionManager::GetMissionList() {
	std::vector<std::string> missions;
	fs::path missionDir("Resources/missions");
	
	if (!fs::exists(missionDir)) {
		fs::create_directories(missionDir);
	}

	try {
		for (const auto& entry : fs::directory_iterator(missionDir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				missions.push_back(entry.path().string());
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to scan mission directory: " << e.what() << std::endl;
	}

	// 1つもミッションがない場合は、デフォルトミッションを生成してリストに加える
	if (missions.empty()) {
		std::string defaultPath = (missionDir / "mission01.json").string();
		CreateDefaultMission(defaultPath);
		missions.push_back(defaultPath);
	}

	return missions;
}

void MissionManager::CreateDefaultMission(const std::string& filepath) {
	currentMission_ = MissionData();
	currentMission_.name = "Mission 01: First Strike";
	currentMission_.description = "Destroy all target drones in the airspace.";
	currentMission_.playerHP = 100.0f;
	currentMission_.playerPosition = { 0.0f, 100.0f, 0.0f };
	
	// 元の StageScene::Initialize にあった配置
	currentMission_.enemies = {
		{ {  100.0f, 100.0f,  200.0f }, "Resources/planeplane.obj", 50.0f, AIType::ChaseAttack },
		{ {  -80.0f, 100.0f,  300.0f }, "Resources/planeplane.obj", 50.0f, AIType::ChaseAttack },
		{ {  200.0f, 100.0f,  450.0f }, "Resources/planeplane.obj", 50.0f, AIType::ChaseAttack },
		{ { -150.0f, 100.0f,  500.0f }, "Resources/planeplane.obj", 50.0f, AIType::CruiseEvade },
		{ {   50.0f, 100.0f,  700.0f }, "Resources/planeplane.obj", 50.0f, AIType::CruiseEvade },
	};

	Save(filepath);
}
