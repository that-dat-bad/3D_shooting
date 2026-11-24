#include "SceneFactory.h"
#include "TitleScene.h"
#include "StageScene.h"
#include "ClearScene.h"
#include "GameOverScene.h"
#ifdef USE_IMGUI
#include "DebugScene.h"
#include "MissionEditorScene.h"
#include "EffectEditorScene.h"
#include "AircraftEditorScene.h"
#endif

std::unique_ptr<IScene> SceneFactory::CreateScene(int sceneID) {
	std::unique_ptr<IScene> newScene = nullptr;

	switch (sceneID) {
	case SCENE::TITLE:
		newScene = std::make_unique<TitleScene>();
		break;
	case SCENE::STAGE:
		newScene = std::make_unique<StageScene>();
		break;
	case SCENE::CLEAR:
		newScene = std::make_unique<ClearScene>();
		break;
	case SCENE::GAMEOVER:
		newScene = std::make_unique<GameOverScene>();
		break;
#ifdef USE_IMGUI
	case SCENE::DEBUG:
		newScene = std::make_unique<DebugScene>();
		break;
	case SCENE::MISSION_EDITOR:
		newScene = std::make_unique<MissionEditorScene>();
		break;
	case SCENE::EFFECT_EDITOR:
		newScene = std::make_unique<EffectEditorScene>();
		break;
	case SCENE::AIRCRAFT_EDITOR:
		newScene = std::make_unique<AircraftEditorScene>();
		break;
#endif
	}

	return newScene;
}
