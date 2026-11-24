#include "AircraftEditorScene.h"

#ifdef USE_IMGUI

#include "../../engine/base/WinApp.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/Model/SkyboxCommon.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../engine/Graphics/System/DirectXCommon.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/io/Input.h"
#include "../../../external/imgui/imgui.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

using namespace MyMath;
namespace fs = std::filesystem;

namespace {
	constexpr float kPi = 3.14159265358979323846f;
	constexpr float kDegToRad = kPi / 180.0f;
	constexpr float kFixedDeltaTime = 1.0f / 60.0f;
	constexpr float kPreviewBaseHeight = 1.5f;

	Vector3 UnprojectNDC(const Vector3& ndc, const Matrix4x4& invVP) {
		const float x = ndc.x * invVP.m[0][0] + ndc.y * invVP.m[1][0] + ndc.z * invVP.m[2][0] + invVP.m[3][0];
		const float y = ndc.x * invVP.m[0][1] + ndc.y * invVP.m[1][1] + ndc.z * invVP.m[2][1] + invVP.m[3][1];
		const float z = ndc.x * invVP.m[0][2] + ndc.y * invVP.m[1][2] + ndc.z * invVP.m[2][2] + invVP.m[3][2];
		const float w = ndc.x * invVP.m[0][3] + ndc.y * invVP.m[1][3] + ndc.z * invVP.m[2][3] + invVP.m[3][3];
		if (std::fabs(w) > 1e-6f) {
			const float invW = 1.0f / w;
			return { x * invW, y * invW, z * invW };
		}
		return { x, y, z };
	}

	Vector4 GetPartWireColor(DamagePart part, bool highlighted) {
		if (highlighted) {
			return { 1.0f, 0.25f, 0.95f, 1.0f };
		}
		switch (part) {
		case DamagePart::Fuse:
		case DamagePart::Fuse1:
			return { 0.30f, 0.92f, 1.0f, 0.78f };
		case DamagePart::Wing_L:
		case DamagePart::Wing1_L:
		case DamagePart::Wing2_L:
		case DamagePart::Aileron_L:
			return { 0.35f, 1.0f, 0.55f, 0.85f };
		case DamagePart::Wing_R:
		case DamagePart::Wing1_R:
		case DamagePart::Wing2_R:
		case DamagePart::Aileron_R:
			return { 0.55f, 1.0f, 0.35f, 0.85f };
		case DamagePart::Tail:
		case DamagePart::Fin:
		case DamagePart::Rudder:
		case DamagePart::Elevator0:
		case DamagePart::Elevator1:
			return { 1.0f, 0.75f, 0.25f, 0.88f };
		case DamagePart::Engine1:
		case DamagePart::Propeller1:
			return { 1.0f, 0.40f, 0.25f, 0.90f };
		default:
			return { 0.75f, 0.85f, 1.0f, 0.75f };
		}
	}
}

void AircraftEditorScene::Initialize() {
	sceneID = SCENE::AIRCRAFT_EDITOR;

	// マウスカーソルを必ず表示・解放状態にする
	if (Input::GetInstance()) {
		Input::GetInstance()->UnlockCursor();
	}
	ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
	while (::ShowCursor(TRUE) < 0) {}
	::SetCursor(::LoadCursor(nullptr, IDC_ARROW));

	// ポストエフェクトをクリア
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->ClearActiveEffects();
	}

	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();

	if (!isInitializedOnce_) {
		TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");
		TextureManager::GetInstance()->LoadTexture("assets/textures/circle2.png");
		TextureManager::GetInstance()->LoadTexture("assets/textures/qwantani_dusk_2_puresky_2k.dds");
		whiteTexIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");

		const uint32_t skyboxTexIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/qwantani_dusk_2_puresky_2k.dds");
		skybox_ = std::make_unique<Skybox>();
		skybox_->Initialize(SkyboxCommon::GetInstance());
		skybox_->SetCamera(camera);
		skybox_->SetTextureIndex(skyboxTexIndex);
		skybox_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		Object3dCommon::GetInstance()->SetDefaultEnvTextureIndex(skyboxTexIndex);

		environmentManager_.Initialize();
		environmentManager_.SetTimeOfDay(timeOfDay_);

		visualModel_.Initialize(Object3dCommon::GetInstance(), camera);

		afterburner_ = std::make_unique<Afterburner>();
		afterburner_->Initialize();

		previewDamageModel_.Initialize();

		DirectXCommon::GetInstance()->FlushTextureUploads();
		isInitializedOnce_ = true;
	} else {
		if (skybox_) {
			skybox_->SetCamera(camera);
		}
		visualModel_.SetCamera(camera);
	}

	// 共通ライティング設定をエディタ向けに整える
	Object3dCommon::GetInstance()->SetLightType(1);
	Object3dCommon::GetInstance()->SetShadingModel(1);
	Object3dCommon::GetInstance()->SetSpecularModel(2);

	RefreshFileLists();
	LoadAircraftCfg(AircraftConfig::GetActiveConfigPath());

	// 現在の視点で即座にカメラ行列を更新
	orbitTarget_ = { 0.0f, kPreviewBaseHeight, 0.0f };
	orbitYaw_ = -2.45f;
	orbitPitch_ = 0.25f;
	orbitDistance_ = 19.0f;
	turntableAngle_ = 0.0f;

	if (camera) {
		const Vector3 forward = {
			std::cos(orbitPitch_) * std::sin(orbitYaw_),
			-std::sin(orbitPitch_),
			std::cos(orbitPitch_) * std::cos(orbitYaw_)
		};
		const Vector3 camPos = Subtract(orbitTarget_, Multiply(orbitDistance_, forward));
		camera->SetRotate({ orbitPitch_, orbitYaw_, 0.0f });
		camera->SetTranslate(camPos);
		camera->UpdateMatrices();
		Object3dCommon::GetInstance()->SetCameraPosition(camPos);
	}
	CameraManager::GetInstance()->Update();
}

void AircraftEditorScene::RefreshFileLists() {
	availableCfgFiles_ = AircraftConfig::GetAvailableAircraftList("Resources/aircraft");
	std::sort(availableCfgFiles_.begin(), availableCfgFiles_.end());

	availableModelFiles_.clear();
	const fs::path modelsDir("Resources/models");
	if (fs::exists(modelsDir)) {
		try {
			for (const auto& entry : fs::recursive_directory_iterator(modelsDir)) {
				if (!entry.is_regular_file()) {
					continue;
				}
				const std::string ext = entry.path().extension().string();
				if (ext == ".gltf" || ext == ".glb" || ext == ".obj") {
					std::string normalized = entry.path().generic_string();
					availableModelFiles_.push_back(normalized);
				}
			}
		} catch (...) {
		}
	}
	std::sort(availableModelFiles_.begin(), availableModelFiles_.end());

	// 現在ロード中のCFGに合わせてコンボインデックスを同期
	const std::string currentCfgName = fs::path(aircraftConfig_.GetCurrentFilePath()).filename().string();
	selectedCfgIndex_ = 0;
	for (int i = 0; i < static_cast<int>(availableCfgFiles_.size()); ++i) {
		if (fs::path(availableCfgFiles_[i]).filename().string() == currentCfgName) {
			selectedCfgIndex_ = i;
			break;
		}
	}
}

void AircraftEditorScene::LoadAircraftCfg(const std::string& path) {
	const std::string targetPath = path.empty() ? std::string("Resources/aircraft/mig21.cfg") : path;
	if (!aircraftConfig_.LoadFromCfg(targetPath)) {
		SetStatus("Failed to load CFG: " + targetPath, true);
		return;
	}

	AircraftConfig::SetActiveConfigPath(targetPath);

	const std::string fileName = fs::path(targetPath).filename().string();
	strcpy_s(saveAsNameBuf_, sizeof(saveAsNameBuf_), fileName.c_str());

	const auto& general = aircraftConfig_.GetGeneral();
	ReloadVisualModel(general.modelPath);

	if (afterburner_) {
		const std::string abPath = general.afterburnerConfigPath.empty()
			? std::string("assets/configs/afterburner_m21.json")
			: general.afterburnerConfigPath;
		afterburner_->LoadConfig(abPath);
	}

	// コンボボックス選択も同期
	for (int i = 0; i < static_cast<int>(availableCfgFiles_.size()); ++i) {
		if (fs::path(availableCfgFiles_[i]).filename().string() == fileName) {
			selectedCfgIndex_ = i;
			break;
		}
	}

	SetStatus("Loaded: " + fileName, false);
}

void AircraftEditorScene::ReloadVisualModel(const std::string& modelPath) {
	if (modelPath.empty()) {
		return;
	}

	ModelManager::GetInstance()->LoadModel(modelPath);
	Model* model = ModelManager::GetInstance()->FindModel(modelPath);
	if (!model) {
		SetStatus("Model not found: " + modelPath, true);
		return;
	}

	visualModel_.Clear();
	visualModel_.SetupFromSingleModel(model);
	if (visualModel_.GetPartCount() == 0) {
		// パーツ名ノードを持たない汎用メッシュは胴体スロットへ一括割り当て
		visualModel_.SetModelForPart(DamagePart::Fuse, model);
	}
	visualModel_.ResetAllPartsVisible();
	loadedModelPath_ = modelPath;
	ApplyMaterialFromConfig();
	RebuildPreviewMeshCollider();
}

void AircraftEditorScene::RebuildPreviewMeshCollider() {
	highlightedMeshPartIndex_ = -1;
	highlightedColliderIndex_ = -1;
	hasRaycastSample_ = false;
	lastRaycastHit_ = {};

	if (loadedModelPath_.empty()) {
		return;
	}
	if (Model* model = ModelManager::GetInstance()->FindModel(loadedModelPath_)) {
		previewDamageModel_.BuildMeshCollider(model, visualModel_.GetBaseRotation());
	}
}

void AircraftEditorScene::ApplyMaterialFromConfig() {
	const auto& general = aircraftConfig_.GetGeneral();
	visualModel_.SetMaterialProperties(
		general.shininess,
		general.specularIntensity,
		general.environmentCoefficient);
	visualModel_.SetupCanopyMaterial();
}

void AircraftEditorScene::SetStatus(const std::string& msg, bool isError) {
	statusMessage_ = msg;
	statusIsError_ = isError;
	statusTimer_ = isError ? 6.0f : 4.0f;
}

void AircraftEditorScene::SetCameraPreset(float yaw, float pitch, float distance, const MyMath::Vector3& target) {
	orbitYaw_ = yaw;
	orbitPitch_ = pitch;
	orbitDistance_ = distance;
	orbitTarget_ = target;
}

void AircraftEditorScene::UpdateOrbitCamera() {
	Input* input = Input::GetInstance();
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (!camera) {
		return;
	}

	const ImGuiIO& io = ImGui::GetIO();
	const bool imguiWantsMouse = io.WantCaptureMouse;
	const bool imguiWantsKeyboard = io.WantCaptureKeyboard;

	if (input) {
		const Input::MouseMove mm = input->GetMouseMove();

		if (!imguiWantsMouse) {
			// 右ドラッグ: オービット回転
			if (input->PushMouse(1)) {
				constexpr float kOrbitSens = 0.006f;
				orbitYaw_ += static_cast<float>(mm.lX) * kOrbitSens;
				orbitPitch_ += static_cast<float>(mm.lY) * kOrbitSens;
				orbitPitch_ = std::clamp(orbitPitch_, -1.45f, 1.45f);
			}

			// 中ドラッグ: ターゲットのパン移動
			if (input->PushMouse(2)) {
				const float panScale = orbitDistance_ * 0.0015f;
				const Matrix4x4 camRotMat = MakeAffineMatrix(
					Vector3{ 1.0f, 1.0f, 1.0f },
					Vector3{ orbitPitch_, orbitYaw_, 0.0f },
					Vector3{ 0.0f, 0.0f, 0.0f });
				const Vector3 camRight = { camRotMat.m[0][0], camRotMat.m[0][1], camRotMat.m[0][2] };
				const Vector3 camUp = { camRotMat.m[1][0], camRotMat.m[1][1], camRotMat.m[1][2] };
				orbitTarget_ = Add(orbitTarget_, Multiply(-static_cast<float>(mm.lX) * panScale, camRight));
				orbitTarget_ = Add(orbitTarget_, Multiply(static_cast<float>(mm.lY) * panScale, camUp));
			}

			// ホイールズーム
			if (mm.lZ != 0) {
				const float zoomFactor = 1.0f - static_cast<float>(mm.lZ) * 0.0012f;
				orbitDistance_ = std::clamp(orbitDistance_ * zoomFactor, 3.0f, 80.0f);
			}
		}

		if (!imguiWantsKeyboard) {
			const float panSpeed = (input->PushKey(DIK_LSHIFT) ? 0.35f : 0.12f) * (orbitDistance_ / 18.0f);
			const Matrix4x4 yawRot = MakeRotateYMatrix(orbitYaw_);
			Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
			if (input->PushKey(DIK_W)) { moveDir.z += panSpeed; }
			if (input->PushKey(DIK_S)) { moveDir.z -= panSpeed; }
			if (input->PushKey(DIK_A)) { moveDir.x -= panSpeed; }
			if (input->PushKey(DIK_D)) { moveDir.x += panSpeed; }
			if (input->PushKey(DIK_E)) { moveDir.y += panSpeed; }
			if (input->PushKey(DIK_Q)) { moveDir.y -= panSpeed; }
			orbitTarget_ = Add(orbitTarget_, TransformNormal(moveDir, yawRot));

			if (input->TriggerKey(DIK_R)) {
				SetCameraPreset(-2.45f, 0.25f, 19.0f, { 0.0f, kPreviewBaseHeight, 0.0f });
			}
			if (input->TriggerKey(DIK_SPACE)) {
				autoTurntable_ = !autoTurntable_;
			}
		}
	}

	const Vector3 forward = {
		std::cos(orbitPitch_) * std::sin(orbitYaw_),
		-std::sin(orbitPitch_),
		std::cos(orbitPitch_) * std::cos(orbitYaw_)
	};
	const Vector3 camPos = Subtract(orbitTarget_, Multiply(orbitDistance_, forward));

	camera->SetRotate({ orbitPitch_, orbitYaw_, 0.0f });
	camera->SetTranslate(camPos);
	camera->UpdateMatrices();
	Object3dCommon::GetInstance()->SetCameraPosition(camPos);
}

MyMath::Matrix4x4 AircraftEditorScene::ComputeAircraftWorldMatrix() const {
	const Vector3 rotRad = {
		previewAttitudeDeg_.x * kDegToRad,
		previewAttitudeDeg_.y * kDegToRad + turntableAngle_,
		previewAttitudeDeg_.z * kDegToRad
	};
	return MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, rotRad, { 0.0f, kPreviewBaseHeight, 0.0f });
}

void AircraftEditorScene::Update() {
	while (::ShowCursor(TRUE) < 0) {}

	if (statusTimer_ > 0.0f) {
		statusTimer_ = (std::max)(0.0f, statusTimer_ - kFixedDeltaTime);
	}

	if (autoTurntable_) {
		turntableAngle_ += turntableSpeed_ * kFixedDeltaTime;
		if (turntableAngle_ > kPi * 2.0f) {
			turntableAngle_ -= kPi * 2.0f;
		}
	}

	UpdateOrbitCamera();

	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->ClearActiveEffects();
	}

	environmentManager_.SetTimeOfDay(timeOfDay_);
	environmentManager_.Update(camera, 0.0f);

	if (isSkyboxVisible_ && skybox_) {
		skybox_->Update();
	}

	const Matrix4x4 aircraftWorld = ComputeAircraftWorldMatrix();
	visualModel_.Update(aircraftWorld, kFixedDeltaTime);
	UpdateMeshRaycastTest();

	if (afterburner_) {
		const float normalLimit = aircraftConfig_.GetEngine().normalThrottleLimit;
		const float wepLimit = aircraftConfig_.GetEngine().wepThrottleLimit;
		float abIntensity = 0.0f;
		if (wepLimit > normalLimit + 0.005f && previewThrottle_ > normalLimit + 0.005f) {
			const float wepRatio = std::clamp((previewThrottle_ - normalLimit) / (wepLimit - normalLimit), 0.0f, 1.0f);
			abIntensity = 0.35f + 0.65f * wepRatio;
		}
		afterburner_->Update(kFixedDeltaTime, abIntensity);
	}

	DrawEditorWindows();
}

void AircraftEditorScene::UpdateMeshRaycastTest() {
	if (!enableMeshRaycastTest_ || !previewDamageModel_.HasMeshCollider()) {
		return;
	}
	Input* input = Input::GetInstance();
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (!input || !camera) {
		return;
	}
	if (ImGui::GetIO().WantCaptureMouse || !input->TriggerMouse(0)) {
		return;
	}

	const float w = static_cast<float>(WinApp::kClientWidth);
	const float h = static_cast<float>(WinApp::kClientHeight);
	const Vector2 mousePos = input->GetVirtualMousePosition(w, h);
	const float ndcX = (mousePos.x / w) * 2.0f - 1.0f;
	const float ndcY = 1.0f - (mousePos.y / h) * 2.0f;

	const Matrix4x4 invVP = Inverse(camera->GetViewProjectionMatrix());
	const Vector3 worldNear = UnprojectNDC({ ndcX, ndcY, 0.0f }, invVP);
	const Vector3 worldFar = UnprojectNDC({ ndcX, ndcY, 1.0f }, invVP);

	const Matrix4x4 invAircraft = Inverse(ComputeAircraftWorldMatrix());
	const Vector3 localStart = TransformV3(worldNear, invAircraft);
	const Vector3 localEnd = TransformV3(worldFar, invAircraft);
	const Vector3 localDelta = Subtract(localEnd, localStart);
	const float rayLen = Length(localDelta);
	if (rayLen < 1e-4f) {
		return;
	}

	Ray localRay;
	localRay.origin = localStart;
	localRay.direction = Multiply(1.0f / rayLen, localDelta);
	localRay.maxDistance = rayLen;

	lastRaycastHit_ = previewDamageModel_.GetMeshCollider().IntersectRayLocal(
		localRay,
		[this](DamagePart p) { return visualModel_.IsPartVisible(p); });
	hasRaycastSample_ = true;

	if (lastRaycastHit_.hit) {
		SetStatus(
			std::string("Mesh Hit: ") + DamageModel::GetPartName(lastRaycastHit_.part) +
			" (Tri #" + std::to_string(lastRaycastHit_.triangleIndex) + ")",
			false);
	} else {
		SetStatus("Mesh Raycast: Miss", false);
	}
}

void AircraftEditorScene::DrawFloorGrid(Camera* camera) {
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim || !camera) {
		return;
	}

	if (showGrid_) {
		constexpr int kHalfCells = 12;
		constexpr float kCellSize = 2.0f;
		constexpr float kExtent = kHalfCells * kCellSize;
		const Vector4 gridCol = { 0.26f, 0.36f, 0.44f, 0.45f };
		const Vector4 centerLineX = { 0.70f, 0.25f, 0.25f, 0.70f };
		const Vector4 centerLineZ = { 0.25f, 0.50f, 0.85f, 0.70f };

		for (int i = -kHalfCells; i <= kHalfCells; ++i) {
			const float offset = static_cast<float>(i) * kCellSize;
			prim->DrawLine3D({ -kExtent, 0.0f, offset }, { kExtent, 0.0f, offset }, (i == 0) ? centerLineX : gridCol, camera);
			prim->DrawLine3D({ offset, 0.0f, -kExtent }, { offset, 0.0f, kExtent }, (i == 0) ? centerLineZ : gridCol, camera);
		}
	}

	if (showAxes_) {
		const Vector3 origin = { 0.0f, kPreviewBaseHeight, 0.0f };
		prim->DrawLine3D(origin, { 4.5f, kPreviewBaseHeight, 0.0f }, { 1.0f, 0.25f, 0.25f, 0.95f }, camera);
		prim->DrawLine3D(origin, { 0.0f, kPreviewBaseHeight + 4.5f, 0.0f }, { 0.25f, 1.0f, 0.35f, 0.95f }, camera);
		prim->DrawLine3D(origin, { 0.0f, kPreviewBaseHeight, 4.5f }, { 0.30f, 0.65f, 1.0f, 0.95f }, camera);
	}
}

void AircraftEditorScene::DrawCrossMarker(
	const MyMath::Vector3& localPos,
	float radius,
	const MyMath::Matrix4x4& worldMat,
	const MyMath::Vector4& color,
	Camera* camera) {
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim || !camera) {
		return;
	}
	const Vector3 pX0 = TransformV3({ localPos.x - radius, localPos.y, localPos.z }, worldMat);
	const Vector3 pX1 = TransformV3({ localPos.x + radius, localPos.y, localPos.z }, worldMat);
	const Vector3 pY0 = TransformV3({ localPos.x, localPos.y - radius, localPos.z }, worldMat);
	const Vector3 pY1 = TransformV3({ localPos.x, localPos.y + radius, localPos.z }, worldMat);
	const Vector3 pZ0 = TransformV3({ localPos.x, localPos.y, localPos.z - radius }, worldMat);
	const Vector3 pZ1 = TransformV3({ localPos.x, localPos.y, localPos.z + radius }, worldMat);
	prim->DrawLine3D(pX0, pX1, color, camera);
	prim->DrawLine3D(pY0, pY1, color, camera);
	prim->DrawLine3D(pZ0, pZ1, color, camera);
}

void AircraftEditorScene::DrawWireBox(
	const MyMath::Vector3& center,
	const MyMath::Vector3& halfSize,
	const MyMath::Matrix4x4& worldMat,
	const MyMath::Vector4& color,
	Camera* camera) {
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim || !camera) {
		return;
	}

	Vector3 corners[8];
	for (int i = 0; i < 8; ++i) {
		const float sx = (i & 1) ? halfSize.x : -halfSize.x;
		const float sy = (i & 2) ? halfSize.y : -halfSize.y;
		const float sz = (i & 4) ? halfSize.z : -halfSize.z;
		corners[i] = TransformV3({ center.x + sx, center.y + sy, center.z + sz }, worldMat);
	}

	constexpr int kEdges[12][2] = {
		{0, 1}, {2, 3}, {4, 5}, {6, 7},
		{0, 2}, {1, 3}, {4, 6}, {5, 7},
		{0, 4}, {1, 5}, {2, 6}, {3, 7}
	};
	for (const auto& edge : kEdges) {
		prim->DrawLine3D(corners[edge[0]], corners[edge[1]], color, camera);
	}
}

void AircraftEditorScene::DrawMeshColliderGizmo(Camera* camera, const MyMath::Matrix4x4& aircraftWorld) {
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	const auto& mc = previewDamageModel_.GetMeshCollider();
	if (!prim || !camera || !mc.IsValid()) {
		return;
	}

	const auto& parts = mc.GetParts();
	constexpr size_t kMaxEditorWireLines = 6000;
	size_t drawnLines = 0;

	for (int i = 0; i < static_cast<int>(parts.size()); ++i) {
		const auto& part = parts[i];
		if (!visualModel_.IsPartVisible(part.part)) {
			continue;
		}
		const Vector4 color = GetPartWireColor(part.part, i == highlightedMeshPartIndex_);
		for (const auto& edge : part.wireEdges) {
			if (drawnLines >= kMaxEditorWireLines) {
				break;
			}
			const Vector3 wa = TransformV3(edge.a, aircraftWorld);
			const Vector3 wb = TransformV3(edge.b, aircraftWorld);
			prim->DrawLine3D(wa, wb, color, camera);
			++drawnLines;
		}
		if (drawnLines >= kMaxEditorWireLines) {
			break;
		}
	}

	// 左クリックレイキャストのヒット三角形＆法線表示
	if (hasRaycastSample_ && lastRaycastHit_.hit) {
		const Vector3 w0 = TransformV3(lastRaycastHit_.hitTriangle.v0, aircraftWorld);
		const Vector3 w1 = TransformV3(lastRaycastHit_.hitTriangle.v1, aircraftWorld);
		const Vector3 w2 = TransformV3(lastRaycastHit_.hitTriangle.v2, aircraftWorld);
		const Vector4 hitTriCol = { 1.0f, 0.15f, 0.15f, 1.0f };
		prim->DrawLine3D(w0, w1, hitTriCol, camera);
		prim->DrawLine3D(w1, w2, hitTriCol, camera);
		prim->DrawLine3D(w2, w0, hitTriCol, camera);

		DrawCrossMarker(lastRaycastHit_.localPoint, 0.30f, aircraftWorld, { 1.0f, 0.95f, 0.15f, 1.0f }, camera);

		const Vector3 wPt = TransformV3(lastRaycastHit_.localPoint, aircraftWorld);
		const Vector3 wNormalEnd = TransformV3(
			Add(lastRaycastHit_.localPoint, Multiply(1.4f, lastRaycastHit_.localNormal)),
			aircraftWorld);
		prim->DrawLine3D(wPt, wNormalEnd, { 1.0f, 0.95f, 0.15f, 1.0f }, camera);
	}
}

void AircraftEditorScene::DrawAircraftGizmos(
	Camera* camera,
	const MyMath::Matrix4x4& aircraftWorld,
	const MyMath::Matrix4x4& visualWorld) {
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim || !camera) {
		return;
	}

	// 0. メッシュコライダー（三角形ワイヤーフレーム）
	if (showMeshCollider_) {
		DrawMeshColliderGizmo(camera, aircraftWorld);
	}

	// 1. 重心 (CoG: 黄色) & 圧力中心 (CP: シアン)
	if (showCoGAndCP_) {
		const float cgZ = aircraftConfig_.GetAirframe().centerOfGravityZ;
		DrawCrossMarker({ 0.0f, 0.0f, cgZ }, 0.85f, aircraftWorld, { 1.0f, 0.90f, 0.15f, 1.0f }, camera);
		DrawCrossMarker({ 0.0f, 0.0f, -0.2f }, 0.70f, aircraftWorld, { 0.20f, 0.95f, 1.0f, 1.0f }, camera);
		// 重心と圧力中心を結ぶ線
		const Vector3 cgW = TransformV3({ 0.0f, 0.0f, cgZ }, aircraftWorld);
		const Vector3 cpW = TransformV3({ 0.0f, 0.0f, -0.2f }, aircraftWorld);
		prim->DrawLine3D(cgW, cpW, { 1.0f, 0.85f, 0.4f, 0.85f }, camera);
	}

	// 2. ダメージモデル サブコライダー群
	if (showSubColliders_) {
		const auto& cols = previewDamageModel_.GetSubColliders();
		for (int i = 0; i < static_cast<int>(cols.size()); ++i) {
			const auto& col = cols[i];
			const bool isHighlighted = (i == highlightedColliderIndex_);
			Vector4 color = { 0.35f, 0.90f, 0.45f, 0.80f };
			if (col.part == DamagePart::Pilot) {
				color = { 1.0f, 0.85f, 0.20f, 0.95f };
			} else if (col.part == DamagePart::Engine1 || col.part == DamagePart::Tank1 || col.part == DamagePart::Tank2) {
				color = { 1.0f, 0.45f, 0.20f, 0.88f };
			}
			if (isHighlighted) {
				color = { 1.0f, 0.25f, 0.95f, 1.0f };
			}
			DrawWireBox(col.center, col.halfSize, aircraftWorld, color, camera);
		}
	}

	// 3. ミサイルパイロン位置
	if (showPylons_) {
		for (const auto& offset : previewMissileData_.pylonLocalOffsets) {
			DrawCrossMarker(offset, 0.35f, aircraftWorld, { 1.0f, 0.55f, 0.15f, 0.95f }, camera);
			// パイロンから胴体側への吊り下げガイド線
			const Vector3 pTop = TransformV3({ offset.x, offset.y + 0.35f, offset.z }, aircraftWorld);
			const Vector3 pBot = TransformV3(offset, aircraftWorld);
			prim->DrawLine3D(pTop, pBot, { 1.0f, 0.55f, 0.15f, 0.85f }, camera);
		}
	}

	// 4. アフターバーナーノズル位置＆噴射方向
	if (showNozzleGizmo_ && afterburner_) {
		auto drawNozzle = [&](const Vector3& localOffset) {
			const Matrix4x4 nozzleLocal = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, afterburner_->GetNozzleRotation(), localOffset);
			const Matrix4x4 nozzleWorld = nozzleLocal * visualWorld;
			DrawCrossMarker({ 0.0f, 0.0f, 0.0f }, 0.35f, nozzleWorld, { 1.0f, 0.25f, 0.25f, 1.0f }, camera);
			const Vector3 p0 = TransformV3({ 0.0f, 0.0f, 0.0f }, nozzleWorld);
			const Vector3 p1 = TransformV3({ 0.0f, 0.0f, 2.5f }, nozzleWorld);
			prim->DrawLine3D(p0, p1, { 0.25f, 0.85f, 1.0f, 0.95f }, camera);
		};

		if (afterburner_->IsTwinEngine()) {
			drawNozzle(afterburner_->GetLeftNozzleOffset());
			drawNozzle(afterburner_->GetRightNozzleOffset());
		} else {
			drawNozzle(afterburner_->GetSingleNozzleOffset());
		}
	}

	// 5. 機関砲マズル発射位置＆前方射線ガイド
	if (showGunMuzzles_) {
		const auto& muzzles = aircraftConfig_.GetGunpod().muzzleOffsets;
		for (int i = 0; i < static_cast<int>(muzzles.size()); ++i) {
			const auto& offset = muzzles[static_cast<size_t>(i)];
			const bool isSelected = (i == selectedMuzzleIndex_);
			const Vector4 markerCol = isSelected
				? Vector4{ 1.0f, 0.95f, 0.20f, 1.0f }
				: Vector4{ 1.0f, 0.55f, 0.15f, 0.92f };
			const Vector4 lineCol = isSelected
				? Vector4{ 1.0f, 0.92f, 0.30f, 0.95f }
				: Vector4{ 1.0f, 0.55f, 0.15f, 0.65f };

			DrawCrossMarker(offset, isSelected ? 0.35f : 0.26f, aircraftWorld, markerCol, camera);
			const Vector3 pStart = TransformV3(offset, aircraftWorld);
			const Vector3 pEnd = TransformV3({ offset.x, offset.y, offset.z + 6.5f }, aircraftWorld);
			prim->DrawLine3D(pStart, pEnd, lineCol, camera);
		}
	}
}

void AircraftEditorScene::Draw() {
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();

	if (isSkyboxVisible_ && skybox_) {
		SkyboxCommon::GetInstance()->SetupCommonState();
		skybox_->Draw();
	}

	Object3dCommon::GetInstance()->SetupCommonState();
	environmentManager_.Draw(camera);

	visualModel_.Draw();

	DrawFloorGrid(camera);

	const Matrix4x4 aircraftWorld = ComputeAircraftWorldMatrix();
	const Matrix4x4 baseRotate = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, visualModel_.GetBaseRotation(), { 0.0f, 0.0f, 0.0f });
	const Matrix4x4 visualWorld = Multiply(baseRotate, aircraftWorld);

	DrawAircraftGizmos(camera, aircraftWorld, visualWorld);

	if (afterburner_) {
		afterburner_->Draw(visualWorld, camera);
	}
}

void AircraftEditorScene::DrawUI() {
	TextRenderer* tr = TextRenderer::GetInstance();
	if (!tr) {
		return;
	}
	const auto& general = aircraftConfig_.GetGeneral();
	const std::string banner = "[DEV] AIRCRAFT EDITOR SCENE (F10) // " + general.name +
		" [" + fs::path(aircraftConfig_.GetCurrentFilePath()).filename().string() + "]";
	tr->Print("HackGen", banner, 18.0f, 56.0f, 15.0f, { 0.35f, 0.92f, 1.0f, 0.92f });
}

void AircraftEditorScene::Finalize() {
	statusTimer_ = 0.0f;
}

#endif // USE_IMGUI
