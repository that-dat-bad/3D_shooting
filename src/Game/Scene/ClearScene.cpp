#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "ClearScene.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../Environment/TerrainChunkManager.h"
#include "../System/GameResult.h"
#include "../System/AsyncPreloadManager.h"
#include "../../engine/Graphics/UI/UITextRegistry.h"
#include "../../io/Input.h"
#include "WinApp.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include "../../engine/base/Math/Easing.h"

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

// 画面サイズ定数
static constexpr float kScreenWidth = static_cast<float>(WinApp::kClientWidth);
static constexpr float kScreenHeight = static_cast<float>(WinApp::kClientHeight);

ClearScene::ClearScene() {
	sceneID = SCENE::CLEAR;
}

ClearScene::~ClearScene() {
}

void ClearScene::PrewarmAssets() {
	if (!isUIInitialized_) {
#ifdef _DEBUG
		auto tStart = std::chrono::high_resolution_clock::now();
#endif
		SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

		// 必須テクスチャのロード
		TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");

		// ============================
		// 疑似OS環境（タクティカルシアン仕様：TacticalCyan）および背景パネル初期化
		// ============================
		InitializeOS(spriteCommon, PseudoOS::Theme::TacticalCyan,
			"DAWN_OS TACTICAL OPERATION DEBRIEF [v4.12.0]",
			"MISSION DIRECTIVE // STATUS: OBJECTIVE ACCOMPLISHED",
			"DAWN_OS : MISSION DEBRIEF",
			{ 0.01f, 0.02f, 0.05f, 0.95f });

		// スタートメニュー項目の登録
		std::vector<PseudoOS::StartMenuItem> smItems = {
			{ "[01] REPLAY COMBAT SORTIE", [this]() { replayButton_.Click(); }, false },
			{ "[02] RETURN TO ROOT OS (TITLE)", [this]() { titleButton_.Click(); }, false },
			{ "[03] RESET WINDOW POSITIONS", [this]() {
				pseudoOS_.ResetWindowPositions();
			}, false },
			{ "[04] SYSTEM LOGOUT", []() { ::PostQuitMessage(0); }, true }
		};
		pseudoOS_.SetStartMenuItems(smItems);

		pseudoOS_.ClearWindows();

		// Window 0: メインデブリーフィングコンソール（右側配置・左側に3D軌跡と等高線を広く見せる）
		PseudoOS::Window win0;
		win0.id = 0;
		win0.title = "DEBRIEF : OPERATION DAWN [SUCCESS]";
		win0.pos = { 700.0f, 44.0f };
		win0.size = { 540.0f, 632.0f };
		win0.normalPos = win0.pos;
		win0.normalSize = win0.size;
		win0.titleBarHeight = 34.0f;
		win0.isOpen = true;
		win0.isMinimized = false;
		win0.animScale = 0.0f;
		win0.onDrawContent = [this](const PseudoOS::Window& win, PseudoOS* os) {
			DrawDebriefWindow(win);
		};
		pseudoOS_.AddWindow(win0);

		// メイン操作ウィンドウ（DEBRIEF: win0）を初期状態で最前面＆FOCUSに設定
		pseudoOS_.BringWindowToFront(0);

		// ============================
		// 「MISSION COMPLETE」テキスト
		// ============================
		missionCompleteText_.Initialize("HackGen", "MISSION COMPLETE // DEBRIEF", 26.0f);
		missionCompleteText_.SetAnchorPoint({ 0.5f, 0.0f });
		missionCompleteText_.SetColor({ 0.25f, 0.95f, 0.85f, 1.0f });

		// ============================
		// リザルト情報テキスト
		// ============================
		float fontSize = 18.0f;
		Vector4 labelColor = { 0.55f, 0.85f, 0.95f, 0.95f };
		Vector4 valueColor = { 0.95f, 1.0f, 1.0f, 1.0f };

		// 撃破数
		enemiesDestroyedLabel_.Initialize("HackGen", "ENEMIES DESTROYED     :", fontSize);
		enemiesDestroyedLabel_.SetColor(labelColor);

		enemiesDestroyedValue_.Initialize("HackGen", "0 UNITS", 22.0f);
		enemiesDestroyedValue_.SetAnchorPoint({ 1.0f, 0.0f });
		enemiesDestroyedValue_.SetColor(valueColor);

		// クリアタイム
		clearTimeLabel_.Initialize("HackGen", "TOTAL SORTIE TIME     :", fontSize);
		clearTimeLabel_.SetColor(labelColor);

		clearTimeValue_.Initialize("HackGen", "00:00", 22.0f);
		clearTimeValue_.SetAnchorPoint({ 1.0f, 0.0f });
		clearTimeValue_.SetColor(valueColor);

		// 残りHP
		hpRemainingLabel_.Initialize("HackGen", "SURVIVING INTEGRITY   :", fontSize);
		hpRemainingLabel_.SetColor(labelColor);

		hpRemainingValue_.Initialize("HackGen", "100 %", 22.0f);
		hpRemainingValue_.SetAnchorPoint({ 1.0f, 0.0f });
		hpRemainingValue_.SetColor({ 0.3f, 1.0f, 0.5f, 1.0f });

		// ============================
		// ボタン配置
		// ============================
		float buttonWidth = 236.0f;
		float buttonHeight = 44.0f;

		replayButton_.Initialize(spriteCommon, "[01] REPLAY MISSION", 18.0f);
		replayButton_.SetLeftAligned(true, 42.0f);
		replayButton_.SetSize({ buttonWidth, buttonHeight });
		replayButton_.SetNormalColor({ 0.03f, 0.14f, 0.20f, 0.88f });
		replayButton_.SetHoverColor({ 0.08f, 0.35f, 0.50f, 0.95f });
		replayButton_.SetSelectedColor({ 0.12f, 0.55f, 0.75f, 0.98f });
		replayButton_.SetTextColor({ 0.40f, 0.90f, 1.00f, 0.95f });
		replayButton_.SetSelectedTextColor({ 0.90f, 1.00f, 1.00f, 1.0f });
		replayButton_.SetOnClick([this]() {
			btnClickFlashTimer_ = 0.18f;
			clickedButtonIdx_ = 0;
			isRewinding_ = true;
			rewindTimer_ = 0.0f;
			const std::string curMissionPath = MissionManager::GetInstance().GetCurrentFilePath();
			if (!curMissionPath.empty()) {
				AsyncPreloadManager::GetInstance()->StartStagePreloadAsync(curMissionPath);
			} else {
				AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();
			}
		});

		titleButton_.Initialize(spriteCommon, "[02] RETURN TO BASE", 18.0f);
		titleButton_.SetLeftAligned(true, 42.0f);
		titleButton_.SetSize({ buttonWidth, buttonHeight });
		titleButton_.SetNormalColor({ 0.03f, 0.14f, 0.20f, 0.88f });
		titleButton_.SetHoverColor({ 0.08f, 0.35f, 0.50f, 0.95f });
		titleButton_.SetSelectedColor({ 0.12f, 0.55f, 0.75f, 0.98f });
		titleButton_.SetTextColor({ 0.40f, 0.90f, 1.00f, 0.95f });
		titleButton_.SetSelectedTextColor({ 0.90f, 1.00f, 1.00f, 1.0f });
		titleButton_.SetOnClick([this]() {
			btnClickFlashTimer_ = 0.18f;
			clickedButtonIdx_ = 1;
			isUploading_ = true;
			uploadTimer_ = 0.0f;
			pseudoOS_.PlayWindowCloseAnimation(0.28f);
		});

		selectionManager_.AddButton(&replayButton_);
		selectionManager_.AddButton(&titleButton_);

		// アップロード用スプライト
		uploadSprite_ = std::make_unique<Sprite>();
		uploadSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");

		isUIInitialized_ = true;

#ifdef _DEBUG
		auto tEnd = std::chrono::high_resolution_clock::now();
		double prewarmMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		char buf[256];
		snprintf(buf, sizeof(buf), "  [ClearScene::PrewarmAssets] Assets prewarmed in %.2f ms\n", prewarmMs);
		OutputDebugStringA(buf);

		FILE* fp = nullptr;
		if (fopen_s(&fp, "scene_transition.log", "a") == 0 && fp) {
			fputs(buf, fp);
			fclose(fp);
		}
#endif
	}
}

void ClearScene::Initialize() {
#ifdef _DEBUG
	auto tStart = std::chrono::high_resolution_clock::now();
#endif
	sceneID = SCENE::CLEAR;
	SetOSCursorVisible(false);
	Input::GetInstance()->UnlockCursor();
	selectionManager_.Clear();
	animTimer_ = 0.0f;
	fadeInAlpha_ = 0.0f;

	const GameResult& result = GameResult::GetInstance();

	PrewarmAssets();

	// ウィンドウ位置リセット＆展開アニメーション開始
	pseudoOS_.ResetWindowPositions();
	pseudoOS_.PlayWindowOpenAnimation(0.35f);
	pseudoOS_.BringWindowToFront(0);

	// 3D等高線マップ & プレイヤー飛行軌跡用カメラの初期化
	trajCenter_ = { 0.0f, 0.0f, 0.0f };
	trajOrbitDist_ = 2800.0f;
	trajDurationSec_ = 0.0f;
	if (!result.flightTrajectory.empty()) {
		Vector3 minP = result.flightTrajectory.front().pos;
		Vector3 maxP = result.flightTrajectory.front().pos;
		for (const auto& s : result.flightTrajectory) {
			minP.x = (std::min)(minP.x, s.pos.x);
			minP.y = (std::min)(minP.y, s.pos.y);
			minP.z = (std::min)(minP.z, s.pos.z);
			maxP.x = (std::max)(maxP.x, s.pos.x);
			maxP.y = (std::max)(maxP.y, s.pos.y);
			maxP.z = (std::max)(maxP.z, s.pos.z);
		}
		trajDurationSec_ = (std::max)(0.0f, result.flightTrajectory.back().timeSec - result.flightTrajectory.front().timeSec);
		const Vector3& endP = result.flightTrajectory.back().pos;
		// 自機最終位置の少し下を注視点にし、自機・軌跡・地表等高線がすべて画面内に収まるようにする
		trajCenter_ = {
			endP.x,
			endP.y - 120.0f,
			endP.z
		};
		float span = (std::max)(maxP.x - minP.x, maxP.z - minP.z);
		trajOrbitDist_ = std::clamp(span * 1.35f, 2200.0f, 5500.0f);
	}
	tacticalCam_.SetFovY(0.55f);
	tacticalCam_.SetNearClip(20.0f);
	tacticalCam_.SetFarClip(80000.0f);
	UpdateTacticalCamera();

	auto* tcm = TerrainChunkManager::GetInstance();
	if (tcm) {
		if (!tcm->IsLoaded()) {
			tcm->Initialize(Object3dCommon::GetInstance(), &tacticalCam_, result.terrainPath);
		}
		tcm->SetCamera(&tacticalCam_);
		tcm->SetDrawDistance(32000.0f);
		tcm->Update(trajCenter_);
	}

	// 最新リザルト値の反映
	char enemyBuf[32];
	snprintf(enemyBuf, sizeof(enemyBuf), "%d UNITS", result.enemiesDestroyed);
	enemiesDestroyedValue_.SetText(enemyBuf);

	int minutes = static_cast<int>(result.clearTimeSeconds) / 60;
	int seconds = static_cast<int>(result.clearTimeSeconds) % 60;
	char timeBuf[32];
	snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", minutes, seconds);
	clearTimeValue_.SetText(timeBuf);

	int hpPercent = 0;
	if (result.playerMaxHP > 0.0f) {
		hpPercent = static_cast<int>((result.playerHPRemaining / result.playerMaxHP) * 100.0f);
	}
	char hpBuf[32];
	snprintf(hpBuf, sizeof(hpBuf), "%d %%", hpPercent);
	hpRemainingValue_.SetText(hpBuf);
	if (hpPercent >= 70) {
		hpRemainingValue_.SetColor({ 0.3f, 1.0f, 0.5f, 1.0f });
	} else if (hpPercent >= 30) {
		hpRemainingValue_.SetColor({ 1.0f, 0.9f, 0.3f, 1.0f });
	} else {
		hpRemainingValue_.SetColor({ 1.0f, 0.3f, 0.3f, 1.0f });
	}

	selectionManager_.Clear();
	selectionManager_.AddButton(&replayButton_);
	selectionManager_.AddButton(&titleButton_);

	isUploading_ = false;
	uploadTimer_ = 0.0f;
	isRewinding_ = false;
	rewindTimer_ = 0.0f;
	btnClickFlashTimer_ = 0.0f;
	clickedButtonIdx_ = -1;
	isAutoDemoActive_ = result.isClearAutoDemo;
	autoDemoTimer_ = 0.0f;
	isDemoUploadPreview_ = false;

	if (auto* w0 = pseudoOS_.GetWindow(HalcyonClearDefaults::kDefaultMainWinId)) {
		w0->title = result.isHalcyonDaysTutorial
			? HalcyonClearDefaults::kTutorialMainWinTitle
			: HalcyonClearDefaults::kDefaultMainWinTitle;
	}
	missionCompleteText_.SetText(result.isHalcyonDaysTutorial
		? HalcyonClearDefaults::kTutorialMissionHeader
		: HalcyonClearDefaults::kDefaultMissionHeader);
	titleButton_.SetLabel(result.isHalcyonDaysTutorial
		? HalcyonClearDefaults::kTutorialReturnBtnLabel
		: HalcyonClearDefaults::kDefaultReturnBtnLabel);
	selectionManager_.SetSelectedIndex(result.isHalcyonDaysTutorial
		? HalcyonClearDefaults::kReturnButtonIndex
		: HalcyonClearDefaults::kReplayButtonIndex);

	UITextRegistry::GetInstance()->Register("Clear_MissionComplete", &missionCompleteText_);
	UITextRegistry::GetInstance()->Register("Clear_EnemiesLabel", &enemiesDestroyedLabel_);
	UITextRegistry::GetInstance()->Register("Clear_EnemiesValue", &enemiesDestroyedValue_);
	UITextRegistry::GetInstance()->Register("Clear_TimeLabel", &clearTimeLabel_);
	UITextRegistry::GetInstance()->Register("Clear_TimeValue", &clearTimeValue_);
	UITextRegistry::GetInstance()->Register("Clear_HPLabel", &hpRemainingLabel_);
	UITextRegistry::GetInstance()->Register("Clear_HPValue", &hpRemainingValue_);
	UITextRegistry::GetInstance()->Register("Clear_BtnReplay", replayButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Clear_BtnTitle", titleButton_.GetLabelText());

#ifdef _DEBUG
	auto tEnd = std::chrono::high_resolution_clock::now();
	double totalMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	char buf[256];
	snprintf(buf, sizeof(buf), "  [ClearScene::Initialize] Initialized in %.2f ms (%.4f s)\n", totalMs, totalMs / 1000.0);
	OutputDebugStringA(buf);

	FILE* fp = nullptr;
	if (fopen_s(&fp, "scene_transition.log", "a") == 0 && fp) {
		fputs(buf, fp);
		fclose(fp);
	}
#endif
}

void ClearScene::Update() {
	SetOSCursorVisible(false);
	float dt = 1.0f / 60.0f;
	animTimer_ += dt;

	if (fadeInAlpha_ < 1.0f) {
		fadeInAlpha_ += 1.5f * dt;
		if (fadeInAlpha_ > 1.0f) fadeInAlpha_ = 1.0f;
	}

	if (btnClickFlashTimer_ > 0.0f) {
		btnClickFlashTimer_ -= dt;
		if (btnClickFlashTimer_ < 0.0f) btnClickFlashTimer_ = 0.0f;
	}

	UpdateTacticalCamera();
	if (auto* tcm = TerrainChunkManager::GetInstance(); tcm && tcm->IsLoaded()) {
		tcm->SetCamera(&tacticalCam_);
		tcm->Update(trajCenter_);
	}

	Input* input = Input::GetInstance();
	if (isAutoDemoActive_ && input) {
		if (input->TriggerMouse(0) ||
		    input->TriggerKey(DIK_SPACE) ||
		    input->TriggerKey(DIK_RETURN) ||
		    input->TriggerKey(DIK_ESCAPE)) {
			isAutoDemoActive_ = false;
			isDemoUploadPreview_ = false;
		}
	}

	// 疑似OSデスクトップ環境更新（入力自動取得＆ウィンドウ開閉アニメーション自動管理）
	pseudoOS_.Update(dt);

	if (isAutoDemoActive_) {
		UpdateAutoDemo(dt);
		if (sceneID != SCENE::CLEAR) {
			return;
		}
	}

	// VHS巻き戻し演出
	if (isRewinding_) {
		rewindTimer_ += dt;
		if (rewindTimer_ >= kRewindDuration) {
			GameResult::GetInstance().isRetry = true;
			sceneID = SCENE::STAGE;
			return;
		}
	}
	// アップロード＆タイトル復帰演出
	else if (isUploading_) {
		uploadTimer_ += dt;
		if (!isDemoUploadPreview_ && uploadTimer_ >= 1.40f) {
			GameResult::GetInstance().titleEntryReason = TitleEntryReason::ReturnFromClear;
			sceneID = SCENE::TITLE;
			return;
		}
	} else if (!isAutoDemoActive_) {
		int prevIndex = selectionManager_.GetSelectedIndex();
		selectionManager_.Update();
		int currIndex = selectionManager_.GetSelectedIndex();

		// キーボード/ゲームパッド操作があった場合、またはメニュー選択時はメインウィンドウ（win0）を最前面フォーカスに維持
		if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_DOWN) ||
		    input->TriggerKey(DIK_LEFT) || input->TriggerKey(DIK_RIGHT) ||
		    input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) ||
		    currIndex != prevIndex) {
			pseudoOS_.BringWindowToFront(0);
		}
	}

	// 中央ウィンドウの現在位置にボタン座標を追従
	auto* w0 = pseudoOS_.GetWindow(0);
	if (w0) {
		float btnY = w0->pos.y + w0->size.y - 70.0f;
		float b1X = w0->pos.x + 24.0f;
		float b2X = w0->pos.x + w0->size.x - 260.0f;
		replayButton_.SetPosition({ b1X, btnY });
		replayButton_.Update();
		titleButton_.SetPosition({ b2X, btnY });
		titleButton_.Update();

		missionCompleteText_.SetPosition({ w0->pos.x + w0->size.x * 0.5f, w0->pos.y + 44.0f });
		missionCompleteText_.Update();
	}

#ifdef USE_IMGUI
	ImGui::Begin("CLEAR SCENE DEBUG");
	ImGui::Text("FadeIn: %.2f", fadeInAlpha_);
	ImGui::Text("Rewinding: %s (%.2f)", isRewinding_ ? "YES" : "NO", rewindTimer_);
	ImGui::Text("Uploading: %s (%.2f)", isUploading_ ? "YES" : "NO", uploadTimer_);
	const GameResult& result = GameResult::GetInstance();
	ImGui::Text("Enemies: %d", result.enemiesDestroyed);
	ImGui::Text("Time: %.1fs", result.clearTimeSeconds);
	ImGui::Text("HP: %.1f/%.1f", result.playerHPRemaining, result.playerMaxHP);
	ImGui::End();
#endif
}

void ClearScene::UpdateTacticalCamera() {
	const float pitch = 0.52f;
	const float yaw = animTimer_ * 0.06f + 0.45f;
	Vector3 camPos = {
		trajCenter_.x - trajOrbitDist_ * std::cos(pitch) * std::sin(yaw),
		trajCenter_.y + trajOrbitDist_ * std::sin(pitch),
		trajCenter_.z - trajOrbitDist_ * std::cos(pitch) * std::cos(yaw)
	};
	tacticalCam_.SetRotate({ pitch, yaw, 0.0f });
	tacticalCam_.SetTranslate(camPos);
	tacticalCam_.Update();
}

void ClearScene::DrawTacticalAircraftMarker(
	PrimitiveModel* prim,
	const Vector3& pos,
	const Vector3& forward,
	const Vector3& up,
	float groundY,
	const Vector4& color) {
	if (!prim) return;

	// 垂直ドロップライン（AC7風の機体下への垂線）
	prim->DrawLine3D(pos, { pos.x, groundY + 4.0f, pos.z }, { color.x, color.y, color.z, 0.85f }, &tacticalCam_);

	Vector3 dir = forward;
	float dLen = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
	if (dLen > 0.001f) {
		dir = { dir.x / dLen, dir.y / dLen, dir.z / dLen };
	} else {
		dir = { 0.0f, 0.0f, 1.0f };
	}

	Vector3 rightV = {
		up.y * dir.z - up.z * dir.y,
		up.z * dir.x - up.x * dir.z,
		up.x * dir.y - up.y * dir.x
	};
	float rLen = std::sqrt(rightV.x * rightV.x + rightV.y * rightV.y + rightV.z * rightV.z);
	if (rLen < 0.001f) {
		rightV = { 1.0f, 0.0f, 0.0f };
	} else {
		rightV = { rightV.x / rLen, rightV.y / rLen, rightV.z / rLen };
	}
	Vector3 upV = {
		dir.y * rightV.z - dir.z * rightV.y,
		dir.z * rightV.x - dir.x * rightV.z,
		dir.x * rightV.y - dir.y * rightV.x
	};

	auto makePt = [&](float fOff, float rOff, float uOff) -> Vector3 {
		return {
			pos.x + dir.x * fOff + rightV.x * rOff + upV.x * uOff,
			pos.y + dir.y * fOff + rightV.y * rOff + upV.y * uOff,
			pos.z + dir.z * fOff + rightV.z * rOff + upV.z * uOff
		};
	};

	// コンパクトで視認性の高いカーナビ風3Dシェブロン（複数機表示対応サイズ）
	const float hThick = 6.5f;
	Vector3 topPts[4] = {
		makePt( 48.0f,   0.0f,  hThick),
		makePt(-34.0f, -28.0f,  hThick),
		makePt(-16.0f,   0.0f,  hThick),
		makePt(-34.0f,  28.0f,  hThick)
	};
	Vector3 botPts[4] = {
		makePt( 48.0f,   0.0f, -hThick),
		makePt(-34.0f, -28.0f, -hThick),
		makePt(-16.0f,   0.0f, -hThick),
		makePt(-34.0f,  28.0f, -hThick)
	};
	// 緑単色ソリッド面（上面2枚・下面2枚・側面8枚）
	prim->DrawTriangle3D(topPts[0], topPts[1], topPts[2], color, &tacticalCam_);
	prim->DrawTriangle3D(topPts[0], topPts[2], topPts[3], color, &tacticalCam_);
	prim->DrawTriangle3D(botPts[0], botPts[1], botPts[2], color, &tacticalCam_);
	prim->DrawTriangle3D(botPts[0], botPts[2], botPts[3], color, &tacticalCam_);
	for (int k = 0; k < 4; ++k) {
		int nk = (k + 1) % 4;
		prim->DrawTriangle3D(topPts[k], botPts[k], botPts[nk], color, &tacticalCam_);
		prim->DrawTriangle3D(topPts[k], botPts[nk], topPts[nk], color, &tacticalCam_);
	}

	// 枠線（面よりちょっと濃いめの緑・Zファイティング防止のため極小オフセット）
	const float edgeExpand = 0.4f;
	Vector3 edgeTopPts[4] = {
		makePt( 48.0f + edgeExpand,   0.0f,               hThick + edgeExpand),
		makePt(-34.0f - edgeExpand, -28.0f - edgeExpand,  hThick + edgeExpand),
		makePt(-16.0f - edgeExpand,   0.0f,               hThick + edgeExpand),
		makePt(-34.0f - edgeExpand,  28.0f + edgeExpand,  hThick + edgeExpand)
	};
	Vector3 edgeBotPts[4] = {
		makePt( 48.0f + edgeExpand,   0.0f,              -hThick - edgeExpand),
		makePt(-34.0f - edgeExpand, -28.0f - edgeExpand, -hThick - edgeExpand),
		makePt(-16.0f - edgeExpand,   0.0f,              -hThick - edgeExpand),
		makePt(-34.0f - edgeExpand,  28.0f + edgeExpand, -hThick - edgeExpand)
	};
	const Vector4 edgeCol = { color.x * 0.30f, color.y * 0.46f, color.z * 0.34f, 1.00f };
	for (int k = 0; k < 4; ++k) {
		int nk = (k + 1) % 4;
		prim->DrawLine3D(edgeTopPts[k], edgeTopPts[nk], edgeCol, &tacticalCam_);
		prim->DrawLine3D(edgeBotPts[k], edgeBotPts[nk], edgeCol, &tacticalCam_);
		prim->DrawLine3D(edgeTopPts[k], edgeBotPts[k], edgeCol, &tacticalCam_);
	}
	prim->DrawLine3D(edgeTopPts[0], edgeTopPts[2], edgeCol, &tacticalCam_);

	// 地表の小さな着地点リング
	uint32_t ringTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");
	float pulse = 1.0f + 0.18f * std::sin(animTimer_ * 4.5f);
	prim->DrawRing(
		{ 65.0f * pulse, 65.0f * pulse, 1.0f },
		{ 1.5707963f, 0.0f, animTimer_ * 0.9f },
		{ pos.x, groundY + 8.0f, pos.z },
		{ color.x, color.y, color.z, 0.75f },
		ringTex,
		&tacticalCam_,
		BlendMode::kAdd);
}

void ClearScene::Draw() {
	// 1. 3D等高線地形 + プレイヤー飛行軌跡の描画（余計な情報なし）
	if (auto* tcm = TerrainChunkManager::GetInstance(); tcm && tcm->IsLoaded()) {
		tcm->DrawContourMap(&tacticalCam_);
	}

	const auto& traj = GameResult::GetInstance().flightTrajectory;
	if (PrimitiveModel* prim = PrimitiveModel::GetInstance(); prim && !traj.empty()) {
		const Vector4 playerGreen = { 0.16f, 0.90f, 0.34f, 1.00f };
		const Vector4 trajCol = playerGreen;
		const Vector4 trajFutureCol = { playerGreen.x, playerGreen.y, playerGreen.z, 0.25f };
		const Vector4 trajGroundCol = { playerGreen.x, playerGreen.y, playerGreen.z, 0.40f };
		auto* tcm = TerrainChunkManager::GetInstance();

		// 記録された実時刻に基づき1回再生し、クリア地点で静止表示
		const float playDuration = (trajDurationSec_ > 0.1f) ? (std::min)(trajDurationSec_, 10.0f) : 1.0f;
		const float playFrac = std::clamp(animTimer_ / playDuration, 0.0f, 1.0f);
		const float targetTime = traj.front().timeSec + trajDurationSec_ * playFrac;

		Vector3 curPos = traj.back().pos;
		Vector3 curForward = traj.back().forward;
		Vector3 curUp = traj.back().up;
		size_t activeSegIdx = (traj.size() >= 2) ? (traj.size() - 2) : 0;

		if (traj.size() >= 2 && trajDurationSec_ > 0.001f) {
			for (size_t i = 0; i + 1 < traj.size(); ++i) {
				if (targetTime <= traj[i + 1].timeSec || i + 2 == traj.size()) {
					activeSegIdx = i;
					float segDt = (std::max)(0.0001f, traj[i + 1].timeSec - traj[i].timeSec);
					float localT = std::clamp((targetTime - traj[i].timeSec) / segDt, 0.0f, 1.0f);
					const auto& s0 = traj[i];
					const auto& s1 = traj[i + 1];
					curPos = {
						s0.pos.x + (s1.pos.x - s0.pos.x) * localT,
						s0.pos.y + (s1.pos.y - s0.pos.y) * localT,
						s0.pos.z + (s1.pos.z - s0.pos.z) * localT
					};
					curForward = {
						s0.forward.x + (s1.forward.x - s0.forward.x) * localT,
						s0.forward.y + (s1.forward.y - s0.forward.y) * localT,
						s0.forward.z + (s1.forward.z - s0.forward.z) * localT
					};
					curUp = {
						s0.up.x + (s1.up.x - s0.up.x) * localT,
						s0.up.y + (s1.up.y - s0.up.y) * localT,
						s0.up.z + (s1.up.z - s0.up.z) * localT
					};
					break;
				}
			}

			// 実際の飛行軌跡を描画（通過済み発光 + 未到達薄線）
			for (size_t i = 0; i + 1 < traj.size(); ++i) {
				const Vector3& a = traj[i].pos;
				const Vector3& b = (i == activeSegIdx) ? curPos : traj[i + 1].pos;
				if (i <= activeSegIdx) {
					prim->DrawLine3D(a, b, trajCol, &tacticalCam_);
					prim->DrawLine3D({ a.x, a.y + 4.0f, a.z }, { b.x, b.y + 4.0f, b.z }, trajCol, &tacticalCam_);
					if (tcm && tcm->IsLoaded()) {
						float ga = tcm->GetTerrainHeightAt(a.x, a.z, 0.0f) + 5.0f;
						float gb = tcm->GetTerrainHeightAt(b.x, b.z, 0.0f) + 5.0f;
						prim->DrawLine3D({ a.x, ga, a.z }, { b.x, gb, b.z }, trajGroundCol, &tacticalCam_);
					}
					if (i == activeSegIdx && playFrac < 1.0f) {
						prim->DrawLine3D(curPos, traj[i + 1].pos, trajFutureCol, &tacticalCam_);
					}
				} else {
					prim->DrawLine3D(a, traj[i + 1].pos, trajFutureCol, &tacticalCam_);
				}
			}
		}

		float groundY = (tcm && tcm->IsLoaded()) ? tcm->GetTerrainHeightAt(curPos.x, curPos.z, 0.0f) : 0.0f;
		DrawTacticalAircraftMarker(prim, curPos, curForward, curUp, groundY, playerGreen);
	}

	// CRTモニター感・走査線・ブルーム・微細色収差の適用
	PostEffect* postEffect = PostEffect::GetInstance();
	if (postEffect) {
		postEffect->ClearActiveEffects();

		if (isRewinding_) {
			// 本物のVHS巻き戻しシェーダー（走査線乱れ＋高速スクロール＋水平ノイズ＋色収差＋湾曲）
			ActivePostEffect scanline;
			scanline.type = PostEffectType::kScanLine;
			scanline.intensity = 0.60f;
			scanline.dirX = 260.0f;
			scanline.dirY = -55.0f; // 超高速巻き戻しスクロール
			postEffect->AddActiveEffect(scanline);

			ActivePostEffect noise;
			noise.type = PostEffectType::kRandom;
			noise.intensity = 0.45f;
			postEffect->AddActiveEffect(noise);

			ActivePostEffect chromatic;
			chromatic.type = PostEffectType::kChromaticAberration;
			chromatic.intensity = 0.08f;
			postEffect->AddActiveEffect(chromatic);

			ActivePostEffect dist;
			dist.type = PostEffectType::kLensDistortion;
			dist.intensity = 0.10f;
			postEffect->AddActiveEffect(dist);
		} else {
			// 1. ネオン・シアン発光ブルーム（しっとりとした軍用モニターの輝き）
			ActivePostEffect bloom;
			bloom.type = PostEffectType::kBloom;
			bloom.intensity = 0.85f;
			bloom.dirX = 1.10f; // ブルーム強度
			bloom.dirY = 0.72f; // 輝度閾値
			postEffect->AddActiveEffect(bloom);

			// 2. CRTモニター走査線（繊細なスキャンライン）
			ActivePostEffect scanline;
			scanline.type = PostEffectType::kScanLine;
			scanline.intensity = 0.08f;
			scanline.dirX = 520.0f; // 走査線の細かさ
			scanline.dirY = 5.0f;   // 穏やかなスクロール
			postEffect->AddActiveEffect(scanline);

			// 3. 微細モニターノイズ
			ActivePostEffect noise;
			noise.type = PostEffectType::kRandom;
			noise.intensity = 0.12f;
			postEffect->AddActiveEffect(noise);

			// 4. 上品な色収差（RGBフリンジ）
			ActivePostEffect chromatic;
			chromatic.type = PostEffectType::kChromaticAberration;
			chromatic.intensity = 0.006f;
			postEffect->AddActiveEffect(chromatic);

			// 5. 四隅のCRTビネット
			ActivePostEffect vig;
			vig.type = PostEffectType::kVignette;
			vig.intensity = 0.28f;
			postEffect->AddActiveEffect(vig);
		}
	}
}

void ClearScene::DrawUI() {
	auto DrawRectHelper = [this](const Vector2& pos, const Vector2& size, const Vector4& col, BlendMode blend = BlendMode::kNormal) {
		if (!uploadSprite_) return;
		uploadSprite_->SetBlendMode(blend);
		uploadSprite_->SetPosition(pos);
		uploadSprite_->SetAnchorPoint({ 0.0f, 0.0f });
		uploadSprite_->SetSize(size);
		uploadSprite_->SetRotation(0.0f);
		uploadSprite_->SetColor(col);
		uploadSprite_->Update();
		uploadSprite_->Draw();
	};

	// 1. 背景パネルは3D等高線マップを見せるため透過（非描画）

	// 画面全体のゆっくり動くサイバースキャンビーム
	float beamY = std::fmod(animTimer_ * 80.0f, kScreenHeight - 32.0f);
	DrawRectHelper({ 0.0f, beamY }, { kScreenWidth, 2.0f }, { 0.25f, 0.85f, 1.0f, 0.18f }, BlendMode::kAdd);

	// 2. 疑似OSデスクトップ環境（マルチウィンドウ、タスクバー、カスタムカーソル）
	pseudoOS_.Draw();

	if (isAutoDemoActive_) {
		const float bannerW = 480.0f;
		const float bannerH = 24.0f;
		const float bannerX = (kScreenWidth - bannerW) * 0.5f;
		const float bannerY = 10.0f;
		const float blink = (std::fmod(animTimer_, 1.0f) < 0.72f) ? 1.0f : 0.65f;
		pseudoOS_.DrawPanel({ bannerX, bannerY }, { bannerW, bannerH },
			{ 0.01f, 0.05f, 0.08f, 0.94f }, { 0.30f, 0.95f, 1.0f, 0.85f * blink }, 1.0f);
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"[ AUTO DEMO PLAYBACK // CLICK OR PRESS SPACE FOR MANUAL ]",
			kScreenWidth * 0.5f,
			bannerY + 5.0f,
			11.5f,
			{ 0.45f, 1.0f, 0.85f, blink },
			{ 0.5f, 0.0f });
	}

	// ------------------------------------------------------------
	// ボタン決定フラッシュ＆波紋フィードバック（0.18秒）
	// ------------------------------------------------------------
	if (btnClickFlashTimer_ > 0.0f && uploadSprite_) {
		float flashAlpha = btnClickFlashTimer_ / 0.18f;
		DrawRectHelper({ 0.0f, 0.0f }, { kScreenWidth, kScreenHeight }, { 0.85f, 1.0f, 0.95f, flashAlpha * 0.40f }, BlendMode::kAdd);
	}

	// ------------------------------------------------------------
	// VHSテープのリワインド（巻き戻し）演出
	// ------------------------------------------------------------
	DrawRewindEffectUI();

	// ------------------------------------------------------------
	// データアップロードと帰還システムログブリッジ演出
	// ------------------------------------------------------------
	DrawUploadSequenceUI();
}

void ClearScene::UpdateAutoDemo(float dt) {
	const float prevT = autoDemoTimer_;
	autoDemoTimer_ += dt;
	const float t = autoDemoTimer_;

	auto crossed = [&](float triggerT) {
		return prevT < triggerT && t >= triggerT;
	};

	if (crossed(1.2f)) {
		pseudoOS_.BringWindowToFront(1);
	}
	if (crossed(2.0f)) {
		pseudoOS_.BringWindowToFront(3);
	}
	if (crossed(2.8f)) {
		if (auto* w2 = pseudoOS_.GetWindow(2)) {
			w2->isMinimized = true;
		}
	}
	if (crossed(3.6f)) {
		if (auto* w2 = pseudoOS_.GetWindow(2)) {
			w2->isOpen = true;
			w2->isMinimized = false;
		}
		pseudoOS_.BringWindowToFront(2);
	}
	if (crossed(4.4f)) {
		pseudoOS_.BringWindowToFront(0);
		selectionManager_.SetSelectedIndex(HalcyonClearDefaults::kReturnButtonIndex);
	}
	if (crossed(4.9f)) {
		btnClickFlashTimer_ = 0.18f;
		clickedButtonIdx_ = 1;
		isUploading_ = true;
		isDemoUploadPreview_ = true;
		uploadTimer_ = 0.0f;
		pseudoOS_.PlayWindowCloseAnimation(0.28f);
	}
	if (crossed(6.2f)) {
		isUploading_ = false;
		isDemoUploadPreview_ = false;
		uploadTimer_ = 0.0f;
		pseudoOS_.PlayWindowOpenAnimation(0.35f);
		pseudoOS_.BringWindowToFront(0);
		selectionManager_.SetSelectedIndex(HalcyonClearDefaults::kReplayButtonIndex);
	}
	if (crossed(7.0f)) {
		isAutoDemoActive_ = false;
		isDemoUploadPreview_ = false;
		GameResult::GetInstance().isRetry = true;
		sceneID = SCENE::STAGE;
		return;
	}
}

void ClearScene::Finalize() {
	PostEffect::GetInstance()->ClearActiveEffects();
	UITextRegistry::GetInstance()->Clear();
}
