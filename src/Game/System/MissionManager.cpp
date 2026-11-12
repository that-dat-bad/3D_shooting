#include "MissionManager.h"
#include <fstream>
#include <iostream>
#include <cmath>

namespace fs = std::filesystem;

namespace MissionDefaults {
	inline constexpr const char* kManifestVersion = "1.0";
	inline constexpr const char* kMissionDirectory = "Resources/missions";
	inline constexpr const char* kJsonExtension = ".json";
	inline constexpr const char* kManifestFileName = "missions.json";
	inline constexpr const char* kDefaultMissionId = "[00]";
	inline constexpr const char* kDefaultMissionPath = "Resources/missions/mission00.json";
	inline constexpr const char* kDefaultMissionName = "Obuchenie";
	inline constexpr const char* kDefaultMissionAlias = "ZADANIE 00";
	inline constexpr const char* kDefaultMissionSubTitle = "オブチェーニエ：訓練・教程 — 極夜の異変 —";
	inline constexpr const char* kDefaultMissionArea = "VERNA OUTER CANYON (POLAR DUSK)";
	inline constexpr const char* kDefaultMissionDesc =
		"定期哨戒と基礎訓練（Obuchenie）から、監視所炎上・西側機との遭遇へ突入する導入。飛行隊長『Corax』の誘導に従い、M-21A（STAR-BORDER-04 // CALLSIGN: ROOKIE）の基本機動と峡谷低空哨戒ルートを習得せよ。";
	inline constexpr float kDefaultPlayerHp = 100.0f;
	inline constexpr MyMath::Vector3 kDefaultPlayerStartPos = { 220.0f, -290.0f, 800.0f };
	inline constexpr float kUnlimitedTimeLimit = 0.0f;
}

MissionManager& MissionManager::GetInstance() {
	static MissionManager instance;
	return instance;
}

MissionManager::MissionManager() {
	currentFilePath_ = MissionDefaults::kDefaultMissionPath;
}

void MissionManager::CacheInitialObjectiveStatuses() {
	initialObjectiveStatuses_.clear();
	initialObjectiveStatuses_.reserve(currentMission_.objectives.size());
	for (const auto& obj : currentMission_.objectives) {
		initialObjectiveStatuses_.push_back(obj.status);
	}

	initialTriggerEnabled_.clear();
	initialTriggerEnabled_.reserve(currentMission_.triggers.size());
	for (const auto& trig : currentMission_.triggers) {
		initialTriggerEnabled_.push_back(trig.isEnabled);
	}
}

bool MissionManager::Load(const std::string& filepath) {
	if (!fs::exists(filepath)) {
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
		CacheInitialObjectiveStatuses();
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
		file << j.dump(MissionSchemaDefaults::kJsonDumpIndent);
		currentFilePath_ = filepath;
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to save mission JSON: " << e.what() << std::endl;
		return false;
	}
}

void MissionManager::ApplyLiveMissionData(const MissionData& liveData, const std::string& liveFilePath) {
	currentMission_ = liveData;
	if (!liveFilePath.empty()) {
		currentFilePath_ = liveFilePath;
	}
	CacheInitialObjectiveStatuses();
}

std::vector<std::string> MissionManager::GetMissionList() {
	std::vector<std::string> missions;
	fs::path missionDir(MissionDefaults::kMissionDirectory);
	
	if (!fs::exists(missionDir)) {
		fs::create_directories(missionDir);
	}

	try {
		for (const auto& entry : fs::directory_iterator(missionDir)) {
			if (entry.is_regular_file() &&
				entry.path().extension() == MissionDefaults::kJsonExtension &&
				entry.path().filename() != MissionDefaults::kManifestFileName) {
				missions.push_back(entry.path().string());
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to scan mission directory: " << e.what() << std::endl;
	}

	if (missions.empty()) {
		std::string defaultPath = MissionDefaults::kDefaultMissionPath;
		CreateDefaultMission(defaultPath);
		missions.push_back(defaultPath);
	}

	return missions;
}

bool MissionManager::LoadMissionManifest(const std::string& manifestPath) {
	if (!fs::exists(manifestPath)) {
		CreateDefaultMissionManifest(manifestPath);
	}

	std::ifstream file(manifestPath);
	if (!file.is_open()) {
		return false;
	}

	try {
		nlohmann::json j;
		file >> j;
		missionEntries_.clear();
		if (j.contains("missions")) {
			missionEntries_ = j.at("missions").get<std::vector<MissionEntry>>();
		}
		isManifestLoaded_ = true;
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to parse mission manifest JSON: " << e.what() << std::endl;
		return false;
	}
}

bool MissionManager::SaveMissionManifest(const std::string& manifestPath) {
	try {
		fs::path path(manifestPath);
		fs::path parent = path.parent_path();
		if (!parent.empty() && !fs::exists(parent)) {
			fs::create_directories(parent);
		}

		std::ofstream file(manifestPath);
		if (!file.is_open()) {
			return false;
		}

		nlohmann::json j;
		j["version"] = MissionDefaults::kManifestVersion;
		j["missions"] = missionEntries_;
		file << j.dump(MissionSchemaDefaults::kJsonDumpIndent);
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to save mission manifest JSON: " << e.what() << std::endl;
		return false;
	}
}

const std::vector<MissionEntry>& MissionManager::GetMissionEntries() {
	if (!isManifestLoaded_) {
		LoadMissionManifest();
	}
	return missionEntries_;
}

void MissionManager::CreateDefaultMissionManifest(const std::string& manifestPath) {
	missionEntries_ = {
		{
			MissionDefaults::kDefaultMissionId,
			MissionDefaults::kDefaultMissionPath,
			MissionDefaults::kDefaultMissionName,
			MissionDefaults::kDefaultMissionAlias,
			MissionDefaults::kDefaultMissionSubTitle,
			false
		}
	};
	SaveMissionManifest(manifestPath);
	isManifestLoaded_ = true;
}

void MissionManager::CreateDefaultMission(const std::string& filepath) {
	currentMission_ = MissionData();
	currentMission_.name = MissionDefaults::kDefaultMissionName;
	currentMission_.aliasName = MissionDefaults::kDefaultMissionAlias;
	currentMission_.subTitle = MissionDefaults::kDefaultMissionSubTitle;
	currentMission_.areaName = MissionDefaults::kDefaultMissionArea;
	currentMission_.description = MissionDefaults::kDefaultMissionDesc;
	currentMission_.playerHP = MissionDefaults::kDefaultPlayerHp;
	currentMission_.playerPosition = MissionDefaults::kDefaultPlayerStartPos;
	currentMission_.playerYawDeg = MissionSchemaDefaults::kDefaultPlayerYawDeg;
	currentMission_.timeLimit = MissionDefaults::kUnlimitedTimeLimit;
	currentMission_.enemies.clear();
	currentMission_.groundEnemies.clear();
	currentMission_.objectives.clear();
	currentMission_.triggers.clear();
	currentMission_.waypointPaths.clear();
	currentMission_.tutorialLayout = TutorialSpatialLayout{};

	CacheInitialObjectiveStatuses();
	Save(filepath);
}

// ============================================================
// Phase 1 & 2: ランタイム評価ロジック
// ============================================================

void MissionManager::ResetRuntimeState() {
	triggerMissionComplete_ = false;
	triggerMissionFail_ = false;
	pendingEnemySpawns_.clear();
	pendingGroundEnemySpawns_.clear();
	activeMessages_.clear();
	pendingRadioMessages_.clear();

	for (size_t i = static_cast<size_t>(MissionSchemaDefaults::kZeroIndex); i < currentMission_.triggers.size(); ++i) {
		currentMission_.triggers[i].isFired = false;
		if (i < initialTriggerEnabled_.size()) {
			currentMission_.triggers[i].isEnabled = initialTriggerEnabled_[i];
		}
	}

	for (size_t i = static_cast<size_t>(MissionSchemaDefaults::kZeroIndex); i < currentMission_.objectives.size(); ++i) {
		if (i < initialObjectiveStatuses_.size()) {
			currentMission_.objectives[i].status = initialObjectiveStatuses_[i];
		}
	}
}

void MissionManager::EvaluateObjectives(
	const MyMath::Vector3& playerPos,
	float playerHP,
	int destroyedCount,
	bool allDestroyed,
	float elapsedTime,
	const EnemyManager* enemyManager
) {
	(void)playerHP;
	const EnemyManager* activeEm = enemyManager ? enemyManager : boundEnemyManager_;
	using namespace MyMath;

	for (auto& obj : currentMission_.objectives) {
		if (obj.status != ObjectiveStatus::Active) {
			continue;
		}

		switch (obj.type) {
		case ObjectiveType::DestroyAll:
			if (allDestroyed) {
				obj.status = ObjectiveStatus::Completed;
			}
			break;

		case ObjectiveType::DestroyCount:
			if (destroyedCount >= obj.requiredKills) {
				obj.status = ObjectiveStatus::Completed;
			}
			break;

		case ObjectiveType::DestroyTarget:
			if (activeEm && activeEm->IsUnitDestroyedByIndex(obj.targetIndex)) {
				obj.status = ObjectiveStatus::Completed;
			}
			break;

		case ObjectiveType::ReachArea: {
			const Vector3 diff = Subtract(playerPos, obj.areaCenter);
			const float dist = Length(diff);
			if (dist <= obj.areaRadius) {
				obj.status = ObjectiveStatus::Completed;
			}
			break;
		}

		case ObjectiveType::Survive:
			if (elapsedTime >= obj.duration) {
				obj.status = ObjectiveStatus::Completed;
			}
			break;

		case ObjectiveType::EscortProtect:
			break;
		}
	}
}

void MissionManager::EvaluateTriggers(
	const MyMath::Vector3& playerPos,
	float playerHP,
	int destroyedCount,
	float elapsedTime,
	const EnemyManager* enemyManager
) {
	const EnemyManager* activeEm = enemyManager ? enemyManager : boundEnemyManager_;
	using namespace MyMath;

	for (size_t i = static_cast<size_t>(MissionSchemaDefaults::kZeroIndex); i < currentMission_.triggers.size(); ++i) {
		auto& trigger = currentMission_.triggers[i];

		if (!trigger.isEnabled) continue;
		if (trigger.isFired && !trigger.isRepeatable) continue;

		bool conditionMet = false;

		switch (trigger.condition) {
		case TriggerCondition::OnMissionStart:
			conditionMet = true;
			break;

		case TriggerCondition::OnTimer:
			conditionMet = (elapsedTime >= trigger.timerSeconds);
			break;

		case TriggerCondition::OnAreaEnter: {
			const Vector3 diff = Subtract(playerPos, trigger.areaCenter);
			const float dist = Length(diff);
			conditionMet = (dist <= trigger.areaRadius);
			break;
		}

		case TriggerCondition::OnAreaLeave: {
			const Vector3 diff = Subtract(playerPos, trigger.areaCenter);
			const float dist = Length(diff);
			conditionMet = (dist > trigger.areaRadius);
			break;
		}

		case TriggerCondition::OnEnemyDestroyed:
			if (activeEm) {
				conditionMet = activeEm->IsUnitDestroyedByIndex(trigger.enemyIndex);
			}
			break;

		case TriggerCondition::OnKillCount:
			conditionMet = (destroyedCount >= trigger.killCount);
			break;

		case TriggerCondition::OnPlayerDamaged:
			conditionMet = (playerHP <= trigger.hpThreshold);
			break;

		case TriggerCondition::OnObjectiveComplete:
			if (trigger.objectiveIndex >= MissionSchemaDefaults::kZeroIndex &&
				trigger.objectiveIndex < static_cast<int>(currentMission_.objectives.size())) {
				conditionMet = (currentMission_.objectives[static_cast<size_t>(trigger.objectiveIndex)].status == ObjectiveStatus::Completed);
			}
			break;
		}

		if (conditionMet) {
			trigger.isFired = true;
			for (const auto& action : trigger.actions) {
				ExecuteAction(action);
			}
		}
	}
}

void MissionManager::ExecuteAction(const TriggerAction& action) {
	switch (action.type) {
	case TriggerActionType::ShowMessage: {
		RuntimeMessage msg;
		msg.text = action.message;
		msg.remainingTime = action.messageDuration;
		activeMessages_.push_back(msg);
		break;
	}

	case TriggerActionType::SpawnEnemies:
		pendingEnemySpawns_.insert(
			pendingEnemySpawns_.end(),
			action.spawnEnemies.begin(),
			action.spawnEnemies.end()
		);
		pendingGroundEnemySpawns_.insert(
			pendingGroundEnemySpawns_.end(),
			action.spawnGroundEnemies.begin(),
			action.spawnGroundEnemies.end()
		);
		break;

	case TriggerActionType::SetObjective:
		if (action.objectiveIndex >= MissionSchemaDefaults::kZeroIndex &&
			action.objectiveIndex < static_cast<int>(currentMission_.objectives.size())) {
			currentMission_.objectives[static_cast<size_t>(action.objectiveIndex)].status =
				StringToObjectiveStatus(action.newStatus);
		}
		break;

	case TriggerActionType::MissionComplete:
		triggerMissionComplete_ = true;
		break;

	case TriggerActionType::MissionFail:
		triggerMissionFail_ = true;
		break;

	case TriggerActionType::ActivateTrigger:
		if (action.activateTriggerIndex >= MissionSchemaDefaults::kZeroIndex &&
			action.activateTriggerIndex < static_cast<int>(currentMission_.triggers.size())) {
			currentMission_.triggers[static_cast<size_t>(action.activateTriggerIndex)].isEnabled = true;
			currentMission_.triggers[static_cast<size_t>(action.activateTriggerIndex)].isFired = false;
		}
		break;

	case TriggerActionType::PlayRadioDialogue: {
		RadioMessage radioMsg{};
		radioMsg.speakerName = action.speakerName;
		radioMsg.dialogueText = action.message;
		radioMsg.faction = action.speakerFaction;
		radioMsg.holdDuration = (action.messageDuration > RadioDefaults::kZero)
			? action.messageDuration
			: RadioDefaults::kDefaultHoldSeconds;
		radioMsg.charInterval = RadioDefaults::kDefaultCharIntervalSeconds;
		radioMsg.interrupt = action.radioInterrupt;
		pendingRadioMessages_.push_back(radioMsg);
		break;
	}
	}
}

bool MissionManager::AreAllPrimaryObjectivesCompleted() const {
	bool hasPrimary = false;
	for (const auto& obj : currentMission_.objectives) {
		if (obj.isPrimary) {
			hasPrimary = true;
			if (obj.status != ObjectiveStatus::Completed) {
				return false;
			}
		}
	}
	return hasPrimary;
}

bool MissionManager::IsAnyPrimaryObjectiveFailed() const {
	for (const auto& obj : currentMission_.objectives) {
		if (obj.isPrimary && obj.status == ObjectiveStatus::Failed) {
			return true;
		}
	}
	return false;
}

std::vector<EnemySpawnData> MissionManager::PopPendingEnemySpawns() {
	std::vector<EnemySpawnData> result = std::move(pendingEnemySpawns_);
	pendingEnemySpawns_.clear();
	return result;
}

std::vector<GroundEnemySpawnData> MissionManager::PopPendingGroundEnemySpawns() {
	std::vector<GroundEnemySpawnData> result = std::move(pendingGroundEnemySpawns_);
	pendingGroundEnemySpawns_.clear();
	return result;
}

std::vector<RadioMessage> MissionManager::PopPendingRadioMessages() {
	std::vector<RadioMessage> result = std::move(pendingRadioMessages_);
	pendingRadioMessages_.clear();
	return result;
}

void MissionManager::UpdateMessages(float deltaTime) {
	for (auto it = activeMessages_.begin(); it != activeMessages_.end(); ) {
		it->remainingTime -= deltaTime;
		if (it->remainingTime <= MissionSchemaDefaults::kZero) {
			it = activeMessages_.erase(it);
		} else {
			++it;
		}
	}
}
