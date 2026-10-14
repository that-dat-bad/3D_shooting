#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "TitleScene.h"
#include "../System/GameResult.h"
#include "../System/AsyncPreloadManager.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/UI/UITextRegistry.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/SkyboxCommon.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../engine/Graphics/System/BlendMode.h"
#include "../../engine/Graphics/System/DirectXCommon.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "WinApp.h"
#include "../../engine/io/Input.h"
#include <cmath>
#include <filesystem>
#include <chrono>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

// 画面サイズ定数
static constexpr float kScreenWidth = static_cast<float>(WinApp::kClientWidth);   // 1280
static constexpr float kScreenHeight = static_cast<float>(WinApp::kClientHeight); // 720

// 静的リソースキャッシュの実体定義
TitleScene::TitleResourceCache TitleScene::s_cache_;

void TitleScene::ClearStaticCache() {
	s_cache_ = TitleResourceCache{};
}

void TitleScene::SetCursorVisible(bool visible) {
	isCursorShown_ = visible;
#ifdef USE_IMGUI
	ImGui::SetMouseCursor(visible ? ImGuiMouseCursor_Arrow : ImGuiMouseCursor_None);
#endif
	HWND hwnd = Input::GetInstance() ? Input::GetInstance()->GetHwnd() : nullptr;
	bool isForeground = (hwnd && ::GetForegroundWindow() == hwnd);
	if (visible || !isForeground) {
		while (ShowCursor(TRUE) < 0) {}
		::SetCursor(LoadCursor(nullptr, IDC_ARROW));
	} else {
		while (ShowCursor(FALSE) >= 0) {}
		::SetCursor(nullptr);
	}
}

void TitleScene::Initialize() {
	auto tStart = std::chrono::high_resolution_clock::now();
	SetCursorVisible(false); // ドローン視点・コンソール画面ではOSカーソルを完全非表示（項目7）
	sceneID = SCENE::TITLE;
	selectionManager_.Clear();
	lastMenuSelIdx_ = 0;
	subWindowSwitchTimer_ = 0.0f;
	animTimer_ = 0.0f;
	menuEnterTimer_ = 0.0f;
	activeSatLayer_ = 1;
	activeSysDetailTab_ = 0;
	activeIntelTarget_ = 0;
	isEngineTesting_ = false;
	engineTestTimer_ = 0.0f;
	defconLevel_ = 2;
	loadingProgress_ = 0.0f;
	isLoadingComplete_ = false;
	completeHoldTimer_ = 0.0f;
	prewarmPhase_ = 0;
	isTitleCamerasPrewarmed_ = false;
	loadingPhaseText_ = "INITIALIZING OPTICAL CAMERAS...";


	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();
	TextureManager* tm = TextureManager::GetInstance();


	// 必須テクスチャのロード
	tm->LoadTexture("assets/textures/white1x1.png");
	whiteTexIndex_ = tm->GetTextureIndexByFilePath("assets/textures/white1x1.png");

	tm->LoadTexture("assets/textures/soft_shadow.png");
	softShadowTexIndex_ = tm->GetTextureIndexByFilePath("assets/textures/soft_shadow.png");

	tm->LoadTexture("assets/textures/crash_scorch.png");
	crashScorchTexIndex_ = tm->GetTextureIndexByFilePath("assets/textures/crash_scorch.png");

	tm->LoadTexture("assets/textures/tunnel_wall.png");
	tunnelWallTexIndex_ = tm->GetTextureIndexByFilePath("assets/textures/tunnel_wall.png");

	tm->LoadTexture("assets/textures/title_logo_dawn_cropped.png");

	// パーティクルグループの作成（CAM 03 墜落現場用: リアルな濃煙パフテクスチャ）
	ParticleManager::GetInstance()->CreateParticleGroup("TitleSmoke", "assets/textures/smoke_puff.png");
	ParticleManager::GetInstance()->SetBlendMode("TitleSmoke", BlendMode::kNormal);

	ParticleManager::GetInstance()->CreateParticleGroup("TitleSpark", "assets/textures/circle.png");
	ParticleManager::GetInstance()->SetBlendMode("TitleSpark", BlendMode::kAdd);

	// ============================
	// 3D背景の初期化 (A+B)
	// ============================
	CameraManager::GetInstance()->CreateCamera("TitleCamera");
	CameraManager::GetInstance()->SetActiveCamera("TitleCamera");
	Camera* titleCamera = CameraManager::GetInstance()->GetActiveCamera();
	if (titleCamera) {
		titleCamera->SetTranslate({0.0f, 2.0f, -15.0f});
		titleCamera->SetRotate({0.0f, 0.0f, 0.0f});
		titleCamera->Update();
	}

	tm->LoadTexture("assets/textures/cedar_bridge_sunset_1_2k.dds");
	uint32_t skyboxTexIndex = tm->GetTextureIndexByFilePath("assets/textures/cedar_bridge_sunset_1_2k.dds");
	Object3dCommon::GetInstance()->SetDefaultEnvTextureIndex(skyboxTexIndex);

	const bool fromCache = s_cache_.isCached;
	if (fromCache) {
		// ============================================================
		// キャッシュからの0ms高速復元（D3D12リソース生成2,000バッファ完全スキップ）
		// ============================================================
		skybox_ = std::move(s_cache_.skybox);
		locations_ = std::move(s_cache_.locations);
		afterburner_ = std::move(s_cache_.afterburner);
		consoleBgSprite_ = std::move(s_cache_.consoleBgSprite);
		sortieFlashSprite_ = std::move(s_cache_.sortieFlashSprite);
		recDotSprite_ = std::move(s_cache_.recDotSprite);
		titleLogoSprite_ = std::move(s_cache_.titleLogoSprite);
		headerLampSprite_ = std::move(s_cache_.headerLampSprite);
		pressSpaceBannerBg_ = std::move(s_cache_.pressSpaceBannerBg);
		osSpritePool_ = std::move(s_cache_.osSpritePool);
		cursorSprites_ = std::move(s_cache_.cursorSprites);
		windowCloseMaskSprites_ = std::move(s_cache_.windowCloseMaskSprites);
		windowCloseBorderSprites_ = std::move(s_cache_.windowCloseBorderSprites);
		menuBorderSprites_ = std::move(s_cache_.menuBorderSprites);
		buttonBorders_ = std::move(s_cache_.buttonBorders);
		glitchSprites_ = std::move(s_cache_.glitchSprites);
		osdSprites_ = std::move(s_cache_.osdSprites);
		pressSpaceBorders_ = std::move(s_cache_.pressSpaceBorders);
		menuBorderOffsets_ = s_cache_.menuBorderOffsets;
		buttonBorderOutlineOffsets_ = s_cache_.buttonBorderOutlineOffsets;
		buttonBorderBracketOffsets_ = s_cache_.buttonBorderBracketOffsets;
		buttonBorderIndicatorOffsets_ = s_cache_.buttonBorderIndicatorOffsets;
		s_cache_.isCached = false; // 所有権を this に移行

		// カメラを全オブジェクトに再接続
		if (skybox_) {
			skybox_->SetCamera(titleCamera);
		}
		for (auto& loc : locations_) {
			if (loc.envObject) loc.envObject->SetCamera(titleCamera);
			for (auto& extra : loc.extraEnvParts) {
				if (extra.object) extra.object->SetCamera(titleCamera);
			}
			if (loc.visualModel) loc.visualModel->SetCamera(titleCamera);
			if (loc.debrisModel) loc.debrisModel->SetCamera(titleCamera);
		}

		// スプライトステートとウィンドウ位置のリセット
		osSpritePoolIdx_ = 0;
		if (consoleBgSprite_) {
			consoleBgSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
			consoleBgSprite_->Update();
		}
		if (sortieFlashSprite_) {
			sortieFlashSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
			sortieFlashSprite_->Update();
		}

		menuWindowPos_ = { 392.0f, 44.0f };
		menuCardPanel_.Initialize(spriteCommon);
		menuCardPanel_.SetPosition(menuWindowPos_);
		menuCardPanel_.SetSize({ 520.0f, 616.0f });
		menuCardPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f });

		menuHeaderPanel_.Initialize(spriteCommon);
		menuHeaderPanel_.SetPosition(menuWindowPos_);
		menuHeaderPanel_.SetSize({ 520.0f, 34.0f });
		menuHeaderPanel_.SetBackgroundColor({ 0.05f, 0.22f, 0.12f, 0.98f });

		isWindowDragging_ = false;
		wasLButtonDown_ = false;
		isStartMenuOpen_ = false;
		ResetOSDesktopWindows();
	} else {
		// ============================================================
		// 初回生成（パーツ・セグメント・モデルの削減は一切行わず100%保持）
		// ============================================================
		// コンソールブート時の完全漆黒背景スプライト（項目1: 格納庫の透けを100%遮蔽）
		consoleBgSprite_ = std::make_unique<Sprite>();
		consoleBgSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
		consoleBgSprite_->SetAnchorPoint({ 0.0f, 0.0f });
		consoleBgSprite_->SetPosition({ 0.0f, 0.0f });
		consoleBgSprite_->SetSize({ kScreenWidth, kScreenHeight });
		consoleBgSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		consoleBgSprite_->Update();

		// 出撃シーケンス用ブラックアウトスプライト（黒フェード）
		sortieFlashSprite_ = std::make_unique<Sprite>();
		sortieFlashSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sortieFlashSprite_->SetAnchorPoint({ 0.0f, 0.0f });
		sortieFlashSprite_->SetPosition({ 0.0f, 0.0f });
		sortieFlashSprite_->SetSize({ kScreenWidth, kScreenHeight });
		sortieFlashSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
		sortieFlashSprite_->Update();

		// フルOSデスクトップ環境の初期化（512枚プール＆24枚カーソル）
		InitializeOSDesktop(spriteCommon);

		skybox_ = std::make_unique<Skybox>();
		skybox_->Initialize(SkyboxCommon::GetInstance());
		skybox_->SetCamera(titleCamera);
		skybox_->SetTextureIndex(skyboxTexIndex);

	ModelManager::GetInstance()->LoadModel("Resources/models/m21_wg.gltf");
	ModelManager::GetInstance()->LoadModel("Resources/models/m21.gltf");
	ModelManager::GetInstance()->LoadModel("Resources/models/m21_crashed.gltf");

	tm->LoadTexture("assets/textures/qwantani_dusk_2_puresky_2k.dds");
	const uint32_t sunsetSky = tm->GetTextureIndexByFilePath("assets/textures/cedar_bridge_sunset_1_2k.dds");
	const uint32_t duskSky = tm->GetTextureIndexByFilePath("assets/textures/qwantani_dusk_2_puresky_2k.dds");
	const uint32_t whiteSky = tm->GetTextureIndexByFilePath("assets/textures/white1x1.png");

	locations_.clear();
	locations_.resize(static_cast<size_t>(TitleLocation::Count));

	// ------------------------------------------------------------
	// 1. 地下トンネル基地: トンネル + 道路 + 誘導灯（自発光） + 天井回転灯（飛行中M-21）
	// ------------------------------------------------------------
	{
		LocationData& loc = locations_[static_cast<int>(TitleLocation::Tunnel)];
		loc.id = TitleLocation::Tunnel;
		loc.name = "Tunnel";
		loc.aircraftModelPath = "Resources/models/m21.gltf"; // 車輪なし・ギア格納飛行モデル
		loc.origin = { 0.0f, 0.0f, 0.0f };
		loc.envModelPath = "Resources/models/title/Tunnel.obj";
		loc.aircraftOffset = { 0.0f, 2.0f, 0.0f };          // トンネル中央の空中に浮上（高度2.0m）
		loc.aircraftRotation = { 0.0f, 3.14159265f, 0.0f }; // 機首を奥（進行方向 +Z）に向けて前進飛行
		loc.propellerRpm = 2500.0f;                          // 巡航飛行推力
		loc.skyboxTexIndex = whiteSky;
		loc.skyboxColor = { 0.0f, 0.0f, 0.0f, 1.0f }; // 天球を完全な黒にして暗黒空間にする

		// 追加パーツ：道路、誘導灯、回転灯
		loc.extraEnvParts.clear();
		{
			// 中央の道路と誘導灯、回転灯
			ExtraEnvPart road;
			road.modelPath = "Resources/models/title/Tunnel_road.obj";
			loc.extraEnvParts.push_back(std::move(road));

			ExtraEnvPart guide;
			guide.modelPath = "Resources/models/title/Tunnel_guide.obj";
			loc.extraEnvParts.push_back(std::move(guide));

			ExtraEnvPart rotateLight;
			rotateLight.modelPath = "Resources/models/title/Tunnel_rotate.obj";
			rotateLight.rotateY = true;
			rotateLight.rotateSpeed = 4.0f; // 回転灯のY軸自転 (rad/sec)
			loc.extraEnvParts.push_back(std::move(rotateLight));

			// トンネルを前後に大幅に連結（端に15個ずつ、中央と合わせて計31個、全長約1.25km）
			constexpr float kTunnelLength = 40.383094f;
			constexpr int kTunnelSegmentsPerSide = 15;
			for (int i = 1; i <= kTunnelSegmentsPerSide; ++i) {
				for (float dir : { -1.0f, 1.0f }) {
					float zOffset = dir * kTunnelLength * static_cast<float>(i);

					ExtraEnvPart tunnelPart;
					tunnelPart.modelPath = "Resources/models/title/Tunnel.obj";
					tunnelPart.offset = { 0.0f, 0.0f, zOffset };
					loc.extraEnvParts.push_back(std::move(tunnelPart));

					ExtraEnvPart roadPart;
					roadPart.modelPath = "Resources/models/title/Tunnel_road.obj";
					roadPart.offset = { 0.0f, 0.0f, zOffset };
					loc.extraEnvParts.push_back(std::move(roadPart));

					ExtraEnvPart guidePart;
					guidePart.modelPath = "Resources/models/title/Tunnel_guide.obj";
					guidePart.offset = { 0.0f, 0.0f, zOffset };
					loc.extraEnvParts.push_back(std::move(guidePart));

					// 2セグメントごと（約80m間隔）に天井警告回転灯を追加配置
					if (i % 2 == 0) {
						ExtraEnvPart rotatePart;
						rotatePart.modelPath = "Resources/models/title/Tunnel_rotate.obj";
						rotatePart.offset = { 0.0f, 0.0f, zOffset };
						rotatePart.rotateY = true;
						rotatePart.rotateSpeed = 4.0f;
						loc.extraEnvParts.push_back(std::move(rotatePart));
					}
				}
			}
		}

		LocationLighting& l = loc.lighting;
		l.lightType = 2 | 4;                              // ポイントライト＋後方投光スポットライトでトンネル壁面・路面を照らす
		l.dirIntensity = 0.0f;
		// スラスターノズル位置（Z=-3.8m）からのエレクトリックブルー照り返し（白飛び抑制：項目1）
		l.pointColor = { 0.18f, 0.60f, 0.95f, 1.0f };
		l.pointOffset = { 0.0f, 1.95f, -3.8f };
		l.pointIntensity = 1.8f;                          // 過剰発光を抑え、機体ディテールを保つ
		l.pointRadius = 22.0f;
		l.pointDecay = 1.2f;
		l.pointFlicker = 0.15f;                           // ジェット燃焼の動的フリッカー
		// 後方および壁面・床面へのスラスター照射
		l.spotColor = { 0.25f, 0.68f, 0.95f, 1.0f };
		l.spotOffset = { 0.0f, 2.0f, -2.5f };
		l.spotDirection = MyMath::Normalize({ 0.0f, -0.35f, -0.93f });
		l.spotIntensity = 1.5f;                           // 過剰発光を抑制
		l.spotDistance = 30.0f;
		l.spotDecay = 1.2f;
		l.spotAngleDeg = 48.0f;
		l.spotFalloffStartDeg = 24.0f;

		// トンネル後方チェイスカメラ（機体テールの単発アフターバーナー＆ショックダイヤモンドを克明に捉える）
		loc.camera = { 8.2f, 0.7f, 1.05f, 0.20f, 0.15f, 0.22f, 0.45f, true };
		loc.grade = { { 0.05f, 0.08f, 0.15f }, 0.08f, 0.50f };
	}

	// ------------------------------------------------------------
	// 2. 格納庫: 元のハンガーモデル + 作業灯 + 天井投光器（金属質感とスペキュラ反射を適正化：項目4, 5, 9）
	// ------------------------------------------------------------
	{
		LocationData& loc = locations_[static_cast<int>(TitleLocation::Hangar)];
		loc.id = TitleLocation::Hangar;
		loc.name = "Hangar";
		loc.origin = { 0.0f, 0.0f, 0.0f };
		loc.envModelPath = "Resources/models/title/hangar.obj";
		loc.aircraftOffset = { 0.0f, 2.02f, 0.0f };       // 車輪底面（Y=-2.02m）が床面（Y=0）にピタリ接地
		loc.aircraftRotation = { 0.0f, 0.5f, 0.0f };
		loc.skyboxTexIndex = duskSky;                    // 建造物トラスのないピュアスカイでキャノピーへの白い斜線映り込みを根絶
		loc.skyboxColor = { 0.10f, 0.10f, 0.12f, 1.0f }; // 屋内なので空は暗く抑える

		LocationLighting& l = loc.lighting;
		l.lightType = 1 | 2 | 4;
		l.dirColor = { 0.60f, 0.70f, 0.88f, 1.0f };      // シャッター隙間からの青白い外光
		l.dirDirection = MyMath::Normalize({ 0.45f, -0.65f, 0.60f });
		l.dirIntensity = 0.55f;
		l.pointColor = { 1.0f, 0.85f, 0.65f, 1.0f };     // 作業灯（暖色投光）
		l.pointOffset = { -4.5f, 2.5f, -3.5f };
		l.pointIntensity = 1.6f;
		l.pointRadius = 22.0f;
		l.pointDecay = 1.2f;
		l.spotColor = { 0.95f, 0.98f, 1.0f, 1.0f };       // 天井投光器（キャノピー白飛び明滅バグ防止：強度抑制：項目4）
		l.spotOffset = { -2.0f, 10.5f, 2.5f };
		l.spotDirection = MyMath::Normalize({ 0.18f, -0.95f, -0.22f });
		l.spotIntensity = 1.4f;                           // 3.4fから1.4fに抑制！これでキャノピーに白いポリゴンが焼き付かない
		l.spotDistance = 28.0f;
		l.spotDecay = 1.2f;
		l.spotAngleDeg = 42.0f;
		l.spotFalloffStartDeg = 24.0f;

		// 安定した見下ろし・見上げの周回カメラ（注視点高さを持ち上げ、下部バナーとの主翼被りを物理的に解消：項目5）
		loc.camera = { 11.5f, 0.0f, 2.4f, 0.0f, 1.55f, 0.05f, 0.0f, false };
		loc.grade = { { 0.05f, 0.08f, 0.15f }, 0.08f, 0.55f };
	}

	// ------------------------------------------------------------
	// 3. 墜落現場: 広大な荒野地形モデル + 夕暮れ・黄昏天球 + 激突焦げ跡（項目2, 8）
	// ------------------------------------------------------------
	{
		LocationData& loc = locations_[static_cast<int>(TitleLocation::CrashedForest)];
		loc.id = TitleLocation::CrashedForest;
		loc.name = "CrashedForest";
		loc.origin = { 5000.0f, 0.0f, 5000.0f };
		loc.envModelPath = "assets/models/terrain.obj"; // 起伏のある荒野・地面テクスチャモデル（背景の虚無を解消：項目2）
		loc.envScale = { 15.0f, 2.5f, 15.0f };
		loc.envRotation = { 0.0f, 0.0f, 0.0f };
		loc.aircraftModelPath = "Resources/models/m21_crashed.gltf"; // 激突・焦げ跡テクスチャ適用モデル（項目8）
		loc.aircraftOffset = { 0.0f, 0.32f, 0.0f };         // 機体が地面に激突し食い込む
		loc.aircraftRotation = { 0.22f, -0.85f, 0.38f };    // 機首突っ込み + 左に激しく傾斜
		loc.hiddenParts = {
			DamagePart::Wing_L, DamagePart::Wing1_L, DamagePart::Wing2_L,
			DamagePart::Aileron_L, DamagePart::Tank1,
			DamagePart::Elevator0, DamagePart::Rudder,      // 左水平尾翼・方向舵も千切れ飛ぶ激突ダメージ
		};
		loc.debrisOffset = { -6.5f, 0.38f, 4.2f };          // 千切れた左翼が激突痕に突き刺さる
		loc.debrisRotation = { 0.95f, 0.70f, -0.45f };
		loc.skyboxTexIndex = duskSky;                       // 暗黒の虚無を解消し、夕暮れ・黄昏の空を配置（項目2）
		loc.skyboxColor = { 0.38f, 0.32f, 0.36f, 1.0f };   // 哀愁のある夕暮れトーン
		loc.extraEnvParts.clear();

		LocationLighting& l = loc.lighting;
		l.lightType = 1 | 2;
		l.dirColor = { 0.55f, 0.65f, 0.75f, 1.0f };      // 木漏れの冷たい光
		l.dirDirection = MyMath::Normalize({ 0.3f, -0.8f, -0.4f });
		l.dirIntensity = 0.5f;
		l.pointColor = { 1.0f, 0.45f, 0.15f, 1.0f };     // 炎の照り返し
		l.pointOffset = { 1.5f, 1.2f, -1.0f };
		l.pointIntensity = 1.6f;
		l.pointRadius = 14.0f;
		l.pointDecay = 1.3f;
		l.pointFlicker = 0.35f;

		// めり込み防止：十分な距離（15.5m）と俯瞰高さ（3.2m）で全景を見渡すシネマティックカメラ
		loc.camera = { 15.5f, 0.0f, 3.2f, 0.0f, 0.8f, 0.05f, 0.0f, false };
		loc.grade = { { 0.04f, 0.05f, 0.07f }, 0.04f, 0.5f };
	}

	for (auto& loc : locations_) {
		SetupLocation(loc, titleCamera);
	}
	} // end of else (!fromCache) for 3D setup

	// 誘導灯・回転灯の自発光（エミッシブ）設定
	Model* guideModel = ModelManager::GetInstance()->FindModel("Resources/models/title/Tunnel_guide.obj");
	if (guideModel) {
		guideModel->SetMaterialEnableLighting(0, false);
		guideModel->SetMaterialColor(0, { guideEmissiveColor_.x, guideEmissiveColor_.y, guideEmissiveColor_.z, 1.0f });
		guideModel->SetMaterialEmissive(0, guideEmissiveColor_, guideEmissiveIntensity_);
	}
	Model* rotateModel = ModelManager::GetInstance()->FindModel("Resources/models/title/Tunnel_rotate.obj");
	if (rotateModel) {
		rotateModel->SetMaterialEnableLighting(0, false);
		rotateModel->SetMaterialColor(0, { 2.5f, 0.8f, 0.1f, 1.0f });
		rotateModel->SetMaterialEmissive(0, { 2.5f, 0.8f, 0.1f }, 3.5f);
	}

	TitleEntryReason entryReason = GameResult::GetInstance().titleEntryReason;
	if (entryReason == TitleEntryReason::RebootFromGameOver) {
		// 項目8: ゲームオーバーからの復帰時、起動コンソール画面に繋がり、再びOSが立ち上がってタイトルメニューに戻る
		state_ = TitleState::ConsoleBoot;
		consoleBootTimer_ = 0.0f;
		currentConsoleDuration_ = 0.85f; // 緊急リブート: 2.5倍速で切迫感のあるタイピング速度
		consoleExitTimer_ = 0.0f;
		loadingProgress_ = 0.0f;
		isLoadingComplete_ = false;
		completeHoldTimer_ = 0.0f;
		prewarmPhase_ = 0;
		isTitleCamerasPrewarmed_ = false;
		loadingPhaseText_ = "INITIALIZING OPTICAL CAMERAS...";
		isRebootLog_ = true;
		previousSortieLog_ = "PREVIOUS SORTIE: KERNEL PANIC // EMERGENCY SYSTEM RECOVERY ACTIVE";
		previousSortieColor_ = { 1.0f, 0.40f, 0.35f, 0.90f };
	} else if (entryReason == TitleEntryReason::ReturnFromClear) {
		// 項目10: クリアからの復帰時、セッション終了・再認証セキュリティ演出
		state_ = TitleState::ReAuthentication;
		reauthTimer_ = 0.0f;
		isRebootLog_ = false;
		currentConsoleDuration_ = 1.40f;
		previousSortieLog_ = "PREVIOUS SORTIE: MISSION ACCOMPLISHED // FLIGHT LOG ARCHIVED";
		previousSortieColor_ = { 0.35f, 1.0f, 0.65f, 0.90f };
	} else {
		state_ = TitleState::DroneView;
		isRebootLog_ = false;
		currentConsoleDuration_ = 1.40f;
		previousSortieLog_ = "";
	}
	// エントリ理由をリセット
	GameResult::GetInstance().titleEntryReason = TitleEntryReason::InitialBoot;

	stateTimer_ = 0.0f;
	cutTimer_ = 0.0f;
	cameraTheta_ = 0.0f;

	// ライティング
	Object3dCommon::GetInstance()->SetShadingModel(1);
	Object3dCommon::GetInstance()->SetSpecularModel(2);
	ChangeLocation(0);

	// ============================
	// 背景オーバーレイ（暗い半透明パネル）
	// ============================
	backgroundPanel_.Initialize(spriteCommon);
	backgroundPanel_.SetPosition({ 0.0f, 0.0f });
	backgroundPanel_.SetSize({ kScreenWidth, kScreenHeight });
	backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 0.3f }); // 少し透明度を上げて後ろの3Dが見えるように

	// ============================
	// タイトルテキスト（コンソールプロジェクト識別バナー: 鮮烈なターミナルグリーン）
	// ============================
	titleText_.Initialize("HackGen", "DAWN", 48.0f);
	titleText_.SetAnchorPoint({ 0.5f, 0.0f });
	titleText_.SetPosition({ kScreenWidth * 0.5f, 106.0f });
	titleText_.SetColor({ 0.22f, 1.0f, 0.52f, 1.0f }); // 鮮烈なエレクトリック・ターミナルグリーン
	titleText_.SetDropShadow(true, { 2.0f, 3.0f }, { 0.0f, 0.0f, 0.0f, 0.95f });
	titleText_.SetOutline(true, 1.5f, { 0.02f, 0.15f, 0.06f, 0.95f });

	// ============================
	// サブタイトルテキスト（ターミナルグリーン統一＆重なり解消）
	// ============================
	subtitleText_.Initialize("HackGen", "- OPERATION : DAWN -", 14.0f);
	subtitleText_.SetAnchorPoint({ 0.5f, 0.0f });
	subtitleText_.SetPosition({ kScreenWidth * 0.5f, 196.0f });
	subtitleText_.SetColor({ 0.35f, 0.95f, 0.60f, 0.95f }); // ターミナルグリーン
	subtitleText_.SetDropShadow(true, { 1.5f, 2.0f }, { 0.0f, 0.0f, 0.0f, 0.90f });

	// ============================
	// メニューボタン（コンソール端末コマンド行）
	// ============================
	float buttonWidth = 390.0f;
	float buttonHeight = 44.0f;
	float buttonX = (kScreenWidth - buttonWidth) * 0.5f;
	float buttonStartY = 248.0f;
	float buttonSpacing = 56.0f;

	const Vector4 btnNormalBg   = { 0.0f, 0.0f, 0.0f, 0.0f };       // 漆黒ウィンドウを活かしたクリーンな背景
	const Vector4 btnHoverBg    = { 0.05f, 0.25f, 0.12f, 0.50f };  // 上品な深緑半透明ハイライト
	const Vector4 btnSelectedBg = { 0.05f, 0.25f, 0.12f, 0.50f };  // 上品な深緑半透明ハイライト
	const Vector4 btnNormalText = { 0.35f, 0.92f, 0.55f, 0.95f };  // ターミナルグリーン文字
	const Vector4 btnSelText    = { 0.85f, 1.00f, 0.90f, 1.00f };  // 鮮やかな白熱発光グリーン文字

	// START ボタン
	startButton_.Initialize(spriteCommon, "[01] EXECUTE SORTIE", 21.0f);
	startButton_.SetLeftAligned(true, 100.0f);
	startButton_.SetTextOffset({ 0.0f, 3.0f });
	startButton_.GetLabelText()->SetFontName("HackGen");
	startButton_.SetPosition({ buttonX, buttonStartY });
	startButton_.SetSize({ buttonWidth, buttonHeight });
	startButton_.SetNormalColor(btnNormalBg);
	startButton_.SetHoverColor(btnHoverBg);
	startButton_.SetSelectedColor(btnSelectedBg);
	startButton_.SetTextColor(btnNormalText);
	startButton_.SetSelectedTextColor(btnSelText);
	startButton_.SetOnClick([this]() {
		state_ = TitleState::SortieSequence;
		sortieTimer_ = 0.0f;
		isEngineTesting_ = true;
		engineTestTimer_ = kSortieDuration;
		SetCursorVisible(false); // 出撃開始と同時にOSカーソルを消去（項目9）
		// 非同期処理による軽量化：出撃演出ムービーの裏でバックグラウンドスレッドでステージリソースを非同期プリロード！
		AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();
	});

	// EDITOR ボタン
	editorButton_.Initialize(spriteCommon, "[02] MISSION EDITOR", 21.0f);
	editorButton_.SetLeftAligned(true, 100.0f);
	editorButton_.SetTextOffset({ 0.0f, 3.0f });
	editorButton_.GetLabelText()->SetFontName("HackGen");
	editorButton_.SetPosition({ buttonX, buttonStartY + buttonSpacing });
	editorButton_.SetSize({ buttonWidth, buttonHeight });
	editorButton_.SetNormalColor(btnNormalBg);
	editorButton_.SetHoverColor(btnHoverBg);
	editorButton_.SetSelectedColor(btnSelectedBg);
	editorButton_.SetTextColor(btnNormalText);
	editorButton_.SetSelectedTextColor(btnSelText);
	editorButton_.SetOnClick([this]() {
		sceneID = SCENE::MISSION_EDITOR;
	});

	// SETTINGS ボタン
	settingsButton_.Initialize(spriteCommon, "[03] SYSTEM CONFIG", 21.0f);
	settingsButton_.SetLeftAligned(true, 100.0f);
	settingsButton_.SetTextOffset({ 0.0f, 3.0f });
	settingsButton_.GetLabelText()->SetFontName("HackGen");
	settingsButton_.SetPosition({ buttonX, buttonStartY + buttonSpacing * 2.0f });
	settingsButton_.SetSize({ buttonWidth, buttonHeight });
	settingsButton_.SetNormalColor(btnNormalBg);
	settingsButton_.SetHoverColor(btnHoverBg);
	settingsButton_.SetSelectedColor(btnSelectedBg);
	settingsButton_.SetTextColor(btnNormalText);
	settingsButton_.SetSelectedTextColor(btnSelText);
	settingsButton_.SetOnClick([this]() {
		// 設定画面は将来実装
	});

	// EXIT ボタン
	exitButton_.Initialize(spriteCommon, "[04] ABORT / EXIT", 21.0f);
	exitButton_.SetLeftAligned(true, 100.0f);
	exitButton_.SetTextOffset({ 0.0f, 3.0f });
	exitButton_.GetLabelText()->SetFontName("HackGen");
	exitButton_.SetPosition({ buttonX, buttonStartY + buttonSpacing * 3.0f });
	exitButton_.SetSize({ buttonWidth, buttonHeight });
	exitButton_.SetNormalColor(btnNormalBg);
	exitButton_.SetHoverColor(btnHoverBg);
	exitButton_.SetSelectedColor(btnSelectedBg);
	exitButton_.SetTextColor(btnNormalText);
	exitButton_.SetSelectedTextColor(btnSelText);
	exitButton_.SetOnClick([]() {
		PostQuitMessage(0);
	});

	selectionManager_.AddButton(&startButton_);
	selectionManager_.AddButton(&editorButton_);
	selectionManager_.AddButton(&settingsButton_);
	selectionManager_.AddButton(&exitButton_);

	// ============================================================
	// 初回のみ実行するスプライト・エフェクト初期化（キャッシュ復元時は0msスキップ）
	// ============================================================
	if (!fromCache) {
		// コンソールメニューカードの初期化
		InitializeMenuCard(spriteCommon);

		// 操作案内テキスト＆ミリタリー黒帯（枠線中央にジャストフィット配置：UIずれ解消）
		const float bannerW = 600.0f;
		const float bannerH = 34.0f; // 高さを34pxに設定し、テキストが上下枠線に重ならない余裕を確保
		const float bannerX = (kScreenWidth - bannerW) * 0.5f;
		const float bannerY = 672.0f; // 画面最下部（672～706px）の安全領域

		pressSpaceBannerBg_ = std::make_unique<Sprite>();
		pressSpaceBannerBg_->Initialize(spriteCommon, "assets/textures/white1x1.png");
		pressSpaceBannerBg_->SetAnchorPoint({ 0.0f, 0.0f });
		pressSpaceBannerBg_->SetPosition({ bannerX, bannerY });
		pressSpaceBannerBg_->SetSize({ bannerW, bannerH });
		pressSpaceBannerBg_->SetColor({ 0.01f, 0.02f, 0.01f, 0.98f }); // 完全不透明に近い漆黒ミリタリー帯
		pressSpaceBannerBg_->Update();

		pressSpaceBorders_.clear();
		auto addBannerLine = [&](const Vector2& pos, const Vector2& size, const MyMath::Vector4& col) {
			auto sp = std::make_unique<Sprite>();
			sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
			sp->SetAnchorPoint({ 0.0f, 0.0f });
			sp->SetPosition(pos);
			sp->SetSize(size);
			sp->SetColor(col);
			sp->Update();
			pressSpaceBorders_.push_back(std::move(sp));
		};
		const MyMath::Vector4 bannerLineCol = { 0.20f, 0.85f, 0.50f, 0.70f };
		// 上下の境界線 (1px)
		addBannerLine({ bannerX, bannerY }, { bannerW, 1.0f }, bannerLineCol);
		addBannerLine({ bannerX, bannerY + bannerH - 1.0f }, { bannerW, 1.0f }, bannerLineCol);
		// 左右のコーナーブラケット (2px)
		addBannerLine({ bannerX, bannerY }, { 8.0f, 2.0f }, bannerLineCol);
		addBannerLine({ bannerX, bannerY }, { 2.0f, 8.0f }, bannerLineCol);
		addBannerLine({ bannerX + bannerW - 8.0f, bannerY }, { 8.0f, 2.0f }, bannerLineCol);
		addBannerLine({ bannerX + bannerW - 2.0f, bannerY }, { 2.0f, 8.0f }, bannerLineCol);
		addBannerLine({ bannerX, bannerY + bannerH - 2.0f }, { 8.0f, 2.0f }, bannerLineCol);
		addBannerLine({ bannerX, bannerY + bannerH - 8.0f }, { 2.0f, 8.0f }, bannerLineCol);
		addBannerLine({ bannerX + bannerW - 8.0f, bannerY + bannerH - 2.0f }, { 8.0f, 2.0f }, bannerLineCol);
		addBannerLine({ bannerX + bannerW - 2.0f, bannerY + bannerH - 8.0f }, { 2.0f, 8.0f }, bannerLineCol);

		// DAWN タイトルロゴスプライト初期化（白文字 + MiG-21通過）
		titleLogoSprite_ = std::make_unique<Sprite>();
		titleLogoSprite_->Initialize(spriteCommon, "assets/textures/title_logo_dawn_cropped.png");
		titleLogoSprite_->SetAnchorPoint({ 0.5f, 0.5f });
		titleLogoSprite_->SetPosition({ kScreenWidth * 0.5f, 158.0f });
		titleLogoSprite_->SetSize({ 410.0f, 91.8f });
		titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // 鮮烈な白文字＆機体
		titleLogoSprite_->SetBlendMode(BlendMode::kNormal);
		titleLogoSprite_->Update();

		// ドローンカメラOSD初期化
		InitializeOSD(spriteCommon);

		// カメラ映像ウィンドウ閉じる演出の初期化（項目1）
		InitializeWindowCloseVisuals(spriteCommon);

		// 高品位アフターバーナーの初期化（Mig-21 単発エンジン・ショックダイヤモンド）
		afterburner_ = std::make_unique<Afterburner>();
		afterburner_->Initialize();
		afterburner_->SetTwinEngine(false);
		afterburner_->SetSingleNozzleOffset({ 0.0f, 0.011f, 14.95f });
		afterburner_->SetFlameRadius(0.38f);
		afterburner_->SetFlameLength(6.2f);
		afterburner_->SetShockDiamondCount(5);
		afterburner_->SetFlameColor({ 0.28f, 0.65f, 1.0f });
		afterburner_->SetCoreColor({ 0.88f, 0.95f, 1.0f });
		afterburner_->SetIntensity(1.35f);

#ifdef USE_IMGUI
		if (!afterburnerConfigPath_.empty()) {
			afterburner_->LoadConfig(afterburnerConfigPath_);
		}
#endif
	}

	// 正確なascent/descentアンカー計算により、バナー枠の中心にジャストフィット
	const float bannerH = 34.0f;
	const float bannerY = 672.0f;
	pressSpaceText_.Initialize("HackGen", "[ PRESS SPACE OR CLICK TO START / SKIP ]", 14.0f);
	pressSpaceText_.SetAnchorPoint({ 0.5f, 0.5f });
	pressSpaceText_.SetPosition({ kScreenWidth * 0.5f, bannerY + bannerH * 0.5f });
	pressSpaceText_.SetColor({ 0.35f, 1.0f, 0.65f, 0.95f });
	pressSpaceText_.SetDropShadow(true, { 1.5f, 1.5f }, { 0.0f, 0.0f, 0.0f, 0.95f });
	UIStateStyle pulseStyle;
	pulseStyle.color = { 0.35f, 1.0f, 0.65f, 0.95f };
	pulseStyle.scale = 1.0f;
	pulseStyle.loopMotion = UILoopMotion::Pulse;
	pulseStyle.motionIntensity = 1.15f;
	pulseStyle.motionSpeed = 2.2f;
	pressSpaceText_.SetStyle("Normal", pulseStyle);

	// UITextRegistryに登録（エディタから編集可能にする）
	UITextRegistry::GetInstance()->Register("Title_Main", &titleText_);
	UITextRegistry::GetInstance()->Register("Title_Sub", &subtitleText_);
	UITextRegistry::GetInstance()->Register("Title_BtnStart", startButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Title_BtnSettings", settingsButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Title_BtnExit", exitButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Title_PressSpace", &pressSpaceText_);

	// 全UI初期化完了後にメニューウィンドウ位置を確定更新
	UpdateMenuWindowPosition(menuWindowPos_);

	auto tEnd = std::chrono::high_resolution_clock::now();
	double totalMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	char buf[256];
	snprintf(buf, sizeof(buf), "  [TitleScene::Initialize] Initialized in %.2f ms (%.4f s) [%s]\n",
		totalMs, totalMs / 1000.0, fromCache ? "FROM CACHE" : "FRESH LOAD");
	OutputDebugStringA(buf);

	FILE* fp = nullptr;
	if (fopen_s(&fp, "scene_transition.log", "a") == 0 && fp) {
		fputs(buf, fp);
		fclose(fp);
	}
}

void TitleScene::Update() {
	float dt = 1.0f / 60.0f;
	animTimer_ += dt;
	stateTimer_ += dt;

	// マウスカーソルが非表示モードの場合、自アプリがフォアグラウンドの時のみ消去（他アプリ操作時はOS標準カーソルを表示）
	HWND hwnd = Input::GetInstance() ? Input::GetInstance()->GetHwnd() : nullptr;
	bool isForeground = (hwnd && ::GetForegroundWindow() == hwnd);
	if (!isForeground) {
		while (ShowCursor(TRUE) < 0) {}
		::SetCursor(LoadCursor(nullptr, IDC_ARROW));
	} else if (!isCursorShown_) {
#ifdef USE_IMGUI
		ImGui::SetMouseCursor(ImGuiMouseCursor_None);
#endif
		::SetCursor(nullptr);
	}

	if (afterburner_) {
		afterburner_->Update(dt, afterburner_->GetIntensity());
	}

	if (transitionTimer_ > 0.0f) {
		transitionTimer_ -= dt;
		if (transitionTimer_ < 0.0f) { transitionTimer_ = 0.0f; }
	}

	if (state_ == TitleState::DroneView) {
		cutTimer_ += dt;
		// 一定時間ごとに次のロケーションへカット切替
		if (!lockLocation_ && cutTimer_ > kCutDuration) {
			ChangeLocation((currentLocationIndex_ + 1) % static_cast<int>(locations_.size()));
		}
	}

	LocationData& loc = CurrentLocation();

	// ★ 重要：カメラ更新を機体・背景モデルの行列計算より「先」に実行！
	if (state_ == TitleState::DroneView || state_ == TitleState::WindowClose || state_ == TitleState::ConsoleBoot) {
		UpdateLocationCamera(loc, dt, false);
	} else if (state_ == TitleState::Menu) {
		UpdateLocationCamera(loc, dt, true);
	}

	// 現在ロケーションの機体・残骸・背景を更新
	{
		MyMath::Vector3 pos = MyMath::Add(loc.origin, loc.aircraftOffset);
		MyMath::Vector3 rot = loc.aircraftRotation;

		// トンネル内の飛行モーション（低空巡航飛行の浮遊動揺・バンク・推力微振動）
		if (loc.id == TitleLocation::Tunnel && enableTunnelMotion_) {
			float swayY = std::sin(animTimer_ * 2.2f) * 0.08f + std::sin(animTimer_ * 4.5f) * 0.02f;
			float swayX = std::sin(animTimer_ * 1.3f) * 0.18f;
			float engineVibe = std::sin(animTimer_ * 48.0f) * 0.003f;
			pos.x += swayX;
			pos.y += swayY + engineVibe;

			// 機体の姿勢（バンク傾き ＆ 迎え角ピッチ）
			float roll = -std::cos(animTimer_ * 1.3f) * 0.045f;
			float pitch = 0.025f + std::sin(animTimer_ * 2.2f) * 0.015f;
			rot.x += pitch;
			rot.z += roll;
		}

		currentAircraftMatrix_ = MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, rot, pos);
		loc.visualModel->Update(currentAircraftMatrix_, dt);
		if (loc.debrisModel) {
			MyMath::Vector3 debrisPos = MyMath::Add(loc.origin, loc.debrisOffset);
			loc.debrisModel->Update(MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, loc.debrisRotation, debrisPos), dt);
		}

		// トンネル専用：手前方向へ超高速スクロールして猛烈な飛行前進感を演出（時速約470km/h）
		float tunnelScrollZ = 0.0f;
		if (loc.id == TitleLocation::Tunnel) {
			constexpr float kTunnelLength = 40.383094f;
			constexpr float kFlightSpeed = 130.0f; // 飛行速度 (m/s) 約468km/h
			tunnelScrollZ = std::fmod(animTimer_ * kFlightSpeed, kTunnelLength);
		}

		// 墜落現場専用：折れた左翼付け根・破損エンジンから立ち上るリアルな濃煙と微小火の粉（項目4）
		if (loc.id == TitleLocation::CrashedForest) {
			crashSmokeTimer_ += dt;
			if (crashSmokeTimer_ >= 0.035f) {
				crashSmokeTimer_ = 0.0f;
				MyMath::Vector3 smokePos = MyMath::Add(loc.origin, loc.aircraftOffset);
				smokePos.x -= 1.6f;
				smokePos.y += 0.7f;
				smokePos.z += 0.2f;

				ParticleParameters smokeParams;
				smokeParams.minVelocity = { -0.012f, 0.028f, -0.012f };
				smokeParams.maxVelocity = {  0.012f, 0.065f,  0.012f };
				// R=G=Bを厳格に完全同値化し、ピンクや緑の偏色を100%根絶（項目6: 漆黒・チャコールグレー重煙）
				float g = 0.08f + static_cast<float>(rand() % 100) / 100.0f * 0.10f; // 0.08～0.18
				smokeParams.minColor = { g, g, g, 0.88f };
				smokeParams.maxColor = { g, g, g, 0.94f };
				smokeParams.minLifeTime = 3.5f;
				smokeParams.maxLifeTime = 5.2f;
				smokeParams.minScale = 1.2f;
				smokeParams.maxScale = 4.8f;
				smokeParams.scaleEasing = 0.40f;
				smokeParams.randomPositionRange = 0.35f;
				smokeParams.acceleration = { 0.0004f, 0.0001f, 0.0002f };
				ParticleManager::GetInstance()->Emit("TitleSmoke", smokePos, smokeParams, 2);
			}

			crashSparkTimer_ += dt;
			if (crashSparkTimer_ >= 0.12f) { // 火花は控えめに、極小の赤熱火の粉（ember）のみ
				crashSparkTimer_ = 0.0f;
				MyMath::Vector3 sparkPos = MyMath::Add(loc.origin, loc.aircraftOffset);
				sparkPos.x -= 1.5f;
				sparkPos.y += 0.5f;
				sparkPos.z += 0.1f;

				ParticleParameters sparkParams;
				sparkParams.minVelocity = { -0.025f, 0.020f, -0.025f };
				sparkParams.maxVelocity = {  0.025f, 0.055f,  0.025f };
				sparkParams.minColor = { 1.0f, 0.35f, 0.05f, 0.90f };
				sparkParams.maxColor = { 1.0f, 0.80f, 0.20f, 0.95f };
				sparkParams.minLifeTime = 0.4f;
				sparkParams.maxLifeTime = 0.85f;
				sparkParams.minScale = 0.03f;
				sparkParams.maxScale = 0.07f; // 微小な火の粉
				sparkParams.randomPositionRange = 0.15f;
				sparkParams.acceleration = { 0.0f, -0.0020f, 0.0f };
				ParticleManager::GetInstance()->Emit("TitleSpark", sparkPos, sparkParams, 1);
			}
		}

		if (loc.envObject) {
			MyMath::Vector3 envPos = loc.origin;
			envPos.z -= tunnelScrollZ; // 機体前進に伴い背景は手前（-Z方向）へスクロール
			loc.envObject->SetTranslate(envPos);
			loc.envObject->SetRotate(loc.envRotation);
			loc.envObject->SetScale(loc.envScale);
			loc.envObject->Update();
		}
		for (auto& extra : loc.extraEnvParts) {
			if (extra.object) {
				if (extra.rotateY) {
					extra.currentAngle += extra.rotateSpeed * dt;
					if (extra.currentAngle > 6.2831853f) {
						extra.currentAngle -= 6.2831853f;
					}
				}
				MyMath::Vector3 partRot = MyMath::Add(extra.rotation, { 0.0f, extra.currentAngle, 0.0f });
				MyMath::Vector3 partPos = MyMath::Add(loc.origin, extra.offset);
				partPos.z -= tunnelScrollZ; // 追加パーツも手前（-Z方向）へスクロール
				extra.object->SetTranslate(partPos);
				extra.object->SetRotate(partRot);
				extra.object->SetScale(extra.scale);
				extra.object->Update();
			}
		}

		if (loc.id == TitleLocation::Tunnel) {
			Model* guideModel = ModelManager::GetInstance()->FindModel("Resources/models/title/Tunnel_guide.obj");
			if (guideModel) {
				guideModel->SetMaterialEnableLighting(0, false);
				guideModel->SetMaterialColor(0, { guideEmissiveColor_.x, guideEmissiveColor_.y, guideEmissiveColor_.z, 1.0f });
				guideModel->SetMaterialEmissive(0, guideEmissiveColor_, guideEmissiveIntensity_);
			}
		}
	}

	ApplyLocationLighting(loc);

	if (state_ == TitleState::DroneView) {
		SetCursorVisible(false); // ドローン画面中はマウスカーソル非表示（項目7）

		// スペースキー、Enterキー、またはマウスクリック（左クリック）でOSウィンドウ最小化演出を経てコンソールへ（項目1）
		Input* input = Input::GetInstance();
		bool clickTriggered = input->TriggerMouse(0) || ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && !wasLButtonDown_);
		bool enterTriggered = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN)
			|| ((::GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) || ((::GetAsyncKeyState(VK_RETURN) & 0x8000) != 0);
		wasLButtonDown_ = ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) || input->PushMouse(0);

		if (enterTriggered || clickTriggered) {
			state_ = TitleState::WindowClose;
			windowCloseTimer_ = 0.0f;
			if (CurrentLocation().id == TitleLocation::CrashedForest) {
				ChangeLocation(0); // 通信途絶現場からメニュー移行時は基地メイン回線（格納庫）に接続復帰
			}
			// ローディング中にバックグラウンドで並行準備できるようプリロードを開始
			AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();
		}

		// ポストエフェクト (ドローン風 + ブルーム発光)
		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();

		// ブルーム（過剰発光・白飛びを抑制し、機体ディテールを鮮明に保つ：項目1）
		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = (loc.id == TitleLocation::Tunnel) ? 2.6f : 2.8f; // ブラー半径
		bloom.dirX = (loc.id == TitleLocation::Tunnel) ? 1.05f : 1.15f;    // ブルーム強度 (過剰な2.4fから適正な1.05fへ抑制)
		bloom.dirY = (loc.id == TitleLocation::Tunnel) ? 0.78f : 0.74f;    // 輝度閾値 (0.60fから0.78fに引き上げ、高輝度部のみ自然に光らせる)
		postEffect->AddActiveEffect(bloom);

		// 墜落現場専用：適度で落ち着いた砂嵐モニター演出（過剰な激しさを抑制）
		if (loc.id == TitleLocation::CrashedForest) {
			// 控えめなグリッチスパイク
			float glitchSpike = (rand() % 100 < 8) ? (0.08f + static_cast<float>(rand() % 100) / 100.0f * 0.12f) : 0.0f;

			// 1. 落ち着いた砂嵐ノイズ（0.65fから0.22fへ大幅抑制）
			ActivePostEffect noise;
			noise.type = PostEffectType::kRandom;
			noise.intensity = 0.22f + glitchSpike;
			postEffect->AddActiveEffect(noise);

			// 2. 繊細なCRT走査線（0.28fから0.12fへ抑制、ゆっくりスクロール）
			ActivePostEffect scanline;
			scanline.type = PostEffectType::kScanLine;
			scanline.intensity = 0.12f + glitchSpike * 0.2f;
			scanline.dirX = 520.0f; // 走査線の細かさ
			scanline.dirY = 6.0f;   // 穏やかなスクロール
			postEffect->AddActiveEffect(scanline);

			// 3. 控えめで上品な色収差（0.032fから0.012fへ抑制し、文字の可読性を確保）
			ActivePostEffect chromatic;
			chromatic.type = PostEffectType::kChromaticAberration;
			chromatic.intensity = 0.012f + glitchSpike * 0.015f;
			postEffect->AddActiveEffect(chromatic);

			// 4. 控えめなレンズ歪み（0.036fから0.010fへ抑制し、過剰な歪曲を解消）
			ActivePostEffect lens;
			lens.type = PostEffectType::kLensDistortion;
			lens.intensity = 0.010f;
			postEffect->AddActiveEffect(lens);
		} else {
			// 通常ロケーション（Tunnel など）
			// カメラ切替時の砂嵐ノイズトランジション演出（項目10）
			if (transitionTimer_ > 0.0f) {
				float t = transitionTimer_ / kTransitionDuration;
				ActivePostEffect noise;
				noise.type = PostEffectType::kRandom;
				noise.intensity = 0.35f * t;
				postEffect->AddActiveEffect(noise);

				ActivePostEffect glitchScan;
				glitchScan.type = PostEffectType::kScanLine;
				glitchScan.intensity = 0.18f * t;
				postEffect->AddActiveEffect(glitchScan);
			} else {
				// 通常時の繊細な走査線（項目9）
				ActivePostEffect scanline;
				scanline.type = PostEffectType::kScanLine;
				scanline.intensity = 0.045f;
				postEffect->AddActiveEffect(scanline);
			}

			// 控えめで上品な色収差（項目1, 9: 残像・過剰ボケを防止）
			ActivePostEffect chromatic;
			chromatic.type = PostEffectType::kChromaticAberration;
			chromatic.intensity = 0.006f;
			postEffect->AddActiveEffect(chromatic);

			// 控えめなレンズ歪み
			ActivePostEffect lens;
			lens.type = PostEffectType::kLensDistortion;
			lens.intensity = 0.008f;
			postEffect->AddActiveEffect(lens);
		}

		// ロケーション固有の色味（ビネット + カラーオーバーレイ）
		if (loc.grade.vignette > 0.0f || loc.grade.tintIntensity > 0.0f) {
			ActivePostEffect vignette;
			vignette.type = PostEffectType::kVignette;
			vignette.intensity = loc.grade.vignette;
			vignette.dirX = loc.grade.tintIntensity;
			vignette.colorR = loc.grade.tint.x;
			vignette.colorG = loc.grade.tint.y;
			vignette.colorB = loc.grade.tint.z;
			postEffect->AddActiveEffect(vignette);
		}

		if (pressSpaceBannerBg_) {
			pressSpaceBannerBg_->Update();
		}
		for (auto& sp : pressSpaceBorders_) {
			sp->Update();
		}
		pressSpaceText_.Update();

	} else if (state_ == TitleState::WindowClose) {
		SetCursorVisible(false); // ウィンドウ縮小中もOSカーソル非表示
		windowCloseTimer_ += dt;

		Input* input = Input::GetInstance();
		bool clickTriggered = input->TriggerMouse(0) || ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && !wasLButtonDown_);
		wasLButtonDown_ = ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) || input->PushMouse(0);
		bool enterTriggered = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN)
			|| ((::GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) || ((::GetAsyncKeyState(VK_RETURN) & 0x8000) != 0);

		// クリックやSpaceで即時スキップ、または時間経過で即コンソールへ
		if (enterTriggered || clickTriggered || windowCloseTimer_ >= kWindowCloseDuration) {
			state_ = TitleState::ConsoleBoot;
			consoleBootTimer_ = 0.0f;
			consoleExitTimer_ = 0.0f;
			loadingProgress_ = 0.0f;
			isLoadingComplete_ = false;
			completeHoldTimer_ = 0.0f;
			prewarmPhase_ = 0;
			isTitleCamerasPrewarmed_ = false;
			loadingPhaseText_ = "INITIALIZING OPTICAL CAMERAS...";
		}

		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();

		float normT = std::clamp(windowCloseTimer_ / kWindowCloseDuration, 0.0f, 1.0f);
		ActivePostEffect scanline;
		scanline.type = PostEffectType::kScanLine;
		scanline.intensity = 0.10f + 0.15f * normT;
		postEffect->AddActiveEffect(scanline);

	} else if (state_ == TitleState::ConsoleBoot) {
		SetCursorVisible(false); // コマンド・ローディング演出中もマウスカーソル非表示
		consoleBootTimer_ += dt;

		// コマンドタイピング時間経過後にローディングバーを開始
		float loadTime = consoleBootTimer_ - kCommandTypingDuration;
		if (loadTime < 0.0f) {
			loadingProgress_ = 0.0f;
		} else {
			// タイトルの全カメラ・全3Dロケーションモデルを段階的に先行読み込み・ウォームアップ！
			if (!isTitleCamerasPrewarmed_) {
				PrewarmAllTitleCamerasAndLocations(dt);
			}

			// バックグラウンドでのプリロード完了判定
			bool isStageReady = AsyncPreloadManager::GetInstance()->IsStagePreloadReady();

			// フェーズ進行度に基づく最低進捗率 (0.0f, 0.25f, 0.50f, 0.75f, 1.0f)
			float phaseProgress = 0.25f * static_cast<float>(prewarmPhase_);
			float timeProgress = loadTime / 0.85f;
			float targetProgress = (std::min)(0.95f, (std::max)(phaseProgress * 0.95f, timeProgress));

			// 全カメラ・ロケーションのウォームアップ完了 & ステージプリロード完了 & 最低バー演出時間（0.70秒）経過で 100% (1.0f) へ
			if (isTitleCamerasPrewarmed_ && isStageReady && loadTime >= 0.70f) {
				targetProgress = 1.0f;
			}

			// 滑らかにプログレスを追従
			loadingProgress_ += (targetProgress - loadingProgress_) * (std::min)(1.0f, dt * 8.0f);
			if (targetProgress >= 1.0f && (1.0f - loadingProgress_) < 0.008f) {
				loadingProgress_ = 1.0f;
			}

			// 100% 達成で complete 状態に突入
			if (loadingProgress_ >= 1.0f && !isLoadingComplete_) {
				isLoadingComplete_ = true;
				completeHoldTimer_ = 0.0f;
			}

			if (isLoadingComplete_) {
				completeHoldTimer_ += dt;
				// complete 表示が少しキープされたらメニューへスムーズに遷移
				if (completeHoldTimer_ >= kCompleteHoldDuration) {
					consoleExitTimer_ += dt;
					if (consoleExitTimer_ >= kConsoleExitDuration) {
						state_ = TitleState::Menu;
						stateTimer_ = 0.0f;
						consoleExitTimer_ = 0.0f;
						menuEnterTimer_ = kMenuEnterDuration; // メニュー起動トランジション開始！
						SetCursorVisible(true);
						if (CurrentLocation().id == TitleLocation::CrashedForest) {
							ChangeLocation(0);
						}
						// メインカメラを TitleCamera に確実に同期
						CameraManager::GetInstance()->SetActiveCamera("TitleCamera");
						UpdateLocationCamera(CurrentLocation(), 0.0f, true);
					}
				}
			}
		}

		if (consoleBgSprite_) {
			consoleBgSprite_->Update();
		}

		// 背景を完全な黒（アルファ1.0）にして格納庫の透けを完全排除
		backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		backgroundPanel_.Update();

		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();

		// 切り替えフェードアウト時の短いCRT走査線グリッチ演出（項目3）
		if (consoleExitTimer_ > 0.0f) {
			float exitRatio = consoleExitTimer_ / kConsoleExitDuration;
			ActivePostEffect scanline;
			scanline.type = PostEffectType::kScanLine;
			scanline.intensity = 0.22f * exitRatio;
			postEffect->AddActiveEffect(scanline);

			ActivePostEffect noise;
			noise.type = PostEffectType::kRandom;
			noise.intensity = 0.15f * exitRatio;
			postEffect->AddActiveEffect(noise);
		}

	} else if (state_ == TitleState::ReAuthentication) {
		// 項目10: セッション終了・再認証セキュリティ演出の更新
		SetCursorVisible(false);
		reauthTimer_ += dt;

		Input* input = Input::GetInstance();
		bool clickTriggered = input->TriggerMouse(0) || ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && !wasLButtonDown_);
		wasLButtonDown_ = ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) || input->PushMouse(0);
		bool skipTriggered = (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) || clickTriggered);

		if (skipTriggered || reauthTimer_ >= kReauthDuration) {
			state_ = TitleState::Menu;
			stateTimer_ = 0.0f;
			menuEnterTimer_ = kMenuEnterDuration;
			SetCursorVisible(true);
			if (CurrentLocation().id == TitleLocation::CrashedForest) {
				ChangeLocation(0);
			}
		}

		if (consoleBgSprite_) {
			consoleBgSprite_->Update();
		}
		backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		backgroundPanel_.Update();

	} else if (state_ == TitleState::Menu) {
		SetCursorVisible(false); // OS標準白カーソルを非表示にし、サイバーグリーンの専用タクティカルポインターを表示（項目5）
		if (menuEnterTimer_ > 0.0f) {
			menuEnterTimer_ -= dt;
		}

		Input* input = Input::GetInstance();
		HWND hwnd = input->GetHwnd();
		Input::MousePosition mousePos = input->GetMouseScreenPosition();
		float mx = static_cast<float>(mousePos.x);
		float my = static_cast<float>(mousePos.y);

		// クライアント領域スケーリング（DPIやリサイズによるゲーム解像度との不一致を完全解消）
		if (hwnd) {
			RECT rc;
			if (::GetClientRect(hwnd, &rc) && (rc.right - rc.left > 0) && (rc.bottom - rc.top > 0)) {
				float actualW = static_cast<float>(rc.right - rc.left);
				float actualH = static_cast<float>(rc.bottom - rc.top);
				mx *= (kScreenWidth / actualW);
				my *= (kScreenHeight / actualH);
			}
		}

		// マウスボタン押下判定（DirectInput ＋ Windows API の多重フォールバック）
		bool isLButtonDown = input->PushMouse(0) || ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
		bool isLButtonTrigger = input->TriggerMouse(0) || (isLButtonDown && !wasLButtonDown_);
		wasLButtonDown_ = isLButtonDown;

		// フルOSデスクトップ環境の更新（タスクバー、マルチウィンドウドラッグ・最小化・フォーカス、デスクトップアイコン、カスタムカーソル）
		UpdateOSDesktop(dt, { mx, my }, isLButtonDown, isLButtonTrigger);

		// メインターミナル（Window 0）がフォーカスされており、非最小化・表示中かつ非ドラッグ時のみメニュー操作を更新
		bool isTermActive = (activeWindowId_ == 0 && osWindows_.size() > 0 && osWindows_[0].isOpen && !osWindows_[0].isMinimized);
		if (isTermActive && !osWindows_[0].isDragging && osWindows_[0].animScale >= 0.85f) {
			selectionManager_.Update();

			// マウスクリックでのダイレクトボタン判定（万が一の判定漏れ防止用フォールバック）
			if (isLButtonTrigger) {
				const float btnX = osWindows_[0].pos.x + 65.0f;
				const float btnW = 390.0f;
				const float btnH = 46.0f;
				const float btnStartY = osWindows_[0].pos.y + 196.0f;
				const float btnSpacing = 56.0f;
				if (mx >= btnX && mx <= btnX + btnW) {
					if (my >= btnStartY && my <= btnStartY + btnH) {
						startButton_.Click();
					} else if (my >= btnStartY + btnSpacing && my <= btnStartY + btnSpacing + btnH) {
						editorButton_.Click();
					} else if (my >= btnStartY + btnSpacing * 2.0f && my <= btnStartY + btnSpacing * 2.0f + btnH) {
						settingsButton_.Click();
					} else if (my >= btnStartY + btnSpacing * 3.0f && my <= btnStartY + btnSpacing * 3.0f + btnH) {
						exitButton_.Click();
					}
				}
			}
		}

		// メニュー選択連動サブウィンドウの切替検知
		int curSelIdx = selectionManager_.GetSelectedIndex();
		if (curSelIdx != lastMenuSelIdx_) {
			lastMenuSelIdx_ = curSelIdx;
			subWindowSwitchTimer_ = kSubWindowSwitchDuration;
		}
		if (subWindowSwitchTimer_ > 0.0f) {
			subWindowSwitchTimer_ -= dt;
			if (subWindowSwitchTimer_ < 0.0f) {
				subWindowSwitchTimer_ = 0.0f;
			}
		}

		if (isEngineTesting_) {
			engineTestTimer_ -= dt;
			if (engineTestTimer_ <= 0.0f) {
				isEngineTesting_ = false;
				engineTestTimer_ = 0.0f;
			}
		}

		UpdateMenuConsoleVisuals();

		// 現在のロケーションをゆっくりOrbitする
		UpdateLocationCamera(loc, dt, true);

		// 背景オーバーレイ（背景の3D映像が美しく透ける半透明ダークガラス：圧倒的なOSデスクトップ感）
		float bgAlpha = (std::min)(stateTimer_ * 2.5f, 0.78f);
		backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, bgAlpha });
		backgroundPanel_.Update();

		// メニュー用ポストエフェクト（背景被写界深度ブラー ＋ ブルーム ＋ 走査線 ＋ スタートメニュートランジション）
		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();

		// 項目6: 背景（格納庫）の被写界深度（ブラー）- 手前UI視認性向上のため背景に軽いブラーを適用
		ActivePostEffect bgBlur;
		bgBlur.type = PostEffectType::kKawaseBlur;
		bgBlur.intensity = 1.8f;
		bgBlur.kernelSize = 1;
		postEffect->AddActiveEffect(bgBlur);

		// 高輝度部を光らせるブルーム（1パスで軽量＆鮮やか）
		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = 2.0f;
		bloom.dirX = 1.1f;
		bloom.dirY = 0.72f;
		postEffect->AddActiveEffect(bloom);

		// 繊細な走査線
		ActivePostEffect scanline;
		scanline.type = PostEffectType::kScanLine;
		scanline.intensity = 0.030f;
		postEffect->AddActiveEffect(scanline);

		// ビネット
		ActivePostEffect vignette;
		vignette.type = PostEffectType::kVignette;
		vignette.intensity = 0.65f;
		postEffect->AddActiveEffect(vignette);

		// 項目4: スタートメニュー操作時の暗転・ノイズクッション演出
		if (startMenuTransitionTimer_ > 0.0f) {
			float smRatio = startMenuTransitionTimer_ / kStartMenuTransitionDuration;
			ActivePostEffect smNoise;
			smNoise.type = PostEffectType::kRandom;
			smNoise.intensity = 0.22f * smRatio;
			postEffect->AddActiveEffect(smNoise);

			ActivePostEffect smScan;
			smScan.type = PostEffectType::kScanLine;
			smScan.intensity = 0.18f * smRatio;
			postEffect->AddActiveEffect(smScan);
		}

		menuCardPanel_.Update();
		menuHeaderPanel_.Update();
		if (titleLogoSprite_) {
			titleLogoSprite_->Update();
		}
		titleText_.Update();
		subtitleText_.Update();

	} else if (state_ == TitleState::SortieSequence) {
		SetCursorVisible(false); // OSカーソル非表示を徹底（項目9）
		sortieTimer_ += dt;
		if (sortieTimer_ >= kSortieDuration) {
			sceneID = SCENE::STAGE; // 出撃演出完了後にステージへシームレス移行（項目1）
		}

		// 背景カメラをゆっくり進める
		UpdateLocationCamera(loc, dt, true);

		// 出撃シーケンスのポストエフェクト（項目3: ボタンクリック時の波紋グリッチ ＋ アフターバーナー加速感）
		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();

		float normT = std::clamp(sortieTimer_ / kSortieDuration, 0.0f, 1.0f);

		if (sortieTimer_ < 0.35f) {
			// クリック直後の一瞬の強烈な波紋グリッチ（項目3）
			float shock = 1.0f - (sortieTimer_ / 0.35f);
			ActivePostEffect noise;
			noise.type = PostEffectType::kRandom;
			noise.intensity = 0.38f * shock;
			postEffect->AddActiveEffect(noise);

			ActivePostEffect ab;
			ab.type = PostEffectType::kChromaticAberration;
			ab.intensity = 0.025f * shock;
			postEffect->AddActiveEffect(ab);

			ActivePostEffect scanline;
			scanline.type = PostEffectType::kScanLine;
			scanline.intensity = 0.18f * shock;
			postEffect->AddActiveEffect(scanline);
		} else {
			// 黒フェードアウト進行に伴う減光処理
			float fadeProgress = 0.0f;
			if (sortieTimer_ > 1.65f) {
				fadeProgress = std::clamp((sortieTimer_ - 1.65f) / (kSortieDuration - 1.65f), 0.0f, 1.0f);
			}
			float fadeMult = 1.0f - fadeProgress * 0.75f;

			ActivePostEffect bloom;
			bloom.type = PostEffectType::kBloom;
			bloom.intensity = (1.8f + 1.8f * normT) * fadeMult;
			bloom.dirX = 1.3f;
			bloom.dirY = 0.70f;
			postEffect->AddActiveEffect(bloom);

			ActivePostEffect ab;
			ab.type = PostEffectType::kChromaticAberration;
			ab.intensity = (0.008f + 0.018f * normT) * fadeMult;
			postEffect->AddActiveEffect(ab);

			ActivePostEffect scanline;
			scanline.type = PostEffectType::kScanLine;
			scanline.intensity = 0.04f * fadeMult;
			postEffect->AddActiveEffect(scanline);
		}
	}

	// 環境切り替え（Skybox）
	if (skybox_) {
		skybox_->SetTextureIndex(loc.skyboxTexIndex);
		skybox_->SetColor(loc.skyboxColor);
		skybox_->Update();
	}

#ifdef USE_IMGUI
	ImGui::Begin("TITLE SCENE DEBUG");
	ImGui::Text("State: %s", state_ == TitleState::DroneView ? "DroneView" : "Menu");
	ImGui::Checkbox("Lock Location", &lockLocation_);
	ImGui::SameLine();
	ImGui::Checkbox("A/B Editor", &showAfterburnerEditor_);
	for (int i = 0; i < static_cast<int>(locations_.size()); ++i) {
		if (i > 0) { ImGui::SameLine(); }
		if (ImGui::RadioButton(locations_[i].name, currentLocationIndex_ == i)) {
			ChangeLocation(i);
		}
	}
	ImGui::Separator();
	LocationData& dbg = CurrentLocation();
	ImGui::Text("Env Model: %s (%s)", dbg.envModelPath.c_str(), dbg.envObject ? "LOADED" : "MISSING");
	if (ImGui::TreeNode("Transform")) {
		ImGui::DragFloat3("Origin", &dbg.origin.x, 0.1f);
		ImGui::DragFloat3("Env Scale", &dbg.envScale.x, 0.01f);
		ImGui::DragFloat3("Env Rotation", &dbg.envRotation.x, 0.01f);
		ImGui::DragFloat3("Aircraft Offset", &dbg.aircraftOffset.x, 0.05f);
		ImGui::DragFloat3("Aircraft Rotation", &dbg.aircraftRotation.x, 0.01f);
		if (dbg.debrisModel) {
			ImGui::DragFloat3("Debris Offset", &dbg.debrisOffset.x, 0.05f);
			ImGui::DragFloat3("Debris Rotation", &dbg.debrisRotation.x, 0.01f);
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Camera")) {
		ImGui::DragFloat("Distance", &dbg.camera.distance, 0.1f, 1.0f, 100.0f);
		ImGui::DragFloat("Distance Sway", &dbg.camera.distanceSway, 0.05f, 0.0f, 10.0f);
		ImGui::DragFloat("Height", &dbg.camera.height, 0.05f);
		ImGui::DragFloat("Height Sway", &dbg.camera.heightSway, 0.05f, 0.0f, 10.0f);
		ImGui::DragFloat("LookAt Height", &dbg.camera.lookAtHeight, 0.05f);
		ImGui::DragFloat("Orbit Speed", &dbg.camera.orbitSpeed, 0.005f);
		ImGui::DragFloat("Bank Tilt", &dbg.camera.bankTilt, 0.01f, 0.0f, 2.0f);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Lighting")) {
		LocationLighting& l = dbg.lighting;
		bool useDir = (l.lightType & 1) != 0, usePoint = (l.lightType & 2) != 0, useSpot = (l.lightType & 4) != 0;
		ImGui::Checkbox("Dir", &useDir); ImGui::SameLine();
		ImGui::Checkbox("Point", &usePoint); ImGui::SameLine();
		ImGui::Checkbox("Spot", &useSpot);
		l.lightType = (useDir ? 1 : 0) | (usePoint ? 2 : 0) | (useSpot ? 4 : 0);
		ImGui::ColorEdit3("Dir Color", &l.dirColor.x);
		ImGui::DragFloat3("Dir Direction", &l.dirDirection.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Dir Intensity", &l.dirIntensity, 0.01f, 0.0f, 10.0f);
		ImGui::ColorEdit3("Point Color", &l.pointColor.x);
		ImGui::DragFloat3("Point Offset", &l.pointOffset.x, 0.05f);
		ImGui::DragFloat("Point Intensity", &l.pointIntensity, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("Point Radius", &l.pointRadius, 0.1f, 0.1f, 200.0f);
		ImGui::DragFloat("Point Decay", &l.pointDecay, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Point Flicker", &l.pointFlicker, 0.01f, 0.0f, 1.0f);
		ImGui::ColorEdit3("Spot Color", &l.spotColor.x);
		ImGui::DragFloat3("Spot Offset", &l.spotOffset.x, 0.05f);
		ImGui::DragFloat3("Spot Direction", &l.spotDirection.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Spot Intensity", &l.spotIntensity, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("Spot Distance", &l.spotDistance, 0.1f, 0.1f, 200.0f);
		ImGui::DragFloat("Spot Decay", &l.spotDecay, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Spot Angle", &l.spotAngleDeg, 0.1f, 1.0f, 89.0f);
		ImGui::DragFloat("Spot Falloff Start", &l.spotFalloffStartDeg, 0.1f, 0.0f, 89.0f);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Grade")) {
		ImGui::ColorEdit3("Tint", &dbg.grade.tint.x);
		ImGui::DragFloat("Tint Intensity", &dbg.grade.tintIntensity, 0.005f, 0.0f, 1.0f);
		ImGui::DragFloat("Vignette", &dbg.grade.vignette, 0.01f, 0.0f, 3.0f);
		ImGui::ColorEdit4("Skybox Color", &dbg.skyboxColor.x);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Guide Light (Emissive)")) {
		ImGui::ColorEdit3("Guide Color", &guideEmissiveColor_.x);
		ImGui::DragFloat("Guide Intensity", &guideEmissiveIntensity_, 0.1f, 0.0f, 20.0f);
		ImGui::TreePop();
	}
	ImGui::End();

	if (showAfterburnerEditor_) {
		DrawAfterburnerEditor();
	}
#endif
}

void TitleScene::Draw() {
	if (state_ == TitleState::ConsoleBoot) {
		// コンソールブート時のみ3D描画をスキップ（黒画面からスタート）
		return;
	}

	LocationData& loc = CurrentLocation();
	Camera* titleCamera = CameraManager::GetInstance()->GetActiveCamera();

	// 墜落現場専用：「何も映さずノイズだけがいいね」
	// ドローン視点時のみ3Dは一切映さず、フラットな背景のみ描画してポストエフェクトの全面砂嵐ノイズに委ねる！
	if (loc.id == TitleLocation::CrashedForest && state_ == TitleState::DroneView) {
		if (skybox_) {
			skybox_->SetColor({ 0.18f, 0.18f, 0.20f, 1.0f });
			SkyboxCommon::GetInstance()->SetupCommonState();
			skybox_->Draw();
		}
		return;
	}

	if (skybox_) {
		if (loc.id == TitleLocation::Tunnel) {
			// 地下トンネル内は完全な暗黒空間（開口部の外側も漆黒にして天球を完全遮蔽）
			skybox_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		} else {
			skybox_->SetColor(loc.skyboxColor);
		}
		SkyboxCommon::GetInstance()->SetupCommonState();
		skybox_->Draw();
	}

	Object3dCommon::GetInstance()->SetupCommonState();

	// 背景セット（モデル未作成ならスキップ）
	if (loc.envObject) {
		loc.envObject->Draw();
	}
	for (auto& extra : loc.extraEnvParts) {
		if (extra.object) {
			extra.object->Draw();
		}
	}

	// 接地影（項目7: ソフトシャドウの濃さ・エッジ調整 - 黒潰れを解消し自然な接地感を表現）
	if (loc.id == TitleLocation::Hangar && titleCamera) {
		PrimitiveModel* pm = PrimitiveModel::GetInstance();
		MyMath::Vector3 origin = loc.origin;
		uint32_t shadowTex = (softShadowTexIndex_ != 0) ? softShadowTexIndex_ : whiteTexIndex_;
		// 主脚（左右車輪下）
		pm->DrawPlane({ 2.8f, 1.0f, 2.8f }, { 0.0f, 0.0f, 0.0f }, { origin.x - 1.4f, origin.y + 0.02f, origin.z + 0.5f }, { 0.0f, 0.0f, 0.0f, 0.32f }, shadowTex, titleCamera);
		pm->DrawPlane({ 2.8f, 1.0f, 2.8f }, { 0.0f, 0.0f, 0.0f }, { origin.x + 1.4f, origin.y + 0.02f, origin.z + 0.5f }, { 0.0f, 0.0f, 0.0f, 0.32f }, shadowTex, titleCamera);
		// 前脚（機首車輪下）
		pm->DrawPlane({ 2.0f, 1.0f, 2.0f }, { 0.0f, 0.0f, 0.0f }, { origin.x, origin.y + 0.02f, origin.z - 2.8f }, { 0.0f, 0.0f, 0.0f, 0.28f }, shadowTex, titleCamera);
		// 胴体中央のアンビエントオクルージョン影（淡いソフトフェード）
		pm->DrawPlane({ 5.5f, 1.0f, 9.5f }, { 0.0f, 0.5f, 0.0f }, { origin.x, origin.y + 0.015f, origin.z }, { 0.0f, 0.0f, 0.0f, 0.20f }, shadowTex, titleCamera);
	}

	// 墜落現場の焦げ跡・激突滑走溝デカール（項目6: 胴体着陸の長大な滑走溝と左翼激突痕）
	if (loc.id == TitleLocation::CrashedForest && titleCamera) {
		PrimitiveModel* pm = PrimitiveModel::GetInstance();
		MyMath::Vector3 origin = loc.origin;
		uint32_t scorchTex = (crashScorchTexIndex_ != 0) ? crashScorchTexIndex_ : whiteTexIndex_;
		// 激突・胴体滑走の長大な溝デカール (Scorch Trench: 手前から激突点まで伸びる黒焦げブレーキ痕)
		pm->DrawPlane({ 4.5f, 1.0f, 22.0f }, { 0.0f, -0.85f, 0.0f }, { origin.x + 4.0f, origin.y + 0.010f, origin.z - 7.0f }, { 0.01f, 0.01f, 0.01f, 0.92f }, scorchTex, titleCamera);
		// 胴体激突地点の大きな焦げ跡クレーターデカール
		pm->DrawPlane({ 10.5f, 1.0f, 16.0f }, { 0.0f, -0.8f, 0.0f }, { origin.x, origin.y + 0.012f, origin.z }, { 0.02f, 0.02f, 0.02f, 0.92f }, scorchTex, titleCamera);
		// 千切れた左翼の激突痕デカール
		pm->DrawPlane({ 6.5f, 1.0f, 9.5f }, { 0.0f, 0.6f, 0.0f }, { origin.x - 6.5f, origin.y + 0.014f, origin.z + 4.2f }, { 0.02f, 0.02f, 0.02f, 0.88f }, scorchTex, titleCamera);
	}

	// 機体・残骸は現在のロケーションのみ描画
	loc.visualModel->Draw();
	if (loc.debrisModel) {
		loc.debrisModel->Draw();
	}

	// トンネル飛行中の演出（項目9: スピード感向上 - 高密度な流動ライト＆推進炎）
	if (loc.id == TitleLocation::Tunnel && titleCamera) {
		PrimitiveModel* pm = PrimitiveModel::GetInstance();

		// 高速で壁面・天井・路面を前方に流れるリニア誘導指標ライト（過剰発光・白飛びを抑制：項目1）
		constexpr float kFlightSpeed = 130.0f;
		constexpr float kMarkerSpacing = 2.4f;
		float scrollZ = -std::fmod(animTimer_ * kFlightSpeed, kMarkerSpacing);
		for (float z = -40.0f; z <= 40.0f; z += kMarkerSpacing) {
			float markerZ = z + scrollZ;
			// 左右の壁面（X = ±6.25m, Y = 1.8m）: 適正なシアンガイドライン（項目1）
			pm->DrawPlane({ 0.14f, 1.0f, 2.8f }, { 0.0f, 0.0f, 1.5707963f }, { -6.25f, 1.8f, markerZ }, { 0.15f, 0.65f, 1.4f, 0.85f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
			pm->DrawPlane({ 0.14f, 1.0f, 2.8f }, { 0.0f, 0.0f, 1.5707963f }, {  6.25f, 1.8f, markerZ }, { 0.15f, 0.65f, 1.4f, 0.85f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);

			// 天井誘導ライン（X = 0.0m, Y = 4.8m）: 適正な黄色ライン（白飛び防止：項目1）
			pm->DrawPlane({ 0.50f, 1.0f, 2.4f }, { 3.14159265f, 0.0f, 0.0f }, { 0.0f, 4.8f, markerZ }, { 1.25f, 0.95f, 0.25f, 0.85f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
			pm->DrawPlane({ 1.10f, 1.0f, 2.6f }, { 3.14159265f, 0.0f, 0.0f }, { 0.0f, 4.805f, markerZ }, { 0.45f, 0.35f, 0.08f, 0.35f }, softShadowTexIndex_ != 0 ? softShadowTexIndex_ : whiteTexIndex_, titleCamera, BlendMode::kAdd);

			// 路面高速流動マーカー（X = ±2.5m, Y = 0.03m）: 適正な黄色マーカー
			pm->DrawPlane({ 0.12f, 1.0f, 1.8f }, { 0.0f, 0.0f, 0.0f }, { -2.5f, 0.03f, markerZ }, { 1.10f, 0.85f, 0.20f, 0.80f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
			pm->DrawPlane({ 0.12f, 1.0f, 1.8f }, { 0.0f, 0.0f, 0.0f }, {  2.5f, 0.03f, markerZ }, { 1.10f, 0.85f, 0.20f, 0.80f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
		}

		// スラスター推進器噴射炎（Afterburner クラスによる単発エンジン・ショックダイヤモンド高品位描画）
		if (afterburner_) {
			afterburner_->Draw(currentAircraftMatrix_, titleCamera);

			// ノズル開口部内部のプラズマ発光ディスク＆照り返しグロー（過剰発光を抑え、機体形状をくっきり見せる：項目1）
			MyMath::Vector3 localNozzleCore = { 0.0f, 0.011f, 14.85f };
			MyMath::Vector3 worldNozzleCore = MyMath::TransformV3(localNozzleCore, currentAircraftMatrix_);
			MyMath::Vector3 nozzleRot = loc.aircraftRotation;
			// ノズル中心のプラズマディスク（開口部サイズジャストに調整）
			pm->DrawPlane({ 0.35f, 1.0f, 0.35f }, { 1.5707963f, nozzleRot.y, 0.0f }, worldNozzleCore, { 1.2f, 1.6f, 2.2f, 0.85f }, softShadowTexIndex_ != 0 ? softShadowTexIndex_ : whiteTexIndex_, titleCamera, BlendMode::kAdd);
			// ノズル外周の控えめなグローハロ
			pm->DrawPlane({ 0.70f, 1.0f, 0.70f }, { 1.5707963f, nozzleRot.y, 0.0f }, worldNozzleCore, { 0.15f, 0.45f, 1.0f, 0.35f }, softShadowTexIndex_ != 0 ? softShadowTexIndex_ : whiteTexIndex_, titleCamera, BlendMode::kAdd);
		}
	}
}

// ============================================================
// ロケーション関連ヘルパー
// ============================================================
namespace {
	static std::unordered_map<std::string, bool> s_assetExistsCache;
	/// ModelManager と同じ規則（そのまま or assets/ 付加）でファイル存在を確認（結果を静的キャッシュ）
	bool TitleAssetExists(const std::string& path) {
		auto it = s_assetExistsCache.find(path);
		if (it != s_assetExistsCache.end()) {
			return it->second;
		}
		bool exists = std::filesystem::exists(path) || std::filesystem::exists("assets/" + path);
		s_assetExistsCache[path] = exists;
		return exists;
	}
	constexpr float kDegToRad = 3.14159265f / 180.0f;
}

void TitleScene::SetupLocation(LocationData& loc, Camera* camera) {
	Object3dCommon* common = Object3dCommon::GetInstance();

	// 背景セットモデル（存在しなければ描画しない＝モデル制作前でも動作する）
	if (!loc.envModelPath.empty()) {
		if (TitleAssetExists(loc.envModelPath)) {
			ModelManager::GetInstance()->LoadModel(loc.envModelPath);
			loc.envObject = std::make_unique<Object3d>();
			loc.envObject->Initialize(common);
			loc.envObject->SetModel(loc.envModelPath);
			loc.envObject->SetCamera(camera);
			loc.envObject->SetBoundingRadius(200.0f); // 大きなセットがLODカリングされないように
		} else {
			OutputDebugStringA(("[TitleScene] Env model not found (skip): " + loc.envModelPath + "\n").c_str());
		}
	}

	// 追加の環境パーツ（道路、誘導灯、回転灯など）
	for (auto& extra : loc.extraEnvParts) {
		if (!extra.modelPath.empty()) {
			if (TitleAssetExists(extra.modelPath)) {
				ModelManager::GetInstance()->LoadModel(extra.modelPath);
				extra.object = std::make_unique<Object3d>();
				extra.object->Initialize(common);
				extra.object->SetModel(extra.modelPath);
				extra.object->SetCamera(camera);
				extra.object->SetBoundingRadius(200.0f);
			} else {
				OutputDebugStringA(("[TitleScene] Extra env model not found (skip): " + extra.modelPath + "\n").c_str());
			}
		}
	}

	// 機体
	loc.visualModel = std::make_unique<AircraftVisualModel>();
	loc.visualModel->Initialize(common, camera);
	loc.visualModel->SetupFromSingleModel(loc.aircraftModelPath);
	loc.visualModel->SetPropellerRpm(loc.propellerRpm);
	if (loc.id == TitleLocation::Tunnel) {
		// トンネル内では前進（+Z方向）に向けるため、モデルの自動180度回転をキャンセル
		loc.visualModel->SetBaseRotation({ 0.0f, 0.0f, 0.0f });
	}
	for (DamagePart part : loc.hiddenParts) {
		loc.visualModel->SetPartVisible(part, false);
	}

	// 千切れたパーツ：本体で消したパーツ「だけ」を表示する別インスタンス
	if (!loc.hiddenParts.empty()) {
		loc.debrisModel = std::make_unique<AircraftVisualModel>();
		loc.debrisModel->Initialize(common, camera);
		loc.debrisModel->SetupFromSingleModel(loc.aircraftModelPath);
		loc.debrisModel->SetPropellerRpm(0.0f);
		for (int i = 0; i < static_cast<int>(DamagePart::Count); ++i) {
			loc.debrisModel->SetPartVisible(static_cast<DamagePart>(i), false);
		}
		for (DamagePart part : loc.hiddenParts) {
			loc.debrisModel->SetPartVisible(part, true);
		}
	}

	// キャノピー（ガラス部分）の半透明・高環境反射・鋭角スペキュラ設定（項目4）
	loc.visualModel->SetupCanopyMaterial();

	// ロケーション別マテリアルの質感設定（項目8: 墜落現場ダメージ、項目9: 重厚なジュラルミン金属感とパネルライン強調）
	if (loc.id == TitleLocation::Hangar) {
		// 格納庫: 軍用機らしいジュラルミン・メタリック質感（モデル本来のカラーを保護）
		loc.visualModel->SetMaterialProperties(48.0f, 1.35f, 0.45f);
	} else if (loc.id == TitleLocation::Tunnel) {
		// トンネル: スラスター光を反射する金属肌（モデル本来のカラーを保護）
		loc.visualModel->SetMaterialProperties(40.0f, 1.20f, 0.40f);
	} else if (loc.id == TitleLocation::CrashedForest) {
		// 墜落現場: 激突・炎上による黒焦げ・煤煙テクスチャを鮮明に映し出すダークトーン（項目8）
		loc.visualModel->SetMaterialProperties(16.0f, 0.25f, 0.05f);
		loc.visualModel->SetMaterialColor({ 0.70f, 0.68f, 0.68f, 1.0f });
		// 残存した右主翼や昇降舵を激突の衝撃力で歪ませる（激突の説得力向上：項目8）
		loc.visualModel->SetPartLocalTransform(DamagePart::Wing_R, { 1.0f, 1.0f, 1.0f }, { 0.08f, -0.05f, 0.12f }, { 0.0f, 0.04f, -0.02f });
		loc.visualModel->SetPartLocalTransform(DamagePart::Wing1_R, { 1.0f, 1.0f, 1.0f }, { 0.10f, -0.06f, 0.15f }, { 0.0f, 0.06f, -0.03f });
		loc.visualModel->SetPartLocalTransform(DamagePart::Elevator1, { 1.0f, 1.0f, 1.0f }, { -0.22f, 0.0f, 0.08f }, { 0.0f, -0.03f, 0.0f });
		if (loc.debrisModel) {
			loc.debrisModel->SetMaterialProperties(14.0f, 0.22f, 0.05f);
			loc.debrisModel->SetMaterialColor({ 0.65f, 0.63f, 0.63f, 1.0f });
		}
	}
}

void TitleScene::ApplyLocationLighting(const LocationData& loc) {
	Object3dCommon* common = Object3dCommon::GetInstance();
	const LocationLighting& l = loc.lighting;
	common->SetLightType(l.lightType);

	if (DirectionalLight* dir = common->GetDirectionalLightData()) {
		dir->color = l.dirColor;
		dir->direction = MyMath::Normalize(l.dirDirection);
		dir->intensity = l.dirIntensity;
	}

	if (PointLight* point = common->GetPointLightData()) {
		float intensity = l.pointIntensity;
		if (l.pointFlicker > 0.0f) {
			// 複数の正弦波を重ねて不規則な炎のゆらぎを作る（0〜1）
			float n = 0.5f
				+ 0.25f * std::sin(animTimer_ * 13.0f)
				+ 0.15f * std::sin(animTimer_ * 29.0f + 1.3f)
				+ 0.10f * std::sin(animTimer_ * 47.0f + 2.1f);
			intensity *= (1.0f - l.pointFlicker) + l.pointFlicker * n * 2.0f;
		}
		point->color = l.pointColor;
		point->position = MyMath::Add(loc.origin, l.pointOffset);
		point->intensity = intensity;
		point->radius = l.pointRadius;
		point->decay = l.pointDecay;
	}

	if (SpotLight* spot = common->GetSpotLightData()) {
		spot->color = l.spotColor;
		spot->position = MyMath::Add(loc.origin, l.spotOffset);
		spot->direction = MyMath::Normalize(l.spotDirection);
		spot->intensity = l.spotIntensity;
		spot->distance = l.spotDistance;
		spot->decay = l.spotDecay;
		spot->cosAngle = std::cos(l.spotAngleDeg * kDegToRad);
		spot->cosFalloffStart = std::cos(l.spotFalloffStartDeg * kDegToRad);
	}
}

void TitleScene::UpdateLocationCamera(const LocationData& loc, float dt, bool menuMode, Camera* targetCamera) {
	Camera* titleCamera = targetCamera ? targetCamera : CameraManager::GetInstance()->GetActiveCamera();
	if (!titleCamera) { return; }

	const LocationCamera& c = loc.camera;

	// メニュー中は少し引いてゆっくり、傾きも穏やかに
	float orbitSpeed = menuMode ? c.orbitSpeed * 1.5f : c.orbitSpeed;
	float distance = menuMode ? c.distance * 1.15f : c.distance + std::sin(animTimer_ * 1.5f) * c.distanceSway;
	float height = c.height + std::sin(animTimer_ * (menuMode ? 0.5f : 2.0f)) * c.heightSway;
	float bankTilt = menuMode ? c.bankTilt * 0.4f : c.bankTilt;

	cameraTheta_ += orbitSpeed * dt;

	if (c.isTunnelCamera) {
		// トンネル専用チェイスカメラワーク：
		// 機体後方（-Z側）から単発アフターバーナーノズル・ショックダイヤモンド・推進炎を正面に捉える
		float swayX = std::sin(cameraTheta_) * 1.3f;
		float currentDist = menuMode ? c.distance * 1.15f : (c.distance + std::sin(animTimer_ * 0.8f) * c.distanceSway);
		float currentH = c.height + std::sin(animTimer_ * 1.1f) * c.heightSway;

		// 機体およびノズルのワールド位置
		// m21.gltf のローカルノズル位置は Z = +14.95m。aircraftRotation (0, π, 0) によりワールドでは Z = -14.95m
		MyMath::Vector3 acPos = MyMath::Add(loc.origin, loc.aircraftOffset);
		float nozzleOffsetZ = afterburner_ ? afterburner_->GetSingleNozzleOffset().z : 14.95f;
		float nozzleOffsetY = afterburner_ ? afterburner_->GetSingleNozzleOffset().y : 0.011f;
		float nozzleOffsetX = afterburner_ ? afterburner_->GetSingleNozzleOffset().x : 0.0f;
		// 機体が 180度回転 (aircraftRotation.y = π) しているため、ローカル+Zはワールド-Z
		MyMath::Vector3 nozzleCenter = { acPos.x - nozzleOffsetX, acPos.y + nozzleOffsetY, acPos.z - nozzleOffsetZ };

		// 注視点：ノズル出口から少し機体側（+Z方向）に入った尾部〜主翼根元付近を注視
		MyMath::Vector3 target;
		target.x = nozzleCenter.x;
		target.y = nozzleCenter.y + c.lookAtHeight;
		target.z = nozzleCenter.z + 2.8f; // ノズルの少し前方（機体内部寄り）を見通すことで自然な構図に

		// カメラ位置：ノズルおよび噴射炎のさらに後方（-Z側）に配置し、少し斜め上方から見下ろす
		MyMath::Vector3 camPos;
		camPos.x = target.x + 0.35f + swayX; // わずかに斜め後方にオフセットして立体感を強調
		camPos.y = nozzleCenter.y + currentH;
		camPos.z = nozzleCenter.z - currentDist; // ノズルから currentDist (約8.2m) 後方

		float dx = target.x - camPos.x;
		float dy = target.y - camPos.y;
		float dz = target.z - camPos.z;
		float yaw = std::atan2(dx, dz);
		float pitch = std::atan2(-dy, std::sqrt(dx * dx + dz * dz));

		// ドローン挙動：左右移動速度（cos）に合わせてカメラを自然にバンク傾き
		float swayVx = std::cos(cameraTheta_) * 1.3f;
		float droneRoll = -swayVx * bankTilt * 0.05f;

		titleCamera->SetTranslate(camPos);
		titleCamera->SetRotate({ pitch, yaw, droneRoll });
		titleCamera->Update();
		return;
	}

	MyMath::Vector3 target = MyMath::Add(loc.origin, loc.aircraftOffset);
	target.y += c.lookAtHeight;

	MyMath::Vector3 camPos;
	camPos.x = target.x + std::cos(cameraTheta_) * distance;
	camPos.y = target.y + height;
	camPos.z = target.z + std::sin(cameraTheta_) * distance;

	// 方向ベクトルの正規化
	MyMath::Vector3 toTarget = MyMath::Subtract(target, camPos);
	MyMath::Vector3 dir = MyMath::Normalize(toTarget);

	// MakeAffineMatrix (Rx * Ry * Rz) に厳密に一致するオイラー角の幾何学解
	// これにより周回時のYaw/Pitchクロスカップリングによる不自然な前後ジッター・ガタつきを根本根絶
	float yaw = std::atan2(dir.x, dir.z);
	float clampedY = std::clamp(dir.y, -0.999f, 0.999f);
	float pitch = -std::asin(clampedY);

	// ドローン挙動：旋回移動による向心バンク傾き（手ぶれ・振動はゼロ、滑らかな旋回傾き）
	float baseRoll = -orbitSpeed * bankTilt * 1.2f;
	float dynamicRoll = baseRoll + std::cos(cameraTheta_) * 0.015f * bankTilt;

	// 墜落現場専用：衝撃で故障しかけた監視カメラの電気的スタッター・微小ブレ
	if (loc.id == TitleLocation::CrashedForest) {
		if (rand() % 100 < 8) { // 8%の確率でカクッとノイズブレ
			float shakeAmt = (static_cast<float>(rand() % 100) - 50.0f) * 0.003f;
			camPos.x += shakeAmt;
			camPos.y += shakeAmt * 0.7f;
			dynamicRoll += shakeAmt * 2.2f;
		}
		float subtleVibe = std::sin(animTimer_ * 28.0f) * 0.006f;
		camPos.y += subtleVibe;
	}

	titleCamera->SetTranslate(camPos);
	titleCamera->SetRotate({ pitch, yaw, dynamicRoll });
	titleCamera->Update();
}

void TitleScene::PrewarmAssets() {
	PrewarmAllTitleCamerasAndLocations(0.016f);
}

void TitleScene::PrewarmAllTitleCamerasAndLocations(float dt) {
	CameraManager* cm = CameraManager::GetInstance();
	if (!cm) { return; }

	// 1. メインの TitleCamera を取得または作成
	if (!cm->HasCamera("TitleCamera")) {
		cm->CreateCamera("TitleCamera");
	}
	Camera* titleCamera = cm->GetCamera("TitleCamera");
	if (!titleCamera) {
		titleCamera = cm->GetActiveCamera();
	}

	// 2. タイトルの全ロケーション専用カメラ（Hangar, Tunnel, CrashedForest）を作成・登録
	const char* camNames[] = {
		"TitleCamera_Hangar",
		"TitleCamera_Tunnel",
		"TitleCamera_CrashedForest"
	};

	for (int i = 0; i < 3; ++i) {
		if (!cm->HasCamera(camNames[i])) {
			cm->CreateCamera(camNames[i]);
		}
	}

	if (prewarmPhase_ == 0) {
		// ============================================================
		// Phase 0: タイトルの全カメラ（周回・定点・Live Feed用）のパラメータ計算・更新
		// ============================================================
		for (size_t i = 0; i < locations_.size() && i < 3; ++i) {
			const LocationData& loc = locations_[i];
			Camera* cam = cm->GetCamera(camNames[i]);
			if (cam) {
				UpdateLocationCamera(loc, 0.0f, false, cam);
			}
		}

		// メインカメラ (TitleCamera) をメニュー遷移時ロケーションに合わせて先行更新
		if (!locations_.empty()) {
			int targetLocIdx = (CurrentLocation().id == TitleLocation::CrashedForest) ? 0 : currentLocationIndex_;
			if (targetLocIdx >= 0 && targetLocIdx < static_cast<int>(locations_.size())) {
				UpdateLocationCamera(locations_[targetLocIdx], 0.0f, true, titleCamera);
			}
		}

		// Skybox と Afterburner を TitleCamera に接続・更新
		if (skybox_) {
			skybox_->SetCamera(titleCamera);
			skybox_->Update();
		}
		if (afterburner_) {
			afterburner_->Update(dt, afterburner_->GetIntensity());
		}

		loadingPhaseText_ = "PREWARMING HANGAR & CRASHED FOREST ASSETS...";
		prewarmPhase_ = 1;
		OutputDebugStringA("[TitleScene] Loading Phase 0 complete: All Title Cameras & Optical Links initialized.\n");
	} else if (prewarmPhase_ == 1) {
		// ============================================================
		// Phase 1: Hangar (CH 01) & CrashedForest (CH 03) の全オブジェクト行列更新
		// ============================================================
		for (size_t i : { static_cast<size_t>(TitleLocation::Hangar), static_cast<size_t>(TitleLocation::CrashedForest) }) {
			if (i < locations_.size()) {
				LocationData& loc = locations_[i];
				if (titleCamera) {
					if (loc.envObject) loc.envObject->SetCamera(titleCamera);
					for (auto& extra : loc.extraEnvParts) {
						if (extra.object) extra.object->SetCamera(titleCamera);
					}
					if (loc.visualModel) loc.visualModel->SetCamera(titleCamera);
					if (loc.debrisModel) loc.debrisModel->SetCamera(titleCamera);
				}

				// 機体モデルの行列計算
				MyMath::Vector3 pos = MyMath::Add(loc.origin, loc.aircraftOffset);
				Matrix4x4 acMat = MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, loc.aircraftRotation, pos);
				if (loc.visualModel) {
					loc.visualModel->Update(acMat, dt);
				}

				// 残骸モデルの行列計算
				if (loc.debrisModel) {
					MyMath::Vector3 debrisPos = MyMath::Add(loc.origin, loc.debrisOffset);
					Matrix4x4 debMat = MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, loc.debrisRotation, debrisPos);
					loc.debrisModel->Update(debMat, dt);
				}

				// 背景セットモデル
				if (loc.envObject) {
					loc.envObject->SetScale(loc.envScale);
					loc.envObject->SetRotate(loc.envRotation);
					loc.envObject->SetTranslate(loc.origin);
					loc.envObject->Update();
				}

				// 追加パーツ群
				for (auto& extra : loc.extraEnvParts) {
					if (extra.object) {
						MyMath::Vector3 ep = MyMath::Add(loc.origin, extra.offset);
						extra.object->SetScale(extra.scale);
						extra.object->SetRotate(extra.rotation);
						extra.object->SetTranslate(ep);
						extra.object->Update();
					}
				}
			}
		}

		loadingPhaseText_ = "SYNCHRONIZING TUNNEL PIPELINES & GPU BUFFERS...";
		prewarmPhase_ = 2;
		OutputDebugStringA("[TitleScene] Loading Phase 1 complete: Hangar & Crashed Forest 3D assets prewarmed.\n");
	} else if (prewarmPhase_ == 2) {
		// ============================================================
		// Phase 2: Tunnel (CH 02) の全オブジェクト（31セグメント約100パーツ）更新 & テクスチャフラッシュ
		// ============================================================
		if (static_cast<size_t>(TitleLocation::Tunnel) < locations_.size()) {
			LocationData& loc = locations_[static_cast<size_t>(TitleLocation::Tunnel)];
			if (titleCamera) {
				if (loc.envObject) loc.envObject->SetCamera(titleCamera);
				for (auto& extra : loc.extraEnvParts) {
					if (extra.object) extra.object->SetCamera(titleCamera);
				}
				if (loc.visualModel) loc.visualModel->SetCamera(titleCamera);
			}

			// 機体モデルの行列計算
			MyMath::Vector3 pos = MyMath::Add(loc.origin, loc.aircraftOffset);
			Matrix4x4 acMat = MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, loc.aircraftRotation, pos);
			if (loc.visualModel) {
				loc.visualModel->Update(acMat, dt);
			}

			// トンネル中央背景
			if (loc.envObject) {
				loc.envObject->SetScale(loc.envScale);
				loc.envObject->SetRotate(loc.envRotation);
				loc.envObject->SetTranslate(loc.origin);
				loc.envObject->Update();
			}

			// トンネルの全セグメントパーツ（約100パーツ）
			for (auto& extra : loc.extraEnvParts) {
				if (extra.object) {
					MyMath::Vector3 ep = MyMath::Add(loc.origin, extra.offset);
					extra.object->SetScale(extra.scale);
					extra.object->SetRotate(extra.rotation);
					extra.object->SetTranslate(ep);
					extra.object->Update();
				}
			}
		}

		// GPUテクスチャアップロードの同期完了
		DirectXCommon::GetInstance()->FlushTextureUploads();

		loadingPhaseText_ = "FINALIZING ALL TITLE CAMERAS & DESKTOP ROOT...";
		prewarmPhase_ = 3;
		OutputDebugStringA("[TitleScene] Loading Phase 2 complete: Tunnel 100+ segments & GPU textures flushed.\n");
	} else if (prewarmPhase_ == 3) {
		// ============================================================
		// Phase 3: 全カメラ・UIの最終確定
		// ============================================================
		menuCardPanel_.Update();
		menuHeaderPanel_.Update();
		if (consoleBgSprite_) { consoleBgSprite_->Update(); }

		// 現在ロケーションのカメラを再確定
		int targetLocIdx = (CurrentLocation().id == TitleLocation::CrashedForest) ? 0 : currentLocationIndex_;
		if (targetLocIdx >= 0 && targetLocIdx < static_cast<int>(locations_.size())) {
			UpdateLocationCamera(locations_[targetLocIdx], 0.0f, true, titleCamera);
		}

		loadingPhaseText_ = "ALL TITLE CAMERAS SYNCHRONIZED";
		prewarmPhase_ = 4;
		isTitleCamerasPrewarmed_ = true;
		OutputDebugStringA("[TitleScene] Loading Phase 3 complete: ALL TITLE CAMERAS & LOCATIONS FULLY SYNCHRONIZED!\n");
	}
}


void TitleScene::ChangeLocation(int index) {
	if (locations_.empty()) { return; }
	currentLocationIndex_ = index % static_cast<int>(locations_.size());
	cutTimer_ = 0.0f;
	transitionTimer_ = kTransitionDuration; // カメラ切替砂嵐トランジションを発動（項目10）
	cameraTheta_ = static_cast<float>(rand() % 628) * 0.01f; // ランダムな角度から映す
	LocationData& loc = CurrentLocation();
	ApplyLocationLighting(loc);
	if (skybox_) {
		skybox_->SetTextureIndex(loc.skyboxTexIndex);
		skybox_->SetColor(loc.skyboxColor);
	}
}

void TitleScene::InitializeMenuCard(SpriteCommon* spriteCommon) {
	menuBorderSprites_.clear();
	menuBorderOffsets_.clear();
	buttonBorders_.clear();
	buttonBorderOutlineOffsets_.clear();
	buttonBorderBracketOffsets_.clear();
	buttonBorderIndicatorOffsets_.clear();

	const float cardWidth = 520.0f;
	const float cardHeight = 616.0f;
	const float cardX = menuWindowPos_.x; // 380.0f
	const float cardY = menuWindowPos_.y; // 52.0f

	auto addBorderLine = [this, spriteCommon, cardX, cardY](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition(pos);
		sp->SetSize(size);
		sp->SetColor(color);
		sp->Update();
		menuBorderOffsets_.push_back({ pos.x - cardX, pos.y - cardY });
		menuBorderSprites_.push_back(std::move(sp));
	};

	// 0. ウィンドウ立体ドロップシャドウ（背後のぼかし背景からウィンドウを美しく浮かび上がらせる）
	addBorderLine({ cardX + 8.0f, cardY + 10.0f }, { cardWidth, cardHeight }, { 0.0f, 0.0f, 0.0f, 0.75f });

	// 1. ターミナルウィンドウ本体背景パネル（完全な漆黒・純黒ブラック、不透明度100%）
	menuCardPanel_.Initialize(spriteCommon);
	menuCardPanel_.SetPosition({ cardX, cardY });
	menuCardPanel_.SetSize({ cardWidth, cardHeight });
	menuCardPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f }); // 100%不透明の完全黒背景

	// 2. ターミナルウィンドウ・タイトルバー背景パネル
	menuHeaderPanel_.Initialize(spriteCommon);
	menuHeaderPanel_.SetPosition({ cardX, cardY });
	menuHeaderPanel_.SetSize({ cardWidth, 32.0f });
	menuHeaderPanel_.SetBackgroundColor({ 0.03f, 0.10f, 0.06f, 1.0f }); // 重厚なダークグリーンのタイトルバー

	// 3. タイトルバーのステータスLEDインジケーターランプ
	headerLampSprite_ = std::make_unique<Sprite>();
	headerLampSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
	headerLampSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	headerLampSprite_->SetPosition({ cardX + 14.0f, cardY + 12.0f });
	headerLampSprite_->SetSize({ 8.0f, 8.0f });
	headerLampSprite_->SetColor({ 0.15f, 1.0f, 0.35f, 0.95f });
	headerLampSprite_->Update();

	// 4. タイトルバー右側のシステムステータスバッジ（誤解を招く飾りボタン [_][口][X] を廃止し、洗練されたOSステータス枠へ刷新）
	const float badgeX = cardX + cardWidth - 110.0f;
	const float badgeY = cardY + 7.0f;
	const float badgeW = 96.0f;
	const float badgeH = 18.0f;
	addBorderLine({ badgeX, badgeY }, { badgeW, badgeH }, { 0.04f, 0.14f, 0.08f, 0.90f });
	addBorderLine({ badgeX, badgeY }, { badgeW, 1.0f }, { 0.22f, 0.75f, 0.42f, 0.85f });
	addBorderLine({ badgeX, badgeY + badgeH - 1.0f }, { badgeW, 1.0f }, { 0.22f, 0.75f, 0.42f, 0.85f });
	addBorderLine({ badgeX, badgeY }, { 1.0f, badgeH }, { 0.22f, 0.75f, 0.42f, 0.85f });
	addBorderLine({ badgeX + badgeW - 1.0f, badgeY }, { 1.0f, badgeH }, { 0.22f, 0.75f, 0.42f, 0.85f });

	// 5. ウィンドウ外周境界線 (太さ 2px)
	const MyMath::Vector4 borderColor = { 0.18f, 0.82f, 0.45f, 0.85f };
	const MyMath::Vector4 cornerColor = { 0.35f, 1.0f, 0.65f, 1.00f };
	const float cLen = 20.0f;
	const float cThick = 3.0f;

	addBorderLine({ cardX, cardY }, { cardWidth, 2.0f }, borderColor);
	addBorderLine({ cardX, cardY + cardHeight - 2.0f }, { cardWidth, 2.0f }, borderColor);
	addBorderLine({ cardX, cardY }, { 2.0f, cardHeight }, borderColor);
	addBorderLine({ cardX + cardWidth - 2.0f, cardY }, { 2.0f, cardHeight }, borderColor);

	// 四隅のコーナーブラケット (太さ 3px)
	addBorderLine({ cardX, cardY }, { cLen, cThick }, cornerColor);
	addBorderLine({ cardX, cardY }, { cThick, cLen }, cornerColor);
	addBorderLine({ cardX + cardWidth - cLen, cardY }, { cLen, cThick }, cornerColor);
	addBorderLine({ cardX + cardWidth - cThick, cardY }, { cThick, cLen }, cornerColor);
	addBorderLine({ cardX, cardY + cardHeight - cThick }, { cLen, cThick }, cornerColor);
	addBorderLine({ cardX, cardY + cardHeight - cLen }, { cThick, cLen }, cornerColor);
	addBorderLine({ cardX + cardWidth - cLen, cardY + cardHeight - cThick }, { cLen, cThick }, cornerColor);
	addBorderLine({ cardX + cardWidth - cThick, cardY + cardHeight - cLen }, { cThick, cLen }, cornerColor);

	// タイトルバー下部区切り線 (太さ 2px, Y = cardY + 32.0f)
	addBorderLine({ cardX, cardY + 32.0f }, { cardWidth, 2.0f }, { 0.20f, 0.85f, 0.50f, 0.80f });

	// タイトル下の水平区切り線 (Y = cardY + 172.0f)
	addBorderLine({ cardX + 24.0f, cardY + 172.0f }, { cardWidth - 48.0f, 1.0f }, { 0.18f, 0.75f, 0.45f, 0.45f });

	// 操作案内枠（Y=482、高さ38px、幅 cardWidth - 50px）
	const float gfX = cardX + 25.0f;
	const float gfY = cardY + 430.0f;
	const float gfW = cardWidth - 50.0f;
	const float gfH = 38.0f;
	const MyMath::Vector4 gfCorner = { 0.22f, 0.85f, 0.50f, 0.65f };
	addBorderLine({ gfX, gfY }, { 10.0f, 1.0f }, gfCorner);
	addBorderLine({ gfX, gfY }, { 1.0f, 10.0f }, gfCorner);
	addBorderLine({ gfX + gfW - 10.0f, gfY }, { 10.0f, 1.0f }, gfCorner);
	addBorderLine({ gfX + gfW - 1.0f, gfY }, { 1.0f, 10.0f }, gfCorner);
	addBorderLine({ gfX, gfY + gfH - 1.0f }, { 10.0f, 1.0f }, gfCorner);
	addBorderLine({ gfX, gfY + gfH - 10.0f }, { 1.0f, 10.0f }, gfCorner);
	addBorderLine({ gfX + gfW - 10.0f, gfY + gfH - 1.0f }, { 10.0f, 1.0f }, gfCorner);
	addBorderLine({ gfX + gfW - 1.0f, gfY + gfH - 10.0f }, { 1.0f, 10.0f }, gfCorner);

	// ウィンドウ最下部ステータスバー背景＆境界線 (高さ 26px)
	const float sbY = cardY + cardHeight - 26.0f;
	addBorderLine({ cardX, sbY }, { cardWidth, 26.0f }, { 0.02f, 0.07f, 0.04f, 1.0f });
	addBorderLine({ cardX, sbY }, { cardWidth, 1.0f }, { 0.18f, 0.70f, 0.40f, 0.60f });

	// ------------------------------------------------------------
	// 各ボタンのアウトライン枠線＆サイバーブラケット（buttonBorders_）
	// ------------------------------------------------------------
	const float btnW = 390.0f;
	const float btnH = 44.0f;
	const float btnX = cardX + 65.0f;
	const float btnStartY = cardY + 196.0f;
	const float btnSpacing = 56.0f;
	const float bcLen = 8.0f;
	const float bcThick = 2.0f;

	auto createLineSprite = [spriteCommon](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition(pos);
		sp->SetSize(size);
		sp->SetColor(color);
		sp->Update();
		return sp;
	};

	for (int i = 0; i < 4; ++i) {
		float by = btnStartY + btnSpacing * static_cast<float>(i);
		ButtonBorderGroup group;
		std::vector<Vector2> outOffsets;
		std::vector<Vector2> brkOffsets;

		auto addOutline = [&](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
			outOffsets.push_back({ pos.x - cardX, pos.y - cardY });
			group.outlines.push_back(createLineSprite(pos, size, color));
		};
		auto addBracket = [&](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
			brkOffsets.push_back({ pos.x - cardX, pos.y - cardY });
			group.brackets.push_back(createLineSprite(pos, size, color));
		};

		// 外周枠線 (1px) 4本
		addOutline({ btnX, by }, { btnW, 1.0f }, { 0.15f, 0.50f, 0.30f, 0.35f });
		addOutline({ btnX, by + btnH - 1.0f }, { btnW, 1.0f }, { 0.15f, 0.50f, 0.30f, 0.35f });
		addOutline({ btnX, by }, { 1.0f, btnH }, { 0.15f, 0.50f, 0.30f, 0.35f });
		addOutline({ btnX + btnW - 1.0f, by }, { 1.0f, btnH }, { 0.15f, 0.50f, 0.30f, 0.35f });

		// 四隅ブラケット (2px) 8本
		addBracket({ btnX, by }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX, by }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX + btnW - bcLen, by }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX + btnW - bcThick, by }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX, by + btnH - bcThick }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX, by + btnH - bcLen }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX + btnW - bcLen, by + btnH - bcThick }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f });
		addBracket({ btnX + btnW - bcThick, by + btnH - bcLen }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f });

		// ボタン左端内側のソリッド・アクティブバー（ボタン枠線と完璧にアライン・統合）
		buttonBorderIndicatorOffsets_.push_back({ (btnX + 1.0f) - cardX, (by + 2.0f) - cardY });
		group.indicator = createLineSprite({ btnX + 1.0f, by + 2.0f }, { 3.0f, btnH - 4.0f }, { 0.08f, 0.25f, 0.15f, 0.25f });

		buttonBorderOutlineOffsets_.push_back(std::move(outOffsets));
		buttonBorderBracketOffsets_.push_back(std::move(brkOffsets));
		buttonBorders_.push_back(std::move(group));
	}
}

void TitleScene::UpdateMenuWindowPosition(const Vector2& newPos) {
	menuWindowPos_ = newPos;

	menuCardPanel_.SetPosition(menuWindowPos_);
	menuCardPanel_.Update();

	menuHeaderPanel_.SetPosition(menuWindowPos_);
	menuHeaderPanel_.Update();

	if (headerLampSprite_) {
		headerLampSprite_->SetPosition({ menuWindowPos_.x + 14.0f, menuWindowPos_.y + 12.0f });
		headerLampSprite_->Update();
	}

	for (size_t i = 0; i < menuBorderSprites_.size() && i < menuBorderOffsets_.size(); ++i) {
		if (menuBorderSprites_[i]) {
			menuBorderSprites_[i]->SetPosition({ menuWindowPos_.x + menuBorderOffsets_[i].x, menuWindowPos_.y + menuBorderOffsets_[i].y });
			menuBorderSprites_[i]->Update();
		}
	}

	if (titleLogoSprite_) {
		titleLogoSprite_->SetPosition({ menuWindowPos_.x + 260.0f, menuWindowPos_.y + 106.0f });
		titleLogoSprite_->SetSize({ 410.0f, 91.8f });
		titleLogoSprite_->Update();
	}

	subtitleText_.SetPosition({ menuWindowPos_.x + 260.0f, menuWindowPos_.y + 154.0f });
	subtitleText_.Update();

	const float btnX = menuWindowPos_.x + 65.0f;
	const float btnStartY = menuWindowPos_.y + 196.0f;
	const float btnSpacing = 56.0f;

	startButton_.SetPosition({ btnX, btnStartY });
	startButton_.Update();
	editorButton_.SetPosition({ btnX, btnStartY + btnSpacing });
	editorButton_.Update();
	settingsButton_.SetPosition({ btnX, btnStartY + btnSpacing * 2.0f });
	settingsButton_.Update();
	exitButton_.SetPosition({ btnX, btnStartY + btnSpacing * 3.0f });
	exitButton_.Update();

	for (size_t i = 0; i < buttonBorders_.size(); ++i) {
		auto& group = buttonBorders_[i];
		if (i < buttonBorderOutlineOffsets_.size()) {
			for (size_t j = 0; j < group.outlines.size() && j < buttonBorderOutlineOffsets_[i].size(); ++j) {
				if (group.outlines[j]) {
					group.outlines[j]->SetPosition({ menuWindowPos_.x + buttonBorderOutlineOffsets_[i][j].x, menuWindowPos_.y + buttonBorderOutlineOffsets_[i][j].y });
					group.outlines[j]->Update();
				}
			}
		}
		if (i < buttonBorderBracketOffsets_.size()) {
			for (size_t j = 0; j < group.brackets.size() && j < buttonBorderBracketOffsets_[i].size(); ++j) {
				if (group.brackets[j]) {
					group.brackets[j]->SetPosition({ menuWindowPos_.x + buttonBorderBracketOffsets_[i][j].x, menuWindowPos_.y + buttonBorderBracketOffsets_[i][j].y });
					group.brackets[j]->Update();
				}
			}
		}
		if (i < buttonBorderIndicatorOffsets_.size() && group.indicator) {
			group.indicator->SetPosition({ menuWindowPos_.x + buttonBorderIndicatorOffsets_[i].x, menuWindowPos_.y + buttonBorderIndicatorOffsets_[i].y });
			group.indicator->Update();
		}
	}
}

void TitleScene::UpdateMenuConsoleVisuals() {
	int selIdx = selectionManager_.GetSelectedIndex();
	bool blink = (std::fmod(animTimer_, 0.36f) < 0.18f);

	// ヘッダーランプの点滅更新
	if (headerLampSprite_) {
		headerLampSprite_->SetColor(blink ? MyMath::Vector4{ 0.15f, 1.0f, 0.35f, 0.95f } : MyMath::Vector4{ 0.05f, 0.30f, 0.12f, 0.30f });
		headerLampSprite_->Update();
	}

	// タイトル＆サブタイトルを完全な鮮烈グリーンに固定（外部INIファイルによる白文字上書きを完全防止）
	titleText_.SetColor({ 0.22f, 1.0f, 0.52f, 1.0f });
	subtitleText_.SetColor({ 0.35f, 0.95f, 0.60f, 0.95f });

	// ボタンテキスト色を毎フレーム直接設定（白文字を完全排除し、鮮烈なターミナルグリーンに統一）
	const Vector4 normalGreen = { 0.35f, 0.92f, 0.55f, 0.95f };
	const Vector4 selectedGreen = { 0.85f, 1.00f, 0.90f, 1.00f };
	startButton_.SetTextColor(normalGreen);
	startButton_.SetSelectedTextColor(selectedGreen);
	editorButton_.SetTextColor(normalGreen);
	editorButton_.SetSelectedTextColor(selectedGreen);
	settingsButton_.SetTextColor(normalGreen);
	settingsButton_.SetSelectedTextColor(selectedGreen);
	exitButton_.SetTextColor(normalGreen);
	exitButton_.SetSelectedTextColor(selectedGreen);

	// 各ボタンのラベル文字列は左揃え固定インデント配置（[01]〜[04]の頭出し位置を垂直に完全整列）
	startButton_.SetLabel("[01] EXECUTE SORTIE");
	editorButton_.SetLabel("[02] MISSION EDITOR");
	settingsButton_.SetLabel("[03] SYSTEM CONFIG");
	exitButton_.SetLabel("[04] ABORT / EXIT");

	// 各ボタン枠スプライトの色動的更新
	for (size_t i = 0; i < buttonBorders_.size(); ++i) {
		bool isSel = (static_cast<int>(i) == selIdx);
		// 選択時は白熱グリーン〜シアンで枠全体が美しく発光
		MyMath::Vector4 outlineCol   = isSel ? MyMath::Vector4{ 0.75f, 1.0f, 0.85f, 0.95f } : MyMath::Vector4{ 0.15f, 0.45f, 0.28f, 0.35f };
		MyMath::Vector4 bracketCol   = isSel ? MyMath::Vector4{ 0.90f, 1.0f, 0.95f, 1.00f } : MyMath::Vector4{ 0.20f, 0.65f, 0.42f, 0.50f };
		MyMath::Vector4 indicatorCol = isSel ? MyMath::Vector4{ 0.35f, 1.0f, 0.65f, 1.00f } : MyMath::Vector4{ 0.08f, 0.22f, 0.16f, 0.20f };

		for (auto& sp : buttonBorders_[i].outlines) {
			sp->SetColor(outlineCol);
			sp->Update();
		}
		for (auto& sp : buttonBorders_[i].brackets) {
			sp->SetColor(bracketCol);
			sp->Update();
		}
		if (buttonBorders_[i].indicator) {
			buttonBorders_[i].indicator->SetColor(indicatorCol);
			buttonBorders_[i].indicator->Update();
		}
	}
}

void TitleScene::DrawMenuCard() {
	menuCardPanel_.Draw();
	menuHeaderPanel_.Draw();
	if (headerLampSprite_) {
		headerLampSprite_->Draw();
	}
	for (auto& sp : menuBorderSprites_) {
		sp->Draw();
	}
	if (state_ != TitleState::SortieSequence) {
		for (auto& group : buttonBorders_) {
			for (auto& sp : group.outlines) { sp->Draw(); }
			for (auto& sp : group.brackets) { sp->Draw(); }
			if (group.indicator) { group.indicator->Draw(); }
		}
	}

	TextRenderer* tr = TextRenderer::GetInstance();
	const float cardWidth = 520.0f;
	const float cardHeight = 616.0f;
	const float cardX = menuWindowPos_.x;
	const float cardY = menuWindowPos_.y;

	// 端末ヘッダーテキスト (HackGen 13.0f) - タイトルバー左側（アクティブ時は鮮烈発光＆FOCUSバッジ）
	bool isActive = (activeWindowId_ == 0);
	if (isActive) {
		DrawOSRect({ cardX, cardY }, { cardWidth, 2.0f }, { 0.70f, 1.0f, 0.88f, 1.0f }); // 上端ネオンアクセントライン
		DrawOSRect({ cardX + 14.0f, cardY + 11.0f }, { 8.0f, 8.0f }, { 0.40f, 1.0f, 0.70f, 1.0f }); // 高輝度LED
		tr->Print("HackGen", "[FOCUS] TERMINAL : DAWN_OS [v4.12]", cardX + 28.0f, cardY + 9.0f, 13.0f, { 0.80f, 1.0f, 0.92f, 1.0f });
	} else {
		DrawOSRect({ cardX + 14.0f, cardY + 11.0f }, { 8.0f, 8.0f }, { 0.12f, 0.30f, 0.16f, 0.45f }); // 消灯LED
		tr->Print("HackGen", "[IDLE]  TERMINAL : DAWN_OS [v4.12]", cardX + 28.0f, cardY + 9.0f, 13.0f, { 0.22f, 0.50f, 0.30f, 0.65f });
	}

	// タイトルバー右側のシステムステータス表示 (SEC: LV5 [OK])
	const float badgeX = cardX + cardWidth - 110.0f;
	const float badgeY = cardY + 7.0f;
	bool secBlink = (std::fmod(animTimer_, 1.2f) < 0.6f);
	Vector4 ledColor = secBlink ? Vector4{ 0.35f, 1.0f, 0.65f, 1.0f } : Vector4{ 0.15f, 0.70f, 0.35f, 0.75f };
	DrawOSRect({ badgeX + 7.0f, badgeY + 6.0f }, { 6.0f, 6.0f }, ledColor);
	tr->Print("HackGen", "SEC:LV5 [OK]", badgeX + 18.0f, badgeY + 3.0f, 10.5f, { 0.45f, 1.0f, 0.70f, 0.95f });

	// 作戦指令ヘッダーラベル - タイトルバー直下の独立行（ロゴの上に綺麗に独立配置）
	tr->Print("HackGenRegular", "/// CLASSIFIED OPERATION DIRECTIVE ///", cardX + cardWidth * 0.5f, cardY + 44.0f, 11.5f, { 0.35f, 0.95f, 0.60f, 0.90f }, { 0.5f, 0.0f });

	// メインタイトル：DAWN ロゴスプライト（Y: 60.1〜151.9px、指令テキストと一切被らず堂々配置）
	if (titleLogoSprite_) {
		titleLogoSprite_->Draw();
	}

	// サブタイトル
	subtitleText_.Draw();

	// システム認証ステータス行（ディバイダーラインにインラインマウント）
	// テキスト背面に完全不透明の黒ベタ矩形を敷くことで、背後の水平線を完璧にマスクして可読性を100%確保
	const float authY = cardY + 172.0f;
	const float authCenterX = cardX + cardWidth * 0.5f;
	Vector2 authSize = tr->CalculateTextSize("HackGenRegular", "AUTH LEVEL 5 GRANTED // READY FOR MISSION INPUT", 11.0f);
	DrawOSRect({ authCenterX - (authSize.x * 0.5f + 8.0f), authY - 8.0f }, { authSize.x + 16.0f, 16.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });
	tr->Print("HackGenRegular", "AUTH LEVEL 5 GRANTED // READY FOR MISSION INPUT", authCenterX, authY, 11.0f, { 0.28f, 0.88f, 0.52f, 0.90f }, { 0.5f, 0.5f });

	if (state_ == TitleState::SortieSequence) {
		// ============================================================
		// 出撃シーケンス専用コンソール（メインターミナル内部に完全統合描画）
		// 他のUI（ボタンやガイド）と一切被らず、美しい映画的演出を実現
		// ============================================================
		float consoleY = cardY + 196.0f;
		const float consoleW = cardWidth - 36.0f;
		const float consoleX = cardX + 18.0f;
		const float consoleH = 390.0f;

		// 1. 出撃コンソール専用パネル（背景ダーク ＋ ネオングリーン/ゴールド二重枠）
		bool blinkFast = (static_cast<int>(sortieTimer_ * 16.0f) % 2) == 0;
		Vector4 panelBorder = blinkFast ? Vector4{ 1.0f, 0.85f, 0.25f, 0.95f } : Vector4{ 0.35f, 1.0f, 0.65f, 0.90f };
		DrawOSPanel({ consoleX, consoleY }, { consoleW, consoleH }, { 0.01f, 0.04f, 0.02f, 0.95f }, panelBorder, 1.5f);

		// 2. 出撃バナーヘッダー
		DrawOSRect({ consoleX + 2.0f, consoleY + 2.0f }, { consoleW - 4.0f, 28.0f }, { 0.04f, 0.18f, 0.09f, 0.95f });
		tr->Print("HackGen", "[ ! ] COMBAT SORTIE INITIATED [ ! ]", consoleX + consoleW * 0.5f, consoleY + 7.0f, 13.0f, panelBorder, { 0.5f, 0.0f });

		// 3. カタパルトチャージプログレスバー（項目3: 負荷タメ・認証停滞の緩急カーブ）
		float catProgress = 0.0f;
		if (sortieTimer_ < 0.40f) {
			// 急速初期チャージ (0% -> 38%)
			float t = sortieTimer_ / 0.40f;
			catProgress = (1.0f - std::pow(1.0f - t, 2.0f)) * 0.38f;
		} else if (sortieTimer_ < 0.75f) {
			// 負荷タメ・蒸気圧充填停滞 (38% -> 42% で一瞬停滞！)
			float t = (sortieTimer_ - 0.40f) / 0.35f;
			catProgress = 0.38f + t * 0.04f;
		} else if (sortieTimer_ < 1.30f) {
			// 一気にチャージ上昇 (42% -> 89%)
			float t = (sortieTimer_ - 0.75f) / 0.55f;
			catProgress = 0.42f + (t * t * (3.0f - 2.0f * t)) * 0.47f;
		} else if (sortieTimer_ < 1.60f) {
			// インターロック解除待ちのタメ (89% -> 91% で一瞬停滞！)
			float t = (sortieTimer_ - 1.30f) / 0.30f;
			catProgress = 0.89f + t * 0.02f;
		} else if (sortieTimer_ < 1.75f) {
			// 100%突破！ (91% -> 100% 一気に完了！)
			float t = (sortieTimer_ - 1.60f) / 0.15f;
			catProgress = 0.91f + (1.0f - std::pow(1.0f - t, 3.0f)) * 0.09f;
		} else {
			catProgress = 1.0f;
		}
		catProgress = std::clamp(catProgress, 0.0f, 1.0f);

		float barY = consoleY + 40.0f;
		tr->Print("HackGen", "CATAPULT CHARGE :", consoleX + 16.0f, barY, 11.0f, { 0.35f, 0.85f, 0.55f, 0.90f });
		char catBuf[32];
		snprintf(catBuf, sizeof(catBuf), "%3.0f %%", catProgress * 100.0f);
		tr->Print("HackGen", catBuf, consoleX + consoleW - 56.0f, barY, 11.0f, { 1.0f, 0.90f, 0.25f, 0.95f });
		barY += 18.0f;

		const float gW = consoleW - 32.0f;
		DrawOSRect({ consoleX + 16.0f, barY }, { gW, 8.0f }, { 0.03f, 0.10f, 0.05f, 0.80f });
		DrawOSRect({ consoleX + 16.0f, barY }, { gW * catProgress, 8.0f }, { 0.25f, 0.95f, 0.50f, 0.95f });
		DrawOSBorder({ consoleX + 16.0f, barY }, { gW, 8.0f }, { 0.20f, 0.65f, 0.35f, 0.75f }, 1.0f);

		// 4. リアルタイム・シーケンス認証ログ
		float logY = barY + 20.0f;
		const float lineGap = 26.0f;
		const Vector4 greenLog = { 0.40f, 1.0f, 0.65f, 0.95f };
		const Vector4 goldLog  = { 1.0f, 0.90f, 0.30f, 0.98f };

		tr->Print("HackGen", "SYSTEM LOGS:", consoleX + 16.0f, logY, 10.5f, { 0.30f, 0.70f, 0.45f, 0.80f });
		logY += 18.0f;

		if (sortieTimer_ >= 0.10f) {
			tr->Print("HackGen", "[0x0400] UPLINK TO MiG-21 AVIONICS... [ONLINE]", consoleX + 18.0f, logY, 11.0f, greenLog);
			logY += lineGap;
		}
		if (sortieTimer_ >= 0.40f) {
			tr->Print("HackGen", "[0x0420] STEAM PRESSURE: 24.8 MPa [NOMINAL]", consoleX + 18.0f, logY, 11.0f, greenLog);
			logY += lineGap;
		}
		if (sortieTimer_ >= 0.80f) {
			tr->Print("HackGen", "[0x0450] WEAPONS ARMED: 20MM CANNON + R-60M", consoleX + 18.0f, logY, 11.0f, greenLog);
			logY += lineGap;
		}
		if (sortieTimer_ >= 1.30f) {
			tr->Print("HackGen", "[0x0480] CANOPY LOCKED // CLEAR FOR TAKEOFF", consoleX + 18.0f, logY, 11.0f, greenLog);
			logY += lineGap;
		}
		if (sortieTimer_ >= 1.70f) {
			tr->Print("HackGen", ">>> AFTERBURNER IGNITION // LAUNCH >>>", consoleX + consoleW * 0.5f, logY + 4.0f, 12.5f, goldLog, { 0.5f, 0.0f });
		}

		// 5. 最下部ステータスバッジ
		float badgeY = consoleY + consoleH - 34.0f;
		DrawOSPanel({ consoleX + 14.0f, badgeY }, { consoleW - 28.0f, 24.0f }, { 0.05f, 0.18f, 0.08f, 0.85f }, { 0.35f, 0.95f, 0.55f, 0.85f }, 1.0f);
		tr->Print("HackGen", "STATUS // SCRAMBLE AUTHORIZED - INTERCEPT READY", consoleX + consoleW * 0.5f, badgeY + 5.0f, 10.5f, { 1.0f, 0.95f, 0.30f, 0.98f }, { 0.5f, 0.0f });
	} else {
		// メニューボタン（コマンド行）
		startButton_.Draw();
		editorButton_.Draw();
		settingsButton_.Draw();
		exitButton_.Draw();

		// 操作ガイド (HackGen 14.0f) - フッター枠（Y=482〜520）の中央にピッタリ収まる緑文字統一
		tr->Print(
			"HackGen",
			"[ UP / DOWN ] SELECT     [ ENTER / SPACE ] EXECUTE",
			cardX + cardWidth * 0.5f,
			cardY + 449.0f,
			14.0f,
			{ 0.35f, 0.95f, 0.55f, 0.95f },
			{ 0.5f, 0.5f }
		);
	}

	// ウィンドウ最下部ステータスバー (高さ26pxのバーの中央にジャスト配置・文字つぶれ防止)
	const float sbCenterY = cardY + cardHeight - 13.0f;
	const float sbFontSize = 13.0f;
	tr->Print(
		"HackGenRegular",
		"ROOT@TACTICAL_OS: ONLINE",
		cardX + 16.0f,
		sbCenterY,
		sbFontSize,
		{ 0.35f, 0.95f, 0.60f, 0.95f },
		{ 0.0f, 0.5f }
	);
	tr->Print(
		"HackGenRegular",
		"PORT: 115200 BAUD // UTF-8",
		cardX + cardWidth - 16.0f,
		sbCenterY,
		sbFontSize,
		{ 0.30f, 0.90f, 0.55f, 0.90f },
		{ 1.0f, 0.5f }
	);


	if (isActive) {
		// アクティブウィンドウ強調: 外周ネオン発光ボーダー ＆ 四隅コーナーブラケット輝度ブースト
		DrawOSBorder({ cardX, cardY }, { cardWidth, cardHeight }, { 0.45f, 1.0f, 0.70f, 0.95f }, 1.5f);
		const float cLen = 22.0f;
		const float cThick = 3.0f;
		const Vector4 cCol = { 0.85f, 1.0f, 0.95f, 1.0f };
		DrawOSRect({ cardX, cardY }, { cLen, cThick }, cCol);
		DrawOSRect({ cardX, cardY }, { cThick, cLen }, cCol);
		DrawOSRect({ cardX + cardWidth - cLen, cardY }, { cLen, cThick }, cCol);
		DrawOSRect({ cardX + cardWidth - cThick, cardY }, { cThick, cLen }, cCol);
		DrawOSRect({ cardX, cardY + cardHeight - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ cardX, cardY + cardHeight - cLen }, { cThick, cLen }, cCol);
		DrawOSRect({ cardX + cardWidth - cLen, cardY + cardHeight - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ cardX + cardWidth - cThick, cardY + cardHeight - cLen }, { cThick, cLen }, cCol);
	} else {
		// 非アクティブウィンドウ減光: ディムオーバーレイで背景に沈め、フォーカスウィンドウを手前に浮き立たせる
		DrawOSRect({ cardX, cardY }, { cardWidth, cardHeight }, { 0.0f, 0.015f, 0.008f, 0.35f });
	}
}

void TitleScene::DrawTransitionGlitch() {
	if (transitionTimer_ <= 0.0f) { return; }

	float t = transitionTimer_ / kTransitionDuration;

	// グリッチ用水平帯の動的更新＆描画
	auto pseudoRand = [](uint32_t& s) -> float {
		s = s * 1664525u + 1013904223u;
		return static_cast<float>(s & 0xFFFF) / 65535.0f;
	};

	uint32_t s = static_cast<uint32_t>(transitionTimer_ * 1000.0f) + 77;
	for (size_t i = 0; i < glitchSprites_.size(); ++i) {
		float y = pseudoRand(s) * kScreenHeight;
		float h = 2.0f + pseudoRand(s) * 32.0f;
		float w = (0.5f + 0.5f * pseudoRand(s)) * kScreenWidth;
		float x = (pseudoRand(s) - 0.2f) * kScreenWidth;
		float alpha = (0.25f + 0.55f * pseudoRand(s)) * t;

		MyMath::Vector4 col;
		float pick = pseudoRand(s);
		if (pick < 0.40f) {
			col = { 0.02f, 0.02f, 0.02f, alpha * 1.2f }; // 黒帯
		} else if (pick < 0.75f) {
			col = { 0.1f, 0.95f, 1.0f, alpha * 0.85f };   // エレクトリックシアン
		} else {
			col = { 0.95f, 0.95f, 0.98f, alpha * 0.75f }; // 白ノイズ
		}

		glitchSprites_[i]->SetPosition({ x, y });
		glitchSprites_[i]->SetSize({ w, h });
		glitchSprites_[i]->SetColor(col);
		glitchSprites_[i]->Update();
		glitchSprites_[i]->Draw();
	}

	// 画面中央の警告テキスト（ミリタリー通信同期アラート）
	TextRenderer* tr = TextRenderer::GetInstance();
	if (std::fmod(transitionTimer_, 0.09f) < 0.06f) {
		const float cx = kScreenWidth * 0.5f;
		const float cy = kScreenHeight * 0.5f;
		tr->Print("HackGen", ">> OPTICAL FEED DISRUPTED - CHANNEL SYNCHRONIZING <<", cx, cy - 40.0f, 17.0f, { 1.0f, 0.82f, 0.15f, 0.95f }, { 0.5f, 0.5f });
		tr->Print("HackGen", "/// SENSOR RE-ACQUISITION IN PROGRESS - STAND BY ///", cx, cy - 16.0f, 13.0f, { 0.2f, 0.92f, 1.0f, 0.85f }, { 0.5f, 0.5f });
	}
}

void TitleScene::InitializeOSD(SpriteCommon* spriteCommon) {
	osdSprites_.clear();
	glitchSprites_.clear();

	auto addBar = [this, spriteCommon](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition(pos);
		sp->SetSize(size);
		sp->SetColor(color);
		sp->Update();
		osdSprites_.push_back(std::move(sp));
	};

	const float left = 48.0f;
	const float right = kScreenWidth - 48.0f;
	const float top = 36.0f;
	const float bottom = kScreenHeight - 36.0f;
	const float len = 44.0f;
	const float thick = 2.0f;
	const MyMath::Vector4 cornerColor = { 0.90f, 0.96f, 1.0f, 0.85f };
	const MyMath::Vector4 shadowColor = { 0.0f, 0.0f, 0.0f, 0.70f };

	// 四隅のL字ブラケット（黒縁ドロップシャドウ付きで明所でもクッキリ視認可能）
	auto addShadowedBar = [&](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
		addBar({ pos.x + 1.0f, pos.y + 1.0f }, size, shadowColor);
		addBar(pos, size, color);
	};

	// 左上 ┌
	addShadowedBar({ left, top }, { len, thick }, cornerColor);
	addShadowedBar({ left, top }, { thick, len }, cornerColor);
	// 右上 ┐
	addShadowedBar({ right - len, top }, { len, thick }, cornerColor);
	addShadowedBar({ right - thick, top }, { thick, len }, cornerColor);
	// 左下 └
	addShadowedBar({ left, bottom - thick }, { len, thick }, cornerColor);
	addShadowedBar({ left, bottom - len }, { thick, len }, cornerColor);
	// 右下 ┘
	addShadowedBar({ right - len, bottom - thick }, { len, thick }, cornerColor);
	addShadowedBar({ right - thick, bottom - len }, { thick, len }, cornerColor);

	// 左右の水平マーカー（ピッチラダー風）
	const float cy = kScreenHeight * 0.5f;
	const MyMath::Vector4 markerColor = { 0.90f, 0.96f, 1.0f, 0.60f };
	addShadowedBar({ left, cy - 1.0f }, { 14.0f, 2.0f }, markerColor);
	addShadowedBar({ right - 14.0f, cy - 1.0f }, { 14.0f, 2.0f }, markerColor);

	// 中央十字レティクル（HUD全体と統一した上品なミリタリーターミナルグリーン）
	const float cx = kScreenWidth * 0.5f;
	const MyMath::Vector4 reticleColor = { 0.22f, 0.88f, 0.48f, 0.65f }; // タクティカルグリーン
	const float rLen = 14.0f;
	const float rGap = 8.0f;
	const float rThick = 2.0f;

	// 十字の4本
	addShadowedBar({ cx - 1.0f, cy - rGap - rLen }, { rThick, rLen }, reticleColor);
	addShadowedBar({ cx - 1.0f, cy + rGap }, { rThick, rLen }, reticleColor);
	addShadowedBar({ cx - rGap - rLen, cy - 1.0f }, { rLen, rThick }, reticleColor);
	addShadowedBar({ cx + rGap, cy - 1.0f }, { rLen, rThick }, reticleColor);

	// センターターゲットサークル/ブラケット（直径 24px の外枠ブラケット）
	const float bSize = 12.0f;
	const float bCorner = 4.0f;
	// 左上
	addShadowedBar({ cx - bSize, cy - bSize }, { bCorner, rThick }, reticleColor);
	addShadowedBar({ cx - bSize, cy - bSize }, { rThick, bCorner }, reticleColor);
	// 右上
	addShadowedBar({ cx + bSize - bCorner, cy - bSize }, { bCorner, rThick }, reticleColor);
	addShadowedBar({ cx + bSize - rThick, cy - bSize }, { rThick, bCorner }, reticleColor);
	// 左下
	addShadowedBar({ cx - bSize, cy + bSize - rThick }, { bCorner, rThick }, reticleColor);
	addShadowedBar({ cx - bSize, cy + bSize - bCorner }, { rThick, bCorner }, reticleColor);
	// 右下
	addShadowedBar({ cx + bSize - bCorner, cy + bSize - rThick }, { bCorner, rThick }, reticleColor);
	addShadowedBar({ cx + bSize - rThick, cy + bSize - bCorner }, { rThick, bCorner }, reticleColor);

	// センタードット
	addShadowedBar({ cx - 1.5f, cy - 1.5f }, { 3.0f, 3.0f }, { 0.75f, 0.90f, 0.98f, 0.65f });

	// REC点滅インジケータ（赤ランプ）
	recDotSprite_ = std::make_unique<Sprite>();
	recDotSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
	recDotSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	recDotSprite_->SetPosition({ left + 10.0f, top + 13.0f });
	recDotSprite_->SetSize({ 8.0f, 8.0f });
	recDotSprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.95f });
	recDotSprite_->Update();

	// グリッチ用スプライト（8個）の事前初期化
	for (int i = 0; i < 8; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition({ 0.0f, 0.0f });
		sp->SetSize({ 0.0f, 0.0f });
		sp->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
		sp->Update();
		glitchSprites_.push_back(std::move(sp));
	}
}

void TitleScene::DrawOSD() {
	const float left = 48.0f;
	const float right = kScreenWidth - 48.0f;
	const float top = 36.0f;
	const float bottom = kScreenHeight - 36.0f;
	const bool isDisconnected = (CurrentLocation().id == TitleLocation::CrashedForest);

	// RECランプの点滅更新
	if (recDotSprite_) {
		if (isDisconnected) {
			// 切断時：高速な警告レッド点滅
			bool errBlink = (std::fmod(animTimer_, 0.4f) < 0.2f);
			recDotSprite_->SetColor(errBlink ? MyMath::Vector4{ 1.0f, 0.15f, 0.15f, 0.95f } : MyMath::Vector4{ 0.3f, 0.05f, 0.05f, 0.20f });
		} else {
			bool recBlink = (std::fmod(animTimer_, 1.0f) < 0.6f);
			recDotSprite_->SetColor(recBlink ? MyMath::Vector4{ 1.0f, 0.15f, 0.15f, 0.95f } : MyMath::Vector4{ 0.3f, 0.05f, 0.05f, 0.20f });
		}
		recDotSprite_->Update();
	}

	// 枠線・レティクルの動的カラー設定（墜落現場 CrashedForest では全て警告赤に！）
	float blinkAlpha = isDisconnected ? (0.70f + 0.30f * std::sin(animTimer_ * 10.0f)) : 1.0f;
	const MyMath::Vector4 redBorderCol = { 1.0f, 0.20f, 0.20f, 0.88f * blinkAlpha };
	const MyMath::Vector4 redReticleCol = { 0.98f, 0.25f, 0.25f, 0.78f * blinkAlpha };

	for (size_t i = 0; i < osdSprites_.size(); ++i) {
		auto& sp = osdSprites_[i];
		if (i % 2 == 1) { // 奇数インデックスが表面のバー（偶数は黒ドロップシャドウ）
			if (isDisconnected) {
				sp->SetColor(redBorderCol);
			} else {
				if (i <= 15) {
					// 四隅ブラケット
					sp->SetColor({ 0.90f, 0.96f, 1.0f, 0.85f });
				} else if (i <= 19) {
					// 水平ピッチマーカー
					sp->SetColor({ 0.90f, 0.96f, 1.0f, 0.60f });
				} else {
					// センターレティクル
					sp->SetColor({ 0.22f, 0.88f, 0.48f, 0.65f });
				}
			}
			sp->Update();
		}
		sp->Draw();
	}
	if (recDotSprite_) {
		recDotSprite_->Draw();
	}

	TextRenderer* tr = TextRenderer::GetInstance();
	const float cx = kScreenWidth * 0.5f;
	const float cy = kScreenHeight * 0.5f;

	if (isDisconnected) {
		// ============================================================
		// 墜落現場専用：DISCONNECTED / 信号途絶HUD表示
		// ============================================================

		// 水平グリッチノイズバーの描画（控えめに時々走る程度に調整）
		for (size_t i = 0; i < glitchSprites_.size(); ++i) {
			if (rand() % 100 < 10) { // 10%の控えめな確率で出現
				float gy = static_cast<float>(rand() % static_cast<int>(kScreenHeight));
				float gh = static_cast<float>(1 + rand() % 12);
				float gw = kScreenWidth;
				float gAlpha = 0.12f + static_cast<float>(rand() % 100) / 100.0f * 0.18f;
				MyMath::Vector4 gCol = (rand() % 3 == 0)
					? MyMath::Vector4{ 0.0f, 0.0f, 0.0f, gAlpha * 1.2f }
					: ((rand() % 2 == 0) ? MyMath::Vector4{ 0.95f, 0.95f, 0.98f, gAlpha } : MyMath::Vector4{ 0.9f, 0.15f, 0.15f, gAlpha * 0.5f });
				glitchSprites_[i]->SetPosition({ 0.0f, gy });
				glitchSprites_[i]->SetSize({ gw, gh });
				glitchSprites_[i]->SetColor(gCol);
				glitchSprites_[i]->Update();
				glitchSprites_[i]->Draw();
			}
		}

		// --- 左上: NO SIGNAL & フリーズタイムコード ---
		tr->Print("HackGen", "NO SIGNAL", left + 24.0f, top + 9.0f, 15.0f, { 1.0f, 0.25f, 0.25f, 0.95f });
		tr->Print("HackGen", "--:--:--:-- [FROZEN]", left + 120.0f, top + 9.0f, 15.0f, { 0.85f, 0.30f, 0.30f, 0.85f });
		tr->Print("HackGen", "CARRIER LINK: SEVERED // SENSOR FAULT DETECTED", left + 8.0f, top + 30.0f, 11.5f, { 1.0f, 0.45f, 0.45f, 0.80f });

		// --- 右上: DISCONNECTED 警告テキスト（点滅）---
		bool blinkRight = (std::fmod(animTimer_, 0.6f) < 0.4f);
		MyMath::Vector4 rightCol = blinkRight ? MyMath::Vector4{ 1.0f, 0.20f, 0.20f, 0.98f } : MyMath::Vector4{ 0.65f, 0.12f, 0.12f, 0.45f };
		tr->Print("HackGen", "BAT --%  SIG [NO CARRIER]  0.0Mb/s  DISCONNECTED", right - 365.0f, top + 9.0f, 13.5f, rightCol);

		// --- 画面中央: 点滅する緊急アラート ---
		bool centerBlink = (std::fmod(animTimer_, 0.8f) < 0.55f);
		if (centerBlink) {
			tr->Print("HackGen", ">> [ DISCONNECTED : SIGNAL LOST ] <<", cx, cy - 42.0f, 17.0f, { 1.0f, 0.15f, 0.15f, 0.95f }, { 0.5f, 0.5f });
			tr->Print("HackGen", "/// CRASH PROTOCOL ENGAGED - RECONNECT PENDING ///", cx, cy - 18.0f, 12.0f, { 1.0f, 0.45f, 0.45f, 0.80f }, { 0.5f, 0.5f });
		}

		// --- 左下: 途絶ロケーション ---
		tr->Print("HackGen", "CAM 03 // CRASH SITE - SECTOR 7 [DISCONNECTED]", left + 8.0f, bottom - 38.0f, 17.0f, { 1.0f, 0.30f, 0.30f, 0.95f });
		tr->Print("HackGen", "TERMINATED OPTICAL LINK // SENSOR OFFLINE", left + 8.0f, bottom - 18.0f, 12.0f, { 0.85f, 0.30f, 0.30f, 0.70f });

		// --- 右下: 切断ステータス ---
		tr->Print("HackGen", "OPTICS: CORRUPTED", right - 210.0f, bottom - 38.0f, 14.0f, { 1.0f, 0.35f, 0.35f, 0.80f });
		tr->Print("HackGen", "LINK: DISCONNECTED", right - 210.0f, bottom - 18.0f, 12.0f, { 1.0f, 0.20f, 0.20f, 0.95f });

	} else {
		// ============================================================
		// 通常ロケーション（Tunnel, Hangar）HUD表示
		// ============================================================

		// --- 左上: REC & タイムコード (HackGen) ---
		tr->Print("HackGen", "REC", left + 24.0f, top + 9.0f, 15.0f, { 1.0f, 0.95f, 0.95f, 0.95f });

		char timeBuf[32];
		int totalSec = static_cast<int>(animTimer_);
		int hours = (totalSec / 3600) % 24;
		int mins = (totalSec / 60) % 60;
		int secs = totalSec % 60;
		int frames = static_cast<int>((animTimer_ - totalSec) * 60.0f);
		snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d:%02d", hours, mins, secs, frames);
		tr->Print("HackGen", timeBuf, left + 64.0f, top + 9.0f, 15.0f, { 0.85f, 0.95f, 1.0f, 0.85f });

		// ドローン姿勢・ジャイロテレメトリ
		float dronePitch = 2.4f * std::sin(animTimer_ * 0.6f);
		float droneRoll = -1.2f * std::cos(animTimer_ * 0.8f);
		float droneAlt = 12.0f + 0.4f * std::sin(animTimer_ * 0.4f);
		char telemBuf[64];
		snprintf(telemBuf, sizeof(telemBuf), "PITCH %+.1f deg  ROLL %+.1f deg  ALT %.1fm", dronePitch, droneRoll, droneAlt);
		tr->Print("HackGen", telemBuf, left + 8.0f, top + 30.0f, 11.5f, { 0.25f, 0.88f, 0.55f, 0.70f });

		// --- 右上: バッテリー / シグナル / データレート動的表示 ---
		int batPercent = 94 - ((static_cast<int>(animTimer_ * 0.04f)) % 2);
		const char* sigBars = (static_cast<int>(animTimer_ * 1.5f) % 7 == 4) ? "|||." : "||||";
		float dataRate = 48.2f + 0.35f * std::sin(animTimer_ * 2.2f);
		char rightHudBuf[64];
		snprintf(rightHudBuf, sizeof(rightHudBuf), "BAT %d%%  SIG [%s]  %.1fMb/s  4K/60P", batPercent, sigBars, dataRate);
		tr->Print("HackGen", rightHudBuf, right - 280.0f, top + 9.0f, 13.5f, { 0.85f, 0.95f, 1.0f, 0.80f });

		// --- 左下: カメラ番号 & ロケーション (HackGen) ---
		std::string camName;
		std::string camCoord;
		if (transitionTimer_ > 0.0f) {
			camName = "CONNECTING... // ACQUIRING OPTICAL FEED";
			camCoord = "FEED SWITCH IN PROGRESS - PLEASE STAND BY";
		} else {
			switch (CurrentLocation().id) {
			case TitleLocation::Hangar:
				camName = "CAM 01 // HANGAR - BAY 4";
				camCoord = "LAT 35.6895 N  LON 139.6917 E  ALT +12m";
				break;
			case TitleLocation::Tunnel:
				camName = "CAM 02 // PURSUIT DRONE - TUNNEL 04";
				camCoord = "SPEED: MACH 1.15 // AFTERBURNER: MAXIMUM";
				break;
			default:
				camName = "CAM 01 // RECON DRONE";
				camCoord = "LAT --.-- N  LON ---.-- E";
				break;
			}
		}
		tr->Print("HackGen", camName, left + 8.0f, bottom - 38.0f, 17.0f, { 0.85f, 0.95f, 1.0f, 0.90f });
		tr->Print("HackGen", camCoord, left + 8.0f, bottom - 18.0f, 12.0f, { 0.65f, 0.75f, 0.85f, 0.60f });

		// --- 右下: 光学系 & リンク情報 (HackGen) ---
		tr->Print("HackGen", "ZOOM 1.0x   F/2.8   ISO 400", right - 210.0f, bottom - 38.0f, 14.0f, { 0.85f, 0.95f, 1.0f, 0.75f });
		tr->Print("HackGen", "LINK: SECURE LIVE FEED", right - 210.0f, bottom - 18.0f, 12.0f, { 0.45f, 0.85f, 0.55f, 0.70f });
	}
}

void TitleScene::InitializeWindowCloseVisuals(SpriteCommon* spriteCommon) {
	windowCloseMaskSprites_.clear();
	// 4枚のマスクスプライト（上下左右の黒枠）
	for (int i = 0; i < 4; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		sp->Update();
		windowCloseMaskSprites_.push_back(std::move(sp));
	}

	windowCloseBorderSprites_.clear();
	// ウィンドウタイトルバーと外枠線、クライアント内部背景、BOOTINGステータス枠（計11本）
	for (int i = 0; i < 11; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetColor({ 0.20f, 0.90f, 0.50f, 0.85f });
		sp->Update();
		windowCloseBorderSprites_.push_back(std::move(sp));
	}
}

void TitleScene::DrawWindowClose() {
	float t = windowCloseTimer_ / kWindowCloseDuration;
	t = (std::min)((std::max)(t, 0.0f), 1.0f);
	// スムーズなイージング (Smoothstep: 最初滑らかに動き始め、中央へ吸い込まれるように最小化：項目7)
	float shrink = t * t * (3.0f - 2.0f * t);
	float curW = (kScreenWidth - 32.0f) * (1.0f - shrink);
	float curH = (kScreenHeight - 32.0f) * (1.0f - shrink);
	float curX = (kScreenWidth - curW) * 0.5f;
	float curY = (kScreenHeight - curH) * 0.5f;

	if (windowCloseMaskSprites_.size() >= 4) {
		// 上マスク: (0, 0) ～ (kScreenWidth, curY)
		windowCloseMaskSprites_[0]->SetPosition({ 0.0f, 0.0f });
		windowCloseMaskSprites_[0]->SetSize({ kScreenWidth, (std::max)(0.0f, curY) });
		windowCloseMaskSprites_[0]->Update();
		windowCloseMaskSprites_[0]->Draw();

		// 下マスク: (0, curY + curH) ～ (kScreenWidth, kScreenHeight - (curY + curH))
		float bottomY = curY + curH;
		windowCloseMaskSprites_[1]->SetPosition({ 0.0f, bottomY });
		windowCloseMaskSprites_[1]->SetSize({ kScreenWidth, (std::max)(0.0f, kScreenHeight - bottomY) });
		windowCloseMaskSprites_[1]->Update();
		windowCloseMaskSprites_[1]->Draw();

		// 左マスク: (0, curY) ～ (curX, curH)
		windowCloseMaskSprites_[2]->SetPosition({ 0.0f, curY });
		windowCloseMaskSprites_[2]->SetSize({ (std::max)(0.0f, curX), (std::max)(0.0f, curH) });
		windowCloseMaskSprites_[2]->Update();
		windowCloseMaskSprites_[2]->Draw();

		// 右マスク: (curX + curW, curY) ～ (kScreenWidth - (curX + curW), curH)
		float rightX = curX + curW;
		windowCloseMaskSprites_[3]->SetPosition({ rightX, curY });
		windowCloseMaskSprites_[3]->SetSize({ (std::max)(0.0f, kScreenWidth - rightX), (std::max)(0.0f, curH) });
		windowCloseMaskSprites_[3]->Update();
		windowCloseMaskSprites_[3]->Draw();
	}

	if (curW > 16.0f && curH > 16.0f && windowCloseBorderSprites_.size() >= 11) {
		// 1. ウィンドウ内部クライアント領域の暗転背景（映像リンク切断に伴い画面がダークターミナルへ移行）
		float innerDarken = (std::min)(0.92f, 0.40f + 0.52f * t);
		windowCloseBorderSprites_[5]->SetPosition({ curX, curY });
		windowCloseBorderSprites_[5]->SetSize({ curW, curH });
		windowCloseBorderSprites_[5]->SetColor({ 0.01f, 0.04f, 0.02f, innerDarken });
		windowCloseBorderSprites_[5]->Update();
		windowCloseBorderSprites_[5]->Draw();

		// 2. ウィンドウ中央のステータス情報枠（BOOTINGメッセージを綺麗に囲むサイバーフレーム）
		// アニメーション中のアルファ曲線: 開始時に素早くフェードインし、終盤ウィンドウ消滅に合わせてフェードアウト
		float fadeIn = (std::min)(1.0f, t / 0.12f);
		float fadeOut = (std::clamp)((0.88f - t) / 0.18f, 0.0f, 1.0f);
		float textAlpha = fadeIn * fadeOut;

		TextRenderer* tr = TextRenderer::GetInstance();
		const std::string line1 = "/// VIDEO LINK DISENGAGED ///";
		const std::string line2 = "/// BOOTING DAWN TERMINAL ///";
		const float baseFontSize = 13.5f;

		Vector2 size1 = tr->CalculateTextSize("HackGenRegular", line1, baseFontSize);
		Vector2 size2 = tr->CalculateTextSize("HackGenRegular", line2, baseFontSize);
		float maxTextW = (std::max)(size1.x, size2.x);
		float lineGap = 6.0f;
		float totalTextH = baseFontSize * 2.0f + lineGap;

		// ウィンドウ枠（グリーンボーダー）の内側に完全に収めるためのマージン
		const float marginX = 20.0f;
		const float marginY = 14.0f;
		float maxBoxW = (std::max)(0.0f, curW - marginX * 2.0f);
		float maxBoxH = (std::max)(0.0f, curH - marginY * 2.0f);

		const float padX = 20.0f;
		const float padY = 12.0f;
		float desiredBoxW = maxTextW + padX * 2.0f;
		float desiredBoxH = totalTextH + padY * 2.0f;

		// ウィンドウ縮小に伴い、ステータス枠と文字サイズを滑らかに同期縮小
		float scaleX = (maxBoxW < desiredBoxW && desiredBoxW > 0.0f) ? (maxBoxW / desiredBoxW) : 1.0f;
		float scaleY = (maxBoxH < desiredBoxH && desiredBoxH > 0.0f) ? (maxBoxH / desiredBoxH) : 1.0f;
		float boxScale = (std::min)(scaleX, scaleY);

		float actualBoxW = desiredBoxW * boxScale;
		float actualBoxH = desiredBoxH * boxScale;
		float actualFontSize = baseFontSize * boxScale;
		float actualLineGap = lineGap * boxScale;

		float centerX = kScreenWidth * 0.5f;
		float centerY = kScreenHeight * 0.5f;
		float boxX = centerX - actualBoxW * 0.5f;
		float boxY = centerY - actualBoxH * 0.5f;

		// ウィンドウ内に十分なスペースがあり、文字が判読できるサイズの間だけ描画
		if (textAlpha > 0.01f && actualFontSize >= 6.5f && actualBoxW >= 40.0f && actualBoxH >= 20.0f) {
			// ステータス枠背景プレート
			windowCloseBorderSprites_[6]->SetPosition({ boxX, boxY });
			windowCloseBorderSprites_[6]->SetSize({ actualBoxW, actualBoxH });
			windowCloseBorderSprites_[6]->SetColor({ 0.02f, 0.09f, 0.04f, 0.85f * textAlpha });
			windowCloseBorderSprites_[6]->Update();
			windowCloseBorderSprites_[6]->Draw();

			// ステータス枠のサイバーボーダー（上・下・左・右）
			MyMath::Vector4 innerBorderCol = { 0.25f, 0.95f, 0.55f, 0.70f * textAlpha };
			float borderThick = (std::max)(1.0f, 1.5f * boxScale);

			windowCloseBorderSprites_[7]->SetPosition({ boxX, boxY });
			windowCloseBorderSprites_[7]->SetSize({ actualBoxW, borderThick }); // 上
			windowCloseBorderSprites_[7]->SetColor(innerBorderCol);
			windowCloseBorderSprites_[7]->Update();
			windowCloseBorderSprites_[7]->Draw();

			windowCloseBorderSprites_[8]->SetPosition({ boxX, boxY + actualBoxH - borderThick });
			windowCloseBorderSprites_[8]->SetSize({ actualBoxW, borderThick }); // 下
			windowCloseBorderSprites_[8]->SetColor(innerBorderCol);
			windowCloseBorderSprites_[8]->Update();
			windowCloseBorderSprites_[8]->Draw();

			windowCloseBorderSprites_[9]->SetPosition({ boxX, boxY });
			windowCloseBorderSprites_[9]->SetSize({ borderThick, actualBoxH }); // 左
			windowCloseBorderSprites_[9]->SetColor(innerBorderCol);
			windowCloseBorderSprites_[9]->Update();
			windowCloseBorderSprites_[9]->Draw();

			windowCloseBorderSprites_[10]->SetPosition({ boxX + actualBoxW - borderThick, boxY });
			windowCloseBorderSprites_[10]->SetSize({ borderThick, actualBoxH }); // 右
			windowCloseBorderSprites_[10]->SetColor(innerBorderCol);
			windowCloseBorderSprites_[10]->Update();
			windowCloseBorderSprites_[10]->Draw();

			// 枠内に綺麗にセンタリングしてBOOTINGテキストを描画
			float line1Y = centerY - (actualFontSize * 0.65f + actualLineGap * 0.5f);
			float line2Y = centerY + (actualFontSize * 0.65f + actualLineGap * 0.5f);

			tr->Print(
				"HackGenRegular",
				line1,
				centerX,
				line1Y,
				actualFontSize,
				{ 0.35f, 0.85f, 0.65f, 0.90f * textAlpha },
				{ 0.5f, 0.5f }
			);
			tr->Print(
				"HackGenRegular",
				line2,
				centerX,
				line2Y,
				actualFontSize,
				{ 0.30f, 1.00f, 0.58f, 1.00f * textAlpha },
				{ 0.5f, 0.5f }
			);
		}

		// 3. OSウィンドウタイトルバー
		float titleBarH = 24.0f;
		windowCloseBorderSprites_[0]->SetPosition({ curX, curY - titleBarH });
		windowCloseBorderSprites_[0]->SetSize({ curW, titleBarH });
		windowCloseBorderSprites_[0]->SetColor({ 0.03f, 0.12f, 0.06f, 0.98f });
		windowCloseBorderSprites_[0]->Update();
		windowCloseBorderSprites_[0]->Draw();

		// 4. 外枠線4辺 (鮮烈なエレクトリックネオングリーン)
		MyMath::Vector4 borderCol = { 0.25f, 0.98f, 0.58f, 0.95f };
		windowCloseBorderSprites_[1]->SetPosition({ curX, curY });
		windowCloseBorderSprites_[1]->SetSize({ curW, 2.0f }); // 上
		windowCloseBorderSprites_[1]->SetColor(borderCol);
		windowCloseBorderSprites_[1]->Update();
		windowCloseBorderSprites_[1]->Draw();

		windowCloseBorderSprites_[2]->SetPosition({ curX, curY + curH - 2.0f });
		windowCloseBorderSprites_[2]->SetSize({ curW, 2.0f }); // 下
		windowCloseBorderSprites_[2]->SetColor(borderCol);
		windowCloseBorderSprites_[2]->Update();
		windowCloseBorderSprites_[2]->Draw();

		windowCloseBorderSprites_[3]->SetPosition({ curX, curY });
		windowCloseBorderSprites_[3]->SetSize({ 2.0f, curH }); // 左
		windowCloseBorderSprites_[3]->SetColor(borderCol);
		windowCloseBorderSprites_[3]->Update();
		windowCloseBorderSprites_[3]->Draw();

		windowCloseBorderSprites_[4]->SetPosition({ curX + curW - 2.0f, curY });
		windowCloseBorderSprites_[4]->SetSize({ 2.0f, curH }); // 右
		windowCloseBorderSprites_[4]->SetColor(borderCol);
		windowCloseBorderSprites_[4]->Update();
		windowCloseBorderSprites_[4]->Draw();

		// 5. タイトルバーテキスト（最小化アニメーション中：幅に応じて動的短縮して枠内維持）
		if (curW > 80.0f) {
			float titleAlpha = (std::min)(1.0f, (curW - 80.0f) / 100.0f);
			if (curW >= 460.0f) {
				tr->Print("HackGen", "[CAM-01 : OPTICAL FEED - MINIMIZING TO SYSTEM CONSOLE...]", curX + 10.0f, curY - 19.0f, 11.5f, { 0.35f, 1.0f, 0.65f, 0.95f * titleAlpha });
			} else if (curW >= 260.0f) {
				tr->Print("HackGen", "[CAM-01 : MINIMIZING...]", curX + 10.0f, curY - 19.0f, 11.5f, { 0.35f, 1.0f, 0.65f, 0.95f * titleAlpha });
			} else if (curW >= 120.0f) {
				tr->Print("HackGen", "[CAM-01]", curX + 8.0f, curY - 19.0f, 11.0f, { 0.35f, 1.0f, 0.65f, 0.95f * titleAlpha });
			}

			if (curW >= 180.0f) {
				tr->Print("HackGen", "[ _ ] [口] [X]", curX + curW - 86.0f, curY - 19.0f, 11.5f, { 0.85f, 0.85f, 0.85f, 0.95f * titleAlpha });
			}
		}
	}
}

void TitleScene::DrawConsoleBoot() {
	// 背景の完全漆黒コンソールスプライト（格納庫の透けを100%遮蔽）
	if (consoleBgSprite_) {
		consoleBgSprite_->Draw();
	}
	backgroundPanel_.SetBackgroundColor({ 0.012f, 0.02f, 0.035f, 1.0f });
	backgroundPanel_.Draw();

	float textAlpha = 1.0f;
	if (consoleExitTimer_ > 0.0f) {
		textAlpha = (std::max)(0.0f, 1.0f - consoleExitTimer_ / kConsoleExitDuration);
	}

	TextRenderer* tr = TextRenderer::GetInstance();
	const float centerX = kScreenWidth * 0.5f;
	const float centerY = kScreenHeight * 0.5f;

	// 中央ターミナルパネル枠
	const float panelW = 680.0f;
	const float panelH = 210.0f;
	const float panelX = centerX - panelW * 0.5f;
	const float panelY = centerY - panelH * 0.5f;

	// パネル背景とサイバー枠線
	pseudoOS_.DrawPanel({ panelX, panelY }, { panelW, panelH },
		{ 0.01f, 0.03f, 0.05f, 0.92f * textAlpha },
		{ 0.20f, 0.70f, 0.90f, 0.85f * textAlpha }, 1.2f);

	// ヘッダータイトル
	tr->Print("HackGen", "DAWN_OS TACTICAL TERMINAL [v4.12.0] // SECURE SESSION",
		centerX, panelY + 16.0f, 12.0f,
		{ 0.45f, 0.85f, 0.95f, 0.90f * textAlpha }, { 0.5f, 0.0f });

	pseudoOS_.DrawRect({ panelX + 16.0f, panelY + 36.0f }, { panelW - 32.0f, 1.0f },
		{ 0.20f, 0.50f, 0.65f, 0.40f * textAlpha });

	// 1. コマンド行演出（コマンドのやつ）
	std::string fullCmd = isRebootLog_
		? "root@dawn-core:~$ ./emergency_recovery --restore-desktop"
		: "root@dawn-core:~$ ./authenticate_sortie --launch-system";

	float typingProg = std::clamp(consoleBootTimer_ / 0.38f, 0.0f, 1.0f);
	int showChars = static_cast<int>(typingProg * fullCmd.size());
	std::string dispCmd = fullCmd.substr(0, showChars);

	// 点滅カーソル
	bool cursorBlink = (std::fmod(consoleBootTimer_, 0.22f) < 0.12f);
	if (typingProg < 1.0f || consoleBootTimer_ < kCommandTypingDuration) {
		if (cursorBlink) { dispCmd += "_"; }
	}

	tr->Print("HackGen", dispCmd.c_str(), panelX + 28.0f, panelY + 50.0f, 16.0f,
		{ 0.35f, 1.0f, 0.65f, 0.98f * textAlpha });

	// 2. ローディング演出（コマンドのやつ -> loading...）
	if (consoleBootTimer_ >= kCommandTypingDuration) {
		// ローディングバーの文字列構築（ユーザー指定: loading...[||||||||||]）
		constexpr int kTotalBars = 24;
		int filledBars = static_cast<int>(loadingProgress_ * kTotalBars);
		if (filledBars > kTotalBars) filledBars = kTotalBars;

		std::string barStr = "loading... [";
		for (int i = 0; i < filledBars; ++i) {
			barStr += "|";
		}
		for (int i = 0; i < (kTotalBars - filledBars); ++i) {
			barStr += " "; // 未完了空白スロット
		}
		barStr += "]";

		// 色
		Vector4 barColor = isLoadingComplete_
			? Vector4{ 0.35f, 1.0f, 0.65f, 1.0f * textAlpha }   // complete時はエレクトリックグリーン
			: Vector4{ 0.25f, 0.90f, 1.0f, 0.98f * textAlpha }; // 進行中は鮮やかなシアン

		tr->Print("HackGen", barStr.c_str(), panelX + 28.0f, panelY + 95.0f, 20.0f, barColor);

		// パーセント or complete 表示
		if (isLoadingComplete_) {
			bool blink = (std::fmod(completeHoldTimer_, 0.16f) < 0.10f);
			Vector4 compCol = blink ? Vector4{ 0.45f, 1.0f, 0.70f, 1.0f * textAlpha } : Vector4{ 0.25f, 0.85f, 0.55f, 0.85f * textAlpha };
			tr->Print("HackGen", "complete!", panelX + panelW - 28.0f, panelY + 95.0f, 20.0f, compCol, { 1.0f, 0.0f });

			// 完了時ステータス
			tr->Print("HackGen", ">> ACCESS GRANTED // RETURNING TO DESKTOP ROOT <<",
				centerX, panelY + 152.0f, 15.0f, compCol, { 0.5f, 0.0f });
		} else {
			char pctBuf[32];
			snprintf(pctBuf, sizeof(pctBuf), "%3d %%", static_cast<int>(loadingProgress_ * 100.0f));
			tr->Print("HackGen", pctBuf, panelX + panelW - 28.0f, panelY + 95.0f, 18.0f,
				{ 0.50f, 0.85f, 0.95f, 0.90f * textAlpha }, { 1.0f, 0.0f });

			// ロードフェーズテキスト（タイトルの全カメラ等の事前読み込み状況をリアルタイム表示）
			const char* phaseText = loadingPhaseText_;
			if (!phaseText || phaseText[0] == '\0') {
				phaseText = "LOADING ALL TITLE CAMERAS & SENSORS...";
			}
			tr->Print("HackGen", phaseText, centerX, panelY + 152.0f, 13.0f,
				{ 0.35f, 0.75f, 0.85f, 0.80f * textAlpha }, { 0.5f, 0.0f });
		}
	}

	// 画面全体のゆっくり動く繊細な走査線
	float scanY = std::fmod(consoleBootTimer_ * 120.0f, kScreenHeight);
	pseudoOS_.DrawRect({ 0.0f, scanY }, { kScreenWidth, 1.0f }, { 0.20f, 0.70f, 0.90f, 0.15f * textAlpha });

	// トランジション進行中：文字グリッチ演出
	if (consoleExitTimer_ > 0.0f) {
		DrawTransitionGlitch();
	}
}

void TitleScene::DrawSortieSequence() {
	// 背景オーバーレイ
	backgroundPanel_.Draw();

	// フルOSデスクトップ環境（マルチウィンドウ・タスクバー・ターミナル内統合出撃コンソール）
	DrawOSDesktop();

	// 終盤のブラックアウト発光（黒フェードアウト）
	if (sortieFlashSprite_) {
		float blackAlpha = 0.0f;
		if (sortieTimer_ > 1.25f) {
			float p = (sortieTimer_ - 1.25f) / (kSortieDuration - 1.25f);
			p = (std::min)(1.0f, (std::max)(0.0f, p));
			blackAlpha = p * (3.0f - 2.0f * p); // スムーズなイージングで暗転
		}
		if (blackAlpha > 0.001f) {
			TextRenderer::GetInstance()->Flush(); // UIテキストを先に描画し、その上に黒フェードを覆い被せる
		}
		sortieFlashSprite_->SetColor({ 0.0f, 0.0f, 0.0f, blackAlpha });
		sortieFlashSprite_->Update();
		sortieFlashSprite_->Draw();
	}
}

// ------------------------------------------------------------
// 項目10: セッション終了・再認証セキュリティ演出描画
// ------------------------------------------------------------
void TitleScene::DrawReAuthentication() {
	if (consoleBgSprite_) {
		consoleBgSprite_->Draw();
	}
	backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	backgroundPanel_.Draw();

	TextRenderer* tr = TextRenderer::GetInstance();
	float centerX = kScreenWidth * 0.5f;
	float centerY = kScreenHeight * 0.5f;

	// タイトル
	tr->Print(
		"HackGen",
		"[ SESSION CLOSED. RE-AUTHENTICATION REQUIRED ]",
		centerX,
		centerY - 100.0f,
		28.0f,
		{ 1.0f, 0.35f, 0.35f, 0.95f },
		{ 0.5f, 0.0f }
	);

	tr->Print(
		"HackGen",
		"(セッション終了。再認証が必要です)",
		centerX,
		centerY - 60.0f,
		18.0f,
		{ 0.85f, 0.85f, 0.85f, 0.85f },
		{ 0.5f, 0.0f }
	);

	// ユーザー名
	if (reauthTimer_ >= 0.35f) {
		tr->Print(
			"HackGen",
			"USER IDENTITY: PILOT_01 // LEVEL-4 SECURITY CLEARANCE",
			centerX,
			centerY - 10.0f,
			18.0f,
			{ 0.30f, 0.90f, 0.60f, 0.90f },
			{ 0.5f, 0.0f }
		);
	}

	// 自動パスワード入力 (0.7s〜1.4s で ***** が1文字ずつ打たれる)
	if (reauthTimer_ >= 0.70f) {
		float pwTime = reauthTimer_ - 0.70f;
		int starCount = std::clamp(static_cast<int>(pwTime / 0.12f), 0, 5);
		std::string stars(starCount, '*');

		bool blink = (std::fmod(reauthTimer_, 0.20f) < 0.10f);
		std::string passDisplay = "PASSWORD: [ " + stars;
		if (starCount < 5 && blink) {
			passDisplay += "_";
		}
		passDisplay += " ]";

		tr->Print(
			"HackGen",
			passDisplay.c_str(),
			centerX,
			centerY + 30.0f,
			22.0f,
			{ 0.25f, 1.0f, 0.55f, 1.0f },
			{ 0.5f, 0.0f }
		);
	}

	// 認証成功表示 (1.5s以降)
	if (reauthTimer_ >= 1.50f) {
		tr->Print(
			"HackGen",
			">> [ AUTHENTICATION SUCCESSFUL. WELCOME BACK ] <<",
			centerX,
			centerY + 75.0f,
			20.0f,
			{ 1.0f, 0.90f, 0.30f, 1.0f },
			{ 0.5f, 0.0f }
		);
	}

	// スキップ案内
	tr->Print(
		"HackGen",
		"[ PRESS SPACE OR CLICK TO SKIP ]",
		centerX,
		kScreenHeight - 50.0f,
		14.0f,
		{ 0.5f, 0.7f, 0.6f, 0.6f },
		{ 0.5f, 0.5f }
	);
}

void TitleScene::DrawUI() {
	if (state_ == TitleState::DroneView) {
		DrawOSD();
		// 操作案内テキストの半透明ダーク帯を描画（項目9: 主翼や車輪との被りを100%防止）
		if (pressSpaceBannerBg_) {
			pressSpaceBannerBg_->Draw();
		}
		for (auto& sp : pressSpaceBorders_) {
			sp->Draw();
		}
		pressSpaceText_.Draw();
		DrawTransitionGlitch();
	} else if (state_ == TitleState::WindowClose) {
		DrawWindowClose();
	} else if (state_ == TitleState::ConsoleBoot) {
		DrawConsoleBoot();
	} else if (state_ == TitleState::ReAuthentication) {
		DrawReAuthentication();
	} else if (state_ == TitleState::Menu) {
		// 背景オーバーレイ
		backgroundPanel_.Draw();

		// メニュー移行トランジション（0.35秒間の滑らかな起動フェードイン＆走査線展開：項目3）
		if (menuEnterTimer_ > 0.0f) {
			DrawTransitionGlitch();
		}

		// フルOSデスクトップ環境（マルチウィンドウ、タスクバー、デスクトップアイコン、カスタムカーソル）
		DrawOSDesktop();
	} else if (state_ == TitleState::SortieSequence) {
		DrawSortieSequence();
	}
}

void TitleScene::Finalize() {
	SetCursorVisible(false); // ゲーム画面へ遷移するためOSカーソルを確実に非表示（項目9）
	UITextRegistry::GetInstance()->Clear();

	// ------------------------------------------------------------
	// 静的リソースキャッシュへの退避（リファクタリング最適化）
	// 次回タイトル訪問時のD3D12リソース生成（2,000バッファ以上）と
	// 3D背景109個のObject3d再初期化をスキップして0ms高速遷移を実現
	// ------------------------------------------------------------
	s_cache_.skybox = std::move(skybox_);
	s_cache_.locations = std::move(locations_);
	s_cache_.afterburner = std::move(afterburner_);
	s_cache_.consoleBgSprite = std::move(consoleBgSprite_);
	s_cache_.sortieFlashSprite = std::move(sortieFlashSprite_);
	s_cache_.recDotSprite = std::move(recDotSprite_);
	s_cache_.titleLogoSprite = std::move(titleLogoSprite_);
	s_cache_.headerLampSprite = std::move(headerLampSprite_);
	s_cache_.pressSpaceBannerBg = std::move(pressSpaceBannerBg_);
	s_cache_.osSpritePool = std::move(osSpritePool_);
	s_cache_.cursorSprites = std::move(cursorSprites_);
	s_cache_.windowCloseMaskSprites = std::move(windowCloseMaskSprites_);
	s_cache_.windowCloseBorderSprites = std::move(windowCloseBorderSprites_);
	s_cache_.menuBorderSprites = std::move(menuBorderSprites_);
	s_cache_.buttonBorders = std::move(buttonBorders_);
	s_cache_.glitchSprites = std::move(glitchSprites_);
	s_cache_.osdSprites = std::move(osdSprites_);
	s_cache_.pressSpaceBorders = std::move(pressSpaceBorders_);
	s_cache_.menuBorderOffsets = menuBorderOffsets_;
	s_cache_.buttonBorderOutlineOffsets = buttonBorderOutlineOffsets_;
	s_cache_.buttonBorderBracketOffsets = buttonBorderBracketOffsets_;
	s_cache_.buttonBorderIndicatorOffsets = buttonBorderIndicatorOffsets_;
	s_cache_.isCached = true;
}

#ifdef USE_IMGUI
void TitleScene::DrawAfterburnerEditor() {
	if (!afterburner_) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(480, 600), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("AFTERBURNER ASSET EDITOR", &showAfterburnerEditor_)) {
		ImGui::End();
		return;
	}

	LocationData& tunnelLoc = locations_[static_cast<int>(TitleLocation::Tunnel)];

	ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.55f, 1.0f), "/// AFTERBURNER & PROPULSION ASSET EDITOR ///");
	ImGui::TextDisabled("Real-time nozzle offset, flame geometry, shock diamonds, color & JSON pipeline");
	ImGui::Separator();

	// ------------------------------------------------------------
	// 1. エンジン構成 (Engine Mode)
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("1. Engine Configuration (Single / Twin)", ImGuiTreeNodeFlags_DefaultOpen)) {
		bool isTwin = afterburner_->IsTwinEngine();
		if (ImGui::Checkbox("Twin Engine Mode", &isTwin)) {
			afterburner_->SetTwinEngine(isTwin);
		}

		if (isTwin) {
			MyMath::Vector3 left = afterburner_->GetLeftNozzleOffset();
			MyMath::Vector3 right = afterburner_->GetRightNozzleOffset();
			bool changed = false;
			if (ImGui::DragFloat3("Left Nozzle Offset (X,Y,Z)", &left.x, 0.005f)) { changed = true; }
			if (ImGui::DragFloat3("Right Nozzle Offset (X,Y,Z)", &right.x, 0.005f)) { changed = true; }
			if (changed) {
				afterburner_->SetNozzleOffsets(left, right);
			}
		} else {
			MyMath::Vector3 single = afterburner_->GetSingleNozzleOffset();
			if (ImGui::DragFloat3("Single Nozzle Offset (X,Y,Z)", &single.x, 0.005f)) {
				afterburner_->SetSingleNozzleOffset(single);
			}
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 0.9f), "Mig-21 standard single nozzle: (0.000, 0.011, 14.950)");
		}

		// ノズル噴射角（向き・オイラー角）
		MyMath::Vector3 nRot = afterburner_->GetNozzleRotation();
		MyMath::Vector3 nRotDeg = {
			nRot.x * 180.0f / 3.14159265f,
			nRot.y * 180.0f / 3.14159265f,
			nRot.z * 180.0f / 3.14159265f
		};
		if (ImGui::DragFloat3("Nozzle Angle (Pitch, Yaw, Roll deg)", &nRotDeg.x, 0.5f, -180.0f, 180.0f, "%.1f deg")) {
			afterburner_->SetNozzleRotation({
				nRotDeg.x * 3.14159265f / 180.0f,
				nRotDeg.y * 3.14159265f / 180.0f,
				nRotDeg.z * 3.14159265f / 180.0f
			});
		}

		if (ImGui::Button("Flip Flame Direction (Yaw 180 deg)")) {
			MyMath::Vector3 r = afterburner_->GetNozzleRotation();
			r.y += 3.14159265f;
			if (r.y > 3.14159265f) { r.y -= 6.2831853f; }
			afterburner_->SetNozzleRotation(r);
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Angle (0, 0, 0)")) {
			afterburner_->SetNozzleRotation({ 0.0f, 0.0f, 0.0f });
		}
	}

	// ------------------------------------------------------------
	// 2. 炎のジオメトリ (Flame Geometry)
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("2. Flame Geometry & Shock Diamonds", ImGuiTreeNodeFlags_DefaultOpen)) {
		float radius = afterburner_->GetFlameRadius();
		if (ImGui::SliderFloat("Flame Radius (m)", &radius, 0.05f, 1.5f, "%.3f m")) {
			afterburner_->SetFlameRadius(radius);
		}

		float length = afterburner_->GetFlameLength();
		if (ImGui::SliderFloat("Flame Length (m)", &length, 0.5f, 20.0f, "%.2f m")) {
			afterburner_->SetFlameLength(length);
		}

		int diamonds = afterburner_->GetShockDiamondCount();
		if (ImGui::SliderInt("Shock Diamonds (Mach Disks)", &diamonds, 0, 10)) {
			afterburner_->SetShockDiamondCount(diamonds);
		}
	}

	// ------------------------------------------------------------
	// 3. カラー & 発光強度 (Color & Intensity)
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("3. Color & Glow Intensity", ImGuiTreeNodeFlags_DefaultOpen)) {
		MyMath::Vector3 flameColor = afterburner_->GetFlameColor();
		if (ImGui::ColorEdit3("Outer Flame Color", &flameColor.x)) {
			afterburner_->SetFlameColor(flameColor);
		}

		MyMath::Vector3 coreColor = afterburner_->GetCoreColor();
		if (ImGui::ColorEdit3("Shock Diamond / Core Color", &coreColor.x)) {
			afterburner_->SetCoreColor(coreColor);
		}

		float intensity = afterburner_->GetIntensity();
		if (ImGui::SliderFloat("AB Throttle / Intensity", &intensity, 0.0f, 3.0f, "%.2f")) {
			afterburner_->SetIntensity(intensity);
		}
	}

	// ------------------------------------------------------------
	// 4. 機体の向き・アライメント調整 (Aircraft Orientation)
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("4. Aircraft Orientation & Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Flight Motion / Sway Animation", &enableTunnelMotion_);
		ImGui::DragFloat3("Aircraft Rotation", &tunnelLoc.aircraftRotation.x, 0.01f);

		if (ImGui::Button("Flip 180 deg (Reverse)")) {
			tunnelLoc.aircraftRotation.y += 3.14159265f;
			if (tunnelLoc.aircraftRotation.y > 6.2831853f) {
				tunnelLoc.aircraftRotation.y -= 6.2831853f;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Align Forward (+Z)")) {
			tunnelLoc.aircraftRotation = { 0.0f, 3.14159265f, 0.0f };
			if (tunnelLoc.visualModel) {
				tunnelLoc.visualModel->SetBaseRotation({ 0.0f, 0.0f, 0.0f });
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Mig-21 Preset")) {
			afterburner_->SetTwinEngine(false);
			afterburner_->SetSingleNozzleOffset({ 0.0f, 0.011f, 14.95f });
			afterburner_->SetNozzleRotation({ 0.0f, 0.0f, 0.0f });
			afterburner_->SetFlameRadius(0.38f);
			afterburner_->SetFlameLength(6.2f);
			afterburner_->SetShockDiamondCount(5);
			afterburner_->SetFlameColor({ 0.28f, 0.65f, 1.0f });
			afterburner_->SetCoreColor({ 0.88f, 0.95f, 1.0f });
			afterburner_->SetIntensity(1.35f);
		}
	}

	// ------------------------------------------------------------
	// 5. JSON 保存 & 読み込み (Asset Pipeline)
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("5. Asset Pipeline (JSON Save/Load)", ImGuiTreeNodeFlags_DefaultOpen)) {
		char pathBuf[256];
		strncpy_s(pathBuf, afterburnerConfigPath_.c_str(), sizeof(pathBuf) - 1);
		if (ImGui::InputText("JSON Path", pathBuf, sizeof(pathBuf))) {
			afterburnerConfigPath_ = pathBuf;
		}

		if (ImGui::Button("Save Config")) {
			if (afterburner_->SaveConfig(afterburnerConfigPath_)) {
				abEditorStatusMsg_ = "SUCCESS: Saved to " + afterburnerConfigPath_;
				abEditorStatusTimer_ = 4.0f;
			} else {
				abEditorStatusMsg_ = "ERROR: Failed to save to " + afterburnerConfigPath_;
				abEditorStatusTimer_ = 5.0f;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Load Config")) {
			if (afterburner_->LoadConfig(afterburnerConfigPath_)) {
				abEditorStatusMsg_ = "SUCCESS: Loaded from " + afterburnerConfigPath_;
				abEditorStatusTimer_ = 4.0f;
			} else {
				abEditorStatusMsg_ = "ERROR: Failed to load from " + afterburnerConfigPath_;
				abEditorStatusTimer_ = 5.0f;
			}
		}

		if (abEditorStatusTimer_ > 0.0f) {
			abEditorStatusTimer_ -= 1.0f / 60.0f;
			if (abEditorStatusMsg_.find("SUCCESS") != std::string::npos) {
				ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", abEditorStatusMsg_.c_str());
			} else {
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", abEditorStatusMsg_.c_str());
			}
		}
	}

	ImGui::End();
}
#endif

// =========================================================================
// フルOSデスクトップ環境の実装 (DAWN_OS Desktop Environment)
// =========================================================================

void TitleScene::InitializeOSDesktop(SpriteCommon* spriteCommon) {
	// スプライトプールの初期化（512枚：プール枯渇防止）
	osSpritePool_.clear();
	for (int i = 0; i < 512; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition({ 0.0f, 0.0f });
		sp->SetSize({ 1.0f, 1.0f });
		sp->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		sp->Update();
		osSpritePool_.push_back(std::move(sp));
	}
	osSpritePoolIdx_ = 0;

	// 専用カーソルスプライトの初期化（24個：プール枯渇と完全分離：項目5）
	cursorSprites_.clear();
	for (int i = 0; i < 24; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition({ 0.0f, 0.0f });
		sp->SetSize({ 1.0f, 1.0f });
		sp->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		sp->Update();
		cursorSprites_.push_back(std::move(sp));
	}

	// 疑似OS共通基盤の初期化（テーマ設定）
	pseudoOS_.Initialize(spriteCommon, PseudoOS::Theme::TerminalGreen);

	ResetOSDesktopWindows();
}

void TitleScene::ResetOSDesktopWindows() {
	osWindows_.clear();

	// Window 0: メインターミナル (DAWN_OS)
	OSWindow win0;
	win0.id = 0;
	win0.title = "TERMINAL : DAWN_OS [v4.12]";
	win0.pos = { 392.0f, 44.0f };
	win0.size = { 520.0f, 616.0f };
	win0.normalPos = win0.pos;
	win0.normalSize = win0.size;
	win0.titleBarHeight = 34.0f;
	win0.isOpen = true;
	win0.isMinimized = false;
	win0.animScale = 1.0f;
	osWindows_.push_back(win0);

	// Window 1: 監視カメラ (CAM-01: OPTICAL FEED)
	OSWindow win1;
	win1.id = 1;
	win1.title = "CAM-01 : OPTICAL LIVE FEED";
	win1.pos = { 18.0f, 44.0f };
	win1.size = { 356.0f, 260.0f };
	win1.normalPos = win1.pos;
	win1.normalSize = win1.size;
	win1.titleBarHeight = 26.0f;
	win1.isOpen = true;
	win1.isMinimized = false;
	win1.animScale = 1.0f;
	osWindows_.push_back(win1);

	// Window 2: 機体テレメトリ (SYS_DIAG: TELEMETRY)
	OSWindow win2;
	win2.id = 2;
	win2.title = "SYS_DIAG : AVIONICS TELEMETRY";
	win2.pos = { 18.0f, 320.0f };
	win2.size = { 356.0f, 344.0f };
	win2.normalPos = win2.pos;
	win2.normalSize = win2.size;
	win2.titleBarHeight = 26.0f;
	win2.isOpen = true;
	win2.isMinimized = false;
	win2.animScale = 1.0f;
	osWindows_.push_back(win2);

	activeWindowId_ = 0; // メインターミナルがデフォルトアクティブ
	windowZOrder_ = { 1, 2, 0 }; // メインターミナル（0）が最前面

	desktopIcons_.clear();
}

void TitleScene::BringWindowToFront(int windowId) {
	activeWindowId_ = windowId;
	auto it = std::find(windowZOrder_.begin(), windowZOrder_.end(), windowId);
	if (it != windowZOrder_.end()) {
		windowZOrder_.erase(it);
	}
	windowZOrder_.push_back(windowId);
}

void TitleScene::DrawOSRect(const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
	if (osSpritePoolIdx_ >= osSpritePool_.size()) { return; }
	auto& sp = osSpritePool_[osSpritePoolIdx_++];
	sp->SetPosition(pos);
	sp->SetSize(size);
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

void TitleScene::DrawOSBorder(const Vector2& pos, const Vector2& size, const MyMath::Vector4& color, float thick) {
	DrawOSRect(pos, { size.x, thick }, color);
	DrawOSRect({ pos.x, pos.y + size.y - thick }, { size.x, thick }, color);
	DrawOSRect(pos, { thick, size.y }, color);
	DrawOSRect({ pos.x + size.x - thick, pos.y }, { thick, size.y }, color);
}

void TitleScene::DrawOSPanel(const Vector2& pos, const Vector2& size, const MyMath::Vector4& bgCol, const MyMath::Vector4& borderCol, float thick) {
	DrawOSRect(pos, size, bgCol);
	DrawOSBorder(pos, size, borderCol, thick);
}

void TitleScene::UpdateOSDesktop(float dt, const Vector2& mousePos, bool isLDown, bool isLTrig) {
	customCursorPos_ = mousePos;
	if (cursorRippleTimer_ > 0.0f) {
		cursorRippleTimer_ -= dt;
	}
	if (isLTrig) {
		cursorRippleTimer_ = 0.30f;
		cursorRipplePos_ = mousePos;
	}

	float mx = mousePos.x;
	float my = mousePos.y;
	const float tbY = 688.0f;

	// --- ウィンドウの最小化・復元アニメーション補間（項目3: トランジション追加）---
	for (auto& win : osWindows_) {
		float targetScale = (win.isOpen && !win.isMinimized) ? 1.0f : 0.0f;
		if (win.animScale < targetScale) {
			win.animScale = (std::min)(targetScale, win.animScale + dt * 6.5f);
		} else if (win.animScale > targetScale) {
			win.animScale = (std::max)(targetScale, win.animScale - dt * 6.5f);
		}
	}

	// --- スタートメニュー選択時のクッション演出（項目4: ハードカット解消）---
	if (startMenuTransitionTimer_ > 0.0f) {
		startMenuTransitionTimer_ -= dt;
		if (startMenuTransitionTimer_ <= 0.0f) {
			if (pendingStartMenuAction_ == 1) {
				state_ = TitleState::SortieSequence;
				sortieTimer_ = 0.0f;
			} else if (pendingStartMenuAction_ == 2) {
				sceneID = SCENE::MISSION_EDITOR;
			} else if (pendingStartMenuAction_ == 3) {
				if (osWindows_.size() >= 3) {
					osWindows_[0].pos = osWindows_[0].normalPos;
					osWindows_[0].isOpen = true; osWindows_[0].isMinimized = false;
					osWindows_[1].pos = osWindows_[1].normalPos;
					osWindows_[1].isOpen = true; osWindows_[1].isMinimized = false;
					osWindows_[2].pos = osWindows_[2].normalPos;
					osWindows_[2].isOpen = true; osWindows_[2].isMinimized = false;
					activeWindowId_ = 0;
				}
			} else if (pendingStartMenuAction_ == 4) {
				state_ = TitleState::ConsoleBoot;
				consoleBootTimer_ = 0.0f;
				consoleExitTimer_ = 0.0f;
				loadingProgress_ = 0.0f;
				isLoadingComplete_ = false;
				completeHoldTimer_ = 0.0f;
				prewarmPhase_ = 0;
				isTitleCamerasPrewarmed_ = false;
				loadingPhaseText_ = "INITIALIZING OPTICAL CAMERAS...";
			} else if (pendingStartMenuAction_ == 5) {
				PostQuitMessage(0);
			}
			pendingStartMenuAction_ = -1;
		}
		return; // クッション演出中は以降の入力を遮断
	}

	// --- 1. タスクバー & スタートメニューの判定 ---
	// スタートボタン (X: 8〜114, Y: 690〜718)
	if (isLTrig && mx >= 8.0f && mx <= 114.0f && my >= 690.0f && my <= 718.0f) {
		isStartMenuOpen_ = !isStartMenuOpen_;
	}

	// スタートメニューが開いている時の項目選択
	if (isStartMenuOpen_) {
		const float smX = 8.0f;
		const float smY = tbY - 188.0f;
		const float smW = 250.0f;
		const float smH = 184.0f;

		if (isLTrig) {
			if (mx >= smX && mx <= smX + smW && my >= smY && my <= smY + smH) {
				float relY = my - smY;
				if (relY >= 32.0f && relY < 60.0f) {
					// [01] EXECUTE SORTIE -> クッション演出経由（項目4）
					startMenuTransitionTimer_ = kStartMenuTransitionDuration;
					pendingStartMenuAction_ = 1;
					isStartMenuOpen_ = false;
				} else if (relY >= 60.0f && relY < 88.0f) {
					// [02] MISSION EDITOR
					startMenuTransitionTimer_ = kStartMenuTransitionDuration;
					pendingStartMenuAction_ = 2;
					isStartMenuOpen_ = false;
				} else if (relY >= 88.0f && relY < 116.0f) {
					// [03] RESET WINDOW LAYOUT
					startMenuTransitionTimer_ = kStartMenuTransitionDuration;
					pendingStartMenuAction_ = 3;
					isStartMenuOpen_ = false;
				} else if (relY >= 116.0f && relY < 144.0f) {
					// [04] REBOOT TERMINAL
					startMenuTransitionTimer_ = kStartMenuTransitionDuration;
					pendingStartMenuAction_ = 4;
					isStartMenuOpen_ = false;
				} else if (relY >= 148.0f && relY < 178.0f) {
					// [05] SHUTDOWN
					PostQuitMessage(0);
				}
			} else if (my < tbY) {
				isStartMenuOpen_ = false;
			}
		}
	}

	// タスクバーアイテムクリック判定 (項目9: 最小化ウィンドウも常時表示＆クリックで即復元)
	if (isLTrig && my >= tbY && my <= kScreenHeight) {
		float tabX = 120.0f;
		for (auto& win : osWindows_) {
			float tabW = 142.0f;
			if (mx >= tabX && mx <= tabX + tabW) {
				if (!win.isOpen || win.isMinimized) {
					// 最小化・閉状態からの拡大復元アニメーション（項目3 & 9）
					win.isOpen = true;
					win.isMinimized = false;
					BringWindowToFront(win.id);
				} else if (activeWindowId_ == win.id) {
					// アクティブウィンドウならタスクバーへ縮小最小化
					win.isMinimized = true;
				} else {
					// フォーカス切替＆最前面化
					BringWindowToFront(win.id);
				}
				break;
			}
			tabX += tabW + 6.0f;
		}
	}

	// --- 2. ウィンドウドラッグ＆操作判定（項目1: 残像を一切出さず単一位置更新）---
	for (auto& win : osWindows_) {
		if (win.isDragging) {
			if (!isLDown) {
				win.isDragging = false;
			} else {
				win.pos = { mx - win.dragOffset.x, my - win.dragOffset.y };
				win.pos.x = std::clamp(win.pos.x, -win.size.x + 80.0f, kScreenWidth - 80.0f);
				win.pos.y = std::clamp(win.pos.y, 0.0f, tbY - win.titleBarHeight);
			}
		}
	}

	// 手前（最前面＝windowZOrder_ の末尾）から奥に向かって逆順に判定
	std::vector<int> testOrder;
	for (auto it = windowZOrder_.rbegin(); it != windowZOrder_.rend(); ++it) {
		testOrder.push_back(*it);
	}

	if (isLTrig && !isStartMenuOpen_ && my < tbY) {
		for (int winId : testOrder) {
			if (winId < 0 || winId >= static_cast<int>(osWindows_.size())) { continue; }
			auto& win = osWindows_[winId];
			if (!win.isOpen || win.isMinimized || win.animScale < 0.5f) { continue; }

			const float btnW = 20.0f;
			const float btnH = 18.0f;
			const float btnY = win.pos.y + (win.titleBarHeight - btnH) * 0.5f;
			const float closeX = win.pos.x + win.size.x - 26.0f;
			const float maxmX  = closeX - 22.0f;
			const float miniX  = maxmX - 22.0f;

			// 右上制御ボタン判定
			if (my >= btnY && my <= btnY + btnH) {
				if (mx >= closeX && mx <= closeX + btnW) {
					// [X] ボタン: タスクバーへ最小化トランジション（項目3）
					win.isMinimized = true;
					break;
				} else if (mx >= maxmX && mx <= maxmX + btnW) {
					// [口] ボタン: 定位置リセット
					win.pos = win.normalPos;
					BringWindowToFront(win.id);
					break;
				} else if (mx >= miniX && mx <= miniX + btnW) {
					// [_] ボタン: タスクバーへ最小化トランジション（項目3）
					win.isMinimized = true;
					break;
				}
			}

			// タイトルバードラッグ開始
			if (mx >= win.pos.x && mx <= win.pos.x + win.size.x &&
				my >= win.pos.y && my <= win.pos.y + win.titleBarHeight) {
				win.isDragging = true;
				win.dragOffset = { mx - win.pos.x, my - win.pos.y };
				BringWindowToFront(win.id);
				break;
			}

			// ウィンドウ内クリックで最前面化
			if (mx >= win.pos.x && mx <= win.pos.x + win.size.x &&
				my >= win.pos.y && my <= win.pos.y + win.size.y) {
				BringWindowToFront(win.id);

				if (win.id == 1) {
					// 左上サブウィンドウのボタン判定
					int selIdx = selectionManager_.GetSelectedIndex();
					const float btnRowY = win.pos.y + win.titleBarHeight + 176.0f;
					const float btnW = 68.0f;
					const float btnH = 22.0f;
					for (int b = 0; b < 3; ++b) {
						float bx = win.pos.x + 14.0f + b * 74.0f;
						if (mx >= bx && mx <= bx + btnW && my >= btnRowY && my <= btnRowY + btnH) {
							cursorRippleTimer_ = 0.30f;
							cursorRipplePos_ = { mx, my };
							if (selIdx == 0) {
								ChangeLocation(b);
							} else if (selIdx == 1) {
								activeSatLayer_ = b;
							} else if (selIdx == 2) {
								activeSysDetailTab_ = b;
							} else if (selIdx == 3) {
								if (b == 0) {
									state_ = TitleState::DroneView;
									stateTimer_ = 0.0f;
									SetCursorVisible(false);
								} else if (b == 1) {
									::PostQuitMessage(0);
								} else if (b == 2) {
									state_ = TitleState::ConsoleBoot;
									consoleBootTimer_ = 0.0f;
									consoleExitTimer_ = 0.0f;
									loadingProgress_ = 0.0f;
									isLoadingComplete_ = false;
									completeHoldTimer_ = 0.0f;
									prewarmPhase_ = 0;
									isTitleCamerasPrewarmed_ = false;
									loadingPhaseText_ = "INITIALIZING OPTICAL CAMERAS...";
								}
							}
							break;
						}
					}
				} else if (win.id == 2) {
					// 左下サブウィンドウのボタン判定
					int selIdx = selectionManager_.GetSelectedIndex();
					const float actBtnY = win.pos.y + win.titleBarHeight + 276.0f;
					const float actBtnW = 104.0f;
					const float actBtnH = 24.0f;
					for (int b = 0; b < 3; ++b) {
						float bx = win.pos.x + 14.0f + b * 112.0f;
						if (mx >= bx && mx <= bx + actBtnW && my >= actBtnY && my <= actBtnY + actBtnH) {
							cursorRippleTimer_ = 0.30f;
							cursorRipplePos_ = { mx, my };
							if (selIdx == 0) {
								if (b == 0) {
									// [ENG TEST]: 推力テスト開始！
									isEngineTesting_ = true;
									engineTestTimer_ = 4.5f;
								} else if (b == 1) {
									// [DEFCON 1/2]: 警戒レベル切替
									defconLevel_ = (defconLevel_ == 2) ? 1 : 2;
								} else if (b == 2) {
									// [SORTIE >>]: 出撃シーケンス発火！
									startButton_.Click();
								}
							} else if (selIdx == 1) {
								if (b == 0) {
									activeIntelTarget_ = 0;
								} else if (b == 1) {
									activeIntelTarget_ = 1;
								} else if (b == 2) {
									// [EDIT MISS >>]: ミッションエディタ起動
									editorButton_.Click();
								}
							} else if (selIdx == 2) {
								if (b == 0) {
									// [MUTE BGM]
								} else if (b == 1) {
									// [RE-SCAN]
								} else if (b == 2) {
									// [CALIBRATE]
								}
							} else if (selIdx == 3) {
								if (b == 0) {
									// [CANCEL]
								} else if (b == 1) {
									// [SUSPEND]
								} else if (b == 2) {
									// [EXIT >>]: 終了
									exitButton_.Click();
								}
							}
							break;
						}
					}
				}
				break;
			}
		}
	}

	if (osWindows_.size() > 0) {
		menuWindowPos_ = osWindows_[0].pos;
		UpdateMenuWindowPosition(menuWindowPos_);
	}

	// メニュー選択に連動したサブウィンドウタイトルの動的更新
	if (osWindows_.size() > 2) {
		int selIdx = selectionManager_.GetSelectedIndex();
		if (selIdx == 0) {
			osWindows_[1].title = "CAM-01 : HANGAR LIVE FEED [OPTICAL]";
			osWindows_[2].title = "SYS_DIAG : AVIONICS TELEMETRY [MiG-21]";
		} else if (selIdx == 1) {
			osWindows_[1].title = "SAT_MAP : TACTICAL SATELLITE RECON";
			osWindows_[2].title = "INTEL : ENEMY FORCES ANALYSIS";
		} else if (selIdx == 2) {
			osWindows_[1].title = "SYS_CFG : HARDWARE & GRAPHICS BUS";
			osWindows_[2].title = "SYS_CFG : AUDIO & CONTROL MATRIX";
		} else {
			osWindows_[1].title = "PWR_MGMT : SYSTEM SHUTDOWN SEQ";
			osWindows_[2].title = "SECURITY : SESSION TERMINATION";
		}
	}
}

void TitleScene::DrawOSTaskbar() {
	TextRenderer* tr = TextRenderer::GetInstance();
	const float tbY = 688.0f;
	const float tbH = 32.0f;

	// 1. タスクバー背景 & 上部境界線
	DrawOSRect({ 0.0f, tbY }, { kScreenWidth, tbH }, { 0.01f, 0.04f, 0.02f, 0.98f });
	DrawOSRect({ 0.0f, tbY }, { kScreenWidth, 1.5f }, { 0.25f, 0.95f, 0.55f, 0.95f });

	// 2. スタートボタン [ ⯈ DAWN_OS ]
	bool smHover = (customCursorPos_.x >= 8.0f && customCursorPos_.x <= 114.0f && customCursorPos_.y >= 690.0f && customCursorPos_.y <= 718.0f);
	Vector4 startBg = (isStartMenuOpen_ || smHover) ? Vector4{ 0.10f, 0.40f, 0.20f, 0.92f } : Vector4{ 0.03f, 0.14f, 0.07f, 0.90f };
	Vector4 startLine = (isStartMenuOpen_ || smHover) ? Vector4{ 0.45f, 1.0f, 0.70f, 1.0f } : Vector4{ 0.25f, 0.85f, 0.50f, 0.85f };
	DrawOSPanel({ 8.0f, tbY + 3.0f }, { 104.0f, 26.0f }, startBg, startLine, 1.5f);
	tr->Print("HackGen", "[⯈ DAWN_OS]", 14.0f, tbY + 8.0f, 12.0f, { 0.40f, 1.0f, 0.65f, 0.98f });

	// 3. 全ウィンドウのタスクバータブ常時表示（項目9: 最小化中も明確に表示しクリックで復元）
	float tabX = 120.0f;
	for (auto& win : osWindows_) {
		float tabW = 142.0f;
		float tabH = 24.0f;
		float tabY = tbY + 4.0f;

		bool isAct = (activeWindowId_ == win.id && !win.isMinimized && win.isOpen);
		bool isMin = (win.isMinimized || !win.isOpen);
		bool isHov = (customCursorPos_.x >= tabX && customCursorPos_.x <= tabX + tabW && customCursorPos_.y >= tabY && customCursorPos_.y <= tabY + tabH);

		Vector4 tBg = isAct ? Vector4{ 0.08f, 0.35f, 0.18f, 0.95f }
		            : (isMin ? Vector4{ 0.04f, 0.10f, 0.06f, 0.85f }
		            : (isHov ? Vector4{ 0.05f, 0.22f, 0.11f, 0.85f } : Vector4{ 0.02f, 0.09f, 0.05f, 0.80f }));

		Vector4 tBorder = isAct ? Vector4{ 0.45f, 1.0f, 0.70f, 0.98f }
		                : (isMin ? Vector4{ 0.70f, 0.60f, 0.18f, 0.80f }
		                : (isHov ? Vector4{ 0.35f, 0.90f, 0.55f, 0.85f } : Vector4{ 0.20f, 0.60f, 0.32f, 0.70f }));

		DrawOSPanel({ tabX, tabY }, { tabW, tabH }, tBg, tBorder, isAct ? 1.5f : 1.0f);
		if (isAct) {
			DrawOSRect({ tabX, tabY }, { tabW, 2.0f }, { 0.75f, 1.0f, 0.88f, 1.0f }); // アクティブタブ上端ネオンライン
		}

		// ステータスLED（最小化中はアンバー点滅：項目9）
		bool amberBlink = (std::fmod(animTimer_, 0.6f) < 0.35f);
		Vector4 dotCol = isAct ? Vector4{ 0.35f, 1.0f, 0.60f, 1.0f }
		               : (isMin ? (amberBlink ? Vector4{ 1.0f, 0.80f, 0.20f, 1.0f } : Vector4{ 0.45f, 0.35f, 0.08f, 0.6f })
		                        : Vector4{ 0.25f, 0.65f, 0.35f, 0.8f });
		DrawOSRect({ tabX + 7.0f, tabY + 8.0f }, { 6.0f, 6.0f }, dotCol);

		std::string tabLabel;
		if (win.id == 0) {
			tabLabel = isMin ? "TERM [_MIN]" : (isAct ? "* TERM: DAWN" : "TERM: DAWN_OS");
		} else if (win.id == 1) {
			int selIdx = selectionManager_.GetSelectedIndex();
			if (isMin) {
				tabLabel = "SUB1 [_MIN]";
			} else {
				if (selIdx == 0) tabLabel = "CAM-01: OPT";
				else if (selIdx == 1) tabLabel = "SAT: RECON";
				else if (selIdx == 2) tabLabel = "SYS: HW BUS";
				else tabLabel = "PWR: SHUTDN";
			}
		} else if (win.id == 2) {
			int selIdx = selectionManager_.GetSelectedIndex();
			if (isMin) {
				tabLabel = "SUB2 [_MIN]";
			} else {
				if (selIdx == 0) tabLabel = "SYS: TELEM";
				else if (selIdx == 1) tabLabel = "INTEL: THREAT";
				else if (selIdx == 2) tabLabel = "SYS: AUDIO";
				else tabLabel = "SEC: LOGOUT";
			}
		}

		Vector4 textCol = isAct ? Vector4{ 0.90f, 1.0f, 0.95f, 1.0f }
		                : (isMin ? Vector4{ 0.95f, 0.85f, 0.35f, 0.90f }
		                : (isHov ? Vector4{ 0.70f, 1.0f, 0.80f, 0.95f } : Vector4{ 0.35f, 0.75f, 0.50f, 0.85f }));
		tr->Print("HackGen", tabLabel, tabX + 18.0f, tabY + 6.0f, 10.5f, textCol);

		tabX += tabW + 6.0f;
	}

	// 4. システムトレイ
	float cpuNoise = 14.0f + 5.0f * std::sin(animTimer_ * 1.5f) + 3.0f * std::sin(animTimer_ * 3.7f);
	float memNoise = 38.0f + 2.0f * std::sin(animTimer_ * 0.7f);
	char sysBuf[64];
	snprintf(sysBuf, sizeof(sysBuf), "CPU: %d%% | MEM: %d%%", static_cast<int>(cpuNoise), static_cast<int>(memNoise));
	tr->Print("HackGen", sysBuf, kScreenWidth - 290.0f, tbY + 9.0f, 11.5f, { 0.30f, 0.80f, 0.45f, 0.85f });

	bool netBlink = (std::fmod(animTimer_, 0.8f) < 0.65f);
	Vector4 netLedCol = netBlink ? Vector4{ 0.25f, 1.0f, 0.55f, 0.95f } : Vector4{ 0.10f, 0.35f, 0.20f, 0.40f };
	DrawOSRect({ kScreenWidth - 152.0f, tbY + 12.0f }, { 6.0f, 6.0f }, netLedCol);
	tr->Print("HackGen", "NET: 10G", kScreenWidth - 140.0f, tbY + 9.0f, 11.0f, { 0.35f, 0.90f, 0.55f, 0.85f });

	char timeBuf[32];
	int totalSec = static_cast<int>(animTimer_);
	int hrs = (totalSec / 3600) % 24;
	int mins = (totalSec / 60) % 60;
	int secs = totalSec % 60;
	snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d UTC", (16 + hrs) % 24, mins, secs);
	tr->Print("HackGen", timeBuf, kScreenWidth - 82.0f, tbY + 9.0f, 11.5f, { 0.40f, 1.0f, 0.65f, 0.95f });

	// 5. スタートメニューポップアップ
	if (isStartMenuOpen_) {
		const float smX = 8.0f;
		const float smY = tbY - 188.0f;
		const float smW = 250.0f;
		const float smH = 184.0f;

		DrawOSPanel({ smX, smY }, { smW, smH }, { 0.01f, 0.04f, 0.02f, 0.98f }, { 0.35f, 1.0f, 0.60f, 0.95f }, 1.5f);
		DrawOSRect({ smX, smY }, { smW, 26.0f }, { 0.03f, 0.16f, 0.08f, 0.98f });
		tr->Print("HackGen", "DAWN_OS : ROOT SESSION", smX + 10.0f, smY + 6.0f, 11.5f, { 0.40f, 1.0f, 0.70f, 0.95f });

		const char* menuItems[] = {
			"[01] EXECUTE COMBAT SORTIE",
			"[02] LAUNCH MISSION EDITOR",
			"[03] RESET WINDOW LAYOUT",
			"[04] REBOOT TERMINAL",
			"[05] SHUTDOWN SYSTEM"
		};

		float itemY = smY + 34.0f;
		for (int i = 0; i < 5; ++i) {
			if (i == 4) {
				DrawOSRect({ smX + 8.0f, itemY - 2.0f }, { smW - 16.0f, 1.0f }, { 0.18f, 0.55f, 0.30f, 0.50f });
				itemY += 6.0f;
			}
			bool itemHov = (customCursorPos_.x >= smX && customCursorPos_.x <= smX + smW &&
			                customCursorPos_.y >= itemY - 4.0f && customCursorPos_.y < itemY + 22.0f);
			if (itemHov) {
				DrawOSRect({ smX + 4.0f, itemY - 2.0f }, { smW - 8.0f, 22.0f }, { 0.08f, 0.30f, 0.15f, 0.85f });
			}
			Vector4 itemCol = itemHov ? Vector4{ 1.0f, 0.95f, 0.30f, 1.0f }
			                : (i == 4 ? Vector4{ 0.90f, 0.40f, 0.35f, 0.90f } : Vector4{ 0.35f, 0.90f, 0.55f, 0.90f });
			tr->Print("HackGen", menuItems[i], smX + 12.0f, itemY + 2.0f, 11.5f, itemCol);
			itemY += 26.0f;
		}
	}
}

void TitleScene::DrawOSWindowCam(const OSWindow& win) {
	if (win.animScale <= 0.01f) { return; } // 完全最小化中は描画スキップ
	TextRenderer* tr = TextRenderer::GetInstance();

	// 最小化・展開トランジション座標計算（項目3）
	float s = win.animScale;
	float smoothS = s * s * (3.0f - 2.0f * s);
	const float tabTargetX = 120.0f + 148.0f + 70.0f;
	const float tabTargetY = 688.0f + 16.0f;
	auto lerpVal = [](float a, float b, float t) { return a + (b - a) * t; };
	Vector2 curPos = {
		lerpVal(tabTargetX - (win.size.x * 0.5f) * smoothS, win.pos.x, smoothS),
		lerpVal(tabTargetY - (win.size.y * 0.5f) * smoothS, win.pos.y, smoothS)
	};
	Vector2 curSize = { win.size.x * smoothS, win.size.y * smoothS };

	bool isActive = (activeWindowId_ == win.id);
	Vector4 borderCol = isActive ? Vector4{ 0.45f, 1.0f, 0.70f, 0.98f * smoothS } : Vector4{ 0.10f, 0.30f, 0.15f, 0.40f * smoothS };
	Vector4 titleBarBg = isActive ? Vector4{ 0.06f, 0.25f, 0.12f, 0.98f } : Vector4{ 0.015f, 0.05f, 0.025f, 0.85f };

	// 連動切り替え演出（切り替え直後のサイバー発光）
	float switchRatio = (kSubWindowSwitchDuration > 0.0f) ? (subWindowSwitchTimer_ / kSubWindowSwitchDuration) : 0.0f;
	if (switchRatio > 0.0f) {
		borderCol.x = (std::min)(1.0f, borderCol.x + 0.50f * switchRatio);
		borderCol.y = (std::min)(1.0f, borderCol.y + 0.30f * switchRatio);
		borderCol.z = (std::min)(1.0f, borderCol.z + 0.45f * switchRatio);
	}

	// 1. ウィンドウ本体パネル（背後ウィンドウのテキスト・スプライトを100%遮蔽する完全不透明プレート）
	DrawOSPanel(curPos, curSize, { 0.01f, 0.04f, 0.02f, 1.0f }, borderCol, isActive ? 2.0f : 1.0f);

	if (smoothS < 0.40f) { return; } // アニメーション極小時は枠のみ描画

	// 2. タイトルバー
	DrawOSRect(curPos, { curSize.x, win.titleBarHeight * smoothS }, titleBarBg);
	DrawOSRect({ curPos.x, curPos.y + win.titleBarHeight * smoothS - 1.0f }, { curSize.x, 1.0f }, borderCol);
	if (isActive) {
		DrawOSRect(curPos, { curSize.x, 2.0f }, { 0.70f, 1.0f, 0.85f, 1.0f }); // 上端ネオンアクセントライン
		DrawOSRect({ curPos.x + 8.0f, curPos.y + 9.0f }, { 6.0f, 6.0f }, { 0.40f, 1.0f, 0.70f, 1.0f }); // 高輝度LED
		tr->Print("HackGen", "[FOCUS] " + win.title, curPos.x + 18.0f, curPos.y + 6.0f, 11.5f * smoothS, { 0.85f, 1.0f, 0.92f, 1.0f });
	} else {
		DrawOSRect({ curPos.x + 8.0f, curPos.y + 9.0f }, { 6.0f, 6.0f }, { 0.12f, 0.30f, 0.16f, 0.45f }); // 消灯LED
		tr->Print("HackGen", "[IDLE]  " + win.title, curPos.x + 18.0f, curPos.y + 6.0f, 11.5f * smoothS, { 0.20f, 0.48f, 0.28f, 0.60f });
	}

	const float btnY = curPos.y + 5.0f;
	const float closeX = curPos.x + curSize.x - 24.0f;
	const float maxmX  = closeX - 22.0f;
	const float miniX  = maxmX - 22.0f;

	tr->Print("HackGen", "[_]", miniX, btnY + 1.0f, 10.5f, { 0.40f, 0.80f, 0.55f, 0.85f });
	tr->Print("HackGen", "[口]", maxmX, btnY + 1.0f, 10.5f, { 0.40f, 0.80f, 0.55f, 0.85f });
	tr->Print("HackGen", "[X]", closeX, btnY + 1.0f, 10.5f, { 0.95f, 0.35f, 0.30f, 0.95f });

	// ボタンホバー判定ラムダ
	auto isBtnHov = [&](float bx, float by, float bw, float bh) {
		return (customCursorPos_.x >= bx && customCursorPos_.x <= bx + bw &&
		        customCursorPos_.y >= by && customCursorPos_.y <= by + bh);
	};

	// 3. ウィンドウ内コンテンツ（中央メニュー連動 ＋ 実際のインタラクティブ操作）
	int selIdx = selectionManager_.GetSelectedIndex();
	float cy = curPos.y + win.titleBarHeight + 10.0f;

	if (selIdx == 0) {
		// ==========================================
		// [01] EXECUTE SORTIE: ハンガーのライブ映像
		// ==========================================
		bool recBlink = (std::fmod(animTimer_, 1.0f) < 0.65f);
		Vector4 recCol = recBlink ? Vector4{ 1.0f, 0.25f, 0.20f, 0.98f } : Vector4{ 0.40f, 0.08f, 0.08f, 0.35f };
		DrawOSRect({ curPos.x + 14.0f, cy + 3.0f }, { 8.0f, 8.0f }, recCol);
		tr->Print("HackGen", "REC [LIVE STREAM]", curPos.x + 28.0f, cy, 11.5f, { 0.95f, 0.95f, 0.95f, 0.95f });
		tr->Print("HackGen", "1080p 60FPS", curPos.x + curSize.x - 90.0f, cy, 11.0f, { 0.35f, 0.85f, 0.55f, 0.85f });
		cy += 24.0f;

		// プレビュー枠
		DrawOSPanel({ curPos.x + 14.0f, cy }, { curSize.x - 28.0f, 112.0f }, { 0.005f, 0.02f, 0.01f, 0.85f }, { 0.15f, 0.50f, 0.28f, 0.60f }, 1.0f);

		float cx = curPos.x + curSize.x * 0.5f;
		float midY = cy + 56.0f;
		DrawOSRect({ cx - 8.0f, midY }, { 16.0f, 1.0f }, { 0.25f, 0.85f, 0.45f, 0.70f });
		DrawOSRect({ cx, midY - 8.0f }, { 1.0f, 16.0f }, { 0.25f, 0.85f, 0.45f, 0.70f });

		const char* locNames[] = { "LOC: HANGAR_BAY_01", "LOC: SUB_TUNNEL_04", "LOC: SECTOR_07_CRASH" };
		int curIdx = std::clamp(currentLocationIndex_, 0, 2);
		tr->Print("HackGen", locNames[curIdx], cx, midY - 26.0f, 12.0f, { 0.45f, 1.0f, 0.70f, 0.95f }, { 0.5f, 0.5f });
		tr->Print("HackGen", "AZ: 042° | EL: +12° | OPTICAL LINK: OK", cx, midY + 18.0f, 10.5f, { 0.30f, 0.75f, 0.45f, 0.80f }, { 0.5f, 0.5f });
		cy += 124.0f;

		// チャンネル切替ボタン（クリックで実際にカメラ位置が切替）
		tr->Print("HackGen", "CAMERA CHANNEL (CLICK TO SWITCH):", curPos.x + 16.0f, cy + 2.0f, 10.5f, { 0.35f, 0.75f, 0.50f, 0.85f });
		cy += 18.0f;

		const char* chLabels[] = { "[CH 01]", "[CH 02]", "[CH 03]" };
		for (int c = 0; c < 3; ++c) {
			float bx = curPos.x + 14.0f + c * 74.0f;
			bool isCur = (currentLocationIndex_ == c);
			bool isHov = isBtnHov(bx, cy, 68.0f, 22.0f);
			Vector4 bBg = isCur ? Vector4{ 0.12f, 0.45f, 0.22f, 0.90f }
			            : (isHov ? Vector4{ 0.06f, 0.25f, 0.12f, 0.85f } : Vector4{ 0.02f, 0.10f, 0.05f, 0.70f });
			Vector4 bBorder = isCur ? Vector4{ 0.55f, 1.0f, 0.75f, 1.0f }
			                : (isHov ? Vector4{ 0.40f, 0.90f, 0.60f, 0.90f } : Vector4{ 0.18f, 0.50f, 0.28f, 0.65f });
			DrawOSPanel({ bx, cy }, { 68.0f, 22.0f }, bBg, bBorder, 1.0f);
			Vector4 textCol = isCur ? Vector4{ 1.0f, 0.95f, 0.30f, 1.0f }
			                : (isHov ? Vector4{ 0.85f, 1.0f, 0.90f, 1.0f } : Vector4{ 0.35f, 0.85f, 0.55f, 0.85f });
			tr->Print("HackGen", chLabels[c], bx + 34.0f, cy + 4.0f, 11.0f, textCol, { 0.5f, 0.0f });
		}
	} else if (selIdx == 1) {
		// ==========================================
		// [02] MISSION EDITOR: 作戦エリアの衛星マップ
		// ==========================================
		bool satBlink = (std::fmod(animTimer_, 0.8f) < 0.50f);
		Vector4 satCol = satBlink ? Vector4{ 0.20f, 0.85f, 1.0f, 0.98f } : Vector4{ 0.05f, 0.25f, 0.45f, 0.40f };
		DrawOSRect({ curPos.x + 14.0f, cy + 3.0f }, { 8.0f, 8.0f }, satCol);
		tr->Print("HackGen", "SAT_LINK [KEYHOLE-12]", curPos.x + 28.0f, cy, 11.5f, { 0.40f, 0.90f, 1.0f, 0.95f });
		tr->Print("HackGen", "RES: 0.25m / MULTISPEC", curPos.x + curSize.x - 146.0f, cy, 10.5f, { 0.30f, 0.75f, 0.85f, 0.85f });
		cy += 24.0f;

		// 衛星マップ描画枠
		const float mapW = curSize.x - 28.0f;
		const float mapH = 112.0f;
		float mapStartX = curPos.x + 14.0f;
		float mapStartY = cy;
		DrawOSPanel({ mapStartX, mapStartY }, { mapW, mapH }, { 0.008f, 0.025f, 0.035f, 0.90f }, { 0.15f, 0.50f, 0.70f, 0.75f }, 1.0f);

		// マップ内の格子グリッド線
		for (int gx = 1; gx < 4; ++gx) {
			DrawOSRect({ mapStartX + mapW * (gx * 0.25f), mapStartY }, { 1.0f, mapH }, { 0.12f, 0.35f, 0.50f, 0.35f });
		}
		for (int gy = 1; gy < 3; ++gy) {
			DrawOSRect({ mapStartX, mapStartY + mapH * (gy * 0.333f) }, { mapW, 1.0f }, { 0.12f, 0.35f, 0.50f, 0.35f });
		}

		if (activeSatLayer_ == 0) {
			// TERRAIN レイヤー
			tr->Print("HackGen", "[LAYER 0: TOPOGRAPHY / ELEVATION]", mapStartX + mapW * 0.5f, mapStartY + 28.0f, 11.0f, { 0.35f, 0.85f, 0.95f, 0.90f }, { 0.5f, 0.5f });
			tr->Print("HackGen", "ELEV: 240m - 1200m | SLOPE: 12.4 DEG", mapStartX + mapW * 0.5f, mapStartY + 50.0f, 10.5f, { 0.25f, 0.75f, 0.80f, 0.85f }, { 0.5f, 0.5f });
			tr->Print("HackGen", "NO OBSTACLE IN GLIDESLOPE PATH", mapStartX + mapW * 0.5f, mapStartY + 72.0f, 10.5f, { 0.40f, 1.0f, 0.70f, 0.90f }, { 0.5f, 0.5f });
		} else if (activeSatLayer_ == 1) {
			// RADAR レイヤー
			float sweepAngle = animTimer_ * 2.2f;
			float scx = mapStartX + mapW * 0.5f;
			float scy = mapStartY + mapH * 0.5f;
			float sLen = 42.0f;
			float endX = scx + sLen * std::cos(sweepAngle);
			float endY = scy + sLen * std::sin(sweepAngle);
			DrawOSRect({ scx, scy }, { 2.0f, 2.0f }, { 0.40f, 1.0f, 0.80f, 1.0f });
			DrawOSRect({ (scx + endX) * 0.5f - 15.0f, (scy + endY) * 0.5f }, { 30.0f, 1.0f }, { 0.20f, 0.90f, 1.0f, 0.75f });
			tr->Print("HackGen", "[LAYER 1: RADAR APERTURE SCAN]", mapStartX + mapW * 0.5f, mapStartY + 24.0f, 11.0f, { 0.35f, 1.0f, 0.70f, 0.90f }, { 0.5f, 0.5f });
			tr->Print("HackGen", "TARGET BOGEY ECHO DETECTED (x4)", mapStartX + mapW * 0.5f, mapStartY + 84.0f, 10.5f, { 1.0f, 0.85f, 0.25f, 0.95f }, { 0.5f, 0.5f });
		} else {
			// THREAT レイヤー
			float cx = mapStartX + mapW * 0.5f;
			float cyM = mapStartY + mapH * 0.5f;
			DrawOSBorder({ cx - 44.0f, cyM - 34.0f }, { 88.0f, 68.0f }, { 1.0f, 0.25f, 0.20f, 0.80f }, 1.2f);
			tr->Print("HackGen", "[LAYER 2: SAM ENGAGEMENT ENVELOPE]", mapStartX + mapW * 0.5f, mapStartY + 24.0f, 11.0f, { 1.0f, 0.40f, 0.35f, 0.95f }, { 0.5f, 0.5f });
			tr->Print("HackGen", "TOR-M1 COVERAGE: 12KM RADIUS", mapStartX + mapW * 0.5f, mapStartY + 84.0f, 10.5f, { 1.0f, 0.30f, 0.25f, 0.95f }, { 0.5f, 0.5f });
		}
		cy += 124.0f;

		tr->Print("HackGen", "SAT OVERLAY LAYER (CLICK TO SWITCH):", curPos.x + 16.0f, cy + 2.0f, 10.5f, { 0.30f, 0.80f, 0.90f, 0.85f });
		cy += 18.0f;

		const char* satLabels[] = { "[TERRAIN]", "[RADAR]", "[THREAT]" };
		for (int c = 0; c < 3; ++c) {
			float bx = curPos.x + 14.0f + c * 74.0f;
			bool isCur = (activeSatLayer_ == c);
			bool isHov = isBtnHov(bx, cy, 68.0f, 22.0f);
			Vector4 bBg = isCur ? Vector4{ 0.08f, 0.30f, 0.40f, 0.90f }
			            : (isHov ? Vector4{ 0.04f, 0.18f, 0.25f, 0.85f } : Vector4{ 0.02f, 0.08f, 0.12f, 0.70f });
			Vector4 bBorder = isCur ? Vector4{ 0.40f, 0.95f, 1.0f, 1.0f }
			                : (isHov ? Vector4{ 0.25f, 0.75f, 0.90f, 0.90f } : Vector4{ 0.15f, 0.45f, 0.60f, 0.65f });
			DrawOSPanel({ bx, cy }, { 68.0f, 22.0f }, bBg, bBorder, 1.0f);
			Vector4 textCol = isCur ? Vector4{ 0.90f, 1.0f, 1.0f, 1.0f }
			                : (isHov ? Vector4{ 0.75f, 0.95f, 1.0f, 1.0f } : Vector4{ 0.35f, 0.80f, 0.90f, 0.85f });
			tr->Print("HackGen", satLabels[c], bx + 34.0f, cy + 4.0f, 11.0f, textCol, { 0.5f, 0.0f });
		}
	} else if (selIdx == 2) {
		// ==========================================
		// [03] SYSTEM CONFIG: ハードウェアバス診断
		// ==========================================
		tr->Print("HackGen", "HARDWARE : D3D12 AGILITY SDK 1.614", curPos.x + 14.0f, cy, 11.5f, { 0.35f, 0.95f, 0.60f, 0.95f });
		cy += 20.0f;
		tr->Print("HackGen", "RENDERER : NVIDIA GEFORCE RTX (FL 12_1)", curPos.x + 14.0f, cy, 11.0f, { 0.30f, 0.75f, 0.45f, 0.85f });
		cy += 22.0f;

		// 診断詳細パネル
		const float hwW = curSize.x - 28.0f;
		const float hwH = 96.0f;
		DrawOSPanel({ curPos.x + 14.0f, cy }, { hwW, hwH }, { 0.01f, 0.035f, 0.02f, 0.85f }, { 0.20f, 0.65f, 0.35f, 0.65f }, 1.0f);

		if (activeSysDetailTab_ == 0) {
			tr->Print("HackGen", "COMMAND QUEUE  : DIRECT (1) / COPY (1)", curPos.x + 22.0f, cy + 10.0f, 10.5f, { 0.35f, 0.90f, 0.55f, 0.90f });
			tr->Print("HackGen", "SWAPCHAIN      : DXGI_FORMAT_R8G8B8A8", curPos.x + 22.0f, cy + 28.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "BUFFER COUNT   : 2 (DOUBLE BUFFERED)", curPos.x + 22.0f, cy + 46.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "VSYNC REFRESH  : 60 HZ LOCKED", curPos.x + 22.0f, cy + 64.0f, 10.5f, { 0.40f, 1.0f, 0.65f, 0.95f });
		} else if (activeSysDetailTab_ == 1) {
			tr->Print("HackGen", "CBV_SRV_UAV    : 142 / 2048 DESCRIPTORS", curPos.x + 22.0f, cy + 10.0f, 10.5f, { 0.35f, 0.90f, 0.55f, 0.90f });
			tr->Print("HackGen", "RTV DESCRIPTORS: 3 / 64 ALLOCATED", curPos.x + 22.0f, cy + 28.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "DSV DESCRIPTORS: 1 / 16 ALLOCATED", curPos.x + 22.0f, cy + 46.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "VRAM DEDICATED : 2.4 GB / 8.0 GB IN USE", curPos.x + 22.0f, cy + 64.0f, 10.5f, { 0.40f, 1.0f, 0.65f, 0.95f });
		} else {
			tr->Print("HackGen", "SHADER MODEL   : SM 6.6 DXIL BYTECODE", curPos.x + 22.0f, cy + 10.0f, 10.5f, { 0.35f, 0.90f, 0.55f, 0.90f });
			tr->Print("HackGen", "ROOT SIGNATURE : ROOT 1.1 COMPACT", curPos.x + 22.0f, cy + 28.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "PIPELINE STATE : PSO CACHED (42 SHADERS)", curPos.x + 22.0f, cy + 46.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "GEOMETRY SHADER: ENABLED & COMPILED", curPos.x + 22.0f, cy + 64.0f, 10.5f, { 0.40f, 1.0f, 0.65f, 0.95f });
		}
		cy += 108.0f;

		tr->Print("HackGen", "DIAGNOSTIC TAB (CLICK TO SWITCH):", curPos.x + 16.0f, cy + 2.0f, 10.5f, { 0.35f, 0.75f, 0.50f, 0.85f });
		cy += 18.0f;

		const char* tabLabels[] = { "[D3D12]", "[HEAP]", "[SHADER]" };
		for (int t = 0; t < 3; ++t) {
			float bx = curPos.x + 14.0f + t * 74.0f;
			bool isCur = (activeSysDetailTab_ == t);
			bool isHov = isBtnHov(bx, cy, 68.0f, 22.0f);
			Vector4 bBg = isCur ? Vector4{ 0.12f, 0.45f, 0.22f, 0.90f }
			            : (isHov ? Vector4{ 0.06f, 0.25f, 0.12f, 0.85f } : Vector4{ 0.02f, 0.10f, 0.05f, 0.70f });
			Vector4 bBorder = isCur ? Vector4{ 0.55f, 1.0f, 0.75f, 1.0f }
			                : (isHov ? Vector4{ 0.40f, 0.90f, 0.60f, 0.90f } : Vector4{ 0.18f, 0.50f, 0.28f, 0.65f });
			DrawOSPanel({ bx, cy }, { 68.0f, 22.0f }, bBg, bBorder, 1.0f);
			Vector4 textCol = isCur ? Vector4{ 1.0f, 0.95f, 0.30f, 1.0f }
			                : (isHov ? Vector4{ 0.85f, 1.0f, 0.90f, 1.0f } : Vector4{ 0.35f, 0.85f, 0.55f, 0.85f });
			tr->Print("HackGen", tabLabels[t], bx + 34.0f, cy + 4.0f, 11.0f, textCol, { 0.5f, 0.0f });
		}
	} else {
		// ==========================================
		// [04] ABORT / EXIT: 電源管理
		// ==========================================
		tr->Print("HackGen", "POWER : DAWN TERMINAL POWER MGMT", curPos.x + 14.0f, cy, 11.5f, { 1.0f, 0.65f, 0.25f, 0.95f });
		cy += 20.0f;
		tr->Print("HackGen", "UPTIME: 00:14:28 | THERMAL: 42.0 ℃", curPos.x + 14.0f, cy, 11.0f, { 0.80f, 0.60f, 0.25f, 0.85f });
		cy += 22.0f;

		DrawOSPanel({ curPos.x + 14.0f, cy }, { curSize.x - 28.0f, 96.0f }, { 0.03f, 0.015f, 0.01f, 0.85f }, { 0.65f, 0.35f, 0.15f, 0.65f }, 1.0f);
		tr->Print("HackGen", "STANDBY CONSUMPTION : 12.4 W", curPos.x + 22.0f, cy + 12.0f, 10.5f, { 0.90f, 0.70f, 0.25f, 0.90f });
		tr->Print("HackGen", "CURRENT BUS PROFILE : FLIGHT_STATION_A", curPos.x + 22.0f, cy + 32.0f, 10.5f, { 0.85f, 0.65f, 0.20f, 0.85f });
		tr->Print("HackGen", "VOLTAGE REGULATOR   : 12.02V STABLE", curPos.x + 22.0f, cy + 52.0f, 10.5f, { 0.85f, 0.65f, 0.20f, 0.85f });
		tr->Print("HackGen", "READY FOR SAFE SHUTDOWN COMMAND", curPos.x + 22.0f, cy + 72.0f, 10.5f, { 1.0f, 0.80f, 0.30f, 0.95f });
		cy += 108.0f;

		tr->Print("HackGen", "ACTION (CLICK TO EXECUTE):", curPos.x + 16.0f, cy + 2.0f, 10.5f, { 0.75f, 0.55f, 0.20f, 0.85f });
		cy += 18.0f;
		const char* pwrLabels[] = { "[LOGOUT]", "[HALT]", "[REBOOT]" };
		for (int p = 0; p < 3; ++p) {
			float bx = curPos.x + 14.0f + p * 74.0f;
			bool isHov = isBtnHov(bx, cy, 68.0f, 22.0f);
			Vector4 bBg = isHov ? Vector4{ 0.25f, 0.10f, 0.05f, 0.85f } : Vector4{ 0.10f, 0.05f, 0.02f, 0.70f };
			Vector4 bBorder = isHov ? Vector4{ 1.0f, 0.55f, 0.25f, 0.95f } : Vector4{ 0.65f, 0.35f, 0.15f, 0.70f };
			DrawOSPanel({ bx, cy }, { 68.0f, 22.0f }, bBg, bBorder, 1.0f);
			Vector4 textCol = isHov ? Vector4{ 1.0f, 0.95f, 0.50f, 1.0f } : Vector4{ 1.0f, 0.75f, 0.30f, 0.85f };
			tr->Print("HackGen", pwrLabels[p], bx + 34.0f, cy + 4.0f, 11.0f, textCol, { 0.5f, 0.0f });
		}
	}

	// 連動切り替え演出（走査線スキャン＆同期プロンプト）
	if (switchRatio > 0.0f) {
		float scanY = curPos.y + win.titleBarHeight + (1.0f - switchRatio) * (curSize.y - win.titleBarHeight);
		DrawOSRect({ curPos.x + 2.0f, scanY }, { curSize.x - 4.0f, 2.0f }, { 0.40f, 1.0f, 0.80f, 0.85f * switchRatio });
		tr->Print("HackGen", ">> SYNCING DATA STREAM...", curPos.x + curSize.x * 0.5f, curPos.y + curSize.y * 0.5f, 11.0f, { 1.0f, 1.0f, 1.0f, 0.95f * switchRatio }, { 0.5f, 0.5f });
	}

	if (isActive) {
		// 四隅コーナーブラケット強調
		const float cLen = 14.0f;
		const float cThick = 2.0f;
		const Vector4 cCol = { 0.85f, 1.0f, 0.95f, 1.0f };
		DrawOSRect(curPos, { cLen, cThick }, cCol);
		DrawOSRect(curPos, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cLen, curPos.y }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cThick, curPos.y }, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x, curPos.y + curSize.y - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x, curPos.y + curSize.y - cLen }, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cLen, curPos.y + curSize.y - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cThick, curPos.y + curSize.y - cLen }, { cThick, cLen }, cCol);
	} else {
		// 非アクティブウィンドウの減光ディムオーバーレイ
		DrawOSRect(curPos, curSize, { 0.0f, 0.015f, 0.008f, 0.35f });
	}
}
void TitleScene::DrawOSWindowTelemetry(const OSWindow& win) {
	if (win.animScale <= 0.01f) { return; } // 完全最小化中は描画スキップ
	TextRenderer* tr = TextRenderer::GetInstance();

	// 最小化・展開トランジション座標計算（項目3）
	float s = win.animScale;
	float smoothS = s * s * (3.0f - 2.0f * s);
	const float tabTargetX = 120.0f + 148.0f * 2.0f + 70.0f;
	const float tabTargetY = 688.0f + 16.0f;
	auto lerpVal = [](float a, float b, float t) { return a + (b - a) * t; };
	Vector2 curPos = {
		lerpVal(tabTargetX - (win.size.x * 0.5f) * smoothS, win.pos.x, smoothS),
		lerpVal(tabTargetY - (win.size.y * 0.5f) * smoothS, win.pos.y, smoothS)
	};
	Vector2 curSize = { win.size.x * smoothS, win.size.y * smoothS };

	bool isActive = (activeWindowId_ == win.id);
	Vector4 borderCol = isActive ? Vector4{ 0.45f, 1.0f, 0.70f, 0.98f * smoothS } : Vector4{ 0.10f, 0.30f, 0.15f, 0.40f * smoothS };
	Vector4 titleBarBg = isActive ? Vector4{ 0.06f, 0.25f, 0.12f, 0.98f } : Vector4{ 0.015f, 0.05f, 0.025f, 0.85f };

	// 連動切り替え演出（切り替え直後のサイバー発光）
	float switchRatio = (kSubWindowSwitchDuration > 0.0f) ? (subWindowSwitchTimer_ / kSubWindowSwitchDuration) : 0.0f;
	if (switchRatio > 0.0f) {
		borderCol.x = (std::min)(1.0f, borderCol.x + 0.50f * switchRatio);
		borderCol.y = (std::min)(1.0f, borderCol.y + 0.30f * switchRatio);
		borderCol.z = (std::min)(1.0f, borderCol.z + 0.45f * switchRatio);
	}

	// 1. ウィンドウ本体パネル（背後ウィンドウのテキスト・スプライトを100%遮蔽する完全不透明プレート）
	DrawOSPanel(curPos, curSize, { 0.01f, 0.04f, 0.02f, 1.0f }, borderCol, isActive ? 2.0f : 1.0f);

	if (smoothS < 0.40f) { return; } // アニメーション極小時は枠のみ描画

	// 2. タイトルバー
	DrawOSRect(curPos, { curSize.x, win.titleBarHeight * smoothS }, titleBarBg);
	DrawOSRect({ curPos.x, curPos.y + win.titleBarHeight * smoothS - 1.0f }, { curSize.x, 1.0f }, borderCol);
	if (isActive) {
		DrawOSRect(curPos, { curSize.x, 2.0f }, { 0.70f, 1.0f, 0.85f, 1.0f }); // 上端ネオンアクセントライン
		DrawOSRect({ curPos.x + 8.0f, curPos.y + 9.0f }, { 6.0f, 6.0f }, { 0.40f, 1.0f, 0.70f, 1.0f }); // 高輝度LED
		tr->Print("HackGen", "[FOCUS] " + win.title, curPos.x + 18.0f, curPos.y + 6.0f, 11.5f * smoothS, { 0.85f, 1.0f, 0.92f, 1.0f });
	} else {
		DrawOSRect({ curPos.x + 8.0f, curPos.y + 9.0f }, { 6.0f, 6.0f }, { 0.12f, 0.30f, 0.16f, 0.45f }); // 消灯LED
		tr->Print("HackGen", "[IDLE]  " + win.title, curPos.x + 18.0f, curPos.y + 6.0f, 11.5f * smoothS, { 0.20f, 0.48f, 0.28f, 0.60f });
	}

	const float closeX = curPos.x + curSize.x - 24.0f;
	const float maxmX  = closeX - 22.0f;
	const float miniX  = maxmX - 22.0f;
	const float btnY   = curPos.y + 5.0f;

	tr->Print("HackGen", "[_]", miniX, btnY + 1.0f, 10.5f, { 0.40f, 0.80f, 0.55f, 0.85f });
	tr->Print("HackGen", "[口]", maxmX, btnY + 1.0f, 10.5f, { 0.40f, 0.80f, 0.55f, 0.85f });
	tr->Print("HackGen", "[X]", closeX, btnY + 1.0f, 10.5f, { 0.95f, 0.35f, 0.30f, 0.95f });

	// ホバー判定ヘルパーラムダ
	auto isBtnHov = [&](float bx, float by, float bw, float bh) {
		return (customCursorPos_.x >= bx && customCursorPos_.x <= bx + bw &&
		        customCursorPos_.y >= by && customCursorPos_.y <= by + bh);
	};

	// 3. ウィンドウ内コンテンツ（中央メニューのカーソル選択に連動したインタラクティブ表示）
	int selIdx = selectionManager_.GetSelectedIndex();
	float cy = curPos.y + win.titleBarHeight + 10.0f;
	const float gW = curSize.x - 28.0f;

	if (selIdx == 0) {
		// ==========================================
		// [01] EXECUTE SORTIE: 機体ステータス (MiG-21)
		// ==========================================
		tr->Print("HackGen", "AIRFRAME : MiG-21 [FISHBED-J]", curPos.x + 14.0f, cy, 12.0f, { 0.40f, 1.0f, 0.65f, 0.95f });
		cy += 20.0f;
		tr->Print("HackGen", "ENGINES  : TUMANSKY R-25-300 x1", curPos.x + 14.0f, cy, 11.0f, { 0.30f, 0.75f, 0.45f, 0.85f });
		cy += 22.0f;

		// 推力・ノズル開度リアルタイム動的変動（テスト時はアフターバーナー急上昇）
		float thrustRatio = 0.94f + 0.04f * std::sin(animTimer_ * 2.8f) + 0.015f * std::cos(animTimer_ * 5.2f);
		if (isEngineTesting_) {
			thrustRatio = 1.085f + 0.02f * std::sin(animTimer_ * 14.0f);
		}
		tr->Print("HackGen", "THRUST OUTPUT :", curPos.x + 14.0f, cy, 11.0f, isEngineTesting_ ? Vector4{ 1.0f, 0.65f, 0.20f, 0.98f } : Vector4{ 0.35f, 0.85f, 0.55f, 0.90f });
		char thrustBuf[32];
		if (isEngineTesting_) {
			snprintf(thrustBuf, sizeof(thrustBuf), "%5.1f %% [A/B]", thrustRatio * 100.0f);
		} else {
			snprintf(thrustBuf, sizeof(thrustBuf), "%5.1f %%", thrustRatio * 100.0f);
		}
		tr->Print("HackGen", thrustBuf, curPos.x + curSize.x - 84.0f, cy, 11.0f, isEngineTesting_ ? Vector4{ 1.0f, 0.35f, 0.20f, 1.0f } : Vector4{ 1.0f, 0.90f, 0.25f, 0.95f });
		cy += 15.0f;

		DrawOSRect({ curPos.x + 14.0f, cy }, { gW, 7.0f }, { 0.03f, 0.10f, 0.05f, 0.80f });
		Vector4 gaugeCol = isEngineTesting_ ? Vector4{ 1.0f, 0.45f, 0.15f, 0.95f } : Vector4{ 0.25f, 0.95f, 0.50f, 0.90f };
		float fillW = (std::min)(gW, gW * (thrustRatio / 1.10f));
		DrawOSRect({ curPos.x + 14.0f, cy }, { fillW, 7.0f }, gaugeCol);
		DrawOSBorder({ curPos.x + 14.0f, cy }, { gW, 7.0f }, { 0.18f, 0.60f, 0.30f, 0.75f }, 1.0f);
		cy += 18.0f;

		// EGT排気温度のリアルタイム微動（テスト時は885℃赤字警告）
		float egtTemp = 641.0f + 4.5f * std::sin(animTimer_ * 1.8f) + 1.8f * std::cos(animTimer_ * 4.3f);
		if (isEngineTesting_) {
			egtTemp = 885.0f + 14.0f * std::sin(animTimer_ * 8.0f);
		}
		char egtBuf[48];
		if (isEngineTesting_) {
			snprintf(egtBuf, sizeof(egtBuf), "EGT (TEMP)    : %3.0f ℃   [OVERHEAT!]", egtTemp);
		} else {
			snprintf(egtBuf, sizeof(egtBuf), "EGT (TEMP)    : %3.0f ℃   [NOMINAL]", egtTemp);
		}
		tr->Print("HackGen", egtBuf, curPos.x + 14.0f, cy, 10.5f, isEngineTesting_ ? Vector4{ 1.0f, 0.30f, 0.20f, 1.0f } : Vector4{ 0.30f, 0.85f, 0.45f, 0.85f });
		cy += 16.0f;

		// 油圧系統（テスト時はMAX FLOW 24.8MPa）
		float hydPress = isEngineTesting_ ? 24.8f : (21.0f + 0.22f * std::sin(animTimer_ * 2.5f));
		char hydBuf[48];
		snprintf(hydBuf, sizeof(hydBuf), "HYDRAULIC SYS : %4.1f MPa [%s]", hydPress, isEngineTesting_ ? "MAX FLOW" : "ONLINE");
		tr->Print("HackGen", hydBuf, curPos.x + 14.0f, cy, 10.5f, { 0.30f, 0.85f, 0.45f, 0.85f });
		cy += 16.0f;

		// ノズル開度（テスト時は100%全開）
		float nozPos = isEngineTesting_ ? 100.0f : (78.5f + 2.8f * std::cos(animTimer_ * 1.4f));
		char nozBuf[48];
		snprintf(nozBuf, sizeof(nozBuf), "NOZZLE POS    : %4.1f %%   [%s]", nozPos, isEngineTesting_ ? "MAX DIVERG" : "ACTIVE");
		tr->Print("HackGen", nozBuf, curPos.x + 14.0f, cy, 10.5f, isEngineTesting_ ? Vector4{ 1.0f, 0.70f, 0.20f, 0.95f } : Vector4{ 0.35f, 0.95f, 0.60f, 0.90f });
		cy += 20.0f;

		tr->Print("HackGen", "AVIONICS SIGNAL SPECTRUM:", curPos.x + 14.0f, cy, 10.5f, { 0.25f, 0.70f, 0.40f, 0.80f });
		cy += 14.0f;

		const int numBars = 14;
		float barWidth = (gW - (numBars - 1) * 3.0f) / numBars;
		for (int b = 0; b < numBars; ++b) {
			float barNoise = 0.35f + 0.50f * std::sin(animTimer_ * (3.0f + b * 0.7f) + b * 0.8f);
			if (isEngineTesting_) {
				barNoise = 0.55f + 0.40f * std::sin(animTimer_ * (6.0f + b * 1.2f) + b * 1.1f);
			}
			barNoise = std::clamp(barNoise, 0.10f, 1.0f);
			float barH = 22.0f * barNoise;
			float bx = curPos.x + 14.0f + b * (barWidth + 3.0f);
			float by = cy + (22.0f - barH);

			Vector4 bCol = (b >= 11 || isEngineTesting_) ? Vector4{ 1.0f, 0.70f, 0.20f, 0.90f } : Vector4{ 0.25f, 0.95f, 0.55f, 0.85f };
			DrawOSRect({ bx, by }, { barWidth, barH }, bCol);
		}
		cy += 28.0f;

		if (defconLevel_ == 1) {
			DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 24.0f }, { 0.25f, 0.05f, 0.05f, 0.90f }, { 1.0f, 0.25f, 0.20f, 0.95f }, 1.0f);
			tr->Print("HackGen", "DEFCON 1 // IMMEDIATE SCRAMBLE HOT", curPos.x + curSize.x * 0.5f, cy + 5.0f, 11.5f, { 1.0f, 0.30f, 0.25f, 0.98f }, { 0.5f, 0.0f });
		} else {
			DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 24.0f }, { 0.05f, 0.15f, 0.07f, 0.85f }, { 0.35f, 0.95f, 0.55f, 0.85f }, 1.0f);
			tr->Print("HackGen", "DEFCON 2 // SCRAMBLE AUTHORIZED", curPos.x + curSize.x * 0.5f, cy + 5.0f, 11.5f, { 1.0f, 0.90f, 0.25f, 0.98f }, { 0.5f, 0.0f });
		}
	} else if (selIdx == 1) {
		// ==========================================
		// [02] MISSION EDITOR: 敵戦力分析データ
		// ==========================================
		tr->Print("HackGen", "THEATER INTEL : CONFLICT ZONE 07", curPos.x + 14.0f, cy, 12.0f, { 1.0f, 0.45f, 0.35f, 0.98f });
		cy += 20.0f;
		tr->Print("HackGen", "THREAT LEVEL  : CRITICAL [4 BOGEYS]", curPos.x + 14.0f, cy, 11.0f, { 1.0f, 0.85f, 0.25f, 0.90f });
		cy += 22.0f;

		// 脅威度メーター 1: 空対空脅威度 (84%)
		float a2aRatio = 0.84f + 0.03f * std::sin(animTimer_ * 3.5f);
		tr->Print("HackGen", "AIR-TO-AIR THREAT :", curPos.x + 14.0f, cy, 10.5f, { 1.0f, 0.40f, 0.35f, 0.90f });
		tr->Print("HackGen", "84 % [CRITICAL]", curPos.x + curSize.x - 100.0f, cy, 10.5f, { 1.0f, 0.30f, 0.25f, 0.98f });
		cy += 14.0f;
		DrawOSRect({ curPos.x + 14.0f, cy }, { gW, 6.0f }, { 0.12f, 0.03f, 0.03f, 0.80f });
		DrawOSRect({ curPos.x + 14.0f, cy }, { gW * a2aRatio, 6.0f }, { 0.98f, 0.30f, 0.25f, 0.95f });
		DrawOSBorder({ curPos.x + 14.0f, cy }, { gW, 6.0f }, { 0.65f, 0.20f, 0.15f, 0.75f }, 1.0f);
		cy += 16.0f;

		// 脅威度メーター 2: 地対空ミサイル脅威度 (52%)
		float samRatio = 0.52f + 0.02f * std::cos(animTimer_ * 2.2f);
		tr->Print("HackGen", "SAM / AAA DANGER  :", curPos.x + 14.0f, cy, 10.5f, { 1.0f, 0.75f, 0.30f, 0.90f });
		tr->Print("HackGen", "52 % [MODERATE]", curPos.x + curSize.x - 100.0f, cy, 10.5f, { 1.0f, 0.80f, 0.25f, 0.95f });
		cy += 14.0f;
		DrawOSRect({ curPos.x + 14.0f, cy }, { gW, 6.0f }, { 0.10f, 0.06f, 0.02f, 0.80f });
		DrawOSRect({ curPos.x + 14.0f, cy }, { gW * samRatio, 6.0f }, { 1.0f, 0.75f, 0.20f, 0.95f });
		DrawOSBorder({ curPos.x + 14.0f, cy }, { gW, 6.0f }, { 0.65f, 0.45f, 0.15f, 0.75f }, 1.0f);
		cy += 16.0f;

		// 敵戦力探知リスト（選択中ターゲットにハイライト枠）
		const char* intelList[] = {
			"BANDIT [A] : Su-27 FLANKER x2 [CAP]",
			"BANDIT [B] : MiG-29 FULCRUM x4 [INT]",
			"SAM SITE   : 9K331 TOR-M1 [ACTIVE]"
		};
		for (int t = 0; t < 3; ++t) {
			if (activeIntelTarget_ == t) {
				DrawOSPanel({ curPos.x + 10.0f, cy - 1.0f }, { gW + 8.0f, 17.0f }, { 0.30f, 0.10f, 0.05f, 0.60f }, { 1.0f, 0.55f, 0.25f, 0.85f }, 1.0f);
			}
			Vector4 tCol = (activeIntelTarget_ == t) ? Vector4{ 1.0f, 0.95f, 0.40f, 1.0f } : Vector4{ 0.95f, 0.55f, 0.35f, 0.85f };
			tr->Print("HackGen", intelList[t], curPos.x + 14.0f, cy, 10.5f, tCol);
			cy += 16.0f;
		}
		cy += 4.0f;

		tr->Print("HackGen", "ECM RADAR JAMMING SPECTRUM:", curPos.x + 14.0f, cy, 10.5f, { 0.90f, 0.40f, 0.30f, 0.80f });
		cy += 14.0f;

		const int numBars = 14;
		float barWidth = (gW - (numBars - 1) * 3.0f) / numBars;
		for (int b = 0; b < numBars; ++b) {
			float barNoise = 0.30f + 0.55f * std::sin(animTimer_ * (4.2f + b * 1.1f) + b * 0.9f);
			barNoise = std::clamp(barNoise, 0.10f, 1.0f);
			float barH = 22.0f * barNoise;
			float bx = curPos.x + 14.0f + b * (barWidth + 3.0f);
			float by = cy + (22.0f - barH);

			Vector4 bCol = (b >= 9) ? Vector4{ 1.0f, 0.30f, 0.25f, 0.95f } : Vector4{ 1.0f, 0.70f, 0.20f, 0.85f };
			DrawOSRect({ bx, by }, { barWidth, barH }, bCol);
		}
		cy += 28.0f;

		DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 24.0f }, { 0.18f, 0.05f, 0.05f, 0.85f }, { 1.0f, 0.35f, 0.30f, 0.85f }, 1.0f);
		tr->Print("HackGen", "ADVISORY // HIGH-G EVASION REQUIRED", curPos.x + curSize.x * 0.5f, cy + 5.0f, 11.5f, { 1.0f, 0.35f, 0.30f, 0.98f }, { 0.5f, 0.0f });
	} else if (selIdx == 2) {
		// ==========================================
		// [03] SYSTEM CONFIG: オーディオ＆入力マトリクス
		// ==========================================
		tr->Print("HackGen", "AUDIO BUS : XAUDIO2 7.1 / MF ENGINE", curPos.x + 14.0f, cy, 12.0f, { 0.35f, 0.95f, 0.60f, 0.95f });
		cy += 20.0f;
		tr->Print("HackGen", "VOLUME    : MASTER 100% | BGM 80%", curPos.x + 14.0f, cy, 11.0f, { 0.30f, 0.80f, 0.50f, 0.85f });
		cy += 22.0f;

		tr->Print("HackGen", "CONTROLLER INPUT MATRIX:", curPos.x + 14.0f, cy, 11.0f, { 0.35f, 0.90f, 0.55f, 0.90f });
		cy += 16.0f;
		DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 56.0f }, { 0.01f, 0.04f, 0.02f, 0.80f }, { 0.20f, 0.60f, 0.35f, 0.65f }, 1.0f);
		tr->Print("HackGen", "PRIMARY  : XINPUT GAMEPAD / FLIGHT STICK", curPos.x + 22.0f, cy + 7.0f, 10.5f, { 0.40f, 0.95f, 0.60f, 0.90f });
		tr->Print("HackGen", "FALLBACK : DIRECTINPUT KB / MOUSE COMBO", curPos.x + 22.0f, cy + 23.0f, 10.5f, { 0.30f, 0.75f, 0.45f, 0.85f });
		tr->Print("HackGen", "CURVES   : PITCH 1.0 | ROLL 1.0 | YAW 1.0", curPos.x + 22.0f, cy + 39.0f, 10.5f, { 0.35f, 0.85f, 0.50f, 0.85f });
		cy += 64.0f;

		tr->Print("HackGen", "AUDIO EQUALIZER SPECTRUM:", curPos.x + 14.0f, cy, 10.5f, { 0.25f, 0.70f, 0.40f, 0.80f });
		cy += 14.0f;

		const int numBars = 14;
		float barWidth = (gW - (numBars - 1) * 3.0f) / numBars;
		for (int b = 0; b < numBars; ++b) {
			float barNoise = 0.25f + 0.60f * std::sin(animTimer_ * (2.4f + b * 0.8f) + b * 0.5f);
			barNoise = std::clamp(barNoise, 0.10f, 1.0f);
			float barH = 22.0f * barNoise;
			float bx = curPos.x + 14.0f + b * (barWidth + 3.0f);
			float by = cy + (22.0f - barH);

			Vector4 bCol = (b >= 11) ? Vector4{ 0.30f, 1.0f, 0.80f, 0.90f } : Vector4{ 0.25f, 0.95f, 0.55f, 0.85f };
			DrawOSRect({ bx, by }, { barWidth, barH }, bCol);
		}
		cy += 28.0f;

		DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 24.0f }, { 0.05f, 0.15f, 0.07f, 0.85f }, { 0.35f, 0.95f, 0.55f, 0.85f }, 1.0f);
		tr->Print("HackGen", "STATUS // HARDWARE BUS INITIALIZED", curPos.x + curSize.x * 0.5f, cy + 5.0f, 11.5f, { 0.40f, 1.0f, 0.65f, 0.98f }, { 0.5f, 0.0f });
	} else {
		// ==========================================
		// [04] ABORT / EXIT: セキュリティセッション終了
		// ==========================================
		tr->Print("HackGen", "SECURITY : SESSION TERMINATION LOG", curPos.x + 14.0f, cy, 12.0f, { 1.0f, 0.70f, 0.25f, 0.95f });
		cy += 20.0f;
		tr->Print("HackGen", "CLEARANCE: LEVEL 5 [OPERATOR AUTH]", curPos.x + 14.0f, cy, 11.0f, { 0.80f, 0.65f, 0.30f, 0.85f });
		cy += 22.0f;

		DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 76.0f }, { 0.03f, 0.02f, 0.01f, 0.80f }, { 0.65f, 0.40f, 0.15f, 0.65f }, 1.0f);
		tr->Print("HackGen", "SESSION CIPHER : AES-256 GCM [ONLINE]", curPos.x + 22.0f, cy + 8.0f, 11.0f, { 0.95f, 0.75f, 0.30f, 0.90f });
		tr->Print("HackGen", "TELEMETRY LOGS : BUFFER ARCHIVED (100%)", curPos.x + 22.0f, cy + 28.0f, 11.0f, { 0.85f, 0.65f, 0.25f, 0.85f });
		tr->Print("HackGen", "NETWORK STATUS : FLUSHING PACKET QUEUE", curPos.x + 22.0f, cy + 48.0f, 11.0f, { 0.85f, 0.65f, 0.25f, 0.85f });
		cy += 84.0f;

		tr->Print("HackGen", "ACTIVITY SPECTRUM:", curPos.x + 14.0f, cy, 10.5f, { 0.70f, 0.50f, 0.20f, 0.80f });
		cy += 14.0f;

		const int numBars = 14;
		float barWidth = (gW - (numBars - 1) * 3.0f) / numBars;
		for (int b = 0; b < numBars; ++b) {
			float barNoise = 0.15f + 0.35f * std::sin(animTimer_ * (1.5f + b * 0.5f) + b * 0.4f);
			barNoise = std::clamp(barNoise, 0.05f, 1.0f);
			float barH = 20.0f * barNoise;
			float bx = curPos.x + 14.0f + b * (barWidth + 3.0f);
			float by = cy + (20.0f - barH);

			DrawOSRect({ bx, by }, { barWidth, barH }, { 0.85f, 0.55f, 0.20f, 0.75f });
		}
		cy += 26.0f;

		DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 24.0f }, { 0.15f, 0.06f, 0.02f, 0.85f }, { 0.85f, 0.45f, 0.18f, 0.85f }, 1.0f);
		tr->Print("HackGen", "READY // CONFIRM SHUTDOWN IN MAIN TERM", curPos.x + curSize.x * 0.5f, cy + 5.0f, 11.5f, { 1.0f, 0.75f, 0.30f, 0.98f }, { 0.5f, 0.0f });
	}

	// 4. 下部インタラクティブ・アクションボタン群（クリックで実際に動作・実行）
	const float actBtnY = curPos.y + win.titleBarHeight + 276.0f;
	const float actBtnW = 104.0f;
	const float actBtnH = 24.0f;

	struct ActionBtnDef {
		std::string label;
		bool isActive;
		Vector4 normBg;
		Vector4 normBorder;
		Vector4 actBg;
		Vector4 actBorder;
		Vector4 textCol;
	};
	std::vector<ActionBtnDef> actBtns;

	if (selIdx == 0) {
		std::string b0Label = isEngineTesting_ ? "[TESTING...]" : "[ENG TEST]";
		actBtns.push_back({
		b0Label,
		isEngineTesting_,
		{ 0.05f, 0.18f, 0.08f, 0.85f }, { 0.25f, 0.85f, 0.45f, 0.80f },
		{ 0.35f, 0.15f, 0.05f, 0.95f }, { 1.0f, 0.55f, 0.20f, 1.0f },
		isEngineTesting_ ? Vector4{ 1.0f, 0.90f, 0.30f, 1.0f } : Vector4{ 0.40f, 1.0f, 0.65f, 0.95f }
		});
		std::string b1Label = (defconLevel_ == 1) ? "[DEFCON 1]" : "[DEFCON 2]";
		bool isDef1 = (defconLevel_ == 1);
		actBtns.push_back({
		b1Label,
		isDef1,
		{ 0.15f, 0.12f, 0.03f, 0.85f }, { 0.85f, 0.70f, 0.20f, 0.80f },
		{ 0.30f, 0.05f, 0.05f, 0.95f }, { 1.0f, 0.25f, 0.20f, 1.0f },
		isDef1 ? Vector4{ 1.0f, 0.35f, 0.30f, 1.0f } : Vector4{ 1.0f, 0.85f, 0.25f, 0.95f }
		});
		actBtns.push_back({
		"[SORTIE >>]",
		false,
		{ 0.06f, 0.22f, 0.12f, 0.90f }, { 0.35f, 1.0f, 0.65f, 0.90f },
		{ 0.12f, 0.45f, 0.25f, 0.98f }, { 0.60f, 1.0f, 0.80f, 1.0f },
		{ 0.45f, 1.0f, 0.70f, 1.0f }
		});
	} else if (selIdx == 1) {
		actBtns.push_back({
		"[TGT: Su-27]",
		(activeIntelTarget_ == 0),
		{ 0.15f, 0.06f, 0.04f, 0.85f }, { 0.85f, 0.40f, 0.25f, 0.80f },
		{ 0.35f, 0.10f, 0.06f, 0.95f }, { 1.0f, 0.50f, 0.30f, 1.0f },
		{ 1.0f, 0.80f, 0.30f, 0.95f }
		});
		actBtns.push_back({
		"[TGT: MiG-29]",
		(activeIntelTarget_ == 1),
		{ 0.15f, 0.06f, 0.04f, 0.85f }, { 0.85f, 0.40f, 0.25f, 0.80f },
		{ 0.35f, 0.10f, 0.06f, 0.95f }, { 1.0f, 0.50f, 0.30f, 1.0f },
		{ 1.0f, 0.80f, 0.30f, 0.95f }
		});
		actBtns.push_back({
		"[EDIT MISS >>]",
		false,
		{ 0.20f, 0.08f, 0.05f, 0.90f }, { 1.0f, 0.55f, 0.30f, 0.90f },
		{ 0.40f, 0.15f, 0.08f, 0.98f }, { 1.0f, 0.75f, 0.45f, 1.0f },
		{ 1.0f, 0.70f, 0.35f, 1.0f }
		});
	} else if (selIdx == 2) {
		actBtns.push_back({
		"[MUTE BGM]",
		false,
		{ 0.04f, 0.12f, 0.08f, 0.85f }, { 0.25f, 0.75f, 0.45f, 0.80f },
		{ 0.08f, 0.25f, 0.15f, 0.95f }, { 0.45f, 0.95f, 0.65f, 1.0f },
		{ 0.35f, 0.90f, 0.55f, 0.90f }
		});
		actBtns.push_back({
		"[RE-SCAN]",
		false,
		{ 0.04f, 0.12f, 0.08f, 0.85f }, { 0.25f, 0.75f, 0.45f, 0.80f },
		{ 0.08f, 0.25f, 0.15f, 0.95f }, { 0.45f, 0.95f, 0.65f, 1.0f },
		{ 0.35f, 0.90f, 0.55f, 0.90f }
		});
		actBtns.push_back({
		"[CALIBRATE]",
		false,
		{ 0.04f, 0.12f, 0.08f, 0.85f }, { 0.25f, 0.75f, 0.45f, 0.80f },
		{ 0.08f, 0.25f, 0.15f, 0.95f }, { 0.45f, 0.95f, 0.65f, 1.0f },
		{ 0.35f, 0.90f, 0.55f, 0.90f }
		});
	} else {
		actBtns.push_back({
		"[CANCEL]",
		false,
		{ 0.12f, 0.08f, 0.03f, 0.85f }, { 0.65f, 0.45f, 0.20f, 0.80f },
		{ 0.25f, 0.15f, 0.05f, 0.95f }, { 0.85f, 0.60f, 0.25f, 1.0f },
		{ 0.85f, 0.65f, 0.30f, 0.90f }
		});
		actBtns.push_back({
		"[SUSPEND]",
		false,
		{ 0.12f, 0.08f, 0.03f, 0.85f }, { 0.65f, 0.45f, 0.20f, 0.80f },
		{ 0.25f, 0.15f, 0.05f, 0.95f }, { 0.85f, 0.60f, 0.25f, 1.0f },
		{ 0.85f, 0.65f, 0.30f, 0.90f }
		});
		actBtns.push_back({
		"[EXIT >>]",
		false,
		{ 0.25f, 0.06f, 0.05f, 0.90f }, { 0.95f, 0.35f, 0.25f, 0.90f },
		{ 0.45f, 0.10f, 0.08f, 0.98f }, { 1.0f, 0.50f, 0.35f, 1.0f },
		{ 1.0f, 0.45f, 0.35f, 1.0f }
		});
	}

	for (size_t b = 0; b < actBtns.size() && b < 3; ++b) {
		float bx = curPos.x + 14.0f + static_cast<float>(b) * 112.0f;
		bool isHov = isBtnHov(bx, actBtnY, actBtnW, actBtnH);
		const auto& def = actBtns[b];

		Vector4 bBg = def.isActive ? def.actBg : (isHov ? Vector4{ (std::min)(1.0f, def.normBg.x * 2.0f), (std::min)(1.0f, def.normBg.y * 2.0f), (std::min)(1.0f, def.normBg.z * 2.0f), 0.95f } : def.normBg);
		Vector4 bBorder = def.isActive ? def.actBorder : (isHov ? Vector4{ 1.0f, 1.0f, 1.0f, 0.98f } : def.normBorder);
		DrawOSPanel({ bx, actBtnY }, { actBtnW, actBtnH }, bBg, bBorder, (def.isActive || isHov) ? 1.5f : 1.0f);

		Vector4 tCol = isHov ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : def.textCol;
		tr->Print("HackGen", def.label, bx + actBtnW * 0.5f, actBtnY + 5.0f, 10.5f, tCol, { 0.5f, 0.0f });
	}

	// 連動切り替え演出（走査線スキャン＆同期プロンプト）
	if (switchRatio > 0.0f) {
		float scanY = curPos.y + win.titleBarHeight + (1.0f - switchRatio) * (curSize.y - win.titleBarHeight);
		DrawOSRect({ curPos.x + 2.0f, scanY }, { curSize.x - 4.0f, 2.0f }, { 0.40f, 1.0f, 0.80f, 0.85f * switchRatio });
		tr->Print("HackGen", ">> RE-CONFIGURING METRICS...", curPos.x + curSize.x * 0.5f, curPos.y + curSize.y * 0.5f, 11.0f, { 1.0f, 1.0f, 1.0f, 0.95f * switchRatio }, { 0.5f, 0.5f });
	}

	if (isActive) {
		// 四隅コーナーブラケット強調
		const float cLen = 14.0f;
		const float cThick = 2.0f;
		const Vector4 cCol = { 0.85f, 1.0f, 0.95f, 1.0f };
		DrawOSRect(curPos, { cLen, cThick }, cCol);
		DrawOSRect(curPos, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cLen, curPos.y }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cThick, curPos.y }, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x, curPos.y + curSize.y - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x, curPos.y + curSize.y - cLen }, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cLen, curPos.y + curSize.y - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cThick, curPos.y + curSize.y - cLen }, { cThick, cLen }, cCol);
	} else {
		// 非アクティブウィンドウの減光ディムオーバーレイ
		DrawOSRect(curPos, curSize, { 0.0f, 0.015f, 0.008f, 0.35f });
	}
}
void TitleScene::DrawDesktopIcons() {
	// 右側のデスクトップアイコンは非表示
}

void TitleScene::DrawCustomCursor() {
	float mx = customCursorPos_.x;
	float my = customCursorPos_.y;
	TextRenderer* tr = TextRenderer::GetInstance();

	size_t curIdx = 0;
	auto drawCursorRect = [&](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
		if (curIdx >= cursorSprites_.size()) { return; }
		auto& sp = cursorSprites_[curIdx++];
		sp->SetPosition(pos);
		sp->SetSize(size);
		sp->SetColor(color);
		sp->Update();
		sp->Draw();
	};

	// 1. クリック波紋エフェクト（クリック地点から放射状に広がるサイバーパルス）
	if (cursorRippleTimer_ > 0.0f) {
		float rp = 1.0f - (cursorRippleTimer_ / 0.30f);
		float rRad = 4.0f + 24.0f * rp;
		float rAlpha = (1.0f - rp) * 0.90f;
		Vector4 ringCol = { 0.35f, 1.0f, 0.65f, rAlpha };
		// リング（外枠4本）
		drawCursorRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y - rRad }, { rRad * 2.0f, 1.5f }, ringCol);
		drawCursorRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y + rRad - 1.5f }, { rRad * 2.0f, 1.5f }, ringCol);
		drawCursorRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y - rRad }, { 1.5f, rRad * 2.0f }, ringCol);
		drawCursorRect({ cursorRipplePos_.x + rRad - 1.5f, cursorRipplePos_.y - rRad }, { 1.5f, rRad * 2.0f }, ringCol);
	}

	// 2. 項目5: ターミナルの色に合わせた緑色のカスタムカーソル（鋭角タクティカル矢印ポインター）
	// 外周黒アウトライン ＋ ネオングリーン本体
	const MyMath::Vector4 kBlackEdge = { 0.0f, 0.0f, 0.0f, 0.95f };
	const MyMath::Vector4 kGreenBody = { 0.22f, 1.0f, 0.55f, 0.98f };
	const MyMath::Vector4 kGreenCore = { 0.85f, 1.0f, 0.90f, 1.00f };

	// 矢印の各ライン（縦軸・斜め軸・返し・尾部）
	// 黒シャドウ・縁
	drawCursorRect({ mx, my }, { 2.0f, 18.0f }, kBlackEdge);
	drawCursorRect({ mx + 1.0f, my + 1.0f }, { 13.0f, 2.0f }, kBlackEdge);
	drawCursorRect({ mx + 2.0f, my + 2.0f }, { 12.0f, 12.0f }, kBlackEdge);

	// 緑本体
	drawCursorRect({ mx + 1.0f, my + 1.0f }, { 2.0f, 15.0f }, kGreenBody);
	drawCursorRect({ mx + 2.0f, my + 2.0f }, { 10.0f, 2.0f }, kGreenBody);
	drawCursorRect({ mx + 3.0f, my + 3.0f }, { 8.0f, 8.0f }, kGreenBody);
	drawCursorRect({ mx + 1.0f, my + 1.0f }, { 2.0f, 4.0f }, kGreenCore); // 先端ハイライト

	// 3. 先端のマイクロクロスヘア（照準レティクル）
	Vector4 reticleCol = { 0.35f, 1.0f, 0.65f, 0.85f };
	drawCursorRect({ mx - 7.0f, my }, { 5.0f, 1.0f }, reticleCol);
	drawCursorRect({ mx + 5.0f, my }, { 5.0f, 1.0f }, reticleCol);
	drawCursorRect({ mx, my - 7.0f }, { 1.0f, 5.0f }, reticleCol);
	drawCursorRect({ mx, my + 5.0f }, { 1.0f, 5.0f }, reticleCol);

	// 4. カーソル右下の微小な座標HUD
	if (mx >= 0.0f && mx <= kScreenWidth && my >= 0.0f && my <= kScreenHeight) {
		char locBuf[32];
		snprintf(locBuf, sizeof(locBuf), "[%d,%d]", static_cast<int>(mx), static_cast<int>(my));
		tr->Print("HackGen", locBuf, mx + 16.0f, my + 14.0f, 9.5f, { 0.35f, 0.95f, 0.55f, 0.80f });
	}
}

void TitleScene::DrawOSDesktop() {
	TextRenderer* tr = TextRenderer::GetInstance();
	osSpritePoolIdx_ = 0;

	// OSデスクトップ同期立ち上がりアニメーション進行度（0.0: ブート直後, 1.0: 立ち上がり完了）
	float enterProg = 1.0f;
	if (menuEnterTimer_ > 0.0f) {
		enterProg = std::clamp(1.0f - (menuEnterTimer_ / kMenuEnterDuration), 0.0f, 1.0f);
	}
	// 滑らかなイーズアウト進行度 (Ease-Out Sine: 最初素早く、最後しっとり定着)
	float smoothProg = (enterProg >= 1.0f) ? 1.0f : std::sin(enterProg * 1.5707963f);

	// 1. デスクトップ上部ヘッダー（OSウォーターマーク & 戦闘連続性ログ：指摘4）
	{
		float hAlpha = smoothProg;
		tr->Print("HackGen", "DAWN_OS TACTICAL DEFENSE MAINFRAME [v4.12.0]", 24.0f, 18.0f, 12.0f, { 0.30f, 0.85f, 0.50f, 0.70f * hAlpha });
		tr->Print("HackGen", "CLEARANCE: LEVEL 5 // SECURE DESKTOP ENVIRONMENT", kScreenWidth - 380.0f, 18.0f, 11.5f, { 0.25f, 0.70f, 0.40f, 0.60f * hAlpha });

		if (!previousSortieLog_.empty()) {
			tr->Print("HackGen", previousSortieLog_.c_str(), 24.0f, 36.0f, 11.0f, { previousSortieColor_.x, previousSortieColor_.y, previousSortieColor_.z, previousSortieColor_.w * hAlpha });
		}
	}
	tr->Flush(); // ヘッダーテキストを先に描画

	// 2. ウィンドウ描画（全ウィンドウが時間差なく完全に同時にアニメーション展開！）
	for (int winId : windowZOrder_) {
		if (winId >= 0 && winId < static_cast<int>(osWindows_.size())) {
			auto& win = osWindows_[winId];
			if (winId == 0) {
				if (win.isOpen && !win.isMinimized && win.animScale >= 0.01f) {
					DrawMenuCard();
					tr->Flush(); // メインターミナルのテキストを描画
				}
			} else if (winId == 1) {
				if (win.isOpen && !win.isMinimized && win.animScale >= 0.01f) {
					DrawOSWindowCam(win);
					tr->Flush(); // サブウィンドウ1のテキストを描画
				}
			} else if (winId == 2) {
				if (win.isOpen && !win.isMinimized && win.animScale >= 0.01f) {
					DrawOSWindowTelemetry(win);
					tr->Flush(); // サブウィンドウ2のテキストを描画
				}
			}
		}
	}

	// 3. デスクトップアイコン（同時にフェードイン）
	DrawDesktopIcons();
	tr->Flush();

	// 4. タスクバー & スタートメニュー（同時にフェードイン）
	DrawOSTaskbar();
	tr->Flush();

	// 5. ターミナルの色に合わせた緑色のカスタムカーソル（最前面: 同時に表示）
	DrawCustomCursor();
	tr->Flush();

	// 起動時の短い立ち上がりHUDインジケーター（画面中央下に一瞬表示してフェード）
	if (menuEnterTimer_ > 0.0f) {
		float hudAlpha = (1.0f - enterProg);
		tr->Print("HackGen", "[ DAWN-OS ALL SYSTEMS ONLINE ]", kScreenWidth * 0.5f, kScreenHeight - 65.0f, 13.0f, { 0.35f, 1.0f, 0.65f, 0.85f * hudAlpha }, { 0.5f, 0.5f });
		tr->Flush();
	}
}

