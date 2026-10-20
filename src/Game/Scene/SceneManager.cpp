#include "SceneManager.h"
#include "IScene.h"
#include "../../engine/io/Input.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <Windows.h>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#include "../../engine/Debug/UIEditorWindow.h"
#endif

const char* SceneManager::GetSceneName(int sceneID) {
	switch (sceneID) {
	case SCENE::TITLE: return "TITLE";
	case SCENE::STAGE: return "STAGE";
	case SCENE::CLEAR: return "CLEAR";
	case SCENE::GAMEOVER: return "GAMEOVER";
	case SCENE::DEBUG: return "DEBUG";
	case SCENE::MISSION_EDITOR: return "MISSION_EDITOR";
	default: return "UNKNOWN";
	}
}

void SceneManager::AppendLog(const std::string& logText) {
#ifdef _DEBUG
	FILE* fp = nullptr;
	if (fopen_s(&fp, "scene_transition.log", "a") == 0 && fp) {
		fputs(logText.c_str(), fp);
		fclose(fp);
	}
	OutputDebugStringA(logText.c_str());
#endif
}

SceneManager::SceneManager(std::unique_ptr<AbstractSceneFactory> factory) 
	: sceneFactory_(std::move(factory)) {
	
#ifdef _DEBUG
	// ログファイルの初期化（ヘッダー書き込み：Debugビルドのみ）
	FILE* fp = nullptr;
	if (fopen_s(&fp, "scene_transition.log", "w") == 0 && fp) {
		std::time_t now = std::time(nullptr);
		std::tm localTm{};
		localtime_s(&localTm, &now);
		char timeBuf[64];
		std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &localTm);

		fprintf(fp, "======================================================================\n");
		fprintf(fp, " Dawn Engine - Scene Transition & Benchmark Profiler Log\n");
		fprintf(fp, " Session Started: %s\n", timeBuf);
		fprintf(fp, "======================================================================\n\n");
		fclose(fp);
	}

	auto tStart = std::chrono::high_resolution_clock::now();
#endif

	// 初期シーン生成
	currentScene = sceneFactory_->CreateScene(SCENE::TITLE);
	currentScene->Initialize();
	currentSceneID = SCENE::TITLE;
	IScene::SetSceneID(SCENE::TITLE);

#ifdef _DEBUG
	auto tEnd = std::chrono::high_resolution_clock::now();
	double initMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

	char buf[256];
	snprintf(buf, sizeof(buf), "[SceneManager] Initial scene TITLE (0) initialized in %.2f ms (%.4f s)\n\n", initMs, initMs / 1000.0);
	AppendLog(buf);
#endif
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
#ifdef _DEBUG
	auto tTotalStart = std::chrono::high_resolution_clock::now();
#endif
	int prevSceneID = currentSceneID;

	// 1. 直前のシーンの Finalize（状態退避）
#ifdef _DEBUG
	auto tFinStart = std::chrono::high_resolution_clock::now();
#endif
	if (currentScene != nullptr) {
		currentScene->Finalize();
		// 現在のシーンを破棄せず、次回のためにキャッシュプールに退避（D3D12リソースを保持）
		scenePool_[currentSceneID] = std::move(currentScene);
	}
#ifdef _DEBUG
	auto tFinEnd = std::chrono::high_resolution_clock::now();
	double finalizeMs = std::chrono::duration<double, std::milli>(tFinEnd - tFinStart).count();

	// 2. キャッシュプールから取り出し、未キャッシュなら初回生成
	auto tRetStart = std::chrono::high_resolution_clock::now();
#endif
	auto it = scenePool_.find(nextSceneID);
	bool fromCache = false;
	if (it != scenePool_.end() && it->second != nullptr) {
		currentScene = std::move(it->second);
		scenePool_.erase(it);
		fromCache = true;
	} else {
		currentScene = sceneFactory_->CreateScene(nextSceneID);
	}
#ifdef _DEBUG
	auto tRetEnd = std::chrono::high_resolution_clock::now();
	double retrieveMs = std::chrono::duration<double, std::milli>(tRetEnd - tRetStart).count();

	// 3. 次のシーンの Initialize
	auto tInitStart = std::chrono::high_resolution_clock::now();
#endif
	if (currentScene != nullptr) {
		currentScene->Initialize();
	}
#ifdef _DEBUG
	auto tInitEnd = std::chrono::high_resolution_clock::now();
	double initializeMs = std::chrono::duration<double, std::milli>(tInitEnd - tInitStart).count();
#endif

	currentSceneID = nextSceneID;
	IScene::SetSceneID(nextSceneID);

#ifdef _DEBUG
	auto tTotalEnd = std::chrono::high_resolution_clock::now();
	double totalMs = std::chrono::duration<double, std::milli>(tTotalEnd - tTotalStart).count();

	// タイムスタンプ取得
	std::time_t now = std::time(nullptr);
	std::tm localTm{};
	localtime_s(&localTm, &now);
	char timeBuf[32];
	std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &localTm);

	// 記録作成
	SceneTransitionRecord rec;
	rec.fromSceneID = prevSceneID;
	rec.toSceneID = nextSceneID;
	rec.isCached = fromCache;
	rec.finalizeTimeMs = finalizeMs;
	rec.retrieveTimeMs = retrieveMs;
	rec.initializeTimeMs = initializeMs;
	rec.totalTimeMs = totalMs;
	rec.timestamp = timeBuf;

	lastTransition_ = rec;
	hasTransitionOccurred_ = true;
	transitionHistory_.push_back(rec);
	if (transitionHistory_.size() > 50) {
		transitionHistory_.erase(transitionHistory_.begin());
	}
	overlayBannerTimer_ = 5.0f; // 5秒間画面HUDに強調表示

	// ログファイルとデバッグ出力へ詳細記録
	char logBuf[512];
	snprintf(logBuf, sizeof(logBuf),
		"[%s] [Scene Transition] %s (%d) -> %s (%d) [%s]\n"
		"  * Finalize   (Prev Scene) : %8.2f ms (%7.4f s)\n"
		"  * Retrieve   (Pool/Alloc) : %8.2f ms (%7.4f s)\n"
		"  * Initialize (Next Scene) : %8.2f ms (%7.4f s)\n"
		"  => TOTAL TRANSITION TIME  : %8.2f ms (%7.4f s)\n\n",
		timeBuf,
		GetSceneName(prevSceneID), prevSceneID,
		GetSceneName(nextSceneID), nextSceneID,
		fromCache ? "CACHED (Pool Reused)" : "NEW (Allocated)",
		finalizeMs, finalizeMs / 1000.0,
		retrieveMs, retrieveMs / 1000.0,
		initializeMs, initializeMs / 1000.0,
		totalMs, totalMs / 1000.0
	);

	AppendLog(logBuf);
#endif
}

void SceneManager::PrewarmAllScenes() {
#ifdef _DEBUG
	auto tStart = std::chrono::high_resolution_clock::now();
#endif
	std::vector<int> preloadIDs = { SCENE::STAGE, SCENE::GAMEOVER, SCENE::CLEAR };
#ifdef _DEBUG
	prewarmRecords_.clear();

	std::string prewarmLog = 
		"\n======================================================================\n"
		"[Boot Prewarm] Initializing All Scenes in Background for 0ms Transition\n"
		"======================================================================\n";
#endif

	for (int id : preloadIDs) {
		if (scenePool_.find(id) == scenePool_.end() && id != currentSceneID) {
#ifdef _DEBUG
			auto sStart = std::chrono::high_resolution_clock::now();
#endif
			auto scene = sceneFactory_->CreateScene(id);
			if (scene) {
				scene->PrewarmAssets();
				scenePool_[id] = std::move(scene);
			}
#ifdef _DEBUG
			auto sEnd = std::chrono::high_resolution_clock::now();
			double ms = std::chrono::duration<double, std::milli>(sEnd - sStart).count();

			ScenePrewarmRecord prec;
			prec.sceneID = id;
			prec.timeMs = ms;
			prewarmRecords_.push_back(prec);

			char buf[128];
			snprintf(buf, sizeof(buf), "  * Prewarmed Scene %-10s (ID: %d): %8.2f ms (%7.4f s)\n",
				GetSceneName(id), id, ms, ms / 1000.0);
			prewarmLog += buf;
#endif
		}
	}

	// 初期シーンが確実にタイトル画面であることを保証
	IScene::SetSceneID(SCENE::TITLE);

#ifdef _DEBUG
	auto tEnd = std::chrono::high_resolution_clock::now();
	totalPrewarmTimeMs_ = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

	char totalBuf[256];
	snprintf(totalBuf, sizeof(totalBuf), 
		"  => Total Boot Prewarm Time: %8.2f ms (%7.4f s)\n"
		"======================================================================\n\n",
		totalPrewarmTimeMs_, totalPrewarmTimeMs_ / 1000.0);
	prewarmLog += totalBuf;

	AppendLog(prewarmLog);
#endif
}

void SceneManager::Update() {
	if (overlayBannerTimer_ > 0.0f) {
		overlayBannerTimer_ -= 1.0f / 60.0f;
	}

	// 現在のシーンの更新
	if (currentScene != nullptr) {
		currentScene->Update();
	}

#ifdef USE_IMGUI
	// F2キーでプロファイラウィンドウ開閉
	if (Input::GetInstance()->TriggerKey(DIK_F2)) {
		ToggleProfilerWindow();
	}
#endif

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
			if (ImGui::MenuItem("Title (F5)", nullptr, currentSceneID == SCENE::TITLE)) {
				IScene::SetSceneID(SCENE::TITLE);
			}
			if (ImGui::MenuItem("Stage (F6)", nullptr, currentSceneID == SCENE::STAGE)) {
				IScene::SetSceneID(SCENE::STAGE);
			}
			if (ImGui::MenuItem("GameOver (F7)", nullptr, currentSceneID == SCENE::GAMEOVER)) {
				IScene::SetSceneID(SCENE::GAMEOVER);
			}
			if (ImGui::MenuItem("Clear (F8)", nullptr, currentSceneID == SCENE::CLEAR)) {
				IScene::SetSceneID(SCENE::CLEAR);
			}
			if (ImGui::MenuItem("Debug", nullptr, currentSceneID == SCENE::DEBUG)) {
				IScene::SetSceneID(SCENE::DEBUG);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Scene Profiler (F2)", nullptr, showProfilerWindow_)) {
				ToggleProfilerWindow();
			}
			if (ImGui::MenuItem("Show Transition Banner (HUD)", nullptr, &showOverlayBanner_)) {
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Tools")) {
			bool isEditorVisible = UIEditorWindow::GetInstance()->IsVisible();
			if (ImGui::MenuItem("Native UI Editor (F1)", nullptr, isEditorVisible)) {
				UIEditorWindow::GetInstance()->Toggle();
			}
			if (ImGui::MenuItem("Scene Profiler (F2)", nullptr, showProfilerWindow_)) {
				ToggleProfilerWindow();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// プロファイラウィンドウの描画
	DrawProfilerImGui();
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

#ifdef _DEBUG
	// 画面上部HUDオーバーレイの描画（Debugビルドのみ）
	DrawOverlayHUD();
#endif
}

void SceneManager::DrawOverlayHUD() {
#ifdef _DEBUG
	if (!showOverlayBanner_ && overlayBannerTimer_ <= 0.0f) {
		return;
	}

	TextRenderer* tr = TextRenderer::GetInstance();
	if (!tr) return;

	if (hasTransitionOccurred_) {
		char line1[256];
		snprintf(line1, sizeof(line1), "[TRANSITION BENCHMARK] %s -> %s | Total: %.2f ms (%.4f s) [%s]",
			GetSceneName(lastTransition_.fromSceneID),
			GetSceneName(lastTransition_.toSceneID),
			lastTransition_.totalTimeMs,
			lastTransition_.totalTimeMs / 1000.0,
			lastTransition_.isCached ? "CACHED" : "NEW");

		// 16.6ms未満ならグリーン、それ以上ならアンバー
		Vector4 c1 = (lastTransition_.totalTimeMs <= 16.67) 
			? Vector4{0.2f, 1.0f, 0.4f, 0.95f} 
			: Vector4{1.0f, 0.85f, 0.2f, 0.95f};

		tr->Print("HackGen", line1, 16.0f, 16.0f, 16.0f, c1);

		char line2[256];
		snprintf(line2, sizeof(line2), "  Breakdown: Finalize: %.2f ms | Retrieve: %.2f ms | Initialize: %.2f ms",
			lastTransition_.finalizeTimeMs,
			lastTransition_.retrieveTimeMs,
			lastTransition_.initializeTimeMs);
		tr->Print("HackGen", line2, 16.0f, 36.0f, 13.0f, { 0.85f, 0.9f, 0.95f, 0.85f });
	} else if (totalPrewarmTimeMs_ > 0.0) {
		char line1[256];
		snprintf(line1, sizeof(line1), "[BOOT PREWARM] Ready: %d scenes prewarmed in %.2f ms (%.4f s)",
			static_cast<int>(prewarmRecords_.size()),
			totalPrewarmTimeMs_,
			totalPrewarmTimeMs_ / 1000.0);
		tr->Print("HackGen", line1, 16.0f, 16.0f, 16.0f, {0.3f, 0.9f, 1.0f, 0.95f});
	}
#endif
}

void SceneManager::DrawProfilerImGui() {
#ifdef USE_IMGUI
	if (!showProfilerWindow_) return;

	ImGui::SetNextWindowSize(ImVec2(560, 480), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Scene Transition Profiler & Stats (F2)", &showProfilerWindow_)) {
		ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "/// SCENE TRANSITION BENCHMARK ///");
		ImGui::Text("Current Active Scene: %s (ID: %d)", GetSceneName(currentSceneID), currentSceneID);
		ImGui::Checkbox("Show HUD Overlay Banner on Screen", &showOverlayBanner_);
		ImGui::Separator();

		// 直前の遷移ステータス
		if (hasTransitionOccurred_) {
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "LAST TRANSITION RESULT:");
			ImGui::Indent();
			ImGui::Text("Transition: %s -> %s", 
				GetSceneName(lastTransition_.fromSceneID), 
				GetSceneName(lastTransition_.toSceneID));
			ImGui::Text("Mode      : %s", lastTransition_.isCached ? "CACHED (Pool Reused)" : "NEW (Factory Created)");

			// ミリ秒と秒数を大きく表示
			ImVec4 timeCol = (lastTransition_.totalTimeMs <= 16.67) 
				? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) 
				: (lastTransition_.totalTimeMs <= 100.0 ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f));

			ImGui::TextColored(timeCol, "TOTAL TIME: %.2f ms  (%.4f sec)", 
				lastTransition_.totalTimeMs, lastTransition_.totalTimeMs / 1000.0);

			ImGui::Text("Phases Breakdown:");
			ImGui::BulletText("Finalize   (Prev scene cleanup/pool save) : %.2f ms (%.4f s)", 
				lastTransition_.finalizeTimeMs, lastTransition_.finalizeTimeMs / 1000.0);
			ImGui::BulletText("Retrieve   (Pool find / Allocation)       : %.2f ms (%.4f s)", 
				lastTransition_.retrieveTimeMs, lastTransition_.retrieveTimeMs / 1000.0);
			ImGui::BulletText("Initialize (Next scene state setup)        : %.2f ms (%.4f s)", 
				lastTransition_.initializeTimeMs, lastTransition_.initializeTimeMs / 1000.0);
			ImGui::Unindent();
		} else {
			ImGui::TextDisabled("No scene transitions have occurred yet during this session.");
		}

		ImGui::Separator();

		// 起動時ウォームアップ情報
		if (ImGui::CollapsingHeader("Boot-up Prewarm Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Total Prewarm Time: %.2f ms (%.4f sec)", totalPrewarmTimeMs_, totalPrewarmTimeMs_ / 1000.0);
			for (const auto& prec : prewarmRecords_) {
				ImGui::BulletText("Scene %-10s : %.2f ms (%.4f s)", 
					GetSceneName(prec.sceneID), prec.timeMs, prec.timeMs / 1000.0);
			}
		}

		// キャッシュプール状況
		if (ImGui::CollapsingHeader("Scene Pool Cache Status", ImGuiTreeNodeFlags_DefaultOpen)) {
			std::vector<int> checkIDs = { SCENE::TITLE, SCENE::STAGE, SCENE::CLEAR, SCENE::GAMEOVER };
			for (int id : checkIDs) {
				bool isCurrent = (id == currentSceneID);
				bool inPool = (scenePool_.find(id) != scenePool_.end() && scenePool_[id] != nullptr);
				if (isCurrent) {
					ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[ACTIVE]  Scene %-10s", GetSceneName(id));
				} else if (inPool) {
					ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[POOLED]  Scene %-10s (Cached in memory)", GetSceneName(id));
				} else {
					ImGui::TextDisabled("[UNCACHED] Scene %-10s", GetSceneName(id));
				}
			}
		}

		// クイック遷移テストボタン
		ImGui::Separator();
		ImGui::Text("Test Quick Transition:");
		if (ImGui::Button("Title (F5)")) { IScene::SetSceneID(SCENE::TITLE); }
		ImGui::SameLine();
		if (ImGui::Button("Stage (F6)")) { IScene::SetSceneID(SCENE::STAGE); }
		ImGui::SameLine();
		if (ImGui::Button("GameOver (F7)")) { IScene::SetSceneID(SCENE::GAMEOVER); }
		ImGui::SameLine();
		if (ImGui::Button("Clear (F8)")) { IScene::SetSceneID(SCENE::CLEAR); }

		// 遷移履歴テーブル
		if (ImGui::CollapsingHeader("Transition History (Log)", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::BeginTable("HistoryTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 140))) {
				ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 60);
				ImGui::TableSetupColumn("From -> To", ImGuiTableColumnFlags_WidthFixed, 110);
				ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed, 65);
				ImGui::TableSetupColumn("Fin (ms)", ImGuiTableColumnFlags_WidthFixed, 60);
				ImGui::TableSetupColumn("Init (ms)", ImGuiTableColumnFlags_WidthFixed, 60);
				ImGui::TableSetupColumn("Total (ms)", ImGuiTableColumnFlags_WidthFixed, 70);
				ImGui::TableHeadersRow();

				for (int i = static_cast<int>(transitionHistory_.size()) - 1; i >= 0; --i) {
					const auto& h = transitionHistory_[i];
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", h.timestamp.c_str());
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%s->%s", GetSceneName(h.fromSceneID), GetSceneName(h.toSceneID));
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%s", h.isCached ? "CACHE" : "NEW");
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%.1f", h.finalizeTimeMs);
					ImGui::TableSetColumnIndex(4);
					ImGui::Text("%.1f", h.initializeTimeMs);
					ImGui::TableSetColumnIndex(5);
					if (h.totalTimeMs <= 16.67) {
						ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%.2f", h.totalTimeMs);
					} else {
						ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%.2f", h.totalTimeMs);
					}
				}
				ImGui::EndTable();
			}
		}

		ImGui::Separator();
		ImGui::TextDisabled("Log file: ./scene_transition.log");
	}
	ImGui::End();
#endif
}
