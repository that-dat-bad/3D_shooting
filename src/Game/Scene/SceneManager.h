#pragma once
#include "IScene.h"
#include <memory>
#include <unordered_map>
#include "AbstractSceneFactory.h"

/// <summary>
/// シーンの遷移やライフサイクルを管理するクラス（シーンプーリング対応で0msシームレス遷移）
/// </summary>
class SceneManager {
private:
	std::unique_ptr<IScene> currentScene = nullptr;
	int currentSceneID; // 現在管理しているシーンIDを保持
	std::unique_ptr<AbstractSceneFactory> sceneFactory_; // シーンファクトリ

	// シーンキャッシュプール（生成済みシーンを保持し、再生成負荷を根絶してミリ秒切り替えを実現）
	std::unordered_map<int, std::unique_ptr<IScene>> scenePool_;

	/// <summary>シーン切り替え処理（プール再利用または新規生成）</summary>
	void SwitchScene(int nextSceneID);

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

	/// <summary>
	/// 全シーン（STAGE, GAMEOVER, CLEAR）を事前生成・初期化してプールに保管（0ms完全シームレス遷移）
	/// </summary>
	void PrewarmAllScenes();
};