#include "DebugScene.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/SkyboxCommon.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../../external/imgui/imgui.h"
#include "../../engine/io/Input.h"
#include "../../engine/base/Math/MyMath.h"

void DebugScene::Initialize() {
	sceneID = SCENE::DEBUG;

	// カメラ初期化（原点に配置）
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (camera) {
		camera->SetTranslate({ 0.0f, 0.0f, 0.0f });
		camera->SetRotate({ 0.0f, 0.0f, 0.0f });
		CameraManager::GetInstance()->Update();
	}

	// 利用可能な Skybox テクスチャ一覧
	skyboxTextures_ = {
		"assets/textures/cedar_bridge_sunset_1_2k.dds",
		"assets/textures/qwantani_dusk_2_puresky_2k.dds",
		"assets/textures/rostock_laage_airport_4k.dds"
	};

	for (const auto& path : skyboxTextures_) {
		TextureManager::GetInstance()->LoadTexture(path);
	}

	// Skybox 初期化
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(SkyboxCommon::GetInstance());
	skybox_->SetCamera(CameraManager::GetInstance()->GetActiveCamera());

	uint32_t skyboxTexIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(skyboxTextures_[currentSkyboxIndex_]);
	skybox_->SetTextureIndex(skyboxTexIndex);
	skybox_->SetColor(skyboxColor_);
}

void DebugScene::Update() {
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (camera) {
		Vector3 rotate = camera->GetRotate();

		const float kRotSpeed = 0.02f;

		// 矢印キーでカメラ回転
		if (Input::GetInstance()->PushKey(DIK_UP)) { rotate.x -= kRotSpeed; }
		if (Input::GetInstance()->PushKey(DIK_DOWN)) { rotate.x += kRotSpeed; }
		if (Input::GetInstance()->PushKey(DIK_LEFT)) { rotate.y -= kRotSpeed; }
		if (Input::GetInstance()->PushKey(DIK_RIGHT)) { rotate.y += kRotSpeed; }

		// マウス右ボタンドラッグで360度カメラ回転
		if (Input::GetInstance()->PushMouse(1)) {
			Input::MouseMove mouseMove = Input::GetInstance()->GetMouseMove();
			const float kMouseSensitivity = 0.003f;
			rotate.y += mouseMove.lX * kMouseSensitivity;
			rotate.x += mouseMove.lY * kMouseSensitivity;
		}

		// Rキーで視点リセット
		if (Input::GetInstance()->TriggerKey(DIK_R)) {
			rotate = { 0.0f, 0.0f, 0.0f };
		}

		camera->SetRotate(rotate);
	}

	if (skybox_) {
		skybox_->Update();
	}

#ifdef USE_IMGUI
	ImGui::Begin("Skybox Viewer");
	ImGui::Text("Controls:");
	ImGui::BulletText("Right-click + Drag: Rotate Camera (360 Look)");
	ImGui::BulletText("Arrow Keys: Rotate Camera");
	ImGui::BulletText("R: Reset Camera");
	ImGui::Separator();

	const char* texNames[] = {
		"Cedar Bridge Sunset (2K)",
		"Qwantani Dusk Pure Sky (2K)",
		"Rostock Laage Airport (4K)"
	};
	if (ImGui::Combo("Environment", &currentSkyboxIndex_, texNames, IM_ARRAYSIZE(texNames))) {
		uint32_t texIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(skyboxTextures_[currentSkyboxIndex_]);
		skybox_->SetTextureIndex(texIndex);
	}

	if (ImGui::ColorEdit4("Skybox Tint / Color", &skyboxColor_.x)) {
		skybox_->SetColor(skyboxColor_);
	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Post Effects")) {
		const char* presetItems[] = { "Select Preset...", "Cinematic Bloom", "VHS Retro", "NVD (Night Vision)", "Digital Glitch", "Luminance Outline" };
		static int currentPresetIndex = 0;
		if (ImGui::Combo("Built-in Preset", &currentPresetIndex, presetItems, IM_ARRAYSIZE(presetItems))) {
			if (currentPresetIndex > 0) {
				PostEffect::GetInstance()->ApplyBuiltInPreset(presetItems[currentPresetIndex]);
				currentPresetIndex = 0;
			}
		}
		if (ImGui::Button("Clear Post Effects")) {
			PostEffect::GetInstance()->ClearActiveEffects();
		}
	}
	ImGui::End();
#endif
}

void DebugScene::Draw() {
	// 最低限 Skybox のみを描画
	if (skybox_) {
		SkyboxCommon::GetInstance()->SetupCommonState();
		skybox_->Draw();
	}
}

void DebugScene::Finalize() {
	skybox_.reset();
}
