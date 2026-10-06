#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "TitleScene.h"
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
#include <cmath>
#include <filesystem>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

// 画面サイズ定数
static constexpr float kScreenWidth = static_cast<float>(WinApp::kClientWidth);   // 1280
static constexpr float kScreenHeight = static_cast<float>(WinApp::kClientHeight); // 720

void TitleScene::SetCursorVisible(bool visible) {
	isCursorShown_ = visible;
#ifdef USE_IMGUI
	ImGui::SetMouseCursor(visible ? ImGuiMouseCursor_Arrow : ImGuiMouseCursor_None);
#endif
	if (visible) {
		while (ShowCursor(TRUE) < 0) {}
		::SetCursor(LoadCursor(nullptr, IDC_ARROW));
	} else {
		while (ShowCursor(FALSE) >= 0) {}
		::SetCursor(nullptr);
	}
}

void TitleScene::Initialize() {
	SetCursorVisible(false); // ドローン視点・コンソール画面ではOSカーソルを完全非表示（項目7）
	sceneID = SCENE::TITLE;
	selectionManager_.Clear();
	animTimer_ = 0.0f;
	menuEnterTimer_ = 0.0f;

	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

	// コンソールブート時の完全漆黒背景スプライト（項目1: 格納庫の透けを100%遮蔽）
	consoleBgSprite_ = std::make_unique<Sprite>();
	consoleBgSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
	consoleBgSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	consoleBgSprite_->SetPosition({ 0.0f, 0.0f });
	consoleBgSprite_->SetSize({ kScreenWidth, kScreenHeight });
	consoleBgSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	consoleBgSprite_->Update();

	// 必須テクスチャのロード
	TextureManager* tm = TextureManager::GetInstance();
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

	TextureManager::GetInstance()->LoadTexture("assets/textures/cedar_bridge_sunset_1_2k.dds");
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(SkyboxCommon::GetInstance());
	skybox_->SetCamera(titleCamera);
	uint32_t skyboxTexIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/cedar_bridge_sunset_1_2k.dds");
	skybox_->SetTextureIndex(skyboxTexIndex);
	Object3dCommon::GetInstance()->SetDefaultEnvTextureIndex(skyboxTexIndex);

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

	state_ = TitleState::DroneView;
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
	// メニューカード（中央の半透明ダークガラスパネル）
	// ============================
	InitializeMenuCard(spriteCommon);

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
	startButton_.Initialize(spriteCommon, "   [01] EXECUTE SORTIE  ", 21.0f);
	startButton_.GetLabelText()->SetFontName("HackGen");
	startButton_.SetPosition({ buttonX, buttonStartY });
	startButton_.SetSize({ buttonWidth, buttonHeight });
	startButton_.SetNormalColor(btnNormalBg);
	startButton_.SetHoverColor(btnHoverBg);
	startButton_.SetSelectedColor(btnSelectedBg);
	startButton_.SetTextColor(btnNormalText);
	startButton_.SetSelectedTextColor(btnSelText);
	startButton_.SetOnClick([this]() {
		sceneID = SCENE::STAGE;
	});

	// EDITOR ボタン
	editorButton_.Initialize(spriteCommon, "   [02] MISSION EDITOR  ", 21.0f);
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
	settingsButton_.Initialize(spriteCommon, "   [03] SYSTEM CONFIG   ", 21.0f);
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
	exitButton_.Initialize(spriteCommon, "   [04] ABORT / EXIT    ", 21.0f);
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

	// コンソールメニューカードの初期化
	InitializeMenuCard(spriteCommon);

	// ============================
	// ============================
	// 操作案内テキスト＆ミリタリー黒帯（枠線中央にジャストフィット配置：UIずれ解消）
	// ============================
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

	// 正確なascent/descentアンカー計算により、バナー枠の中心にジャストフィット
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

	// DAWN タイトルロゴスプライト初期化（白文字 + MiG-21通過）
	titleLogoSprite_ = std::make_unique<Sprite>();
	titleLogoSprite_->Initialize(spriteCommon, "assets/textures/title_logo_dawn_cropped.png");
	titleLogoSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	titleLogoSprite_->SetPosition({ kScreenWidth * 0.5f, 134.0f });
	titleLogoSprite_->SetSize({ 445.0f, 99.7f });
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

void TitleScene::Update() {
	float dt = 1.0f / 60.0f;
	animTimer_ += dt;
	stateTimer_ += dt;

	// マウスカーソルが非表示モードの場合、毎フレーム確実に消去（ImGuiの復活処理を抑止：項目3）
	if (!isCursorShown_) {
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
		bool enterTriggered = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN);
		wasLButtonDown_ = ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) || input->PushMouse(0);

		if (enterTriggered || clickTriggered) {
			state_ = TitleState::WindowClose;
			windowCloseTimer_ = 0.0f;
			if (CurrentLocation().id == TitleLocation::CrashedForest) {
				ChangeLocation(0); // 通信途絶現場からメニュー移行時は基地メイン回線（格納庫）に接続復帰
			}
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
		SetCursorVisible(false); // ウィンドウ縮小中もOSカーソル非表示（項目3）
		windowCloseTimer_ += dt;
		if (windowCloseTimer_ >= kWindowCloseDuration) {
			state_ = TitleState::ConsoleBoot;
			consoleBootTimer_ = 0.0f;
			consoleExitTimer_ = 0.0f;
		}

		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();

		// ウィンドウ縮小に伴う走査線とシャットダウンCRTグリッチ（項目1）
		float t = (std::min)(1.0f, windowCloseTimer_ / kWindowCloseDuration);
		ActivePostEffect scanline;
		scanline.type = PostEffectType::kScanLine;
		scanline.intensity = 0.08f + 0.16f * t;
		postEffect->AddActiveEffect(scanline);

		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.14f * t;
		postEffect->AddActiveEffect(noise);

	} else if (state_ == TitleState::ConsoleBoot) {
		SetCursorVisible(false); // コンソール演出中もマウスカーソル完全非表示（項目7）
		consoleBootTimer_ += dt;
		Input* input = Input::GetInstance();
		bool clickTriggered = input->TriggerMouse(0) || ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && !wasLButtonDown_);
		wasLButtonDown_ = ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) || input->PushMouse(0);
		bool skipTriggered = (consoleBootTimer_ > 0.30f && (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) || clickTriggered));

		// 時間経過またはスキップで、スムーズなフェードアウト＆トランジション演出を開始（項目3）
		if (consoleBootTimer_ >= kConsoleBootDuration || skipTriggered) {
			consoleExitTimer_ += dt;
			if (consoleExitTimer_ >= kConsoleExitDuration) {
				state_ = TitleState::Menu;
				stateTimer_ = 0.0f;
				consoleExitTimer_ = 0.0f;
				menuEnterTimer_ = kMenuEnterDuration; // メニュー起動トランジション開始！
				SetCursorVisible(true);               // メニューに入って初めてカーソルを表示！
				if (CurrentLocation().id == TitleLocation::CrashedForest) {
					ChangeLocation(0); // メニュー突入時に通信途絶現場なら基地メイン回線（格納庫）に復帰
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

	} else if (state_ == TitleState::Menu) {
		SetCursorVisible(true); // メニュー中はマウス操作を有効化（項目7）
		if (menuEnterTimer_ > 0.0f) {
			menuEnterTimer_ -= dt;
		}

		// --- 疑似ウィンドウ（DAWN_OS ターミナル）のマウスドラッグ＆UI操作 ---
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

		const float cardW = 520.0f;
		const float cardH = 616.0f;
		const float tbH = 34.0f;

		// マウスボタン押下判定（DirectInput ＋ Windows API の多重フォールバック）
		bool isLButtonDown = input->PushMouse(0) || ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
		bool isLButtonTrigger = input->TriggerMouse(0) || (isLButtonDown && !wasLButtonDown_);

		// タイトルバー右上の [×] 閉じるボタン (18x18px)
		const float closeBtnX = menuWindowPos_.x + cardW - 34.0f;
		const float closeBtnY = menuWindowPos_.y + 5.0f;
		if (isLButtonTrigger && mx >= closeBtnX && mx <= closeBtnX + 22.0f && my >= closeBtnY && my <= closeBtnY + 22.0f) {
			PostQuitMessage(0);
		}

		// タイトルバーのドラッグ判定（右端のボタン領域を除く上部バー全域）
		if (!isWindowDragging_ && isLButtonTrigger) {
			if (mx >= menuWindowPos_.x && mx <= menuWindowPos_.x + cardW - 36.0f &&
				my >= menuWindowPos_.y && my <= menuWindowPos_.y + tbH) {
				isWindowDragging_ = true;
				windowDragOffset_ = { mx - menuWindowPos_.x, my - menuWindowPos_.y };
			}
		}

		if (isWindowDragging_) {
			if (!isLButtonDown) {
				isWindowDragging_ = false;
			} else {
				Vector2 targetPos = { mx - windowDragOffset_.x, my - windowDragOffset_.y };
				// 画面外への完全な消失を防止（タイトルバーが常に画面内に留まるようクランプ）
				targetPos.x = std::clamp(targetPos.x, -cardW + 80.0f, kScreenWidth - 80.0f);
				targetPos.y = std::clamp(targetPos.y, 0.0f, kScreenHeight - tbH);
				UpdateMenuWindowPosition(targetPos);
			}
		}
		wasLButtonDown_ = isLButtonDown;

		if (!isWindowDragging_) {
			selectionManager_.Update();
		}
		UpdateMenuConsoleVisuals();

		// 現在のロケーションをゆっくりOrbitする
		UpdateLocationCamera(loc, dt, true);

		// 背景を暗くするフェードイン（背景映像の透けを排除し、ネオングリーンUIの視認性を最大化：項目6）
		float bgAlpha = (std::min)(stateTimer_ * 2.5f, 0.96f);
		backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, bgAlpha });
		backgroundPanel_.Update();

		// メニュー用ポストエフェクト（背景の3Dトンネルを美しくぼかす川瀬式ブラー ＋ 発光ブルーム ＋ 繊細な走査線）
		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();

		// 背景をリッチにぼかす川瀬式ブラー
		ActivePostEffect blur;
		blur.type = PostEffectType::kKawaseBlur;
		blur.intensity = 4.5f;
		postEffect->AddActiveEffect(blur);

		// 高輝度部を光らせるブルーム
		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = 2.5f;
		bloom.dirX = 1.3f;
		bloom.dirY = 0.68f;
		postEffect->AddActiveEffect(bloom);

		// 繊細な走査線
		ActivePostEffect scanline;
		scanline.type = PostEffectType::kScanLine;
		scanline.intensity = 0.035f;
		postEffect->AddActiveEffect(scanline);

		// ビネット
		ActivePostEffect vignette;
		vignette.type = PostEffectType::kVignette;
		vignette.intensity = 0.75f;
		postEffect->AddActiveEffect(vignette);

		menuCardPanel_.Update();
		menuHeaderPanel_.Update();
		if (titleLogoSprite_) {
			titleLogoSprite_->Update();
		}
		titleText_.Update();
		subtitleText_.Update();
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
	/// ModelManager と同じ規則（そのまま or assets/ 付加）でファイル存在を確認
	bool TitleAssetExists(const std::string& path) {
		return std::filesystem::exists(path) || std::filesystem::exists("assets/" + path);
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

void TitleScene::UpdateLocationCamera(const LocationData& loc, float dt, bool menuMode) {
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

		Camera* titleCamera = CameraManager::GetInstance()->GetActiveCamera();
		if (titleCamera) {
			titleCamera->SetTranslate(camPos);
			titleCamera->SetRotate({ pitch, yaw, droneRoll });
			titleCamera->Update();
		}
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

	Camera* titleCamera = CameraManager::GetInstance()->GetActiveCamera();
	if (titleCamera) {
		titleCamera->SetTranslate(camPos);
		titleCamera->SetRotate({ pitch, yaw, dynamicRoll });
		titleCamera->Update();
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

	// 4. タイトルバー右側のウィンドウ制御ボタン（[ - ] [ □ ] [ × ]）装飾
	// 各ボタン背景 (18x18px)
	const float winBtnY = cardY + 7.0f;
	const float winBtnSize = 18.0f;
	// 最小化 [-]
	addBorderLine({ cardX + cardWidth - 84.0f, winBtnY }, { winBtnSize, winBtnSize }, { 0.05f, 0.16f, 0.09f, 0.90f });
	addBorderLine({ cardX + cardWidth - 84.0f, winBtnY }, { winBtnSize, 1.0f }, { 0.20f, 0.70f, 0.40f, 0.80f });
	addBorderLine({ cardX + cardWidth - 84.0f, winBtnY + winBtnSize - 1.0f }, { winBtnSize, 1.0f }, { 0.20f, 0.70f, 0.40f, 0.80f });
	addBorderLine({ cardX + cardWidth - 84.0f, winBtnY }, { 1.0f, winBtnSize }, { 0.20f, 0.70f, 0.40f, 0.80f });
	addBorderLine({ cardX + cardWidth - 84.0f + winBtnSize - 1.0f, winBtnY }, { 1.0f, winBtnSize }, { 0.20f, 0.70f, 0.40f, 0.80f });
	addBorderLine({ cardX + cardWidth - 80.0f, winBtnY + 10.0f }, { 10.0f, 2.0f }, { 0.35f, 0.95f, 0.60f, 0.95f }); // 横棒

	// 最大化 [□]
	addBorderLine({ cardX + cardWidth - 58.0f, winBtnY }, { winBtnSize, winBtnSize }, { 0.05f, 0.16f, 0.09f, 0.90f });
	addBorderLine({ cardX + cardWidth - 58.0f, winBtnY }, { winBtnSize, 1.0f }, { 0.20f, 0.70f, 0.40f, 0.80f });
	addBorderLine({ cardX + cardWidth - 58.0f, winBtnY + winBtnSize - 1.0f }, { winBtnSize, 1.0f }, { 0.20f, 0.70f, 0.40f, 0.80f });
	addBorderLine({ cardX + cardWidth - 58.0f, winBtnY }, { 1.0f, winBtnSize }, { 0.20f, 0.70f, 0.40f, 0.80f });
	addBorderLine({ cardX + cardWidth - 58.0f + winBtnSize - 1.0f, winBtnY }, { 1.0f, winBtnSize }, { 0.20f, 0.70f, 0.40f, 0.80f });
	addBorderLine({ cardX + cardWidth - 54.0f, winBtnY + 4.0f }, { 10.0f, 1.0f }, { 0.35f, 0.95f, 0.60f, 0.95f });
	addBorderLine({ cardX + cardWidth - 54.0f, winBtnY + 13.0f }, { 10.0f, 1.0f }, { 0.35f, 0.95f, 0.60f, 0.95f });
	addBorderLine({ cardX + cardWidth - 54.0f, winBtnY + 4.0f }, { 1.0f, 10.0f }, { 0.35f, 0.95f, 0.60f, 0.95f });
	addBorderLine({ cardX + cardWidth - 45.0f, winBtnY + 4.0f }, { 1.0f, 10.0f }, { 0.35f, 0.95f, 0.60f, 0.95f });

	// 閉じる [×]
	addBorderLine({ cardX + cardWidth - 32.0f, winBtnY }, { winBtnSize, winBtnSize }, { 0.08f, 0.16f, 0.10f, 0.90f });
	addBorderLine({ cardX + cardWidth - 32.0f, winBtnY }, { winBtnSize, 1.0f }, { 0.25f, 0.75f, 0.45f, 0.80f });
	addBorderLine({ cardX + cardWidth - 32.0f, winBtnY + winBtnSize - 1.0f }, { winBtnSize, 1.0f }, { 0.25f, 0.75f, 0.45f, 0.80f });
	addBorderLine({ cardX + cardWidth - 32.0f, winBtnY }, { 1.0f, winBtnSize }, { 0.25f, 0.75f, 0.45f, 0.80f });
	addBorderLine({ cardX + cardWidth - 32.0f + winBtnSize - 1.0f, winBtnY }, { 1.0f, winBtnSize }, { 0.25f, 0.75f, 0.45f, 0.80f });
	addBorderLine({ cardX + cardWidth - 28.0f, winBtnY + 5.0f }, { 2.0f, 8.0f }, { 0.35f, 0.95f, 0.60f, 0.95f });
	addBorderLine({ cardX + cardWidth - 21.0f, winBtnY + 5.0f }, { 2.0f, 8.0f }, { 0.35f, 0.95f, 0.60f, 0.95f });
	addBorderLine({ cardX + cardWidth - 26.0f, winBtnY + 8.0f }, { 6.0f, 2.0f }, { 0.35f, 0.95f, 0.60f, 0.95f });

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

	// タイトル下（Y=232）の水平区切り線
	addBorderLine({ cardX + 24.0f, cardY + 180.0f }, { cardWidth - 48.0f, 1.0f }, { 0.18f, 0.75f, 0.45f, 0.45f });

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

		// 左端のアクティブバー
		buttonBorderIndicatorOffsets_.push_back({ (btnX - 8.0f) - cardX, (by + 12.0f) - cardY });
		group.indicator = createLineSprite({ btnX - 8.0f, by + 12.0f }, { 3.0f, 20.0f }, { 0.08f, 0.25f, 0.15f, 0.25f });

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
		menuBorderSprites_[i]->SetPosition({ menuWindowPos_.x + menuBorderOffsets_[i].x, menuWindowPos_.y + menuBorderOffsets_[i].y });
		menuBorderSprites_[i]->Update();
	}

	if (titleLogoSprite_) {
		titleLogoSprite_->SetPosition({ menuWindowPos_.x + 260.0f, menuWindowPos_.y + 82.0f });
		titleLogoSprite_->Update();
	}

	subtitleText_.SetPosition({ menuWindowPos_.x + 260.0f, menuWindowPos_.y + 144.0f });
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
				group.outlines[j]->SetPosition({ menuWindowPos_.x + buttonBorderOutlineOffsets_[i][j].x, menuWindowPos_.y + buttonBorderOutlineOffsets_[i][j].y });
				group.outlines[j]->Update();
			}
		}
		if (i < buttonBorderBracketOffsets_.size()) {
			for (size_t j = 0; j < group.brackets.size() && j < buttonBorderBracketOffsets_[i].size(); ++j) {
				group.brackets[j]->SetPosition({ menuWindowPos_.x + buttonBorderBracketOffsets_[i][j].x, menuWindowPos_.y + buttonBorderBracketOffsets_[i][j].y });
				group.brackets[j]->Update();
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

	// 各ボタンのプロンプトテキスト更新（選択行は >> と 点滅カーソル _）
	startButton_.SetLabel(selIdx == 0 ? (blink ? ">> [01] EXECUTE SORTIE _" : ">> [01] EXECUTE SORTIE  ") : "   [01] EXECUTE SORTIE  ");
	editorButton_.SetLabel(selIdx == 1 ? (blink ? ">> [02] MISSION EDITOR _" : ">> [02] MISSION EDITOR  ") : "   [02] MISSION EDITOR  ");
	settingsButton_.SetLabel(selIdx == 2 ? (blink ? ">> [03] SYSTEM CONFIG  _" : ">> [03] SYSTEM CONFIG   ") : "   [03] SYSTEM CONFIG   ");
	exitButton_.SetLabel(selIdx == 3 ? (blink ? ">> [04] ABORT / EXIT   _" : ">> [04] ABORT / EXIT    ") : "   [04] ABORT / EXIT    ");

	// 各ボタン枠スプライトの色動的更新
	for (size_t i = 0; i < buttonBorders_.size(); ++i) {
		bool isSel = (static_cast<int>(i) == selIdx);
		MyMath::Vector4 outlineCol   = isSel ? MyMath::Vector4{ 0.25f, 1.0f, 0.55f, 0.90f } : MyMath::Vector4{ 0.15f, 0.45f, 0.30f, 0.40f };
		MyMath::Vector4 bracketCol   = isSel ? MyMath::Vector4{ 0.45f, 1.0f, 0.75f, 1.00f } : MyMath::Vector4{ 0.20f, 0.65f, 0.45f, 0.55f };
		MyMath::Vector4 indicatorCol = isSel ? MyMath::Vector4{ 0.35f, 1.0f, 0.65f, 1.00f } : MyMath::Vector4{ 0.08f, 0.22f, 0.16f, 0.25f };

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
	for (auto& group : buttonBorders_) {
		for (auto& sp : group.outlines) { sp->Draw(); }
		for (auto& sp : group.brackets) { sp->Draw(); }
		if (group.indicator) { group.indicator->Draw(); }
	}

	// メニュー初期展開時のレーザー走査線＆枠形成エフェクト（項目2）
	if (menuEnterTimer_ > 0.0f) {
		float enterRatio = 1.0f - (menuEnterTimer_ / kMenuEnterDuration);
		enterRatio = (std::min)((std::max)(enterRatio, 0.0f), 1.0f);
		float cardY = menuWindowPos_.y;
		float laserY = cardY + 616.0f * enterRatio;

		TextRenderer* tr = TextRenderer::GetInstance();
		tr->Print("HackGen", ">> CONSTRUCTING TACTICAL UI FRAMEWORK... 100% <<", menuWindowPos_.x + 260.0f, laserY - 14.0f, 12.0f, { 0.40f, 1.0f, 0.70f, 0.90f }, { 0.5f, 0.5f });
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
	// ウィンドウタイトルバーと枠線（計6本）
	for (int i = 0; i < 6; ++i) {
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

	if (curW > 16.0f && curH > 16.0f && windowCloseBorderSprites_.size() >= 5) {
		// OSウィンドウタイトルバー
		float titleBarH = 24.0f;
		windowCloseBorderSprites_[0]->SetPosition({ curX, curY - titleBarH });
		windowCloseBorderSprites_[0]->SetSize({ curW, titleBarH });
		windowCloseBorderSprites_[0]->SetColor({ 0.03f, 0.12f, 0.06f, 0.98f });
		windowCloseBorderSprites_[0]->Update();
		windowCloseBorderSprites_[0]->Draw();

		// 枠線4辺 (鮮烈なエレクトリックネオングリーン)
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

		// タイトルバーテキスト（最小化アニメーション中：項目7）
		if (curW > 220.0f) {
			TextRenderer* tr = TextRenderer::GetInstance();
			tr->Print("HackGen", "[CAM-01 : OPTICAL FEED - MINIMIZING TO SYSTEM CONSOLE...]", curX + 10.0f, curY - 19.0f, 11.5f, { 0.35f, 1.0f, 0.65f, 0.95f });
			tr->Print("HackGen", "[ _ ] [口] [X]", curX + curW - 86.0f, curY - 19.0f, 11.5f, { 0.85f, 0.85f, 0.85f, 0.95f });
		}
	}

	// 画面奥のデスクトップ・ターミナル開始メッセージ
	TextRenderer* tr = TextRenderer::GetInstance();
	tr->Print("HackGen", "/// VIDEO LINK DISENGAGED // BOOTING DAWN TERMINAL ///", kScreenWidth * 0.5f, kScreenHeight * 0.5f - 8.0f, 14.5f, { 0.20f, 0.90f, 0.48f, 0.75f * t }, { 0.5f, 0.5f });
}

void TitleScene::DrawConsoleBoot() {
	// 背景の完全漆黒コンソールスプライト（格納庫の透けを100%遮蔽）
	if (consoleBgSprite_) {
		consoleBgSprite_->Draw();
	}
	backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	backgroundPanel_.Draw();

	float textAlpha = 1.0f;
	if (consoleExitTimer_ > 0.0f) {
		textAlpha = (std::max)(0.0f, 1.0f - consoleExitTimer_ / kConsoleExitDuration);
	}

	// コンソールからメニューへの切り替え時の一瞬の文字グリッチノイズ演出（項目10）
	auto glitchText = [this](const std::string& orig) -> std::string {
		if (consoleExitTimer_ <= 0.0f) { return orig; }
		std::string s = orig;
		float glitchRatio = consoleExitTimer_ / kConsoleExitDuration;
		static const char kGlitchChars[] = "01#*/_[]><X$!%&~";
		for (size_t i = 0; i < s.size(); ++i) {
			if (s[i] != ' ' && ((rand() % 100) < static_cast<int>(glitchRatio * 65.0f))) {
				s[i] = kGlitchChars[rand() % (sizeof(kGlitchChars) - 1)];
			}
		}
		return s;
	};

	TextRenderer* tr = TextRenderer::GetInstance();
	const float startX = 140.0f;
	float currentY = 140.0f;
	const float fontSize = 19.0f;
	const MyMath::Vector4 greenCol = { 0.20f, 0.95f, 0.45f, 0.96f * textAlpha };
	const MyMath::Vector4 cyanCol  = { 0.25f, 0.90f, 1.0f, 0.90f * textAlpha };

	// ターミナルヘッダー
	tr->Print("HackGen", glitchText("/// TACTICAL DEFENSE MAINFRAME // SECURE TERMINAL v4.12 ///"), startX, currentY, 15.0f, cyanCol);
	currentY += 34.0f;
	tr->Print("HackGen", glitchText("==================================================================="), startX, currentY, 12.0f, { 0.18f, 0.70f, 0.40f, 0.45f * textAlpha });
	currentY += 24.0f;

	if (consoleBootTimer_ >= 0.05f) {
		tr->Print("HackGen", glitchText("[0x0040] BOOT_SEQ: INITIATING TACTICAL SUBSYSTEMS... OK"), startX, currentY, fontSize, greenCol);
		currentY += 32.0f;
	}
	if (consoleBootTimer_ >= 0.25f) {
		tr->Print("HackGen", glitchText("[0x008A] SCAN_FEED: ACQUIRING 3-AXIS DRONE TELEMETRY... [ONLINE]"), startX, currentY, fontSize, greenCol);
		currentY += 32.0f;
	}
	if (consoleBootTimer_ >= 0.50f) {
		tr->Print("HackGen", glitchText("[0x012F] MOUNT_FS : OPERATION DIRECTIVE 'DAWN' DATABASE... OK"), startX, currentY, fontSize, greenCol);
		currentY += 32.0f;
	}
	if (consoleBootTimer_ >= 0.72f) {
		bool blink = (std::fmod(consoleBootTimer_, 0.24f) < 0.12f);
		std::string passLine = "root@dawn-core:~$ ./authenticate_sortie --key **********";
		if (blink) { passLine += " _"; }
		tr->Print("HackGen", glitchText(passLine), startX, currentY, fontSize, { 0.35f, 1.0f, 0.55f, 1.0f * textAlpha });
		currentY += 38.0f;
	}
	if (consoleBootTimer_ >= 0.95f) {
		tr->Print("HackGen", glitchText(">> [ ACCESS GRANTED : WELCOME TO THE COMBAT ZONE ] <<"), startX, currentY, 20.0f, { 1.0f, 0.92f, 0.25f, 0.95f * textAlpha });
	}

	// 画面下部にスキップ案内
	tr->Print("HackGen", "[ PRESS SPACE OR CLICK TO SKIP ]", kScreenWidth * 0.5f, kScreenHeight - 45.0f, 14.0f, { 0.45f, 0.75f, 0.55f, 0.75f * textAlpha }, { 0.5f, 0.5f });

	// トランジション進行中：文字グリッチ＋中央メニューUI枠のレーザー形成演出（項目10）
	if (consoleExitTimer_ > 0.0f) {
		DrawTransitionGlitch();

		// メニューカードの位置（中央 460x400）に向かってUI枠をレーザースキャン形成
		float exitProg = consoleExitTimer_ / kConsoleExitDuration;
		const float cardW = 460.0f;
		const float cardH = 400.0f;
		const float cardX = (kScreenWidth - cardW) * 0.5f;
		const float cardY = (kScreenHeight - cardH) * 0.5f + 25.0f;

		float scanY = cardY + cardH * exitProg;
		float drawnH = cardH * exitProg;

		// 走査線（レーザーライン）
		if (pressSpaceBannerBg_) {
			// レーザースイープ線
			tr->Print("HackGen", "-->> FORMING TACTICAL UI INTERFACE FRAME <<--", kScreenWidth * 0.5f, scanY - 14.0f, 12.0f, { 0.35f, 1.0f, 0.65f, 0.95f }, { 0.5f, 0.5f });
		}
	}
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
	} else if (state_ == TitleState::Menu) {
		// 背景オーバーレイ
		backgroundPanel_.Draw();

		// メニュー移行トランジション（0.35秒間の滑らかな起動フェードイン＆走査線展開：項目3）
		if (menuEnterTimer_ > 0.0f) {
			DrawTransitionGlitch();
		}

		// 中央のコンソール端末ウィンドウ
		DrawMenuCard();

		TextRenderer* tr = TextRenderer::GetInstance();
		const float cardWidth = 520.0f;
		const float cardHeight = 616.0f;
		const float cardX = menuWindowPos_.x;
		const float cardY = menuWindowPos_.y;

		// 端末ヘッダーテキスト (HackGen 13.0f) - タイトルバー左側
		tr->Print("HackGen", "TERMINAL : DAWN_OS [v4.12]", cardX + 28.0f, cardY + 9.0f, 13.0f, { 0.35f, 1.0f, 0.60f, 0.95f });

		// 作戦指令ヘッダーラベル - すべて緑文字に統一
		tr->Print("HackGen", "/// CLASSIFIED OPERATION DIRECTIVE ///", cardX + cardWidth * 0.5f, cardY + 22.0f, 11.0f, { 0.22f, 0.78f, 0.45f, 0.80f }, { 0.5f, 0.0f });

		// メインタイトル：DAWN ロゴスプライト（白文字 + MiG-21通過）
		if (titleLogoSprite_) {
			titleLogoSprite_->Draw();
		}

		// サブタイトル
		subtitleText_.Draw();

		// システム認証ステータス行 - すべて緑文字に統一
		tr->Print("HackGen", "AUTH LEVEL 5 GRANTED // READY FOR MISSION INPUT", cardX + cardWidth * 0.5f, cardY + 162.0f, 10.5f, { 0.22f, 0.78f, 0.45f, 0.85f }, { 0.5f, 0.0f });

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

		// ウィンドウ最下部ステータスバー (HackGen 10.5f) - 緑文字に統一
		tr->Print(
			"HackGen",
			"ROOT@TACTICAL_OS: ONLINE",
			cardX + 16.0f,
			cardY + cardHeight - 18.0f,
			10.5f,
			{ 0.25f, 0.82f, 0.45f, 0.85f }
		);
		tr->Print(
			"HackGen",
			"PORT: 115200 BAUD // UTF-8",
			cardX + cardWidth - 16.0f,
			cardY + cardHeight - 18.0f,
			10.5f,
			{ 0.22f, 0.75f, 0.40f, 0.80f },
			{ 1.0f, 0.0f }
		);
	}
}

void TitleScene::Finalize() {
	SetCursorVisible(true); // シーン終了時にカーソルを復帰（項目7）
	UITextRegistry::GetInstance()->Clear();
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

