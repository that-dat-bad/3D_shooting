#include "EffectEditorScene.h"

#ifdef _DEBUG

#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/Model/SkyboxCommon.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/io/Input.h"
#include <Windows.h>
#include <cmath>
#include <cstdio>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

using namespace MyMath;

void EffectEditorScene::Initialize() {
	sceneID = SCENE::EFFECT_EDITOR;

	// マウスカーソルを表示
	if (Input::GetInstance()) {
		Input::GetInstance()->UnlockCursor();
	}
	while (::ShowCursor(TRUE) < 0) {}
	::SetCursor(::LoadCursor(nullptr, IDC_ARROW));

	// ポストエフェクトをクリア
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->ClearActiveEffects();
	}

	TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("assets/textures/circle2.png");
	TextureManager::GetInstance()->LoadTexture("assets/textures/cedar_bridge_sunset_1_2k.dds");
	whiteTexIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");

	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (camera) {
		camera->SetTranslate({ 0.0f, 3.5f, -18.0f });
		camera->SetRotate({ 0.08f, 0.0f, 0.0f });
		CameraManager::GetInstance()->Update();
	}

	// Skybox初期化
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(SkyboxCommon::GetInstance());
	skybox_->SetCamera(camera);
	const uint32_t skyboxTexIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/cedar_bridge_sunset_1_2k.dds");
	skybox_->SetTextureIndex(skyboxTexIndex);
	Object3dCommon::GetInstance()->SetDefaultEnvTextureIndex(skyboxTexIndex);

	// 地面モデル初期化
	ModelManager::GetInstance()->LoadModel("models/terrain.obj");
	groundObject_ = std::make_unique<Object3d>();
	groundObject_->Initialize(Object3dCommon::GetInstance());
	groundObject_->SetCamera(camera);
	groundObject_->SetModel("models/terrain.obj");
	groundObject_->SetTranslate({ 0.0f, -1.0f, 0.0f });
	groundObject_->Update();

	// エフェクト初期化
	EffectManager::GetInstance()->Initialize();
	emitOrigin_ = { 0.0f, 0.0f, 0.0f };
	simulateConditionTrue_ = true;

	if (!previewHandle_) {
		previewHandle_ = EffectManager::GetInstance()->CreateHandle("default_fire");
	}
	if (previewHandle_) {
		previewHandle_->SetPosition(emitOrigin_);
	}
}

void EffectEditorScene::Update() {
	while (::ShowCursor(TRUE) < 0) {}

	Input* input = Input::GetInstance();
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();

	bool imguiWantsMouse = false;
	bool imguiWantsKeyboard = false;
#ifdef USE_IMGUI
	ImGuiIO& io = ImGui::GetIO();
	imguiWantsMouse = io.WantCaptureMouse;
	imguiWantsKeyboard = io.WantCaptureKeyboard;
#endif

	// カメラ操作
	if (camera && input) {
		Vector3 rotate = camera->GetRotate();
		Vector3 translate = camera->GetTranslate();

		const float kRotSpeed = 0.022f;
		const float kMoveSpeed = input->PushKey(DIK_LSHIFT) ? 0.45f : 0.16f;

		if (!imguiWantsKeyboard) {
			if (input->PushKey(DIK_UP)) { rotate.x -= kRotSpeed; }
			if (input->PushKey(DIK_DOWN)) { rotate.x += kRotSpeed; }
			if (input->PushKey(DIK_LEFT)) { rotate.y -= kRotSpeed; }
			if (input->PushKey(DIK_RIGHT)) { rotate.y += kRotSpeed; }
		}

		if (!imguiWantsMouse && input->PushMouse(1)) {
			Input::MouseMove mm = input->GetMouseMove();
			const float kMouseSens = 0.003f;
			rotate.y += mm.lX * kMouseSens;
			rotate.x += mm.lY * kMouseSens;
		}

		if (!imguiWantsKeyboard) {
			Matrix4x4 matRot = MakeRotateYMatrix(rotate.y);
			Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
			if (input->PushKey(DIK_W)) { moveDir.z += kMoveSpeed; }
			if (input->PushKey(DIK_S)) { moveDir.z -= kMoveSpeed; }
			if (input->PushKey(DIK_A)) { moveDir.x -= kMoveSpeed; }
			if (input->PushKey(DIK_D)) { moveDir.x += kMoveSpeed; }
			if (input->PushKey(DIK_E)) { moveDir.y += kMoveSpeed; }
			if (input->PushKey(DIK_Q)) { moveDir.y -= kMoveSpeed; }

			moveDir = TransformNormal(moveDir, matRot);
			translate = Add(translate, moveDir);

			if (input->TriggerKey(DIK_R)) {
				translate = { 0.0f, 3.5f, -18.0f };
				rotate = { 0.08f, 0.0f, 0.0f };
			}
		}

		camera->SetRotate(rotate);
		camera->SetTranslate(translate);
		Object3dCommon::GetInstance()->SetCameraPosition(translate);
	}

	if (groundObject_) {
		groundObject_->Update();
	}
	if (skybox_) {
		skybox_->Update();
	}

	// ショートカットキー操作
	if (input && !imguiWantsKeyboard) {
		if (input->TriggerKey(DIK_SPACE) && previewHandle_) {
			previewHandle_->emit(emitOrigin_);
		}
		if (input->TriggerKey(DIK_X) && previewHandle_) {
			previewHandle_->stop();
			simulateConditionTrue_ = false;
		}
		if (input->TriggerKey(DIK_C) && ParticleManager::GetInstance()) {
			ParticleManager::GetInstance()->ClearAllParticles();
		}
	}

	// エフェクト発生テスト
	if (simulateConditionTrue_ && previewHandle_) {
		previewHandle_->emit(emitOrigin_);
	}

	// エフェクトマネージャの更新
	EffectManager::GetInstance()->Update();

#ifdef USE_IMGUI
	// エフェクト編集GUI
	EffectManager::GetInstance()->DrawDebugGui(previewHandle_);

	// シーン操作パネル
	ImGui::Begin("Effect Scene & Handle Tester [DEBUG]");
	ImGui::TextColored(ImVec4(0.3f, 0.95f, 0.7f, 1.0f), "/// INDEPENDENT EFFECT EDITOR SCENE (DEBUG ONLY) ///");
	ImGui::Text("Camera: Right-Drag Rotate | WASD+QE Move (Shift: Fast) | R Reset");
	ImGui::Separator();

	ImGui::Checkbox("Skybox Visible", &isSkyboxVisible_);
	ImGui::SameLine();
	ImGui::Checkbox("Ground Visible", &isGroundVisible_);
	ImGui::SameLine();
	ImGui::Checkbox("Show Height Limit Ring", &showHeightLimitGuide_);

	if (ImGui::DragFloat3("Emit Origin (X, Y, Z)", &emitOrigin_.x, 0.1f, -100.0f, 100.0f, "%.2f m")) {
		if (previewHandle_) {
			previewHandle_->SetPosition(emitOrigin_);
		}
	}

	ImGui::Separator();
	ImGui::Text("=== Handle Code Pattern Simulation ===");
	ImGui::Checkbox("Condition True: if (condition) { sample->emit(pos); }", &simulateConditionTrue_);

	if (ImGui::Button("sample->emit() [One-Shot / Start]", ImVec2(220.0f, 30.0f))) {
		if (!previewHandle_) {
			previewHandle_ = EffectManager::GetInstance()->CreateHandle("default_fire");
		}
		if (previewHandle_) {
			previewHandle_->emit(emitOrigin_);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("sample->stop() [X]", ImVec2(150.0f, 30.0f))) {
		simulateConditionTrue_ = false;
		if (previewHandle_) {
			previewHandle_->stop();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("sample->Delete()", ImVec2(140.0f, 30.0f))) {
		simulateConditionTrue_ = false;
		EffectManager::GetInstance()->DeleteHandle(previewHandle_);
	}

	if (!previewHandle_) {
		ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "previewHandle_ is deleted (nullptr).");
		if (ImGui::Button("Re-create Handle (CreateHandle)")) {
			const auto& presets = EffectManager::GetInstance()->GetPresets();
			const std::string defaultName = presets.empty() ? "default_fire" : presets.front().name;
			previewHandle_ = EffectManager::GetInstance()->CreateHandle(defaultName);
			if (previewHandle_) {
				previewHandle_->SetPosition(emitOrigin_);
			}
		}
	}

	ImGui::Separator();
	ImGui::Text("=== One-Shot Built-in Effects Test ===");
	if (ImGui::Button("EmitDestroyEffect (Explosion)")) {
		EffectManager::GetInstance()->EmitDestroyEffect(emitOrigin_);
	}
	ImGui::SameLine();
	if (ImGui::Button("EmitHitEffect")) {
		EffectManager::GetInstance()->EmitHitEffect(emitOrigin_);
	}
	ImGui::SameLine();
	if (ImGui::Button("EmitMuzzleFlash")) {
		EffectManager::GetInstance()->EmitMuzzleFlash(emitOrigin_, { 0.0f, 0.0f, 1.0f });
	}

	ImGui::End();
#endif
}

void EffectEditorScene::DrawReferenceGridAndHeightLimit(Camera* camera) {
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim || !camera) {
		return;
	}

	// グリッド描画
	const float baseY = emitOrigin_.y;
	const Vector4 gridCol = { 0.30f, 0.42f, 0.48f, 0.45f };
	const Vector4 axisXCol = { 0.90f, 0.28f, 0.28f, 0.80f };
	const Vector4 axisZCol = { 0.28f, 0.55f, 0.95f, 0.80f };
	const Vector4 axisYCol = { 0.30f, 0.95f, 0.45f, 0.85f };

	for (int i = -10; i <= 10; i += 2) {
		const float offset = static_cast<float>(i);
		const Vector4 colZ = (i == 0) ? axisXCol : gridCol;
		const Vector4 colX = (i == 0) ? axisZCol : gridCol;
		prim->DrawLine3D(
			{ emitOrigin_.x - 10.0f, baseY, emitOrigin_.z + offset },
			{ emitOrigin_.x + 10.0f, baseY, emitOrigin_.z + offset },
			colZ,
			camera);
		prim->DrawLine3D(
			{ emitOrigin_.x + offset, baseY, emitOrigin_.z - 10.0f },
			{ emitOrigin_.x + offset, baseY, emitOrigin_.z + 10.0f },
			colX,
			camera);
	}

	// 高さ目盛り線
	prim->DrawLine3D(
		emitOrigin_,
		{ emitOrigin_.x, emitOrigin_.y + 15.0f, emitOrigin_.z },
		axisYCol,
		camera);

	if (!showHeightLimitGuide_ || !previewHandle_) {
		return;
	}

	const EffectPreset* preset = EffectManager::GetInstance()->FindPreset(previewHandle_->GetEffectName());
	if (!preset) {
		return;
	}

	// 上昇上限の可視化
	for (size_t i = 0; i < preset->layers.size(); ++i) {
		const float limitH = preset->layers[i].params.maxRiseHeight;
		if (limitH <= 0.0f) {
			continue;
		}
		const Vector3 ceilingPos = { emitOrigin_.x, emitOrigin_.y + limitH, emitOrigin_.z };
		const Vector4 ringCol = (i == 0)
			? Vector4{ 1.0f, 0.72f, 0.20f, 0.55f }
			: Vector4{ 0.25f, 0.90f, 1.0f, 0.55f };
		prim->DrawRing(
			{ 4.5f, 4.5f, 4.5f },
			{ 3.14159265f * 0.5f, 0.0f, 0.0f },
			ceilingPos,
			ringCol,
			whiteTexIndex_,
			camera,
			BlendMode::kAdd);
	}
}

void EffectEditorScene::Draw() {
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();

	if (isSkyboxVisible_ && skybox_) {
		SkyboxCommon::GetInstance()->SetupCommonState();
		skybox_->Draw();
	}

	Object3dCommon::GetInstance()->SetupCommonState();
	if (isGroundVisible_ && groundObject_) {
		groundObject_->Draw();
	}

	DrawReferenceGridAndHeightLimit(camera);
	EffectManager::GetInstance()->Draw(camera);
}

void EffectEditorScene::DrawUI() {
	TextRenderer* tr = TextRenderer::GetInstance();
	if (!tr) {
		return;
	}
	tr->Print(
		"HackGen",
		"[DEBUG] EFFECT MANAGER SCENE (F9) // External .effect Driven",
		18.0f,
		56.0f,
		15.0f,
		{ 0.35f, 1.0f, 0.72f, 0.92f });
}

void EffectEditorScene::Finalize() {
	simulateConditionTrue_ = false;
	if (previewHandle_) {
		EffectManager::GetInstance()->DeleteHandle(previewHandle_);
	}
}

#endif // _DEBUG

