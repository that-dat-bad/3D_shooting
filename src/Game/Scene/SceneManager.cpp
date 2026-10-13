#include "SceneManager.h"
// Removed concrete scene includes
#include "IScene.h"
#include "../../engine/io/Input.h"
#include <chrono>
#include <cstdio>
#include <Windows.h>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#include "../../engine/Debug/UIEditorWindow.h"
#endif

SceneManager::SceneManager(std::unique_ptr<AbstractSceneFactory> factory) 
	: sceneFactory_(std::move(factory)) {
	// 初期シーン生成
	currentScene = sceneFactory_->CreateScene(SCENE::TITLE);
	currentScene->Initialize();
	currentSceneID = SCENE::TITLE;
	IScene::SetSceneID(SCENE::TITLE);

	FILE* fp = nullptr;
	if (fopen_s(&fp, "C:/Users/gamer/source/repos/25B_GE_CG/scene_manager.log", "w") == 0 && fp) {
		fprintf(fp, "[SceneManager] Initialized with Scene ID: %d (TITLE)\n", currentSceneID);
		fclose(fp);
	}
}

SceneManager::~SceneManager() {
	if (currentScene != nullptr) {
		currentScene->Finalize();
		currentScene.reset();
	}
	for (auto& [id, scene] : scenePool_) {
		if (scene != nullptr) {
			scene->Finalize();
			scene.reset();
		}
	}
	scenePool_.clear();
}

void SceneManager::SwitchScene(int nextSceneID) {
	auto tStart = std::chrono::high_resolution_clock::now();
	int prevSceneID = currentSceneID;

	if (currentScene != nullptr) {
		currentScene->Finalize();
		// 現在のシーンを破棄せず、次回のためにキャッシュプールに退避（D3D12リソースを保持）
		scenePool_[currentSceneID] = std::move(currentScene);
	}

	// キャッシュプールに次のシーンが存在すれば0ms高速再利用、未生成なら初回のみファクトリ生成
	auto it = scenePool_.find(nextSceneID);
	bool fromCache = false;
	if (it != scenePool_.end() && it->second != nullptr) {
		currentScene = std::move(it->second);
		scenePool_.erase(it);
		fromCache = true;
	} else {
		currentScene = sceneFactory_->CreateScene(nextSceneID);
	}

	if (currentScene != nullptr) {
		currentScene->Initialize();
	}

	currentSceneID = nextSceneID;
	IScene::SetSceneID(nextSceneID);

	auto tEnd = std::chrono::high_resolution_clock::now();
	double elapsedMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	char logBuf[256];
	snprintf(logBuf, sizeof(logBuf), "[SceneManager] Scene Transition: %d -> %d (%s) took %.2f ms\n",
		prevSceneID, nextSceneID, fromCache ? "CACHED" : "NEW", elapsedMs);
	OutputDebugStringA(logBuf);

	FILE* fp = nullptr;
	if (fopen_s(&fp, "C:/Users/gamer/source/repos/25B_GE_CG/scene_manager.log", "a") == 0 && fp) {
		fprintf(fp, "%s", logBuf);
		fclose(fp);
	}
}

void SceneManager::Update() {

	// 現在のシーンの更新
	if (currentScene != nullptr) {
		currentScene->Update();
	}

#ifdef USE_IMGUI
	// ファンクションキーによるデバッグシーン切り替えショートカット
	if (Input::GetInstance()->TriggerKey(DIK_F5)) {
		IScene::SetSceneID(SCENE::TITLE);
	} else if (Input::GetInstance()->TriggerKey(DIK_F6)) {
		IScene::SetSceneID(SCENE::STAGE);
	} else if (Input::GetInstance()->TriggerKey(DIK_F7)) {
		IScene::SetSceneID(SCENE::GAMEOVER);
	} else if (Input::GetInstance()->TriggerKey(DIK_F8)) {
		IScene::SetSceneID(SCENE::CLEAR);
	}

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Scene")) {
			if (ImGui::MenuItem("Title (F5)")) {
				IScene::SetSceneID(SCENE::TITLE);
			}
			if (ImGui::MenuItem("Stage (F6)")) {
				IScene::SetSceneID(SCENE::STAGE);
			}
			if (ImGui::MenuItem("GameOver (F7)")) {
				IScene::SetSceneID(SCENE::GAMEOVER);
			}
			if (ImGui::MenuItem("Clear (F8)")) {
				IScene::SetSceneID(SCENE::CLEAR);
			}
			if (ImGui::MenuItem("Debug")) {
				IScene::SetSceneID(SCENE::DEBUG);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Tools")) {
			bool isEditorVisible = UIEditorWindow::GetInstance()->IsVisible();
			if (ImGui::MenuItem("Native UI Editor (F1)", nullptr, isEditorVisible)) {
				UIEditorWindow::GetInstance()->Toggle();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
#endif

	// シーン切り替え判定（static変数が変更されたかチェック）
	int nextSceneID = currentScene->GetSceneID();
	if (nextSceneID != currentSceneID) {
		SwitchScene(nextSceneID);
	}
}

void SceneManager::Draw() {
	if (currentScene != nullptr) {
		currentScene->Draw();
	}
}

void SceneManager::DrawUI() {
	if (currentScene != nullptr) {
		currentScene->DrawUI();
	}
}

void SceneManager::PrewarmAllScenes() {
	// ゲーム起動時に全シーン（STAGE, GAMEOVER, CLEAR）を先行生成・GPUアセット初期化してプールに保管
	// 各シーンの PrewarmAssets() のみを呼び出すため、実行時状態や static sceneID は一切汚染されない
	// これにより、ゲーム起動時の初期シーンは100%タイトル画面となり、プレイ中の全シーン移行（Title⇄Stage⇄GameOver⇄Clear）は0msシームレスになる
	auto tStart = std::chrono::high_resolution_clock::now();
	std::vector<int> preloadIDs = { SCENE::STAGE, SCENE::GAMEOVER, SCENE::CLEAR };
	for (int id : preloadIDs) {
		if (scenePool_.find(id) == scenePool_.end() && id != currentSceneID) {
			auto sStart = std::chrono::high_resolution_clock::now();
			auto scene = sceneFactory_->CreateScene(id);
			if (scene) {
				scene->PrewarmAssets();
				scenePool_[id] = std::move(scene);
			}
			auto sEnd = std::chrono::high_resolution_clock::now();
			double ms = std::chrono::duration<double, std::milli>(sEnd - sStart).count();
			char buf[128];
			snprintf(buf, sizeof(buf), "[Prewarm] Scene %d prewarmed in %.2f ms\n", id, ms);
			OutputDebugStringA(buf);
		}
	}

	// 初期シーンが確実にタイトル画面であることを100%保証
	IScene::SetSceneID(SCENE::TITLE);

	auto tEnd = std::chrono::high_resolution_clock::now();
	double totalMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	char buf[128];
	snprintf(buf, sizeof(buf), "[Prewarm] Total scenes prewarmed in %.2f ms. Current scene confirmed as TITLE (0).\n", totalMs);
	OutputDebugStringA(buf);

	FILE* fp = nullptr;
	if (fopen_s(&fp, "C:/Users/gamer/source/repos/25B_GE_CG/scene_manager.log", "a") == 0 && fp) {
		fprintf(fp, "%s", buf);
		fclose(fp);
	}
}