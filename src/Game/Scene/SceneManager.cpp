#include "SceneManager.h"
// Removed concrete scene includes
#include "IScene.h"

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
}

SceneManager::~SceneManager() {
}

void SceneManager::Update() {

	// 現在のシーンの更新
	if (currentScene != nullptr) {
		currentScene->Update();
	}

#ifdef USE_IMGUI
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Scene")) {
			if (ImGui::MenuItem("Title")) {
				IScene::SetSceneID(SCENE::TITLE);
			}
			if (ImGui::MenuItem("Stage")) {
				IScene::SetSceneID(SCENE::STAGE);
			}
			if (ImGui::MenuItem("Result")) {
				IScene::SetSceneID(SCENE::RESULT);
			}
			if (ImGui::MenuItem("Clear")) {
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

		// 新しいシーンを生成する前に、現在のシーンの終了処理を呼ぶ
		if (currentScene != nullptr) {
			currentScene->Finalize();
		}

		currentScene = sceneFactory_->CreateScene(nextSceneID);

		if (currentScene != nullptr) {
			currentScene->Initialize();
		}

		currentSceneID = nextSceneID;
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