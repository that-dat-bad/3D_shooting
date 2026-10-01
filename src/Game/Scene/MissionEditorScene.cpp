#include "MissionEditorScene.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "WinApp.h"

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

void MissionEditorScene::Initialize() {
	sceneID = SCENE::MISSION_EDITOR;

	auto& mm = MissionManager::GetInstance();
	missionMapView_.Initialize();

	// 初期ミッション読み込み
	std::string initialPath = "Resources/missions/mission1.json";
	if (!mm.Load(initialPath)) {
		mm.CreateDefaultMission(initialPath);
		mm.Load(initialPath);
	}
	currentMission_ = mm.GetCurrentMission();
	strcpy_s(tempMissionName_, sizeof(tempMissionName_), currentMission_.name.c_str());
	strcpy_s(tempMissionDesc_, sizeof(tempMissionDesc_), currentMission_.description.c_str());
}

void MissionEditorScene::Update() {
}

void MissionEditorScene::Draw() {
}

void MissionEditorScene::DrawUI() {
#ifdef USE_IMGUI
	DrawMissionEditor();
#endif
}

void MissionEditorScene::Finalize() {
}

void MissionEditorScene::DrawMissionEditor() {
#ifdef USE_IMGUI
	if (ImGui::Begin("Mission Editor", nullptr, ImGuiWindowFlags_MenuBar)) {
		// 1. ファイル管理
		if (ImGui::CollapsingHeader("File Management", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto& mm = MissionManager::GetInstance();
			std::vector<std::string> cfgFiles = mm.GetMissionList();
			std::vector<std::string> fileNames;
			std::vector<const char*> fileNamePtrs;
			for (const auto& f : cfgFiles) {
				std::filesystem::path p(f);
				fileNames.push_back(p.filename().string());
			}
			for (const auto& fn : fileNames) {
				fileNamePtrs.push_back(fn.c_str());
			}

			if (ImGui::Combo("Select Mission", &selectedMissionIndex_, fileNamePtrs.data(), static_cast<int>(fileNamePtrs.size()))) {
			}

			if (ImGui::Button("Load Selected")) {
				if (selectedMissionIndex_ >= 0 && selectedMissionIndex_ < cfgFiles.size()) {
					std::string loadPath = cfgFiles[selectedMissionIndex_];
					if (mm.Load(loadPath)) {
						currentMission_ = mm.GetCurrentMission();
						strcpy_s(tempMissionName_, sizeof(tempMissionName_), currentMission_.name.c_str());
						strcpy_s(tempMissionDesc_, sizeof(tempMissionDesc_), currentMission_.description.c_str());
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset File List")) {
				selectedMissionIndex_ = 0;
			}

			ImGui::Separator();

			// 新規作成
			static char newFileName[64] = "new_mission.json";
			ImGui::InputText("New JSON Name", newFileName, sizeof(newFileName));
			if (ImGui::Button("Create New Mission")) {
				std::string newPath = "Resources/missions/" + std::string(newFileName);
				mm.CreateDefaultMission(newPath);
				mm.Load(newPath);
				currentMission_ = mm.GetCurrentMission();
				
				std::vector<std::string> scanFiles = mm.GetMissionList();
				for (int i = 0; i < scanFiles.size(); ++i) {
					if (scanFiles[i] == newPath) {
						selectedMissionIndex_ = i;
						break;
					}
				}
			}

			ImGui::Separator();

			// 保存
			ImGui::InputText("Save File Name", tempSaveFileName_, sizeof(tempSaveFileName_));
			if (ImGui::Button("Save (Overwrite)")) {
				std::string savePath = "Resources/missions/" + std::string(tempSaveFileName_);
				if (savePath.find(".json") == std::string::npos) {
					savePath += ".json";
				}
				currentMission_.name = tempMissionName_;
				currentMission_.description = tempMissionDesc_;
				mm.GetCurrentMission() = currentMission_;
				mm.Save(savePath);
			}
		}

		// 2. ミッションの基本情報編集
		if (ImGui::CollapsingHeader("Mission Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::InputText("Mission Name", tempMissionName_, sizeof(tempMissionName_));
			ImGui::InputText("Description", tempMissionDesc_, sizeof(tempMissionDesc_));
			
			ImGui::SliderFloat("Player Initial HP", &currentMission_.playerHP, 1.0f, 500.0f);
			ImGui::DragFloat3("Player Initial Pos", &currentMission_.playerPosition.x, 1.0f);
			if (ImGui::Button("Set to Origin")) {
				currentMission_.playerPosition = {0.0f, 0.0f, 0.0f};
			}
		}

		// 3. 敵配置の編集
		if (ImGui::CollapsingHeader("Enemy Spawns", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Total Enemies: %d", static_cast<int>(currentMission_.enemies.size()));
			
			if (ImGui::Button("Add New Enemy at Origin")) {
				EnemySpawnData newEnemy;
				newEnemy.position = {0.0f, 200.0f, 0.0f};
				newEnemy.modelPath = "Resources/planeplane.obj";
				newEnemy.health = 50.0f;
				newEnemy.aiType = AIType::ChaseAttack;
				currentMission_.enemies.push_back(newEnemy);
				selectedEnemyIndex_ = static_cast<int>(currentMission_.enemies.size()) - 1;
			}

			ImGui::Separator();

			std::vector<std::string> enemyLabels;
			for (size_t i = 0; i < currentMission_.enemies.size(); ++i) {
				const auto& e = currentMission_.enemies[i];
				std::string aiName = (e.aiType == AIType::ChaseAttack) ? "Chase" : (e.aiType == AIType::CruiseEvade) ? "Evade" : "Follow";
				enemyLabels.push_back("Enemy " + std::to_string(i) + " [" + aiName + "] @ (" + 
					std::to_string(static_cast<int>(e.position.x)) + ", " + 
					std::to_string(static_cast<int>(e.position.y)) + ", " + 
					std::to_string(static_cast<int>(e.position.z)) + ")");
			}

			std::vector<const char*> enemyLabelPtrs;
			for (const auto& l : enemyLabels) {
				enemyLabelPtrs.push_back(l.c_str());
			}

			if (ImGui::ListBox("Select Enemy to Edit", &selectedEnemyIndex_, enemyLabelPtrs.data(), static_cast<int>(enemyLabelPtrs.size()), 5)) {
			}

			if (selectedEnemyIndex_ >= 0 && selectedEnemyIndex_ < static_cast<int>(currentMission_.enemies.size())) {
				ImGui::Separator();
				ImGui::Text("--- Edit Enemy %d ---", selectedEnemyIndex_);
				auto& enemy = currentMission_.enemies[selectedEnemyIndex_];

				ImGui::DragFloat3("Position", &enemy.position.x, 1.0f);
				if (ImGui::Button("Move to Current Player")) {
					enemy.position = currentMission_.playerPosition;
				}

				ImGui::SliderFloat("Health", &enemy.health, 10.0f, 500.0f);

				int aiTypeInt = (enemy.aiType == AIType::ChaseAttack) ? 0 : (enemy.aiType == AIType::CruiseEvade) ? 1 : 2;
				const char* aiItems[] = { "ChaseAttack", "CruiseEvade", "FollowWaypoint" };
				if (ImGui::Combo("AI Type", &aiTypeInt, aiItems, 3)) {
					if (aiTypeInt == 0) enemy.aiType = AIType::ChaseAttack;
					else if (aiTypeInt == 1) enemy.aiType = AIType::CruiseEvade;
					else enemy.aiType = AIType::FollowWaypoint;
				}

				if (enemy.aiType == AIType::FollowWaypoint) {
					static char wpPathBuf[256];
					strcpy_s(wpPathBuf, sizeof(wpPathBuf), enemy.waypointPathName.c_str());
					if (ImGui::InputText("Waypoint Path Name", wpPathBuf, sizeof(wpPathBuf))) {
						enemy.waypointPathName = wpPathBuf;
					}
				}

				static char modelPathBuf[256];
				strcpy_s(modelPathBuf, sizeof(modelPathBuf), enemy.modelPath.c_str());
				if (ImGui::InputText("Model Path", modelPathBuf, sizeof(modelPathBuf))) {
					enemy.modelPath = modelPathBuf;
				}

				if (ImGui::Button("Duplicate Enemy")) {
					EnemySpawnData dup = enemy;
					dup.position.x += 20.0f;
					currentMission_.enemies.push_back(dup);
					selectedEnemyIndex_ = static_cast<int>(currentMission_.enemies.size()) - 1;
				}
				ImGui::SameLine();
				if (ImGui::Button("Delete Enemy")) {
					currentMission_.enemies.erase(currentMission_.enemies.begin() + selectedEnemyIndex_);
					selectedEnemyIndex_ = -1;
				}
			}
		}

		// 4. 地上目標配置の編集
		if (ImGui::CollapsingHeader("Ground Target Spawns", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Total Ground Targets: %d", static_cast<int>(currentMission_.groundEnemies.size()));

			if (ImGui::Button("Add AAA Turret (Ground)")) {
				GroundEnemySpawnData newGround;
				Vector3 pPos = currentMission_.playerPosition;
				newGround.position = { pPos.x, 0.0f, pPos.z };
				newGround.aiType = GroundAIType::Turret;
				newGround.health = 60.0f;
				currentMission_.groundEnemies.push_back(newGround);
				selectedGroundEnemyIndex_ = static_cast<int>(currentMission_.groundEnemies.size()) - 1;
			}
			ImGui::SameLine();
			if (ImGui::Button("Add Structure")) {
				GroundEnemySpawnData newGround;
				Vector3 pPos = currentMission_.playerPosition;
				newGround.position = { pPos.x, 0.0f, pPos.z };
				newGround.aiType = GroundAIType::Structure;
				newGround.health = 120.0f;
				newGround.param.collisionRadius = 14.0f;
				currentMission_.groundEnemies.push_back(newGround);
				selectedGroundEnemyIndex_ = static_cast<int>(currentMission_.groundEnemies.size()) - 1;
			}
			ImGui::SameLine();
			if (ImGui::Button("Add Patrol Vehicle")) {
				GroundEnemySpawnData newGround;
				Vector3 pPos = currentMission_.playerPosition;
				newGround.position = { pPos.x, 0.0f, pPos.z };
				newGround.aiType = GroundAIType::PatrolVehicle;
				newGround.health = 45.0f;
				currentMission_.groundEnemies.push_back(newGround);
				selectedGroundEnemyIndex_ = static_cast<int>(currentMission_.groundEnemies.size()) - 1;
			}

			ImGui::Separator();

			std::vector<std::string> groundLabels;
			for (size_t i = 0; i < currentMission_.groundEnemies.size(); ++i) {
				const auto& g = currentMission_.groundEnemies[i];
				std::string typeStr = (g.aiType == GroundAIType::Turret) ? "Turret" :
					(g.aiType == GroundAIType::Structure) ? "Structure" : "Vehicle";
				groundLabels.push_back("Ground " + std::to_string(i) + " [" + typeStr + "] @ (" +
					std::to_string(static_cast<int>(g.position.x)) + ", " +
					std::to_string(static_cast<int>(g.position.y)) + ", " +
					std::to_string(static_cast<int>(g.position.z)) + ")");
			}

			std::vector<const char*> groundLabelPtrs;
			for (const auto& l : groundLabels) {
				groundLabelPtrs.push_back(l.c_str());
			}

			if (ImGui::ListBox("Select Ground Target", &selectedGroundEnemyIndex_, groundLabelPtrs.data(), static_cast<int>(groundLabelPtrs.size()), 5)) {
			}

			if (selectedGroundEnemyIndex_ >= 0 && selectedGroundEnemyIndex_ < static_cast<int>(currentMission_.groundEnemies.size())) {
				ImGui::Separator();
				ImGui::Text("--- Edit Ground Target %d ---", selectedGroundEnemyIndex_);
				auto& ground = currentMission_.groundEnemies[selectedGroundEnemyIndex_];

				ImGui::DragFloat3("Position", &ground.position.x, 1.0f);
				if (ImGui::Button("Snap to Ground (Y=0)")) {
					ground.position.y = 0.0f;
				}

				ImGui::SliderFloat("Health", &ground.health, 10.0f, 500.0f);
				ground.param.maxHealth = ground.health;

				int gTypeInt = (ground.aiType == GroundAIType::Turret) ? 0 :
					(ground.aiType == GroundAIType::Structure) ? 1 : 2;
				const char* gItems[] = { "AAA Turret", "Structure", "Patrol Vehicle" };
				if (ImGui::Combo("AI Type", &gTypeInt, gItems, 3)) {
					if (gTypeInt == 0) ground.aiType = GroundAIType::Turret;
					else if (gTypeInt == 1) ground.aiType = GroundAIType::Structure;
					else ground.aiType = GroundAIType::PatrolVehicle;
				}

				if (ground.aiType == GroundAIType::PatrolVehicle) {
					static char wpPathBuf[256];
					strcpy_s(wpPathBuf, sizeof(wpPathBuf), ground.waypointPathName.c_str());
					if (ImGui::InputText("Waypoint Path Name", wpPathBuf, sizeof(wpPathBuf))) {
						ground.waypointPathName = wpPathBuf;
					}
				}

				if (ground.aiType == GroundAIType::Turret || ground.aiType == GroundAIType::PatrolVehicle) {
					ImGui::SliderFloat("Fire Range (m)", &ground.param.fireRange, 200.0f, 3000.0f);
					ImGui::SliderFloat("Turn Speed (deg/s)", &ground.param.turnSpeedDeg, 10.0f, 180.0f);
					ImGui::SliderFloat("Bullet Speed (m/s)", &ground.param.bulletSpeed, 200.0f, 1200.0f);
					ImGui::SliderFloat("Bullet Damage", &ground.param.bulletDamage, 1.0f, 50.0f);
					ImGui::SliderInt("Burst Count", &ground.param.burstCount, 1, 20);
					ImGui::SliderFloat("Burst Cooldown (s)", &ground.param.burstCooldown, 0.5f, 5.0f);
				}

				if (ground.aiType == GroundAIType::PatrolVehicle) {
					ImGui::SliderFloat("Move Speed (m/s)", &ground.param.moveSpeed, 2.0f, 50.0f);
					ImGui::SliderFloat("Patrol Distance (m)", &ground.param.patrolDistance, 50.0f, 1000.0f);
				}

				if (ImGui::Button("Duplicate Ground Target")) {
					GroundEnemySpawnData dup = ground;
					dup.position.x += 30.0f;
					currentMission_.groundEnemies.push_back(dup);
					selectedGroundEnemyIndex_ = static_cast<int>(currentMission_.groundEnemies.size()) - 1;
				}
				ImGui::SameLine();
				if (ImGui::Button("Delete Ground Target")) {
					currentMission_.groundEnemies.erase(currentMission_.groundEnemies.begin() + selectedGroundEnemyIndex_);
					selectedGroundEnemyIndex_ = -1;
				}
			}
		}

		ImGui::Separator();
		DrawObjectiveEditor();
		DrawTriggerEditor();

		if (ImGui::CollapsingHeader("Time Limit")) {
			ImGui::SliderFloat("Time Limit (sec)", &currentMission_.timeLimit, 0.0f, 1800.0f, "%.0f sec");
			ImGui::SameLine();
			ImGui::TextDisabled("(0 = unlimited)");
		}

	}
	ImGui::End();

	// 2D Map Editor
	missionMapView_.Draw(currentMission_, currentMission_.playerPosition);

	// 同期（Map -> Editor）
	if (missionMapView_.GetSelectedEnemyIndex() != -1) selectedEnemyIndex_ = missionMapView_.GetSelectedEnemyIndex();
	if (missionMapView_.GetSelectedGroundEnemyIndex() != -1) selectedGroundEnemyIndex_ = missionMapView_.GetSelectedGroundEnemyIndex();
	if (missionMapView_.GetSelectedTriggerIndex() != -1) selectedTriggerIndex_ = missionMapView_.GetSelectedTriggerIndex();
#endif
}

void MissionEditorScene::DrawObjectiveEditor() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Mission Objectives")) {
		ImGui::Text("Total Objectives: %d", static_cast<int>(currentMission_.objectives.size()));

		const char* objectiveTypeNames[] = {
			"DestroyAll", "DestroyCount", "DestroyTarget",
			"ReachArea", "Survive", "EscortProtect"
		};
		static int newObjType = 0;
		ImGui::Combo("New Objective Type", &newObjType, objectiveTypeNames, 6);
		ImGui::SameLine();
		if (ImGui::Button("Add Objective")) {
			MissionObjective newObj;
			newObj.type = static_cast<ObjectiveType>(newObjType);
			newObj.name = std::string("Objective ") + std::to_string(currentMission_.objectives.size());
			newObj.status = ObjectiveStatus::Active;
			currentMission_.objectives.push_back(newObj);
			selectedObjectiveIndex_ = static_cast<int>(currentMission_.objectives.size()) - 1;
		}

		ImGui::Separator();

		for (int i = 0; i < static_cast<int>(currentMission_.objectives.size()); ++i) {
			const auto& obj = currentMission_.objectives[i];
			ImVec4 color;
			switch (obj.status) {
			case ObjectiveStatus::Completed: color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); break;
			case ObjectiveStatus::Failed:    color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); break;
			case ObjectiveStatus::Inactive:  color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); break;
			default:                         color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f); break;
			}
			std::string label = (obj.isPrimary ? "[PRIMARY] " : "[SECONDARY] ") + obj.name + " (" + ObjectiveTypeToString(obj.type) + ") - " + ObjectiveStatusToString(obj.status);
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			if (ImGui::Selectable(label.c_str(), selectedObjectiveIndex_ == i)) {
				selectedObjectiveIndex_ = i;
			}
			ImGui::PopStyleColor();
		}

		if (selectedObjectiveIndex_ >= 0 && selectedObjectiveIndex_ < static_cast<int>(currentMission_.objectives.size())) {
			ImGui::Separator();
			ImGui::Text("--- Edit Objective %d ---", selectedObjectiveIndex_);
			auto& obj = currentMission_.objectives[selectedObjectiveIndex_];

			static char objNameBuf[128];
			strcpy_s(objNameBuf, sizeof(objNameBuf), obj.name.c_str());
			if (ImGui::InputText("Name##Obj", objNameBuf, sizeof(objNameBuf))) obj.name = objNameBuf;

			static char objDescBuf[256];
			strcpy_s(objDescBuf, sizeof(objDescBuf), obj.description.c_str());
			if (ImGui::InputText("Description##Obj", objDescBuf, sizeof(objDescBuf))) obj.description = objDescBuf;

			int typeInt = static_cast<int>(obj.type);
			if (ImGui::Combo("Type##Obj", &typeInt, objectiveTypeNames, 6)) obj.type = static_cast<ObjectiveType>(typeInt);

			const char* statusNames[] = { "Inactive", "Active", "Completed", "Failed" };
			int statusInt = static_cast<int>(obj.status);
			if (ImGui::Combo("Initial Status##Obj", &statusInt, statusNames, 4)) obj.status = static_cast<ObjectiveStatus>(statusInt);

			ImGui::Checkbox("Primary Objective", &obj.isPrimary);
			ImGui::SameLine();
			ImGui::Checkbox("Hidden", &obj.isHidden);

			switch (obj.type) {
			case ObjectiveType::DestroyCount: ImGui::SliderInt("Required Kills", &obj.requiredKills, 1, 50); break;
			case ObjectiveType::DestroyTarget: ImGui::InputInt("Target Enemy Index", &obj.targetIndex); break;
			case ObjectiveType::ReachArea:
				ImGui::DragFloat3("Area Center##Obj", &obj.areaCenter.x, 1.0f);
				ImGui::SliderFloat("Area Radius##Obj", &obj.areaRadius, 10.0f, 2000.0f);
				if (ImGui::Button("Set Area to Origin##Obj")) obj.areaCenter = {0.0f, 0.0f, 0.0f};
				break;
			case ObjectiveType::Survive: ImGui::SliderFloat("Survive Duration (sec)", &obj.duration, 10.0f, 1800.0f); break;
			case ObjectiveType::EscortProtect:
				ImGui::InputInt("Escort Target Index", &obj.targetIndex);
				ImGui::SliderFloat("Min HP Threshold", &obj.minHP, 0.0f, 100.0f);
				break;
			default: break;
			}

			if (ImGui::Button("Delete Objective")) {
				currentMission_.objectives.erase(currentMission_.objectives.begin() + selectedObjectiveIndex_);
				selectedObjectiveIndex_ = -1;
			}
		}
	}
#endif
}

void MissionEditorScene::DrawTriggerEditor() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Mission Triggers")) {
		ImGui::Text("Total Triggers: %d", static_cast<int>(currentMission_.triggers.size()));

		const char* conditionNames[] = {
			"OnMissionStart", "OnTimer", "OnAreaEnter", "OnAreaLeave",
			"OnEnemyDestroyed", "OnKillCount", "OnPlayerDamaged", "OnObjectiveComplete"
		};
		static int newCondType = 0;
		ImGui::Combo("New Trigger Condition", &newCondType, conditionNames, 8);
		ImGui::SameLine();
		if (ImGui::Button("Add Trigger")) {
			MissionTrigger newTrig;
			newTrig.condition = static_cast<TriggerCondition>(newCondType);
			newTrig.name = std::string("Trigger ") + std::to_string(currentMission_.triggers.size());
			currentMission_.triggers.push_back(newTrig);
			selectedTriggerIndex_ = static_cast<int>(currentMission_.triggers.size()) - 1;
		}

		ImGui::Separator();

		for (int i = 0; i < static_cast<int>(currentMission_.triggers.size()); ++i) {
			const auto& trig = currentMission_.triggers[i];
			ImVec4 color = trig.isEnabled ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
			std::string label = trig.name + " [" + TriggerConditionToString(trig.condition) + "]" + (trig.isEnabled ? "" : " (DISABLED)") + (trig.isFired ? " (FIRED)" : "") + " | Actions: " + std::to_string(trig.actions.size());

			ImGui::PushStyleColor(ImGuiCol_Text, color);
			if (ImGui::Selectable(label.c_str(), selectedTriggerIndex_ == i)) {
				selectedTriggerIndex_ = i;
				selectedActionIndex_ = -1;
			}
			ImGui::PopStyleColor();
		}

		if (selectedTriggerIndex_ >= 0 && selectedTriggerIndex_ < static_cast<int>(currentMission_.triggers.size())) {
			ImGui::Separator();
			ImGui::Text("--- Edit Trigger %d ---", selectedTriggerIndex_);
			auto& trig = currentMission_.triggers[selectedTriggerIndex_];

			static char trigNameBuf[128];
			strcpy_s(trigNameBuf, sizeof(trigNameBuf), trig.name.c_str());
			if (ImGui::InputText("Name##Trig", trigNameBuf, sizeof(trigNameBuf))) trig.name = trigNameBuf;

			int condInt = static_cast<int>(trig.condition);
			if (ImGui::Combo("Condition##Trig", &condInt, conditionNames, 8)) trig.condition = static_cast<TriggerCondition>(condInt);

			ImGui::Checkbox("Enabled##Trig", &trig.isEnabled);
			ImGui::SameLine();
			ImGui::Checkbox("Repeatable##Trig", &trig.isRepeatable);

			switch (trig.condition) {
			case TriggerCondition::OnTimer: ImGui::SliderFloat("Timer (sec)##Trig", &trig.timerSeconds, 0.0f, 1800.0f); break;
			case TriggerCondition::OnAreaEnter:
			case TriggerCondition::OnAreaLeave:
				ImGui::DragFloat3("Area Center##Trig", &trig.areaCenter.x, 1.0f);
				ImGui::SliderFloat("Area Radius##Trig", &trig.areaRadius, 10.0f, 2000.0f);
				if (ImGui::Button("Set Area to Origin##Trig")) trig.areaCenter = {0.0f, 0.0f, 0.0f};
				break;
			case TriggerCondition::OnEnemyDestroyed: ImGui::InputInt("Enemy Index##Trig", &trig.enemyIndex); break;
			case TriggerCondition::OnKillCount: ImGui::SliderInt("Kill Count##Trig", &trig.killCount, 1, 50); break;
			case TriggerCondition::OnPlayerDamaged: ImGui::SliderFloat("HP Threshold##Trig", &trig.hpThreshold, 0.0f, 100.0f); break;
			case TriggerCondition::OnObjectiveComplete: ImGui::InputInt("Objective Index##Trig", &trig.objectiveIndex); break;
			default: break;
			}

			ImGui::Separator();
			ImGui::Text("Actions (%d):", static_cast<int>(trig.actions.size()));

			const char* actionTypeNames[] = { "SpawnEnemies", "ShowMessage", "SetObjective", "MissionComplete", "MissionFail", "ActivateTrigger" };
			static int newActionType = 1; 
			ImGui::Combo("New Action Type##Act", &newActionType, actionTypeNames, 6);
			ImGui::SameLine();
			if (ImGui::Button("Add Action##Act")) {
				TriggerAction newAction;
				newAction.type = static_cast<TriggerActionType>(newActionType);
				trig.actions.push_back(newAction);
				selectedActionIndex_ = static_cast<int>(trig.actions.size()) - 1;
			}

			for (int a = 0; a < static_cast<int>(trig.actions.size()); ++a) {
				ImGui::PushID(a);
				auto& action = trig.actions[a];
				std::string actLabel = std::string("[") + std::to_string(a) + "] " + TriggerActionTypeToString(action.type);

				if (ImGui::Selectable(actLabel.c_str(), selectedActionIndex_ == a)) selectedActionIndex_ = a;

				if (selectedActionIndex_ == a) {
					int actTypeInt = static_cast<int>(action.type);
					if (ImGui::Combo("Action Type##ActEdit", &actTypeInt, actionTypeNames, 6)) action.type = static_cast<TriggerActionType>(actTypeInt);

					switch (action.type) {
					case TriggerActionType::ShowMessage: {
						static char msgBuf[256];
						strcpy_s(msgBuf, sizeof(msgBuf), action.message.c_str());
						if (ImGui::InputText("Message##Act", msgBuf, sizeof(msgBuf))) action.message = msgBuf;
						ImGui::SliderFloat("Duration##Act", &action.messageDuration, 1.0f, 15.0f);
						break;
					}
					case TriggerActionType::SetObjective:
						ImGui::InputInt("Objective Index##Act", &action.objectiveIndex);
						{
							const char* newStatusItems[] = { "Active", "Completed", "Failed", "Inactive" };
							static int newStatusIdx = 0;
							if (action.newStatus == "Completed") newStatusIdx = 1; else if (action.newStatus == "Failed") newStatusIdx = 2; else if (action.newStatus == "Inactive") newStatusIdx = 3; else newStatusIdx = 0;
							if (ImGui::Combo("New Status##Act", &newStatusIdx, newStatusItems, 4)) action.newStatus = newStatusItems[newStatusIdx];
						}
						break;
					case TriggerActionType::SpawnEnemies:
						ImGui::Text("Air spawns: %d | Ground spawns: %d", static_cast<int>(action.spawnEnemies.size()), static_cast<int>(action.spawnGroundEnemies.size()));
						if (ImGui::Button("Add Air Enemy to Spawn##Act")) {
							EnemySpawnData e; e.position = currentMission_.playerPosition; e.modelPath = "Resources/planeplane.obj"; e.health = 50.0f; e.aiType = AIType::ChaseAttack;
							action.spawnEnemies.push_back(e);
						}
						break;
					case TriggerActionType::ActivateTrigger: ImGui::InputInt("Trigger Index to Activate##Act", &action.activateTriggerIndex); break;
					default: break;
					}

					if (ImGui::Button("Delete Action##Act")) {
						trig.actions.erase(trig.actions.begin() + a);
						selectedActionIndex_ = -1;
						ImGui::PopID();
						break;
					}
				}
				ImGui::PopID();
			}

			ImGui::Separator();
			if (ImGui::Button("Delete Trigger##Trig")) {
				currentMission_.triggers.erase(currentMission_.triggers.begin() + selectedTriggerIndex_);
				selectedTriggerIndex_ = -1;
				selectedActionIndex_ = -1;
			}
		}
	}
#endif
}
