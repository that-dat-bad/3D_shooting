#include "ParticleContestShowcase.h"

#ifdef USE_IMGUI

#include "GpuExplosionEmitter.h"
#include "../../engine/Graphics/Camera/Camera.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../../external/imgui/imgui.h"
#include <algorithm>
#include <cstdio>

using namespace MyMath;

void ParticleContestShowcase::Initialize(Camera* camera) {
	countermeasures_.Reset(999);
	aeroVapor_.Reset();
	ambientWeather_.Reset(AmbientWeatherSystem::WeatherPreset::Off);
	ambientWeather_.SetHalfExtent(68.0f);
	virtualJet_.Initialize(camera);

	// GPU統計リードバックをショーケース中は自動有効化
	if (GPUParticleManager* gpu = GPUParticleManager::GetInstance(); gpu && gpu->IsInitialized()) {
		prevReadbackEnabled_ = gpu->IsReadbackDebugEnabled();
		gpu->SetReadbackDebugEnabled(true);
		gpu->SetDefaultFloorY(0.0f);
		gpu->ClearWakeFields();
	}

	// EffectEditorScene の EmitDestroyEffect からもGPU多段階爆発を発火させる
	if (EffectManager* em = EffectManager::GetInstance()) {
		em->SetDestroyEffectHook([](const Vector3& pos) {
			GpuExplosionEmitter::EmitMultiStage(pos, 0.0f, 1.0f, GpuExplosionEmitter::ScalePreset::Tactical);
		});
	}

	grandFinaleActive_ = false;
	grandFinaleTimer_ = 0.0f;
	grandFinaleStage_ = 0;
	bloomEnabled_ = true;
	SyncBloomPostEffect();
}

void ParticleContestShowcase::Finalize() {
	if (EffectManager* em = EffectManager::GetInstance()) {
		em->SetDestroyEffectHook(nullptr);
	}
	if (GPUParticleManager* gpu = GPUParticleManager::GetInstance(); gpu && gpu->IsInitialized()) {
		gpu->ClearAllParticles();
		gpu->ClearWakeFields();
		GpuAmbientVolume offVol{};
		offVol.active = 0u;
		gpu->SetAmbientVolume(offVol);
		gpu->SetReadbackDebugEnabled(prevReadbackEnabled_);
	}
	if (Object3dCommon* objCommon = Object3dCommon::GetInstance()) {
		if (PointLight* pLight = objCommon->GetPointLightData()) {
			pLight->intensity = 0.0f;
		}
	}
	if (PostEffect* pe = PostEffect::GetInstance()) {
		pe->ClearActiveEffects();
	}
	virtualJet_.Finalize();
}

void ParticleContestShowcase::OnParticlesCleared() {
	ambientWeather_.RequestReseed();
}

void ParticleContestShowcase::Update(float dt, Camera* camera, const Vector3& emitOrigin) {
	const float clampedDt = std::clamp(dt, 0.0f, 0.1f);

	if (grandFinaleActive_) {
		UpdateGrandFinaleSequence(clampedDt, emitOrigin);
	}

	// 爆発ポイントライト閃光の滑らかな減衰（StageSceneと同等）
	DecayExplosionPointLight(clampedDt);

	// 仮想フライバイ機更新＆ウェイク場送信
	virtualJet_.Update(clampedDt, emitOrigin, ambientWeather_, aeroVapor_, countermeasures_);

	// カウンターメジャー燃焼更新（仮想機がONなら機体から、OFFなら原点上方から射出）
	CountermeasureSystem::AircraftEmitterState cmState{};
	if (virtualJet_.IsEnabled()) {
		cmState.position = virtualJet_.GetPosition();
		cmState.velocity = virtualJet_.GetVelocity();
		cmState.forward = virtualJet_.GetForward();
		cmState.right = virtualJet_.GetRight();
		cmState.up = virtualJet_.GetUp();
	} else {
		cmState.position = { emitOrigin.x, emitOrigin.y + 8.0f, emitOrigin.z };
		cmState.velocity = { 0.0f, 0.0f, 38.0f };
		cmState.forward = { 0.0f, 0.0f, 1.0f };
		cmState.right = { 1.0f, 0.0f, 0.0f };
		cmState.up = { 0.0f, 1.0f, 0.0f };
	}
	cmState.floorY = emitOrigin.y;
	countermeasures_.Update(clampedDt, cmState);

	// 環境吹雪・氷晶システム更新
	const Vector3 camPos = camera ? camera->GetTranslate() : Vector3{ 0.0f, 5.0f, -18.0f };
	ambientWeather_.Update(clampedDt, camPos);

	SyncBloomPostEffect();
	DrawImGuiPanel(camera, emitOrigin);
}

void ParticleContestShowcase::Draw(Camera* camera) {
	virtualJet_.Draw(camera);
}

void ParticleContestShowcase::DecayExplosionPointLight(float dt) const {
	if (Object3dCommon* objCommon = Object3dCommon::GetInstance()) {
		if (PointLight* pLight = objCommon->GetPointLightData(); pLight && pLight->intensity > 0.0f) {
			pLight->intensity = (std::max)(0.0f, pLight->intensity - 240.0f * dt);
		}
	}
}

void ParticleContestShowcase::UpdateGrandFinaleSequence(float dt, const Vector3& origin) {
	grandFinaleTimer_ += dt;

	if (grandFinaleStage_ == 0) {
		// Stage 0: 極地ストーム起動＋仮想機バンク旋回開始
		ambientWeather_.SetPreset(AmbientWeatherSystem::WeatherPreset::ArcticStorm);
		virtualJet_.SetEnabled(true);
		virtualJet_.SetFlightMode(VirtualFlybyJet::FlightMode::BankedOrbit);
		virtualJet_.SetWingVaporEnabled(true);
		bloomEnabled_ = true;
		grandFinaleStage_ = 1;
	} else if (grandFinaleStage_ == 1 && grandFinaleTimer_ >= 0.7f) {
		// Stage 1: チャフ＆フレア斉射
		countermeasures_.TriggerSalvo(true);
		grandFinaleStage_ = 2;
	} else if (grandFinaleStage_ == 2 && grandFinaleTimer_ >= 2.0f) {
		// Stage 2: マッハ1ソニックブーム衝撃波
		aeroVapor_.TriggerSonicBoom(
			virtualJet_.GetPosition(),
			virtualJet_.GetForward(),
			virtualJet_.GetVelocity(),
			1.25f);
		grandFinaleStage_ = 3;
	} else if (grandFinaleStage_ == 3 && grandFinaleTimer_ >= 3.2f) {
		// Stage 3: 原点に18,000粒子ウルトラリッチ多段階爆発
		GpuExplosionEmitter::EmitMultiStage(
			{ origin.x, origin.y + 2.5f, origin.z },
			origin.y,
			1.25f,
			GpuExplosionEmitter::ScalePreset::MassiveShowcase);
		grandFinaleStage_ = 4;
	} else if (grandFinaleStage_ == 4 && grandFinaleTimer_ >= 4.4f) {
		// Stage 4: 追い打ちチャフ＆フレア斉射
		countermeasures_.TriggerSalvo(true);
		grandFinaleActive_ = false;
	}
}

void ParticleContestShowcase::SyncBloomPostEffect() const {
	PostEffect* pe = PostEffect::GetInstance();
	if (!pe) {
		return;
	}
	pe->ClearActiveEffects();
	if (bloomEnabled_) {
		ActivePostEffect bloom{};
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = bloomRadius_;
		bloom.dirX = bloomStrength_;
		bloom.dirY = bloomThreshold_;
		pe->AddActiveEffect(bloom);
	}
}

void ParticleContestShowcase::DrawImGuiPanel(Camera* camera, const Vector3& emitOrigin) {
	ImGui::Begin("Particle Contest Showcase [GPU 65k]");

	ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "/// GPU 65,536-PARTICLE CONTEST SHOWCASE ///");
	ImGui::TextWrapped("Shader Tech: 2-Bucket ExecuteIndirect (Alpha+HDR) | 3D Curl Noise | Blackbody Ramp | Stretched Billboard | Depth Soft Particles | Floor Bounce | Aircraft Wake Vortex Field");
	ImGui::Separator();

	// 1. Live GPU Particle Telemetry
	if (GPUParticleManager* gpu = GPUParticleManager::GetInstance(); gpu && gpu->IsInitialized()) {
		const uint32_t totalAlive = gpu->GetDebugAliveCount();
		const uint32_t alphaAlive = gpu->GetDebugAlphaAliveCount();
		const uint32_t addAlive = gpu->GetDebugAdditiveAliveCount();
		const uint32_t maxP = gpu->GetMaxParticles();
		const float util = static_cast<float>(totalAlive) / static_cast<float>(maxP);

		char overlay[96];
		snprintf(overlay, sizeof(overlay), "%u / %u (%.1f%%)", totalAlive, maxP, util * 100.0f);
		ImGui::ProgressBar(util, ImVec2(-1.0f, 20.0f), overlay);
		ImGui::Text("Alpha Bucket (Smoke/Vapor/Snow/Chaff): %u", alphaAlive);
		ImGui::Text("Additive HDR Bucket (Fire/Sparks/Shock): %u | Emitted/Frame: %u",
			addAlive, gpu->GetLastFrameEmitCount());
		if (const uint32_t dropped = gpu->GetLastFrameDroppedEmitRequests(); dropped > 0u) {
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Warning: Dropped Emit Requests: %u", dropped);
		}
	}

	ImGui::Separator();

	// 2. ワンクリック・グランドフィナーレ＆カメラプリセット
	if (ImGui::Button("PLAY ALL-IN-ONE GRAND FINALE SHOWCASE", ImVec2(-1.0f, 34.0f))) {
		grandFinaleActive_ = true;
		grandFinaleTimer_ = 0.0f;
		grandFinaleStage_ = 0;
		if (camera) {
			camera->SetTranslate({ emitOrigin.x, emitOrigin.y + 18.0f, emitOrigin.z - 56.0f });
			camera->SetRotate({ 0.22f, 0.0f, 0.0f });
		}
	}

	if (camera) {
		if (ImGui::Button("Cam: Overview")) {
			camera->SetTranslate({ emitOrigin.x, emitOrigin.y + 16.0f, emitOrigin.z - 48.0f });
			camera->SetRotate({ 0.22f, 0.0f, 0.0f });
		}
		ImGui::SameLine();
		if (ImGui::Button("Cam: Ground Bounce")) {
			camera->SetTranslate({ emitOrigin.x + 22.0f, emitOrigin.y + 4.5f, emitOrigin.z - 28.0f });
			camera->SetRotate({ 0.06f, -0.62f, 0.0f });
		}
		ImGui::SameLine();
		if (ImGui::Button("Cam: Wake Vortex")) {
			camera->SetTranslate({ emitOrigin.x, emitOrigin.y + 28.0f, emitOrigin.z - 42.0f });
			camera->SetRotate({ 0.45f, 0.0f, 0.0f });
		}
	}

	ImGui::Separator();

	// 3. 個別エフェクト実演トリガー
	if (ImGui::CollapsingHeader("1. Chaff & Flare Deployment", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::Button("Deploy Chaff & Flare Salvo", ImVec2(-1.0f, 28.0f))) {
			countermeasures_.TriggerSalvo(true);
		}
		ImGui::Text("Active Burning Flares: %zu", countermeasures_.GetActiveFlareCount());
	}

	if (ImGui::CollapsingHeader("2. Vapor Cone, High-G Wing Cloud & Sonic Boom", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::Button("Trigger Mach 1.0 Sonic Boom Shockwave", ImVec2(-1.0f, 28.0f))) {
			if (virtualJet_.IsEnabled()) {
				aeroVapor_.TriggerSonicBoom(
					virtualJet_.GetPosition(),
					virtualJet_.GetForward(),
					virtualJet_.GetVelocity(),
					1.2f);
			} else {
				aeroVapor_.TriggerSonicBoom(
					{ emitOrigin.x, emitOrigin.y + 6.0f, emitOrigin.z },
					{ 0.0f, 0.0f, 1.0f },
					{ 0.0f, 0.0f, 280.0f },
					1.2f);
			}
		}
		ImGui::Checkbox("Continuous Wing/Cone Vapor on Flyby Jet", &virtualJet_.WingVaporEnabledRef());
	}

	if (ImGui::CollapsingHeader("3. GPU 65k Multi-Stage Explosion", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* presetItems[] = { "Tactical (~4,800 particles)", "Massive Showcase (~18,000 particles)" };
		ImGui::Combo("Density Preset", &explosionPresetIndex_, presetItems, 2);
		ImGui::SliderFloat("Explosion Scale", &explosionScale_, 0.5f, 2.2f, "%.2fx");
		if (ImGui::Button("DETONATE GPU MULTI-STAGE EXPLOSION", ImVec2(-1.0f, 30.0f))) {
			const auto preset = (explosionPresetIndex_ == 1)
				? GpuExplosionEmitter::ScalePreset::MassiveShowcase
				: GpuExplosionEmitter::ScalePreset::Tactical;
			GpuExplosionEmitter::EmitMultiStage(
				{ emitOrigin.x, emitOrigin.y + 2.5f * explosionScale_, emitOrigin.z },
				emitOrigin.y,
				explosionScale_,
				preset);
		}
	}

	if (ImGui::CollapsingHeader("4. Polar Blizzard + Aircraft Wake Field Demo", ImGuiTreeNodeFlags_DefaultOpen)) {
		int weatherIdx = static_cast<int>(ambientWeather_.GetPreset());
		const char* weatherNames[] = {
			AmbientWeatherSystem::GetPresetName(AmbientWeatherSystem::WeatherPreset::Off),
			AmbientWeatherSystem::GetPresetName(AmbientWeatherSystem::WeatherPreset::IceDust),
			AmbientWeatherSystem::GetPresetName(AmbientWeatherSystem::WeatherPreset::PolarBlizzard),
			AmbientWeatherSystem::GetPresetName(AmbientWeatherSystem::WeatherPreset::ArcticStorm)
		};
		if (ImGui::Combo("Polar Weather", &weatherIdx, weatherNames, 4)) {
			ambientWeather_.SetPreset(static_cast<AmbientWeatherSystem::WeatherPreset>(weatherIdx));
		}

		ImGui::Checkbox("Enable Virtual Flyby Jet (Wake Vortex Generator)", &virtualJet_.EnabledRef());
		if (virtualJet_.IsEnabled()) {
			int modeIdx = static_cast<int>(virtualJet_.GetFlightMode());
			const char* modeLabels[] = { "High-G Banked Orbit", "Mach 1.0 Low-Pass Flyby" };
			if (ImGui::Combo("Flight Pattern", &modeIdx, modeLabels, 2)) {
				virtualJet_.SetFlightMode(static_cast<VirtualFlybyJet::FlightMode>(modeIdx));
			}
			if (virtualJet_.GetFlightMode() == VirtualFlybyJet::FlightMode::BankedOrbit) {
				ImGui::SliderFloat("Orbit Radius (m)", &virtualJet_.OrbitRadiusRef(), 14.0f, 60.0f, "%.1f m");
				ImGui::SliderFloat("Jet Speed (m/s)", &virtualJet_.SpeedRef(), 60.0f, 260.0f, "%.0f m/s");
			}
			ImGui::SliderFloat("Altitude (m)", &virtualJet_.AltitudeRef(), 4.0f, 25.0f, "%.1f m");
			ImGui::Checkbox("Auto-Deploy Chaff & Flares", &virtualJet_.AutoFlaresRef());
		}
	}

	if (ImGui::CollapsingHeader("5. HDR Bloom & Utilities")) {
		ImGui::Checkbox("Enable Bloom Post-Effect", &bloomEnabled_);
		if (bloomEnabled_) {
			ImGui::SliderFloat("Bloom Radius", &bloomRadius_, 0.5f, 5.0f, "%.2f");
			ImGui::SliderFloat("Bloom Strength", &bloomStrength_, 0.2f, 3.0f, "%.2f");
			ImGui::SliderFloat("Luminance Threshold", &bloomThreshold_, 0.35f, 0.95f, "%.2f");
		}
		if (ImGui::Button("Clear All GPU Particles", ImVec2(-1.0f, 24.0f))) {
			if (GPUParticleManager* gpu = GPUParticleManager::GetInstance(); gpu && gpu->IsInitialized()) {
				gpu->ClearAllParticles();
			}
			countermeasures_.ClearActiveDecoys();
			ambientWeather_.RequestReseed();
		}
	}

	ImGui::End();
}

#endif // USE_IMGUI
