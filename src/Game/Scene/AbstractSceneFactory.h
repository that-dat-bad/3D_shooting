#pragma once
#include <memory>
#include "IScene.h"

/// <summary>
/// シーン生成を抽象化するファクトリインターフェース
/// </summary>
class AbstractSceneFactory {
public:
	virtual ~AbstractSceneFactory() = default;

	/// <summary>
	/// シーンを生成する
	/// </summary>
	/// <param name="sceneID">生成するシーンのID</param>
	/// <returns>生成されたシーンのユニークポインタ</returns>
	virtual std::unique_ptr<IScene> CreateScene(int sceneID) = 0;
};
