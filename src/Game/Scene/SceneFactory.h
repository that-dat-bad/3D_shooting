#pragma once
#include "AbstractSceneFactory.h"

/// <summary>
/// ゲーム固有のシーンを生成する具象ファクトリ
/// </summary>
class SceneFactory : public AbstractSceneFactory {
public:
	std::unique_ptr<IScene> CreateScene(int sceneID) override;
};
