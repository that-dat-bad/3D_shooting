#include "../../engine/Graphics/Camera/CameraManager.h"
#include "DebugScene.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/SkyboxCommon.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../../external/imgui/imgui.h"
#include "../../engine/io/Input.h"
#include "../../engine/base/Math/MyMath.h"

#include <cmath>
#include <cfloat>
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Graphics/Text/TextRenderer.h"

void DebugScene::Initialize() {
	sceneID = SCENE::DEBUG;

	// --- 3Dオブジェクト ---

	sphereObject = std::make_unique<Object3d>();
	sphereObject->Initialize(Object3dCommon::GetInstance());
	sphereObject->SetCamera(CameraManager::GetInstance()->GetActiveCamera());
	ModelManager::GetInstance()->LoadModel("models/sphere.obj");
	ModelManager::GetInstance()->LoadModel("models/plane.gltf");
	ModelManager::GetInstance()->LoadModel("models/plane.obj");
	sphereObject->SetModel("models/sphere.obj");

	terrainObject = std::make_unique<Object3d>();
	terrainObject->Initialize(Object3dCommon::GetInstance());
	terrainObject->SetCamera(CameraManager::GetInstance()->GetActiveCamera());
	ModelManager::GetInstance()->LoadModel("models/terrain.obj");
	terrainObject->SetModel("models/terrain.obj");
	
	terrainObject->SetTranslate({ 0.0f, -1.0f, 0.0f });

	// テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("assets/textures/circle2.png");



	CameraManager::GetInstance()->GetActiveCamera()->SetTranslate({ 0.0f, 0.0f, -10.0f });
	CameraManager::GetInstance()->Update();

	debugRay_.origin = { 0.0f, 2.0f, -5.0f };
	debugRay_.direction = Normalize({ 0.0f, -0.2f, 1.0f });
	debugRay_.maxDistance = 20.0f;

	// --- Skybox ---
	TextureManager::GetInstance()->LoadTexture("assets/textures/cedar_bridge_sunset_1_2k.dds");
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(SkyboxCommon::GetInstance());
	skybox_->SetCamera(CameraManager::GetInstance()->GetActiveCamera());
	
	uint32_t skyboxTexIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/cedar_bridge_sunset_1_2k.dds");
	skybox_->SetTextureIndex(skyboxTexIndex);
	
	// 環境マップの全体設定
	Object3dCommon::GetInstance()->SetDefaultEnvTextureIndex(skyboxTexIndex);

	// エフェクトマネージャーの初期化
	EffectManager::GetInstance()->Initialize();
	// ヒットエフェクト用ビルボードのパーティクルグループ作成
	ParticleManager::GetInstance()->CreateParticleGroup("HitSpark", "assets/textures/white1x1.png");

	uint32_t gpuParticleTexIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");
	GPUParticleManager::GetInstance()->SetTexture(gpuParticleTexIndex);
}

void DebugScene::Update() {
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (camera) {
		Vector3 rotate = camera->GetRotate();
		Vector3 translate = camera->GetTranslate();

		const float kRotSpeed = 0.02f;
		const float kMoveSpeed = 0.1f;

		if (Input::GetInstance()->PushKey(DIK_UP)) { rotate.x -= kRotSpeed; }
		if (Input::GetInstance()->PushKey(DIK_DOWN)) { rotate.x += kRotSpeed; }
		if (Input::GetInstance()->PushKey(DIK_LEFT)) { rotate.y -= kRotSpeed; }
		if (Input::GetInstance()->PushKey(DIK_RIGHT)) { rotate.y += kRotSpeed; }

		// 0: Left, 1: Right, 2: Middle
		if (Input::GetInstance()->PushMouse(1)) {
			Input::MouseMove mouseMove = Input::GetInstance()->GetMouseMove();
			const float kMouseSensitivity = 0.003f;
			rotate.y += mouseMove.lX * kMouseSensitivity;
			rotate.x += mouseMove.lY * kMouseSensitivity;
		}

		Matrix4x4 matRot = MakeRotateYMatrix(rotate.y);
		Vector3 moveDir = { 0.0f, 0.0f, 0.0f };

		if (Input::GetInstance()->PushKey(DIK_W)) { moveDir.z += kMoveSpeed; }
		if (Input::GetInstance()->PushKey(DIK_S)) { moveDir.z -= kMoveSpeed; }
		if (Input::GetInstance()->PushKey(DIK_A)) { moveDir.x -= kMoveSpeed; }
		if (Input::GetInstance()->PushKey(DIK_D)) { moveDir.x += kMoveSpeed; }
		if (Input::GetInstance()->PushKey(DIK_E)) { moveDir.y += kMoveSpeed; }
		if (Input::GetInstance()->PushKey(DIK_Q)) { moveDir.y -= kMoveSpeed; }

		moveDir = TransformNormal(moveDir, matRot);

		translate = Add(translate, moveDir);

		if (Input::GetInstance()->TriggerKey(DIK_R)) {
			rotate = { 0.0f, 0.0f, 0.0f };
			translate = { 0.0f, 0.0f, -10.0f };
		}

		camera->SetRotate(rotate);
		camera->SetTranslate(translate);
	}

	sphereObject->Update();
	terrainObject->Update();
	if (skybox_) {
		skybox_->Update();
	}

	// テキスト描画のテスト
	TextRenderer::GetInstance()->Print("Roboto", "hello!world!", 50.0f, 50.0f, 32.0f, {0.2f, 1.0f, 0.2f, 1.0f});
	TextRenderer::GetInstance()->Print("Roboto", reinterpret_cast<const char*>(u8"ハロー！ワールド"), 50.0f, 90.0f, 24.0f, {1.0f, 1.0f, 1.0f, 1.0f});

	// エフェクトマネージャーの更新
	EffectManager::GetInstance()->Update();

	// デバッグ用: Spaceキーでエフェクト複数発生
	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		// スフィアの位置で全エフェクトを発生させる
		EffectManager::GetInstance()->EmitHitEffect(sphereObject->GetTranslate());
	}
	// デバッグ用: VキーでPlane（板ポリ）のみのエフェクト発生
	if (Input::GetInstance()->TriggerKey(DIK_V)) {
		EffectManager::GetInstance()->EmitHitPlaneEffect(sphereObject->GetTranslate());
	}
	// デバッグ用: BキーでGPUParticleManagerから最大数(1024個)のエフェクトを発生
	if (Input::GetInstance()->TriggerKey(DIK_B)) {
		GPUParticleManager::GetInstance()->SetEmitParams(sphereObject->GetTranslate(), 1024, 5.0f, 0.0f);
		GPUParticleManager::GetInstance()->Emit();
	}

#ifdef USE_IMGUI
	ImGui::Begin("Lighting Settings");
	if (ImGui::CollapsingHeader("Controls Help")) {
		ImGui::Text("WASD : Move Horizontal");
		ImGui::Text("Q / E : Move Vertical");
		ImGui::Text("Arrows : Rotate Camera");
		ImGui::Text("R-Click + Drag : Rotate Camera");
		ImGui::Text("R : Reset Camera");
		ImGui::Text("SPACE : Emit Full Hit Effect");
		ImGui::Text("V : Emit Hit Plane Effect Only");
		ImGui::Separator();
	}

	static int currentShading = 0;
	const char* shadingItems[] = { "Lambert", "Half-Lambert" };
	if (ImGui::Combo("Shading Model", &currentShading, shadingItems, IM_ARRAYSIZE(shadingItems))) {
		Object3dCommon::GetInstance()->SetShadingModel(currentShading);
	}

	static int currentSpecular = 0;
	const char* specularItems[] = { "None", "Phong", "Blinn-Phong" };
	if (ImGui::Combo("Specular Model", &currentSpecular, specularItems, IM_ARRAYSIZE(specularItems))) {
		Object3dCommon::GetInstance()->SetSpecularModel(currentSpecular);
	}

	static bool enableDirectional = false;
	static bool enablePoint = false;
	static bool enableSpot = false;
	
	bool changed = false;
	changed |= ImGui::Checkbox("Skybox Visible", &isSkyboxVisible_);

	if (ImGui::CollapsingHeader("Post Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
		std::vector<ActivePostEffect>& effects = PostEffect::GetInstance()->GetActiveEffects();

		// 内蔵プリセットの選択
		const char* presetItems[] = { "Select Preset...", "NVD (Night Vision)", "VHS Retro", "Cinematic Bloom", "Digital Glitch", "Color Isolation (Red Focus)", "Luminance Outline", "Depth Outline" };
		static int currentPresetIndex = 0;
		if (ImGui::Combo("Load Built-in Preset", &currentPresetIndex, presetItems, IM_ARRAYSIZE(presetItems))) {
			if (currentPresetIndex > 0) {
				PostEffect::GetInstance()->ApplyBuiltInPreset(presetItems[currentPresetIndex]);
				currentPresetIndex = 0; // Combo表示をリセット
			}
		}

		// カスタム保存・読み込み
		static char presetName[64] = "my_preset";
		ImGui::InputText("Custom Preset Name", presetName, sizeof(presetName));
		if (ImGui::Button("Save Preset File")) {
			PostEffect::GetInstance()->SavePreset(presetName);
		}
		ImGui::SameLine();
		if (ImGui::Button("Load Preset File")) {
			PostEffect::GetInstance()->LoadPreset(presetName);
		}
		ImGui::Separator();

		if (ImGui::Button("Add Post Effect")) {
			effects.push_back(ActivePostEffect());
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear All")) {
			PostEffect::GetInstance()->ClearActiveEffects();
		}

		for (size_t i = 0; i < effects.size(); ++i) {
			ImGui::PushID(static_cast<int>(i));

			ImGui::Separator();
			ImGui::Text("Effect %d", static_cast<int>(i));
			ImGui::SameLine();
			if (ImGui::Button("Remove")) {
				effects.erase(effects.begin() + i);
				--i;
				ImGui::PopID();
				continue;
			}
			ImGui::SameLine();
			if (i > 0) {
				if (ImGui::Button("Up")) {
					std::swap(effects[i], effects[i - 1]);
					ImGui::PopID();
					continue;
				}
				ImGui::SameLine();
			}
			if (i < effects.size() - 1) {
				if (ImGui::Button("Down")) {
					std::swap(effects[i], effects[i + 1]);
					ImGui::PopID();
					continue;
				}
			}

			int currentEffect = static_cast<int>(effects[i].type);
			const char* effectItems[] = { "None", "ColorTint", "Vignette", "BoxFilter", "GaussBlur", "KawaseBlur", "RadialBlur", "Dissolve", "Noise (Random)", "ScanLine", "LightAmp", "LensDistortion", "Chromatic Aberration", "Bloom", "Color Isolation", "Luminance Outline", "Depth Outline" };
			if (ImGui::Combo("Type", &currentEffect, effectItems, IM_ARRAYSIZE(effectItems))) {
				effects[i].type = static_cast<PostEffectType>(currentEffect);
				
				// 各エフェクトの選択時の初期パラメータを設定
				if (effects[i].type == PostEffectType::kScanLine) {
					effects[i].kernelSize = 0;
					effects[i].intensity = 0.2f;
					effects[i].dirX = 600.0f;
					effects[i].dirY = 5.0f;
					effects[i].colorR = 1.0f;
					effects[i].colorG = 1.0f;
					effects[i].colorB = 1.0f;
				} else if (effects[i].type == PostEffectType::kColorTint) {
					effects[i].intensity = 1.0f;
					effects[i].colorR = 0.1f;
					effects[i].colorG = 0.95f;
					effects[i].colorB = 0.2f; // デフォルトで緑色に設定
				} else if (effects[i].type == PostEffectType::kVignette) {
					effects[i].intensity = 1.0f;
					effects[i].dirX = 0.0f;
					effects[i].dirY = 0.0f;
					effects[i].colorR = 0.9f;
					effects[i].colorG = 0.05f;
					effects[i].colorB = 0.05f;
				} else if (effects[i].type == PostEffectType::kBoxFilter) {
					effects[i].kernelSize = 3;
				} else if (effects[i].type == PostEffectType::kGaussBlur) {
					effects[i].kernelSize = 5;
					effects[i].intensity = 2.0f;
				} else if (effects[i].type == PostEffectType::kKawaseBlur) {
					effects[i].kernelSize = 3;
				} else if (effects[i].type == PostEffectType::kRadialBlur) {
					effects[i].kernelSize = 10;
					effects[i].intensity = 0.1f;
				} else if (effects[i].type == PostEffectType::kDissolve) {
					effects[i].dissolveThreshold = 0.5f;
					effects[i].dissolveEdgeWidth = 0.05f;
				} else if (effects[i].type == PostEffectType::kLightAmp) {
					effects[i].intensity = 4.0f;
					effects[i].colorR = 0.15f;
					effects[i].colorG = 1.0f;
					effects[i].colorB = 0.3f;
				} else if (effects[i].type == PostEffectType::kLensDistortion) {
					effects[i].intensity = 0.15f; // デフォルトで緩やかな樽型歪み
				} else if (effects[i].type == PostEffectType::kRandom) {
					effects[i].intensity = 0.15f; // デフォルトノイズ強度
				} else if (effects[i].type == PostEffectType::kChromaticAberration) {
					effects[i].intensity = 0.015f; // デフォルト色収差強度
				} else if (effects[i].type == PostEffectType::kBloom) {
					effects[i].intensity = 2.0f;  // 半径
					effects[i].dirX = 1.5f;       // 強さ
					effects[i].dirY = 0.8f;       // 閾値
				} else if (effects[i].type == PostEffectType::kColorIsolation) {
					effects[i].colorR = 0.9f;     // Target Red
					effects[i].colorG = 0.1f;
					effects[i].colorB = 0.1f;
					effects[i].intensity = 0.45f; // Tolerance
					effects[i].dirX = 0.15f;      // Smoothness
					effects[i].dirY = 1.0f;       // Desaturation (Full monochrome)
				} else if (effects[i].type == PostEffectType::kLuminanceOutline || effects[i].type == PostEffectType::kDepthOutline) {
					effects[i].intensity = 6.0f;  // Outline Multiplier / Sensitivity
					effects[i].colorR = 0.0f;     // Black Outline default
					effects[i].colorG = 0.0f;
					effects[i].colorB = 0.0f;
				}
			}

			if (effects[i].type == PostEffectType::kColorTint) {
				float color[3] = { effects[i].colorR, effects[i].colorG, effects[i].colorB };
				if (ImGui::ColorEdit3("Color Tint", color)) {
					effects[i].colorR = color[0];
					effects[i].colorG = color[1];
					effects[i].colorB = color[2];
				}
			} else if (effects[i].type == PostEffectType::kLightAmp) {
				ImGui::DragFloat("Light Amp Multiplier", &effects[i].intensity, 0.05f, 0.0f, FLT_MAX);
				float nvdColor[3] = { effects[i].colorR, effects[i].colorG, effects[i].colorB };
				if (ImGui::ColorEdit3("NVD Color Tint", nvdColor)) {
					effects[i].colorR = nvdColor[0];
					effects[i].colorG = nvdColor[1];
					effects[i].colorB = nvdColor[2];
				}
			} else if (effects[i].type == PostEffectType::kLensDistortion) {
				ImGui::SliderFloat("Lens Distortion Factor", &effects[i].intensity, -1.0f, 1.0f);
			} else if (effects[i].type == PostEffectType::kChromaticAberration) {
				ImGui::SliderFloat("Chromatic Dispersion", &effects[i].intensity, 0.0f, 0.15f, "%.4f");
			} else if (effects[i].type == PostEffectType::kBloom) {
				ImGui::SliderFloat("Bloom Blur Radius", &effects[i].intensity, 0.0f, 10.0f);
				ImGui::SliderFloat("Bloom Strength", &effects[i].dirX, 0.0f, 5.0f);
				ImGui::SliderFloat("Brightness Threshold", &effects[i].dirY, 0.0f, 1.0f);
			} else if (effects[i].type == PostEffectType::kRandom) {
				ImGui::SliderFloat("Noise Strength", &effects[i].intensity, 0.0f, 1.0f);
			} else if (effects[i].type == PostEffectType::kVignette) {
				ImGui::SliderFloat("Vignette Intensity", &effects[i].intensity, 0.0f, 10.0f);
				ImGui::SliderFloat("Overlay Intensity", &effects[i].dirX, 0.0f, 1.0f);
				float vigColor[3] = { effects[i].colorR, effects[i].colorG, effects[i].colorB };
				if (ImGui::ColorEdit3("Overlay Color", vigColor)) {
					effects[i].colorR = vigColor[0];
					effects[i].colorG = vigColor[1];
					effects[i].colorB = vigColor[2];
				}
			} else if (effects[i].type == PostEffectType::kBoxFilter) {
				ImGui::SliderInt("BoxFilter Kernel Size", &effects[i].kernelSize, 1, 31);
			} else if (effects[i].type == PostEffectType::kGaussBlur) {
				ImGui::SliderInt("GaussBlur Kernel Size", &effects[i].kernelSize, 1, 31);
				ImGui::SliderFloat("GaussBlur Sigma", &effects[i].intensity, 0.1f, 10.0f);
			} else if (effects[i].type == PostEffectType::kKawaseBlur) {
				ImGui::SliderInt("Kawase Blur Passes", &effects[i].kernelSize, 1, 10);
			} else if (effects[i].type == PostEffectType::kRadialBlur) {
				ImGui::SliderInt("RadialBlur Samples", &effects[i].kernelSize, 1, 64);
				ImGui::SliderFloat("RadialBlur Width", &effects[i].intensity, 0.0f, 1.0f);
			} else if (effects[i].type == PostEffectType::kDissolve) {
				ImGui::SliderFloat("Dissolve Threshold", &effects[i].dissolveThreshold, 0.0f, 1.0f);
				ImGui::SliderFloat("Dissolve Edge Width", &effects[i].dissolveEdgeWidth, 0.0f, 0.3f);
				int maskCount = PostEffect::GetInstance()->GetDissolveMaskCount();
				if (maskCount > 0) {
					const char* maskItems[] = { "noise0", "noise1" };
					int maxItems = (maskCount < 2) ? maskCount : 2;
					ImGui::Combo("Mask Texture", &effects[i].dissolveMaskIndex, maskItems, maxItems);
				}
			} else if (effects[i].type == PostEffectType::kScanLine) {
				int mode = effects[i].kernelSize;
				const char* modeItems[] = { "Normal Color", "Grayscale + Tint (NVD Mode)" };
				if (ImGui::Combo("ScanLine Mode", &mode, modeItems, IM_ARRAYSIZE(modeItems))) {
					effects[i].kernelSize = mode;
					if (mode == 1) {
						effects[i].intensity = 0.3f;
						effects[i].dirX = 600.0f;
						effects[i].dirY = 5.0f;
						effects[i].colorR = 0.1f;
						effects[i].colorG = 0.95f;
						effects[i].colorB = 0.2f;
					} else {
						effects[i].colorR = 1.0f;
						effects[i].colorG = 1.0f;
						effects[i].colorB = 1.0f;
					}
				}

				ImGui::SliderFloat("Scanline Intensity", &effects[i].intensity, 0.0f, 1.0f);
				ImGui::SliderFloat("Scanline Density", &effects[i].dirX, 10.0f, 2000.0f);
				ImGui::SliderFloat("Scroll Speed", &effects[i].dirY, -100.0f, 100.0f);

				float color[3] = { effects[i].colorR, effects[i].colorG, effects[i].colorB };
				if (ImGui::ColorEdit3("Scanline Color Tint", color)) {
					effects[i].colorR = color[0];
					effects[i].colorG = color[1];
					effects[i].colorB = color[2];
				}
			} else if (effects[i].type == PostEffectType::kColorIsolation) {
				float isoColor[3] = { effects[i].colorR, effects[i].colorG, effects[i].colorB };
				if (ImGui::ColorEdit3("Target Color to Keep", isoColor)) {
					effects[i].colorR = isoColor[0];
					effects[i].colorG = isoColor[1];
					effects[i].colorB = isoColor[2];
				}
				ImGui::SliderFloat("Color Tolerance", &effects[i].intensity, 0.05f, 1.0f);
				ImGui::SliderFloat("Edge Smoothness", &effects[i].dirX, 0.001f, 0.5f);
				ImGui::SliderFloat("Background Desaturation", &effects[i].dirY, 0.0f, 1.0f);
			} else if (effects[i].type == PostEffectType::kLuminanceOutline || effects[i].type == PostEffectType::kDepthOutline) {
				float outColor[3] = { effects[i].colorR, effects[i].colorG, effects[i].colorB };
				if (ImGui::ColorEdit3("Outline Color", outColor)) {
					effects[i].colorR = outColor[0];
					effects[i].colorG = outColor[1];
					effects[i].colorB = outColor[2];
				}
				ImGui::SliderFloat("Outline Sensitivity", &effects[i].intensity, 0.5f, 20.0f);
			}

			ImGui::PopID();
		}
		ImGui::Separator();
	}

	changed |= ImGui::Checkbox("Enable Directional Light", &enableDirectional);
	changed |= ImGui::Checkbox("Enable Point Light", &enablePoint);
	changed |= ImGui::Checkbox("Enable Spot Light", &enableSpot);

	if (changed) {
		int lightType = 0;
		if (enableDirectional) { lightType |= 1; }
		if (enablePoint) { lightType |= 2; }
		if (enableSpot) { lightType |= 4; }
		
		Object3dCommon::GetInstance()->SetLightType(lightType);
	}

	if (ImGui::CollapsingHeader("Directional Light")) {
		DirectionalLight* lightData = Object3dCommon::GetInstance()->GetDirectionalLightData();
		ImGui::DragFloat3("Direction", &lightData->direction.x, 0.01f);
		lightData->direction = Normalize(lightData->direction);
		ImGui::ColorEdit4("Color", &lightData->color.x);
		ImGui::DragFloat("Intensity", &lightData->intensity, 0.01f);
	}

	if (ImGui::CollapsingHeader("Point Light")) {
		PointLight* pointData = Object3dCommon::GetInstance()->GetPointLightData();
		ImGui::DragFloat3("Position", &pointData->position.x, 0.1f);
		ImGui::ColorEdit4("Color", &pointData->color.x);
		ImGui::DragFloat("Intensity", &pointData->intensity, 0.01f);
		ImGui::DragFloat("Radius", &pointData->radius, 0.1f);
		ImGui::DragFloat("Decay", &pointData->decay, 0.01f);
	}

	if (ImGui::CollapsingHeader("Spot Light")) {
		SpotLight* spotData = Object3dCommon::GetInstance()->GetSpotLightData();
		ImGui::DragFloat3("Position", &spotData->position.x, 0.1f);
		ImGui::DragFloat3("Direction", &spotData->direction.x, 0.01f);
		spotData->direction = Normalize(spotData->direction);
		ImGui::ColorEdit4("Color", &spotData->color.x);
		ImGui::DragFloat("Intensity", &spotData->intensity, 0.01f);
		ImGui::DragFloat("Distance", &spotData->distance, 0.1f);
		ImGui::DragFloat("Decay", &spotData->decay, 0.01f);
		ImGui::DragFloat("Cos Angle", &spotData->cosAngle, 0.01f);
		ImGui::DragFloat("Cos Falloff Start", &spotData->cosFalloffStart, 0.01f);
	}
	
	if (sphereObject) {
		Vector3 scale = sphereObject->GetScale();
		if (ImGui::DragFloat3("Sphere Scale", &scale.x, 0.01f)) {
			sphereObject->SetScale(scale);
		}
	}

	if (sphereObject && sphereObject->GetModel()) {
		float shininess = sphereObject->GetModel()->GetShininess();
		if (ImGui::DragFloat("Shininess", &shininess, 1.0f, 1.0f, 256.0f)) {
			sphereObject->GetModel()->SetShininess(shininess);
		}

		float envCoef = sphereObject->GetModel()->GetEnvironmentCoefficient();
		if (ImGui::DragFloat("Env Coefficient", &envCoef, 0.01f, 0.0f, 1.0f)) {
			sphereObject->GetModel()->SetEnvironmentCoefficient(envCoef);
		}
	}

	if (ImGui::CollapsingHeader("Model Selection")) {
		static int currentModelIndex = 0;
		if (ImGui::RadioButton("Sphere", &currentModelIndex, 0)) {
			ModelManager::GetInstance()->LoadModel("models/sphere.obj");
			sphereObject->SetModel("models/sphere.obj");
			sphereObject->SetRotate({ 0.0f, 0.0f, 0.0f });
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Plane", &currentModelIndex, 1)) {
			ModelManager::GetInstance()->LoadModel("models/plane.gltf");
			sphereObject->SetModel("models/plane.gltf");
			sphereObject->SetRotate({ 0.0f, 3.141592f, 0.0f });
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("PlaneOBJ", &currentModelIndex, 2)) {
			ModelManager::GetInstance()->LoadModel("models/plane.obj");
			sphereObject->SetModel("models/plane.obj");
			sphereObject->SetRotate({ 0.0f, 3.141592f, 0.0f });
		}
		Vector3 modelPos = sphereObject->GetTranslate();
		if (ImGui::DragFloat3("Model Translate", &modelPos.x, 0.01f)) {
			sphereObject->SetTranslate(modelPos);
		}

		if (ImGui::CollapsingHeader("Ray-Mesh Collision Debug")) {
			ImGui::DragFloat3("Ray Origin", &debugRay_.origin.x, 0.1f);
			if (ImGui::DragFloat3("Ray Direction", &debugRay_.direction.x, 0.01f)) {
				debugRay_.direction = Normalize(debugRay_.direction);
			}
			ImGui::DragFloat("Ray MaxDistance", &debugRay_.maxDistance, 0.5f);

			if (sphereObject && sphereObject->GetModel()) {
				Matrix4x4 wMat = MakeAffineMatrix(sphereObject->GetScale(), sphereObject->GetRotate(), sphereObject->GetTranslate());
				debugHit_ = {};
				bool isHit = sphereObject->GetModel()->IntersectRay(debugRay_, wMat, &debugHit_);
				ImGui::Text("Raycast Hit: %s", isHit ? "TRUE (HIT!)" : "FALSE");
				if (isHit) {
					ImGui::Text("Hit Distance: %.3f", debugHit_.distance);
					ImGui::Text("Hit Point: (%.2f, %.2f, %.2f)", debugHit_.point.x, debugHit_.point.y, debugHit_.point.z);
					ImGui::Text("Hit Normal: (%.2f, %.2f, %.2f)", debugHit_.normal.x, debugHit_.normal.y, debugHit_.normal.z);
					ImGui::Text("Hit Node: %s", debugHit_.nodeName.c_str());
					ImGui::Text("Hit Tri Index: %u", debugHit_.triangleIndex);
				}
			}
		}
	}

	ImGui::End();

	GPUParticleManager::GetInstance()->DrawImGui();
#endif
	
	if (camera) {
		Object3dCommon::GetInstance()->SetCameraPosition(camera->GetTranslate());
	}
}

void DebugScene::Draw() {
	// 0. Skybox描画 (最初に描画。深度書き込みなしで最遠方に配置)
	if (isSkyboxVisible_ && skybox_) {
		SkyboxCommon::GetInstance()->SetupCommonState();
		skybox_->Draw();
	}

	// 1. 3D描画
	Object3dCommon::GetInstance()->SetupCommonState();
	sphereObject->Draw();
	terrainObject->Draw();

	// スケルトンのデバッグ描画テスト
	sphereObject->DebugDrawSkeleton({ 1.0f, 1.0f, 1.0f, 1.0f });

	// Ray-Mesh デバッグ描画（レイ線 & ヒット時の法線ベクトル）
	Camera* mainCamera = CameraManager::GetInstance()->GetActiveCamera();
	Vector3 rayEnd = Add(debugRay_.origin, Multiply(debugRay_.maxDistance, debugRay_.direction));
	Vector4 rayColor = debugHit_.hit ? Vector4{ 1.0f, 0.0f, 0.0f, 1.0f } : Vector4{ 0.0f, 1.0f, 1.0f, 1.0f };
	PrimitiveModel::GetInstance()->DrawLine3D(debugRay_.origin, rayEnd, rayColor, mainCamera);

	if (debugHit_.hit) {
		Vector3 normalEnd = Add(debugHit_.point, Multiply(1.5f, debugHit_.normal));
		PrimitiveModel::GetInstance()->DrawLine3D(debugHit_.point, normalEnd, { 1.0f, 1.0f, 0.0f, 1.0f }, mainCamera);
	}

	// エフェクト（プリミティブ）描画 (加算合成で光る柱のように描画)
	uint32_t particleTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");
	uint32_t whiteTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");
	Vector3 cylScale = { 2.0f, 8.0f, 2.0f }; // 太さ2, 高さ8
	Vector3 cylRotate = { 0.0f, 0.0f, 0.0f };
	Vector3 cylTranslate = { 5.0f, 0.0f, 0.0f }; // 球の右側に配置
	Vector4 cylColor = { 1.0f, 0.3f, 0.3f, 0.8f }; // 半透明の赤
	PrimitiveModel::GetInstance()->DrawCylinder(cylScale, cylRotate, cylTranslate, cylColor, whiteTex, CameraManager::GetInstance()->GetActiveCamera(), BlendMode::kAdd);

	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (camera) {
		GPUParticleManager::GetInstance()->SetTexture(particleTex);
		GPUParticleManager::GetInstance()->Draw(camera->GetViewProjectionMatrix(), camera->GetWorldMatrix());
	}
	
	GPUParticleManager::GetInstance()->Update();
}

void DebugScene::Finalize() {
	EffectManager::GetInstance()->Finalize();
}
