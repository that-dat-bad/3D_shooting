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
