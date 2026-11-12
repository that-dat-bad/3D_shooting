#include "SceneFactory.h"
#include "DebugScene.h"

std::unique_ptr<IScene> SceneFactory::CreateScene(int sceneID) {
	(void)sceneID;
	return std::make_unique<DebugScene>();
}
