#pragma once
#include "IScene.h"
#include <memory>
#include "../System/MissionManager.h"
#include "../System/MissionMapView.h"
#include "../System/MissionMapView.h"

class MissionEditorScene : public IScene {
public:

	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawUI() override;
	void Finalize() override;

private:
	void DrawMissionEditor();
	void DrawObjectiveEditor();
	void DrawTriggerEditor();

private:
	// ミッション編集用データ
	MissionData currentMission_;

	// ファイルI/O状態
	char tempMissionName_[128] = "";
	char tempMissionDesc_[256] = "";
	char tempSaveFileName_[128] = "mission1";
	int selectedMissionIndex_ = 0;

	// UI状態
	int selectedEnemyIndex_ = -1;
	int selectedGroundEnemyIndex_ = -1;
	int selectedObjectiveIndex_ = -1;
	int selectedTriggerIndex_ = -1;
	int selectedActionIndex_ = -1;

	// War Thunder スタイル 2D Tactical Map
	MissionMapView missionMapView_;
};
