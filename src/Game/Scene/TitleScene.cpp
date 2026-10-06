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

void TitleScene::Initialize() {
	Input::GetInstance()->UnlockCursor(); // メニュー用にマウスを表示・ロック解除
	sceneID = SCENE::TITLE;
	selectionManager_.Clear();
	animTimer_ = 0.0f;

	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();

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
		l.lightType = 2;                                  // トンネル内はポイントライト（回転灯）のみ。格納庫用の投光器や外光は排除
		l.dirIntensity = 0.0f;
		l.pointColor = { 1.0f, 0.42f, 0.12f, 1.0f };     // 天井の回転灯からのアンバー/赤警告光
		l.pointOffset = { 0.0f, 4.8f, 0.0f };
		l.pointIntensity = 1.8f;
		l.pointRadius = 22.0f;
		l.pointDecay = 1.0f;
		l.pointFlicker = 0.45f;                           // 回転灯の明滅ゆらぎ
		l.spotIntensity = 0.0f;                           // 格納庫用の投光器スポットライトは完全無効化

		// トンネル後方チェイスカメラ（機体テールの双発アフターバーナー＆ショックダイヤモンドを克明に捉える）
		loc.camera = { 8.2f, 0.7f, 1.05f, 0.20f, 0.15f, 0.22f, 0.45f, true };
		loc.grade = { { 0.05f, 0.08f, 0.15f }, 0.08f, 0.6f };
	}

	// ------------------------------------------------------------
	// 2. 格納庫: 元のハンガーモデル + 作業灯
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
		l.dirColor = { 0.55f, 0.62f, 0.75f, 1.0f };      // シャッター隙間からの青白い光
		l.dirDirection = MyMath::Normalize({ 0.4f, -0.6f, 0.7f });
		l.dirIntensity = 0.45f;
		l.pointColor = { 1.0f, 0.75f, 0.45f, 1.0f };     // 作業灯（暖色）
		l.pointOffset = { -6.0f, 3.0f, -5.0f };
		l.pointIntensity = 1.1f;
		l.pointRadius = 20.0f;
		l.pointDecay = 1.3f;
		l.spotColor = { 1.0f, 0.98f, 0.92f, 1.0f };       // 天井投光器（斜め上方からのソフト照射）
		l.spotOffset = { -3.5f, 9.5f, 4.0f };
		l.spotDirection = MyMath::Normalize({ 0.25f, -0.90f, -0.35f });
		l.spotIntensity = 1.6f;
		l.spotDistance = 28.0f;
		l.spotDecay = 1.1f;
		l.spotAngleDeg = 48.0f;
		l.spotFalloffStartDeg = 28.0f;

		// 安定した見下ろし・見上げの周回カメラ（項目3: 振動・距離揺れゼロで完全な接地安定性を実現、ガタつきを根絶）
		loc.camera = { 12.0f, 0.0f, 2.2f, 0.0f, 1.2f, 0.05f, 0.0f, false };
		loc.grade = { { 0.05f, 0.08f, 0.15f }, 0.08f, 0.6f };
	}

	// ------------------------------------------------------------
	// 3. 墜落済みの森: 左翼喪失・機首から突っ込んだ残骸 + 炎の照り返し
	// ------------------------------------------------------------
	{
		LocationData& loc = locations_[static_cast<int>(TitleLocation::CrashedForest)];
		loc.id = TitleLocation::CrashedForest;
		loc.name = "CrashedForest";
		loc.origin = { 5000.0f, 0.0f, 5000.0f };
		loc.envModelPath = "";
		loc.aircraftOffset = { 0.0f, 0.4f, 0.0f };
		loc.aircraftRotation = { 0.18f, -0.8f, 0.35f }; // 機首下げ + 左に傾く
		loc.hiddenParts = {
			DamagePart::Wing_L, DamagePart::Wing1_L, DamagePart::Wing2_L,
			DamagePart::Aileron_L, DamagePart::Tank1,
		};
		loc.debrisOffset = { -7.0f, 0.3f, 4.0f };          // 千切れた左翼が後方に突き刺さる
		loc.debrisRotation = { 0.9f, 0.6f, -0.4f };
		loc.skyboxTexIndex = whiteSky;
		loc.skyboxColor = { 0.07f, 0.08f, 0.11f, 1.0f };    // 項目4: 荒涼とした曇天の暗い空（不自然な紫色の丸い太陽フレアを根本解消）

		// 墜落現場の地面パーツ（不時着滑走路・アスファルト敷地）
		loc.extraEnvParts.clear();
		{
			ExtraEnvPart ground;
			ground.modelPath = "Resources/models/title/Tunnel_road.obj";
			ground.offset = { 0.0f, -0.05f, 0.0f };
			ground.scale = { 3.0f, 1.0f, 3.0f };
			loc.extraEnvParts.push_back(std::move(ground));
		}

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
		loc.grade = { { 0.04f, 0.05f, 0.07f }, 0.04f, 0.5f }; // 冷徹なミリタリーダークトーン（紫色を排除）
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
	titleText_.Initialize("Consolas", "DAWN", 48.0f);
	titleText_.SetAnchorPoint({ 0.5f, 0.0f });
	titleText_.SetPosition({ kScreenWidth * 0.5f, 106.0f });
	titleText_.SetColor({ 0.22f, 1.0f, 0.52f, 1.0f }); // 鮮烈なエレクトリック・ターミナルグリーン
	titleText_.SetDropShadow(true, { 2.0f, 3.0f }, { 0.0f, 0.0f, 0.0f, 0.95f });
	titleText_.SetOutline(true, 1.5f, { 0.02f, 0.15f, 0.06f, 0.95f });

	// ============================
	// サブタイトルテキスト（ターミナルグリーン統一＆重なり解消）
	// ============================
	subtitleText_.Initialize("Consolas", "- OPERATION : DAWN -", 14.0f);
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
	startButton_.GetLabelText()->SetFontName("Consolas");
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
	editorButton_.GetLabelText()->SetFontName("Consolas");
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
	settingsButton_.GetLabelText()->SetFontName("Consolas");
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
	exitButton_.GetLabelText()->SetFontName("Consolas");
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
	// PRESS SPACE テキスト（項目7: 画面上部Y=92に配置し、主役の機体シルエット被りを完全解消）
	// ============================
	pressSpaceText_.Initialize("Consolas", "[ PRESS SPACE OR CLICK TO START ]", 17.0f);
	pressSpaceText_.SetAnchorPoint({ 0.5f, 0.5f });
	pressSpaceText_.SetPosition({ kScreenWidth * 0.5f, 76.0f }); // 機首・胴体と重ならない画面上部セーフゾーン
	pressSpaceText_.SetColor({ 0.92f, 0.98f, 0.95f, 0.95f });
	pressSpaceText_.SetDropShadow(true, { 2.0f, 2.0f }, { 0.0f, 0.0f, 0.0f, 0.95f });
	pressSpaceText_.SetOutline(true, 1.5f, { 0.02f, 0.04f, 0.03f, 0.98f });
	UIStateStyle pulseStyle;
	pulseStyle.color = { 0.92f, 0.98f, 0.95f, 0.95f };
	pulseStyle.scale = 1.0f;
	pulseStyle.loopMotion = UILoopMotion::Pulse;
	pulseStyle.motionIntensity = 1.2f;
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
	if (state_ == TitleState::DroneView || state_ == TitleState::ConsoleBoot) {
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

		// 墜落現場専用：折れた左翼付け根・破損エンジンから立ち上るリアルな濃煙と飛び散る火花
		if (loc.id == TitleLocation::CrashedForest) {
			crashSmokeTimer_ += dt;
			if (crashSmokeTimer_ >= 0.04f) {
				crashSmokeTimer_ = 0.0f;
				MyMath::Vector3 smokePos = MyMath::Add(loc.origin, loc.aircraftOffset);
				smokePos.x -= 1.8f;
				smokePos.y += 0.8f;
				smokePos.z += 0.2f;

				ParticleParameters smokeParams;
				smokeParams.minVelocity = { -0.015f, 0.025f, -0.015f };
				smokeParams.maxVelocity = {  0.015f, 0.055f,  0.015f };
				// シリアスな軍用濃煙・黒煙（ポップなシャボン玉色を完全排除）
				smokeParams.minColor = { 0.06f, 0.06f, 0.07f, 0.85f };
				smokeParams.maxColor = { 0.14f, 0.14f, 0.16f, 0.92f };
				smokeParams.minLifeTime = 3.2f;
				smokeParams.maxLifeTime = 5.0f;
				smokeParams.minScale = 1.0f;
				smokeParams.maxScale = 4.2f;
				smokeParams.scaleEasing = 0.45f;
				smokeParams.randomPositionRange = 0.30f;
				smokeParams.acceleration = { 0.0003f, 0.0001f, 0.0002f };
				ParticleManager::GetInstance()->Emit("TitleSmoke", smokePos, smokeParams, 2);
			}

			crashSparkTimer_ += dt;
			if (crashSparkTimer_ >= 0.08f) {
				crashSparkTimer_ = 0.0f;
				MyMath::Vector3 sparkPos = MyMath::Add(loc.origin, loc.aircraftOffset);
				sparkPos.x -= 1.7f;
				sparkPos.y += 0.6f;
				sparkPos.z += 0.1f;

				ParticleParameters sparkParams;
				sparkParams.minVelocity = { -0.035f, 0.025f, -0.035f };
				sparkParams.maxVelocity = {  0.035f, 0.075f,  0.035f };
				sparkParams.minColor = { 1.0f, 0.40f, 0.08f, 1.0f };
				sparkParams.maxColor = { 1.0f, 0.95f, 0.35f, 1.0f };
				sparkParams.minLifeTime = 0.4f;
				sparkParams.maxLifeTime = 0.9f;
				sparkParams.minScale = 0.08f;
				sparkParams.maxScale = 0.22f;
				sparkParams.randomPositionRange = 0.20f;
				sparkParams.acceleration = { 0.0f, -0.0025f, 0.0f };
				ParticleManager::GetInstance()->Emit("TitleSpark", sparkPos, sparkParams, 3);
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

		// スペースキーまたはクリックでハッカー風コンソール演出（項目2）を経てメニューへ
		Input* input = Input::GetInstance();
		if (input->TriggerKey(DIK_SPACE) || input->TriggerMouse(0)) {
			state_ = TitleState::ConsoleBoot;
			consoleBootTimer_ = 0.0f;
		}

		// ポストエフェクト (ドローン風 + ブルーム発光)
		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();

		// ブルーム（適正なしきい値で高輝度部のみ自然に光らせる：項目1, 9）
		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = 3.5f; // ブラー半径
		bloom.dirX = 1.6f;      // ブルーム強度 (strength)
		bloom.dirY = 0.72f;     // 輝度閾値 (threshold: キャノピーの白飛び残像を防止)
		postEffect->AddActiveEffect(bloom);

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

		pressSpaceText_.Update();

	} else if (state_ == TitleState::ConsoleBoot) {
		consoleBootTimer_ += dt;
		Input* input = Input::GetInstance();
		bool skipTriggered = (consoleBootTimer_ > 0.30f && (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) || input->TriggerMouse(0)));

		// 時間経過またはスキップで、スムーズなフェードアウト演出を開始
		if (consoleBootTimer_ >= kConsoleBootDuration || skipTriggered) {
			consoleExitTimer_ += dt;
			if (consoleExitTimer_ >= kConsoleExitDuration) {
				state_ = TitleState::Menu;
				stateTimer_ = 0.0f;
				consoleExitTimer_ = 0.0f;
			}
		}

		// 背景を完全な黒（アルファ1.0）にして格納庫の透けを完全排除
		backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		backgroundPanel_.Update();

		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();

		// 切り替えフェードアウト時の短いCRT走査線グリッチ演出
		if (consoleExitTimer_ > 0.0f) {
			float exitRatio = consoleExitTimer_ / kConsoleExitDuration;
			ActivePostEffect scanline;
			scanline.type = PostEffectType::kScanLine;
			scanline.intensity = 0.15f * exitRatio;
			postEffect->AddActiveEffect(scanline);
		}

	} else if (state_ == TitleState::Menu) {
		selectionManager_.Update();
		UpdateMenuConsoleVisuals();

		// 現在のロケーションをゆっくりOrbitする
		UpdateLocationCamera(loc, dt, true);

		// 背景を暗くするフェードイン（背景の3Dを適度に暗く沈めてウィンドウを引き立てる）
		float bgAlpha = (std::min)(stateTimer_ * 2.0f, 0.62f);
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

	// 墜落現場の焦げ跡・激突痕デカール（項目5: 三角錐発光体は削除し、地面の焦げ跡とパーティクルのみでリアルに演出）
	if (loc.id == TitleLocation::CrashedForest && titleCamera) {
		PrimitiveModel* pm = PrimitiveModel::GetInstance();
		MyMath::Vector3 origin = loc.origin;
		uint32_t scorchTex = (crashScorchTexIndex_ != 0) ? crashScorchTexIndex_ : whiteTexIndex_;
		// 胴体激突地点の大きな焦げ跡デカール
		pm->DrawPlane({ 9.0f, 1.0f, 14.5f }, { 0.0f, -0.8f, 0.0f }, { origin.x, origin.y + 0.012f, origin.z }, { 0.02f, 0.02f, 0.02f, 0.88f }, scorchTex, titleCamera);
		// 千切れた左翼の激突痕デカール
		pm->DrawPlane({ 6.0f, 1.0f, 8.5f }, { 0.0f, 0.6f, 0.0f }, { origin.x - 7.0f, origin.y + 0.012f, origin.z + 4.0f }, { 0.02f, 0.02f, 0.02f, 0.82f }, scorchTex, titleCamera);
		// ※不自然な黄色い三角錐（DrawCone）は完全削除済み。煙と火花パーティクルで表現。
	}

	// 機体・残骸は現在のロケーションのみ描画
	loc.visualModel->Draw();
	if (loc.debrisModel) {
		loc.debrisModel->Draw();
	}

	// トンネル飛行中の演出（項目9: スピード感向上 - 高密度な流動ライト＆推進炎）
	if (loc.id == TitleLocation::Tunnel && titleCamera) {
		PrimitiveModel* pm = PrimitiveModel::GetInstance();

		// 高速で壁面・天井・路面を前方に流れるリニア誘導指標ライト（時速約470km/hの猛烈な後方疾走感）
		constexpr float kFlightSpeed = 130.0f;
		constexpr float kMarkerSpacing = 2.4f;
		float scrollZ = -std::fmod(animTimer_ * kFlightSpeed, kMarkerSpacing);
		for (float z = -40.0f; z <= 40.0f; z += kMarkerSpacing) {
			float markerZ = z + scrollZ;
			// 左右の壁面（X = ±6.25m, Y = 1.8m）
			pm->DrawPlane({ 0.16f, 1.0f, 3.2f }, { 0.0f, 0.0f, 1.5707963f }, { -6.25f, 1.8f, markerZ }, { 0.15f, 0.90f, 1.0f, 0.85f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
			pm->DrawPlane({ 0.16f, 1.0f, 3.2f }, { 0.0f, 0.0f, 1.5707963f }, { 6.25f, 1.8f, markerZ }, { 0.15f, 0.90f, 1.0f, 0.85f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
			// 天井誘導ライン（X = 0.0m, Y = 4.8m）
			pm->DrawPlane({ 0.8f, 1.0f, 2.8f }, { 3.14159265f, 0.0f, 0.0f }, { 0.0f, 4.8f, markerZ }, { 0.95f, 0.70f, 0.15f, 0.75f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
			// 路面高速流動マーカー（X = ±2.5m, Y = 0.03m）
			pm->DrawPlane({ 0.12f, 1.0f, 2.2f }, { 0.0f, 0.0f, 0.0f }, { -2.5f, 0.03f, markerZ }, { 0.95f, 0.75f, 0.15f, 0.60f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
			pm->DrawPlane({ 0.12f, 1.0f, 2.2f }, { 0.0f, 0.0f, 0.0f }, {  2.5f, 0.03f, markerZ }, { 0.95f, 0.75f, 0.15f, 0.60f }, whiteTexIndex_, titleCamera, BlendMode::kAdd);
		}

		// スラスター推進器噴射炎（Afterburner クラスによる単発エンジン・ショックダイヤモンド高品位描画）
		if (afterburner_) {
			afterburner_->Draw(currentAircraftMatrix_, titleCamera);
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
	buttonBorders_.clear();

	const float cardWidth = 520.0f;
	const float cardHeight = 616.0f;
	const float cardX = (kScreenWidth - cardWidth) * 0.5f; // 380.0f
	const float cardY = 52.0f;

	auto addBorderLine = [this, spriteCommon](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition(pos);
		sp->SetSize(size);
		sp->SetColor(color);
		sp->Update();
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
	addBorderLine({ cardX + 24.0f, 232.0f }, { cardWidth - 48.0f, 1.0f }, { 0.18f, 0.75f, 0.45f, 0.45f });

	// 操作案内枠（Y=482、高さ38px、幅 cardWidth - 50px）
	const float gfX = cardX + 25.0f;
	const float gfY = 482.0f;
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
	const float btnX = (kScreenWidth - btnW) * 0.5f;
	const float btnStartY = 248.0f;
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

		// 外周枠線 (1px) 4本
		group.outlines.push_back(createLineSprite({ btnX, by }, { btnW, 1.0f }, { 0.15f, 0.50f, 0.30f, 0.35f }));
		group.outlines.push_back(createLineSprite({ btnX, by + btnH - 1.0f }, { btnW, 1.0f }, { 0.15f, 0.50f, 0.30f, 0.35f }));
		group.outlines.push_back(createLineSprite({ btnX, by }, { 1.0f, btnH }, { 0.15f, 0.50f, 0.30f, 0.35f }));
		group.outlines.push_back(createLineSprite({ btnX + btnW - 1.0f, by }, { 1.0f, btnH }, { 0.15f, 0.50f, 0.30f, 0.35f }));

		// 四隅ブラケット (2px) 8本
		group.brackets.push_back(createLineSprite({ btnX, by }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f }));
		group.brackets.push_back(createLineSprite({ btnX, by }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f }));
		group.brackets.push_back(createLineSprite({ btnX + btnW - bcLen, by }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f }));
		group.brackets.push_back(createLineSprite({ btnX + btnW - bcThick, by }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f }));
		group.brackets.push_back(createLineSprite({ btnX, by + btnH - bcThick }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f }));
		group.brackets.push_back(createLineSprite({ btnX, by + btnH - bcLen }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f }));
		group.brackets.push_back(createLineSprite({ btnX + btnW - bcLen, by + btnH - bcThick }, { bcLen, bcThick }, { 0.20f, 0.70f, 0.40f, 0.50f }));
		group.brackets.push_back(createLineSprite({ btnX + btnW - bcThick, by + btnH - bcLen }, { bcThick, bcLen }, { 0.20f, 0.70f, 0.40f, 0.50f }));

		// 左端のアクティブバー
		group.indicator = createLineSprite({ btnX - 8.0f, by + 12.0f }, { 3.0f, 20.0f }, { 0.08f, 0.25f, 0.15f, 0.25f });

		buttonBorders_.push_back(std::move(group));
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
		tr->Print("Consolas", ">> OPTICAL FEED DISRUPTED - CHANNEL SYNCHRONIZING <<", kScreenWidth * 0.5f - 240.0f, kScreenHeight * 0.5f - 40.0f, 17.0f, { 1.0f, 0.82f, 0.15f, 0.95f });
		tr->Print("Consolas", "/// SENSOR RE-ACQUISITION IN PROGRESS - STAND BY ///", kScreenWidth * 0.5f - 230.0f, kScreenHeight * 0.5f - 16.0f, 13.0f, { 0.2f, 0.92f, 1.0f, 0.85f });
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

	// RECランプの点滅更新
	if (recDotSprite_) {
		bool recBlink = (std::fmod(animTimer_, 1.0f) < 0.6f);
		recDotSprite_->SetColor(recBlink ? MyMath::Vector4{ 1.0f, 0.15f, 0.15f, 0.95f } : MyMath::Vector4{ 0.3f, 0.05f, 0.05f, 0.20f });
		recDotSprite_->Update();
	}

	// 枠・レティクル描画
	for (auto& sp : osdSprites_) {
		sp->Draw();
	}
	if (recDotSprite_) {
		recDotSprite_->Draw();
	}

	TextRenderer* tr = TextRenderer::GetInstance();

	// --- 左上: REC & タイムコード (Consolas) ---
	tr->Print("Consolas", "REC", left + 24.0f, top + 9.0f, 15.0f, { 1.0f, 0.95f, 0.95f, 0.95f });

	char timeBuf[32];
	int totalSec = static_cast<int>(animTimer_);
	int hours = (totalSec / 3600) % 24;
	int mins = (totalSec / 60) % 60;
	int secs = totalSec % 60;
	int frames = static_cast<int>((animTimer_ - totalSec) * 60.0f);
	snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d:%02d", hours, mins, secs, frames);
	tr->Print("Consolas", timeBuf, left + 64.0f, top + 9.0f, 15.0f, { 0.85f, 0.95f, 1.0f, 0.85f });

	// --- 右上: バッテリー / シグナル / 解像度 (Consolas) ---
	tr->Print("Consolas", "BAT 94%   SIG ||||   4K/60P", right - 220.0f, top + 9.0f, 14.0f, { 0.85f, 0.95f, 1.0f, 0.75f });

	// --- 左下: カメラ番号 & ロケーション (Consolas) ---
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
		case TitleLocation::CrashedForest:
			camName = "CAM 03 // CRASH SITE - SECTOR 7";
			camCoord = "LAT 35.6780 N  LON 139.7102 E  ALT +340m";
			break;
		default:
			camName = "CAM 01 // RECON DRONE";
			camCoord = "LAT --.-- N  LON ---.-- E";
			break;
		}
	}
	tr->Print("Consolas", camName, left + 8.0f, bottom - 38.0f, 17.0f, { 0.85f, 0.95f, 1.0f, 0.90f });
	tr->Print("Consolas", camCoord, left + 8.0f, bottom - 18.0f, 12.0f, { 0.65f, 0.75f, 0.85f, 0.60f });

	// --- 右下: 光学系 & リンク情報 (Consolas) ---
	tr->Print("Consolas", "ZOOM 1.0x   F/2.8   ISO 400", right - 210.0f, bottom - 38.0f, 14.0f, { 0.85f, 0.95f, 1.0f, 0.75f });
	tr->Print("Consolas", "LINK: SECURE LIVE FEED", right - 210.0f, bottom - 18.0f, 12.0f, { 0.45f, 0.85f, 0.55f, 0.70f });
}

void TitleScene::DrawConsoleBoot() {
	// 背景の漆黒コンソールパネル（完全暗転）
	backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	backgroundPanel_.Draw();

	float textAlpha = 1.0f;
	if (consoleExitTimer_ > 0.0f) {
		textAlpha = (std::max)(0.0f, 1.0f - consoleExitTimer_ / kConsoleExitDuration);
	}

	TextRenderer* tr = TextRenderer::GetInstance();
	const float startX = 140.0f;
	float currentY = 160.0f;
	const float fontSize = 21.0f;
	const MyMath::Vector4 greenCol = { 0.20f, 0.95f, 0.45f, 0.96f * textAlpha };
	const MyMath::Vector4 cyanCol  = { 0.25f, 0.90f, 1.0f, 0.90f * textAlpha };

	// ターミナルヘッダー
	tr->Print("Consolas", "/// TACTICAL DEFENSE MAINFRAME // SECURE TERMINAL v4.12 ///", startX, currentY, 15.0f, cyanCol);
	currentY += 40.0f;

	if (consoleBootTimer_ >= 0.05f) {
		tr->Print("Consolas", ">> BOOT SEQUENCE INITIATED...", startX, currentY, fontSize, greenCol);
		currentY += 34.0f;
	}
	if (consoleBootTimer_ >= 0.25f) {
		tr->Print("Consolas", ">> SCANNING RECON DRONE TELEMETRY... [3 CHANNELS ONLINE]", startX, currentY, fontSize, greenCol);
		currentY += 34.0f;
	}
	if (consoleBootTimer_ >= 0.50f) {
		tr->Print("Consolas", ">> MOUNTING TACTICAL MISSION DATABASE... OK", startX, currentY, fontSize, greenCol);
		currentY += 34.0f;
	}
	if (consoleBootTimer_ >= 0.72f) {
		bool blink = (std::fmod(consoleBootTimer_, 0.24f) < 0.12f);
		std::string passLine = ">> booting...completed, password? *****";
		if (blink) { passLine += " _"; }
		tr->Print("Consolas", passLine, startX, currentY, fontSize, { 0.35f, 1.0f, 0.55f, 1.0f * textAlpha });
		currentY += 45.0f;
	}
	if (consoleBootTimer_ >= 0.95f) {
		tr->Print("Consolas", "[ ACCESS GRANTED: WELCOME TO THE COMBAT ZONE ]", startX, currentY, 19.0f, { 1.0f, 0.92f, 0.25f, 0.95f * textAlpha });
	}

	// 画面下部にスキップ案内
	tr->Print("Consolas", "[ PRESS SPACE OR CLICK TO SKIP ]", kScreenWidth * 0.5f, kScreenHeight - 45.0f, 14.0f, { 0.45f, 0.60f, 0.70f, 0.60f * textAlpha }, { 0.5f, 0.5f });
}

void TitleScene::DrawUI() {
	if (state_ == TitleState::DroneView) {
		DrawOSD();
		pressSpaceText_.Draw();
		DrawTransitionGlitch();
	} else if (state_ == TitleState::ConsoleBoot) {
		DrawConsoleBoot();
	} else if (state_ == TitleState::Menu) {
		// 背景オーバーレイ
		backgroundPanel_.Draw();

		// 中央のコンソール端末ウィンドウ
		DrawMenuCard();

		TextRenderer* tr = TextRenderer::GetInstance();
		const float cardWidth = 520.0f;
		const float cardHeight = 616.0f;
		const float cardX = (kScreenWidth - cardWidth) * 0.5f;
		const float cardY = 52.0f;

		// 端末ヘッダーテキスト (Consolas 13.0f) - タイトルバー左側
		tr->Print("Consolas", "TERMINAL : DAWN_OS [v4.12]", cardX + 28.0f, cardY + 9.0f, 13.0f, { 0.35f, 1.0f, 0.60f, 0.95f });

		// 作戦指令ヘッダーラベル - すべて緑文字に統一
		tr->Print("Consolas", "/// CLASSIFIED OPERATION DIRECTIVE ///", kScreenWidth * 0.5f, 74.0f, 11.0f, { 0.22f, 0.78f, 0.45f, 0.80f }, { 0.5f, 0.0f });

		// メインタイトル：DAWN ロゴスプライト（白文字 + MiG-21通過）
		if (titleLogoSprite_) {
			titleLogoSprite_->Draw();
		}

		// サブタイトル
		subtitleText_.Draw();

		// システム認証ステータス行 - すべて緑文字に統一
		tr->Print("Consolas", "AUTH LEVEL 5 GRANTED // READY FOR MISSION INPUT", kScreenWidth * 0.5f, 214.0f, 10.5f, { 0.22f, 0.78f, 0.45f, 0.85f }, { 0.5f, 0.0f });

		// メニューボタン（コマンド行）
		startButton_.Draw();
		editorButton_.Draw();
		settingsButton_.Draw();
		exitButton_.Draw();

		// 操作ガイド (Consolas 14.0f) - フッター枠（Y=482〜520）の中央にピッタリ収まる緑文字統一
		tr->Print(
			"Consolas",
			"[ UP / DOWN ] SELECT     [ ENTER / SPACE ] EXECUTE",
			kScreenWidth * 0.5f,
			501.0f,
			14.0f,
			{ 0.35f, 0.95f, 0.55f, 0.95f },
			{ 0.5f, 0.5f }
		);

		// ウィンドウ最下部ステータスバー (Consolas 10.5f) - 緑文字に統一
		tr->Print(
			"Consolas",
			"ROOT@TACTICAL_OS: ONLINE",
			cardX + 16.0f,
			cardY + cardHeight - 18.0f,
			10.5f,
			{ 0.25f, 0.82f, 0.45f, 0.85f }
		);
		tr->Print(
			"Consolas",
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

