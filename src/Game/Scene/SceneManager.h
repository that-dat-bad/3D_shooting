#pragma once
#include "IScene.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include "AbstractSceneFactory.h"

/// <summary>
/// シーン遷移の計測データ記録構造体
/// </summary>
struct SceneTransitionRecord {
	int fromSceneID = -1;
	int toSceneID = -1;
	bool isCached = false;
	double finalizeTimeMs = 0.0;
	double retrieveTimeMs = 0.0;
	double initializeTimeMs = 0.0;
	double totalTimeMs = 0.0;
	std::string timestamp;
};

/// <summary>
/// 起動時事前ウォームアップ記録構造体
/// </summary>
struct ScenePrewarmRecord {
	int sceneID = -1;
	double timeMs = 0.0;
};

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

	// --- シーン遷移プロファイリング情報 ---
	SceneTransitionRecord lastTransition_;
	bool hasTransitionOccurred_ = false;
	std::vector<SceneTransitionRecord> transitionHistory_;
	std::vector<ScenePrewarmRecord> prewarmRecords_;
	double totalPrewarmTimeMs_ = 0.0;
	float overlayBannerTimer_ = 0.0f; // 画面上部HUDバナー表示用タイマー
	bool showProfilerWindow_ = false; // ImGui プロファイラウィンドウ表示フラグ
	bool showOverlayBanner_ = true;   // 画面HUD表示フラグ

	/// <summary>シーン切り替え処理（プール再利用または新規生成）</summary>
	void SwitchScene(int nextSceneID);

	/// <summary>ログファイルへの追記とデバッグ出力</summary>
	void AppendLog(const std::string& logText);

	/// <summary>プロファイラ用ImGui描画</summary>
	void DrawProfilerImGui();

	/// <summary>画面HUDオーバーレイ描画</summary>
	void DrawOverlayHUD();

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

	/// <summary>シーンIDに対応する名称文字列を取得</summary>
	static const char* GetSceneName(int sceneID);

	/// <summary>直近の遷移記録を取得</summary>
	const SceneTransitionRecord& GetLastTransition() const { return lastTransition_; }

	/// <summary>遷移履歴を取得</summary>
	const std::vector<SceneTransitionRecord>& GetTransitionHistory() const { return transitionHistory_; }

	/// <summary>プロファイラウィンドウの表示切替</summary>
	void ToggleProfilerWindow() { showProfilerWindow_ = !showProfilerWindow_; }
};