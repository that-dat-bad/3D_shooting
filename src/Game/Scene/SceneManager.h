#pragma once
#include "IScene.h"
#include <memory> 
#include "AbstractSceneFactory.h"

/// <summary>
/// シーンの遷移やライフサイクルを管理するクラス
/// </summary>
class SceneManager {
private:
	std::unique_ptr<IScene> currentScene = nullptr;
	int currentSceneID; // 現在管理しているシーンIDを保持
	std::unique_ptr<AbstractSceneFactory> sceneFactory_; // シーンファクトリ

public:
	/// <summary>コンストラクタ（初期シーンの生成を行う）</summary>
	SceneManager(std::unique_ptr<AbstractSceneFactory> factory);
	~SceneManager();

	/// <summary>
	/// 現在のシーンの更新と、必要に応じたシーン遷移処理を行う
	/// </summary>
	void Update();

	/// <summary>
	/// 現在のシーンの描画処理を行う
	/// </summary>
	void Draw();

	/// <summary>
	/// 現在のシーンのUI描画処理を行う
	/// </summary>
	void DrawUI();
};