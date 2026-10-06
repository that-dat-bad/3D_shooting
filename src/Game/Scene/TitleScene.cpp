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
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../engine/Graphics/System/BlendMode.h"
#include "../../engine/Graphics/System/DirectXCommon.h"
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
	TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");

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

	TextureManager* tm = TextureManager::GetInstance();
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
		loc.aircraftRotation = { 0.0f, 3.14159265f, 0.0f }; // 機首を手前（カメラ側）に向ける
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

		// トンネル正面から機体を捉えるシネマティックカメラ
		loc.camera = { 14.0f, 1.5f, 0.2f, 0.4f, 0.2f, 0.2f, 0.45f, true };
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
		loc.aircraftOffset = { 0.0f, 0.0f, 0.0f };
		loc.aircraftRotation = { 0.0f, 0.5f, 0.0f };
		loc.propellerRpm = 0.0f;
		loc.skyboxTexIndex = sunsetSky;
		loc.skyboxColor = { 0.12f, 0.12f, 0.15f, 1.0f }; // 屋内なので空はほぼ見せない
		LocationLighting& l = loc.lighting;
		l.lightType = 1 | 2 | 4;
		l.dirColor = { 0.55f, 0.62f, 0.75f, 1.0f };      // シャッター隙間からの青白い光
		l.dirDirection = MyMath::Normalize({ 0.4f, -0.6f, 0.7f });
		l.dirIntensity = 0.35f;
		l.pointColor = { 1.0f, 0.72f, 0.42f, 1.0f };     // 作業灯（暖色）
		l.pointOffset = { -6.0f, 3.0f, -5.0f };
		l.pointIntensity = 0.8f;
		l.pointRadius = 18.0f;
		l.pointDecay = 1.5f;
		l.spotColor = { 1.0f, 0.97f, 0.9f, 1.0f };       // 天井投光器
		l.spotOffset = { 0.0f, 12.0f, 0.0f };
		l.spotDirection = { 0.0f, -1.0f, 0.0f };
		l.spotIntensity = 3.0f;
		l.spotDistance = 25.0f;
		l.spotDecay = 1.2f;
		l.spotAngleDeg = 40.0f;
		l.spotFalloffStartDeg = 22.0f;

		loc.camera = { 11.0f, 1.0f, 1.2f, 0.3f, 1.0f, 0.08f, 0.40f, false };
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
		loc.envModelPath = "Resources/models/title/crash_forest.obj";
		loc.aircraftOffset = { 0.0f, 0.4f, 0.0f };
		loc.aircraftRotation = { 0.18f, -0.8f, 0.35f }; // 機首下げ + 左に傾く
		loc.hiddenParts = {
			DamagePart::Wing_L, DamagePart::Wing1_L, DamagePart::Wing2_L,
			DamagePart::Aileron_L, DamagePart::Tank1,
		};
		loc.debrisOffset = { -7.0f, 0.3f, 4.0f };          // 千切れた左翼が後方に突き刺さる
		loc.debrisRotation = { 0.9f, 0.6f, -0.4f };
		loc.skyboxTexIndex = duskSky;
		loc.skyboxColor = { 0.45f, 0.5f, 0.55f, 1.0f };    // 曇天気味

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

		loc.camera = { 9.0f, 1.0f, 2.0f, 0.4f, 0.6f, 0.06f, 0.40f, false };
		loc.grade = { { 0.25f, 0.3f, 0.2f }, 0.12f, 0.8f };
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
	// タイトルテキスト
	// ============================
	titleText_.Initialize("Roboto", "戦雷", 80.0f);
	titleText_.SetAnchorPoint({ 0.5f, 0.0f });
	titleText_.SetPosition({ kScreenWidth * 0.5f, 120.0f });
	titleText_.SetColor({ 0.95f, 0.90f, 0.70f, 1.0f }); // ゴールド系

	// ============================
	// サブタイトルテキスト
	// ============================
	subtitleText_.Initialize("Roboto", "- THUNDER OF WAR -", 20.0f);
	subtitleText_.SetAnchorPoint({ 0.5f, 0.0f });
	subtitleText_.SetPosition({ kScreenWidth * 0.5f, 220.0f });
	subtitleText_.SetColor({ 0.6f, 0.65f, 0.75f, 0.8f });

	// ============================
	// メニューボタン
	// ============================
	float buttonWidth = 300.0f;
	float buttonHeight = 55.0f;
	float buttonX = (kScreenWidth - buttonWidth) * 0.5f;
	float buttonStartY = 320.0f;
	float buttonSpacing = 70.0f;

	// START ボタン
	startButton_.Initialize(spriteCommon, "START", 30.0f);
	startButton_.SetPosition({ buttonX, buttonStartY });
	startButton_.SetSize({ buttonWidth, buttonHeight });
	startButton_.SetNormalColor({ 0.10f, 0.12f, 0.18f, 0.85f });
	startButton_.SetHoverColor({ 0.15f, 0.25f, 0.45f, 0.95f });
	startButton_.SetSelectedColor({ 0.18f, 0.35f, 0.60f, 0.95f });
	startButton_.SetOnClick([this]() {
		sceneID = SCENE::STAGE;
	});

	// EDITOR ボタン
	editorButton_.Initialize(spriteCommon, "MISSION EDITOR", 30.0f);
	editorButton_.SetPosition({ buttonX, buttonStartY + buttonSpacing });
	editorButton_.SetSize({ buttonWidth, buttonHeight });
	editorButton_.SetNormalColor({ 0.10f, 0.12f, 0.18f, 0.85f });
	editorButton_.SetHoverColor({ 0.15f, 0.45f, 0.25f, 0.95f });
	editorButton_.SetSelectedColor({ 0.18f, 0.60f, 0.35f, 0.95f });
	editorButton_.SetOnClick([this]() {
		sceneID = SCENE::MISSION_EDITOR;
	});

	// SETTINGS ボタン
	settingsButton_.Initialize(spriteCommon, "SETTINGS", 30.0f);
	settingsButton_.SetPosition({ buttonX, buttonStartY + buttonSpacing * 2.0f });
	settingsButton_.SetSize({ buttonWidth, buttonHeight });
	settingsButton_.SetNormalColor({ 0.10f, 0.12f, 0.18f, 0.85f });
	settingsButton_.SetHoverColor({ 0.15f, 0.25f, 0.45f, 0.95f });
	settingsButton_.SetSelectedColor({ 0.18f, 0.35f, 0.60f, 0.95f });
	settingsButton_.SetOnClick([this]() {
		// 設定画面は将来実装
	});

	// EXIT ボタン
	exitButton_.Initialize(spriteCommon, "EXIT", 30.0f);
	exitButton_.SetPosition({ buttonX, buttonStartY + buttonSpacing * 3.0f });
	exitButton_.SetSize({ buttonWidth, buttonHeight });
	exitButton_.SetNormalColor({ 0.10f, 0.12f, 0.18f, 0.85f });
	exitButton_.SetHoverColor({ 0.40f, 0.15f, 0.15f, 0.95f });
	exitButton_.SetSelectedColor({ 0.55f, 0.18f, 0.18f, 0.95f });
	exitButton_.SetOnClick([]() {
		PostQuitMessage(0);
	});

	selectionManager_.AddButton(&startButton_);
	selectionManager_.AddButton(&editorButton_);
	selectionManager_.AddButton(&settingsButton_);
	selectionManager_.AddButton(&exitButton_);

	// ============================
	// PRESS SPACE テキスト
	// ============================
	pressSpaceText_.Initialize("Roboto", "PRESS SPACE OR CLICK TO START", 24.0f);
	pressSpaceText_.SetAnchorPoint({ 0.5f, 0.5f });
	pressSpaceText_.SetPosition({ kScreenWidth * 0.5f, kScreenHeight * 0.8f });
	pressSpaceText_.SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	UIStateStyle pulseStyle;
	pulseStyle.color = {1.0f, 1.0f, 1.0f, 1.0f};
	pulseStyle.scale = 1.0f;
	pulseStyle.loopMotion = UILoopMotion::Pulse;
	pulseStyle.motionIntensity = 3.0f;
	pulseStyle.motionSpeed = 2.0f;
	pressSpaceText_.SetStyle("Normal", pulseStyle);
	
	// UITextRegistryに登録（エディタから編集可能にする）
	UITextRegistry::GetInstance()->Register("Title_Main", &titleText_);
	UITextRegistry::GetInstance()->Register("Title_Sub", &subtitleText_);
	UITextRegistry::GetInstance()->Register("Title_BtnStart", startButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Title_BtnSettings", settingsButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Title_BtnExit", exitButton_.GetLabelText());
	UITextRegistry::GetInstance()->Register("Title_PressSpace", &pressSpaceText_);

	// ドローンカメラOSD初期化
	InitializeOSD(spriteCommon);
}

void TitleScene::Update() {
	float dt = 1.0f / 60.0f;
	animTimer_ += dt;
	stateTimer_ += dt;

	if (state_ == TitleState::DroneView) {
		cutTimer_ += dt;
		// 一定時間ごとに次のロケーションへカット切替
		if (!lockLocation_ && cutTimer_ > kCutDuration) {
			ChangeLocation((currentLocationIndex_ + 1) % static_cast<int>(locations_.size()));
		}
	}

	LocationData& loc = CurrentLocation();

	// 現在ロケーションの機体・残骸・背景を更新
	{
		MyMath::Vector3 pos = MyMath::Add(loc.origin, loc.aircraftOffset);
		MyMath::Vector3 rot = loc.aircraftRotation;

		// トンネル内の飛行モーション（低空巡航飛行の浮遊動揺・バンク・推力微振動）
		if (loc.id == TitleLocation::Tunnel) {
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

		loc.visualModel->Update(MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, rot, pos), dt);
		if (loc.debrisModel) {
			MyMath::Vector3 debrisPos = MyMath::Add(loc.origin, loc.debrisOffset);
			loc.debrisModel->Update(MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, loc.debrisRotation, debrisPos), dt);
		}

		// トンネル専用：手前から奥へ高速スクロールして飛行感を演出（時速約160km/h）
		float tunnelScrollZ = 0.0f;
		if (loc.id == TitleLocation::Tunnel) {
			constexpr float kTunnelLength = 40.383094f;
			constexpr float kFlightSpeed = 45.0f; // 飛行速度 (m/s)
			tunnelScrollZ = std::fmod(animTimer_ * kFlightSpeed, kTunnelLength);
		}

		if (loc.envObject) {
			MyMath::Vector3 envPos = loc.origin;
			envPos.z += tunnelScrollZ;
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
				partPos.z += tunnelScrollZ;
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
		UpdateLocationCamera(loc, dt, false);

		// スペースキーまたはクリックでメニューへ
		Input* input = Input::GetInstance();
		if (input->TriggerKey(DIK_SPACE) || input->TriggerMouse(0)) {
			state_ = TitleState::Menu;
			stateTimer_ = 0.0f;
		}

		// ポストエフェクト (ドローン風 + ブルーム発光)
		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();

		// ブルーム（誘導灯や警告回転灯の鮮烈な発光グロー）
		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = 4.0f; // ブラー半径
		bloom.dirX = 2.2f;      // ブルーム強度 (strength)
		bloom.dirY = 0.55f;     // 輝度閾値 (threshold)
		postEffect->AddActiveEffect(bloom);

		ActivePostEffect scanline;
		scanline.type = PostEffectType::kScanLine;
		scanline.intensity = 0.12f;
		postEffect->AddActiveEffect(scanline);

		ActivePostEffect chromatic;
		chromatic.type = PostEffectType::kChromaticAberration;
		chromatic.intensity = 0.025f;
		postEffect->AddActiveEffect(chromatic);

		ActivePostEffect lens;
		lens.type = PostEffectType::kLensDistortion;
		lens.intensity = 0.015f;
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

	} else if (state_ == TitleState::Menu) {
		selectionManager_.Update();

		// 背景を暗くするフェードイン
		float bgAlpha = (std::min)(stateTimer_ * 2.0f, 0.7f);
		backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, bgAlpha });

		// 現在のロケーションをゆっくりOrbitする
		UpdateLocationCamera(loc, dt, true);

		// メニュー用のクリーンなポストエフェクト（ブルームを正しく発光）
		PostEffect* postEffect = PostEffect::GetInstance();
		postEffect->ClearActiveEffects();
		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = 4.0f;
		bloom.dirX = 2.0f;
		bloom.dirY = 0.55f;
		postEffect->AddActiveEffect(bloom);

		backgroundPanel_.Update();
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
#endif
}

void TitleScene::Draw() {
	LocationData& loc = CurrentLocation();
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
	// 機体・残骸は現在のロケーションのみ描画
	loc.visualModel->Draw();
	if (loc.debrisModel) {
		loc.debrisModel->Draw();
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
		// トンネル専用カメラワーク：トンネル内開口部から機体正面を捉え、壁突き抜けを防止
		float swayX = std::sin(cameraTheta_) * 1.8f;
		float currentDist = menuMode ? c.distance * 1.15f : (c.distance + std::sin(animTimer_ * 0.8f) * c.distanceSway);
		float currentH = c.height + std::sin(animTimer_ * 1.2f) * c.heightSway;

		MyMath::Vector3 target = MyMath::Add(loc.origin, loc.aircraftOffset);
		target.y += c.lookAtHeight;

		MyMath::Vector3 camPos;
		camPos.x = target.x + swayX;
		camPos.y = target.y + currentH;
		camPos.z = target.z - currentDist; // 手前（-Z側）から奥の機体正面（+Z側）を見通す

		float dx = target.x - camPos.x;
		float dy = target.y - camPos.y;
		float dz = target.z - camPos.z;
		float yaw = std::atan2(dx, dz);
		float pitch = std::atan2(-dy, std::sqrt(dx * dx + dz * dz));

		// ドローン挙動：左右移動速度（cos）に合わせて機体をバンク傾き（手ぶれ・振動はゼロ）
		float swayVx = std::cos(cameraTheta_) * 1.8f;
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

	float dx = target.x - camPos.x;
	float dy = target.y - camPos.y;
	float dz = target.z - camPos.z;
	float yaw = std::atan2(dx, dz);
	float pitch = std::atan2(-dy, std::sqrt(dx * dx + dz * dz));

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
	cameraTheta_ = static_cast<float>(rand() % 628) * 0.01f; // ランダムな角度から映す
	LocationData& loc = CurrentLocation();
	ApplyLocationLighting(loc);
	if (skybox_) {
		skybox_->SetTextureIndex(loc.skyboxTexIndex);
		skybox_->SetColor(loc.skyboxColor);
	}
}

void TitleScene::InitializeOSD(SpriteCommon* spriteCommon) {
	osdSprites_.clear();

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
	const MyMath::Vector4 cornerColor = { 0.85f, 0.95f, 1.0f, 0.70f };

	// 四隅のL字ブラケット
	// 左上 ┌
	addBar({ left, top }, { len, thick }, cornerColor);
	addBar({ left, top }, { thick, len }, cornerColor);
	// 右上 ┐
	addBar({ right - len, top }, { len, thick }, cornerColor);
	addBar({ right - thick, top }, { thick, len }, cornerColor);
	// 左下 └
	addBar({ left, bottom - thick }, { len, thick }, cornerColor);
	addBar({ left, bottom - len }, { thick, len }, cornerColor);
	// 右下 ┘
	addBar({ right - len, bottom - thick }, { len, thick }, cornerColor);
	addBar({ right - thick, bottom - len }, { thick, len }, cornerColor);

	// 左右の水平マーカー（ピッチラダー風）
	const float cy = kScreenHeight * 0.5f;
	const MyMath::Vector4 markerColor = { 0.85f, 0.95f, 1.0f, 0.35f };
	addBar({ left, cy - 0.5f }, { 14.0f, 1.0f }, markerColor);
	addBar({ right - 14.0f, cy - 0.5f }, { 14.0f, 1.0f }, markerColor);

	// 中央十字レティクル
	const float cx = kScreenWidth * 0.5f;
	const MyMath::Vector4 reticleColor = { 0.85f, 0.95f, 1.0f, 0.40f };
	addBar({ cx - 0.5f, cy - 16.0f }, { 1.0f, 8.0f }, reticleColor);
	addBar({ cx - 0.5f, cy + 8.0f }, { 1.0f, 8.0f }, reticleColor);
	addBar({ cx - 16.0f, cy - 0.5f }, { 8.0f, 1.0f }, reticleColor);
	addBar({ cx + 8.0f, cy - 0.5f }, { 8.0f, 1.0f }, reticleColor);
	// センタードット
	addBar({ cx - 1.0f, cy - 1.0f }, { 2.0f, 2.0f }, { 0.85f, 0.95f, 1.0f, 0.60f });

	// REC点滅インジケータ（赤ランプ）
	recDotSprite_ = std::make_unique<Sprite>();
	recDotSprite_->Initialize(spriteCommon, "assets/textures/white1x1.png");
	recDotSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	recDotSprite_->SetPosition({ left + 10.0f, top + 13.0f });
	recDotSprite_->SetSize({ 8.0f, 8.0f });
	recDotSprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.95f });
	recDotSprite_->Update();
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

	// --- 左上: REC & タイムコード ---
	tr->Print("Roboto", "REC", left + 24.0f, top + 9.0f, 15.0f, { 1.0f, 0.95f, 0.95f, 0.95f });

	char timeBuf[32];
	int totalSec = static_cast<int>(animTimer_);
	int hours = (totalSec / 3600) % 24;
	int mins = (totalSec / 60) % 60;
	int secs = totalSec % 60;
	int frames = static_cast<int>((animTimer_ - totalSec) * 60.0f);
	snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d:%02d", hours, mins, secs, frames);
	tr->Print("Roboto", timeBuf, left + 64.0f, top + 9.0f, 15.0f, { 0.85f, 0.95f, 1.0f, 0.85f });

	// --- 右上: バッテリー / シグナル / 解像度 ---
	tr->Print("Roboto", "BAT 94%   SIG ||||   4K/60P", right - 220.0f, top + 9.0f, 14.0f, { 0.85f, 0.95f, 1.0f, 0.75f });

	// --- 左下: カメラ番号 & ロケーション（ユーザー画像再現） ---
	std::string camName;
	std::string camCoord;
	switch (CurrentLocation().id) {
	case TitleLocation::Hangar:
		camName = "CAM 01 // HANGAR - BAY 4";
		camCoord = "LAT 35.6895 N  LON 139.6917 E  ALT +12m";
		break;
	case TitleLocation::Tunnel:
		camName = "CAM 02 // SUBTERRANEAN TUNNEL";
		camCoord = "LAT 35.6912 N  LON 139.6880 E  ALT -24m";
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
	tr->Print("Roboto", camName, left + 8.0f, bottom - 38.0f, 17.0f, { 0.85f, 0.95f, 1.0f, 0.90f });
	tr->Print("Roboto", camCoord, left + 8.0f, bottom - 18.0f, 12.0f, { 0.65f, 0.75f, 0.85f, 0.60f });

	// --- 右下: 光学系 & リンク情報 ---
	tr->Print("Roboto", "ZOOM 1.0x   F/2.8   ISO 400", right - 210.0f, bottom - 38.0f, 14.0f, { 0.85f, 0.95f, 1.0f, 0.75f });
	tr->Print("Roboto", "LINK: SECURE LIVE FEED", right - 210.0f, bottom - 18.0f, 12.0f, { 0.45f, 0.85f, 0.55f, 0.70f });
}

void TitleScene::DrawUI() {
	if (state_ == TitleState::DroneView) {
		DrawOSD();
		pressSpaceText_.Draw();
	} else if (state_ == TitleState::Menu) {
		// 背景パネル
		backgroundPanel_.Draw();

		// タイトルテキスト
		titleText_.Draw();
		subtitleText_.Draw();

		// メニューボタン
		startButton_.Draw();
		editorButton_.Draw();
		settingsButton_.Draw();
		exitButton_.Draw();

		// 操作ガイド
		TextRenderer::GetInstance()->Print(
			"Roboto",
			"↑↓: SELECT   SPACE/ENTER: DECIDE",
			kScreenWidth * 0.5f - 170.0f,
			kScreenHeight - 60.0f,
			16.0f,
			{ 0.5f, 0.5f, 0.55f, 0.6f }
		);
	}
}

void TitleScene::Finalize() {
	UITextRegistry::GetInstance()->Clear();
}
