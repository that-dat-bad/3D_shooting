#include "MissionManager.h"
#include <fstream>
#include <iostream>
#include <cmath>

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
	currentMission_.name = "Mission 01: Combined Strike";
	currentMission_.description = "Destroy all target drones and ground defense installations.";
	currentMission_.playerHP = 100.0f;
	currentMission_.playerPosition = { 0.0f, 100.0f, 0.0f };
	currentMission_.timeLimit = 300.0f;
	
	// 空中目標
	currentMission_.enemies = {
		{ {  100.0f, 100.0f,  200.0f }, "Resources/planeplane.obj", 50.0f, AIType::ChaseAttack },
		{ {  -80.0f, 100.0f,  300.0f }, "Resources/planeplane.obj", 50.0f, AIType::ChaseAttack },
		{ {  200.0f, 100.0f,  450.0f }, "Resources/planeplane.obj", 50.0f, AIType::ChaseAttack },
		{ { -150.0f, 100.0f,  500.0f }, "Resources/planeplane.obj", 50.0f, AIType::CruiseEvade },
		{ {   50.0f, 100.0f,  700.0f }, "Resources/planeplane.obj", 50.0f, AIType::CruiseEvade },
	};

	// 地上目標
	GroundEnemySpawnData turret1;
	turret1.position = { 0.0f, 0.0f, 400.0f };
	turret1.aiType = GroundAIType::Turret;
	turret1.health = 60.0f;

	GroundEnemySpawnData turret2;
	turret2.position = { -120.0f, 0.0f, 600.0f };
	turret2.aiType = GroundAIType::Turret;
	turret2.health = 60.0f;

	GroundEnemySpawnData structure1;
	structure1.position = { 150.0f, 0.0f, 550.0f };
	structure1.aiType = GroundAIType::Structure;
	structure1.health = 120.0f;

	GroundEnemySpawnData vehicle1;
	vehicle1.position = { -50.0f, 0.0f, 300.0f };
	vehicle1.aiType = GroundAIType::PatrolVehicle;
	vehicle1.health = 45.0f;

	currentMission_.groundEnemies = { turret1, turret2, structure1, vehicle1 };

	// --- Phase 1: デフォルト目標 ---
	MissionObjective primaryObj;
	primaryObj.name = "Destroy All Targets";
	primaryObj.description = "Destroy all enemy aircraft and ground installations.";
	primaryObj.type = ObjectiveType::DestroyAll;
	primaryObj.status = ObjectiveStatus::Active;
	primaryObj.isPrimary = true;
	currentMission_.objectives.push_back(primaryObj);

	// --- Phase 1: デフォルトトリガー ---
	// 60秒後に増援出現
	MissionTrigger reinforcementTrigger;
	reinforcementTrigger.name = "Reinforcement Wave";
	reinforcementTrigger.condition = TriggerCondition::OnTimer;
	reinforcementTrigger.timerSeconds = 60.0f;
	reinforcementTrigger.isEnabled = true;

	TriggerAction msgAction;
	msgAction.type = TriggerActionType::ShowMessage;
	msgAction.message = "Warning: Enemy reinforcements detected!";
	msgAction.messageDuration = 4.0f;
	reinforcementTrigger.actions.push_back(msgAction);

	TriggerAction spawnAction;
	spawnAction.type = TriggerActionType::SpawnEnemies;
	spawnAction.spawnEnemies = {
		{ { 300.0f, 150.0f, 800.0f }, "Resources/planeplane.obj", 50.0f, AIType::ChaseAttack },
		{ { -300.0f, 150.0f, 900.0f }, "Resources/planeplane.obj", 50.0f, AIType::ChaseAttack },
	};
	reinforcementTrigger.actions.push_back(spawnAction);

	currentMission_.triggers.push_back(reinforcementTrigger);

	Save(filepath);
}


// ============================================================
// Phase 1: ランタイム評価ロジック
// ============================================================

void MissionManager::ResetRuntimeState() {
	triggerMissionComplete_ = false;
	triggerMissionFail_ = false;
	pendingEnemySpawns_.clear();
	pendingGroundEnemySpawns_.clear();
	activeMessages_.clear();

	// 全トリガーの発火フラグをリセット
	for (auto& trigger : currentMission_.triggers) {
		trigger.isFired = false;
	}

	// 全目標のステータスを初期値に戻す
	// (JSONに保存されたstatusを初期状態として扱う)
	// ※Active以外のステータスで保存されている場合はそのまま維持
}

void MissionManager::EvaluateObjectives(
	const MyMath::Vector3& playerPos,
	float playerHP,
	int destroyedCount,
	bool allDestroyed,
	float elapsedTime
) {
	using namespace MyMath;

	for (auto& obj : currentMission_.objectives) {
		// 非アクティブ or 既に完了/失敗の目標はスキップ
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
			// targetIndex は MissionData::enemies のインデックス
			// ランタイムで個別撃破の追跡はEnemyManagerに依存
			// ここでは簡易的にDestroyCountのフォールバックとして扱う
			// （Phase 2 以降で個別ユニットID追跡に改善予定）
			break;

		case ObjectiveType::ReachArea: {
			Vector3 diff = Subtract(playerPos, obj.areaCenter);
			float dist = Length(diff);
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
			// 護衛対象のHP追跡（Phase 2 以降で個別ユニット参照を実装予定）
			break;
		}
	}
}

void MissionManager::EvaluateTriggers(
	const MyMath::Vector3& playerPos,
	float playerHP,
	int destroyedCount,
	float elapsedTime
) {
	using namespace MyMath;

	for (size_t i = 0; i < currentMission_.triggers.size(); ++i) {
		auto& trigger = currentMission_.triggers[i];

		// 無効 or 発火済み（繰り返し不可）のトリガーはスキップ
		if (!trigger.isEnabled) continue;
		if (trigger.isFired && !trigger.isRepeatable) continue;

		bool conditionMet = false;

		switch (trigger.condition) {
		case TriggerCondition::OnMissionStart:
			// ミッション開始時に一度だけ発火（ResetRuntimeState後の最初の評価で発火）
			conditionMet = true;
			break;

		case TriggerCondition::OnTimer:
			conditionMet = (elapsedTime >= trigger.timerSeconds);
			break;

		case TriggerCondition::OnAreaEnter: {
			Vector3 diff = Subtract(playerPos, trigger.areaCenter);
			float dist = Length(diff);
			conditionMet = (dist <= trigger.areaRadius);
			break;
		}

		case TriggerCondition::OnAreaLeave: {
			Vector3 diff = Subtract(playerPos, trigger.areaCenter);
			float dist = Length(diff);
			conditionMet = (dist > trigger.areaRadius);
			break;
		}

		case TriggerCondition::OnEnemyDestroyed:
			// Phase 2 で個別ユニット追跡実装予定
			break;

		case TriggerCondition::OnKillCount:
			conditionMet = (destroyedCount >= trigger.killCount);
			break;

		case TriggerCondition::OnPlayerDamaged:
			conditionMet = (playerHP <= trigger.hpThreshold);
			break;

		case TriggerCondition::OnObjectiveComplete:
			if (trigger.objectiveIndex >= 0 &&
				trigger.objectiveIndex < static_cast<int>(currentMission_.objectives.size())) {
				conditionMet = (currentMission_.objectives[trigger.objectiveIndex].status == ObjectiveStatus::Completed);
			}
			break;
		}

		if (conditionMet) {
			trigger.isFired = true;

			// 全アクションを実行
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
		// 追加敵を保留リストに追加（StageScene側で取り出して生成する）
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
		if (action.objectiveIndex >= 0 &&
			action.objectiveIndex < static_cast<int>(currentMission_.objectives.size())) {
			currentMission_.objectives[action.objectiveIndex].status =
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
		if (action.activateTriggerIndex >= 0 &&
			action.activateTriggerIndex < static_cast<int>(currentMission_.triggers.size())) {
			currentMission_.triggers[action.activateTriggerIndex].isEnabled = true;
			currentMission_.triggers[action.activateTriggerIndex].isFired = false;
		}
		break;
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
	// 主目標がひとつもない場合は「達成」とは判定しない
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

void MissionManager::UpdateMessages(float deltaTime) {
	for (auto it = activeMessages_.begin(); it != activeMessages_.end(); ) {
		it->remainingTime -= deltaTime;
		if (it->remainingTime <= 0.0f) {
			it = activeMessages_.erase(it);
		} else {
			++it;
		}
	}
}
