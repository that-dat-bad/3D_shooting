#include "AmbientWeatherSystem.h"
#include "../../engine/Graphics/Particle/EffectGpuPresetBridge.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Graphics/Particle/GPUParticleTypes.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

namespace {
	constexpr float kCrystalMinLife = 9.5f;
	constexpr float kCrystalMaxLife = 14.5f;
	constexpr float kCrystalAvgLife = 12.0f;
	constexpr float kFogMinLife = 10.0f;
	constexpr float kFogMaxLife = 16.0f;
	constexpr float kFogAvgLife = 13.0f;
}

const char* AmbientWeatherSystem::GetPresetName(WeatherPreset preset) {
	switch (preset) {
	case WeatherPreset::Off:           return "Off";
	case WeatherPreset::IceDust:       return "Ice Dust (Clear Polar)";
	case WeatherPreset::PolarBlizzard: return "Polar Blizzard";
	case WeatherPreset::ArcticStorm:   return "Arctic Storm (Dense Wake Demo)";
	default:                           return "Off";
	}
}

AmbientWeatherSystem::PresetSpec AmbientWeatherSystem::GetSpec(WeatherPreset preset) {
	switch (preset) {
	case WeatherPreset::IceDust:
		return { 2400u, 360u, { -3.5f, -4.5f, -2.0f }, 95.0f, 0.65f, 0.075f };
	case WeatherPreset::PolarBlizzard:
		return { 5600u, 880u, { -12.0f, -9.5f, -6.0f }, 88.0f, 0.82f, 0.115f };
	case WeatherPreset::ArcticStorm:
		return { 9600u, 1400u, { -18.0f, -12.5f, -9.0f }, 75.0f, 0.92f, 0.145f };
	case WeatherPreset::Off:
	default:
		return { 0u, 0u, { 0.0f, 0.0f, 0.0f }, 80.0f, 0.0f, 0.0f };
	}
}

void AmbientWeatherSystem::Reset(WeatherPreset preset) {
	preset_ = preset;
	needsInitialSeed_ = (preset_ != WeatherPreset::Off);
	crystalEmitCarry_ = 0.0f;
	fogEmitCarry_ = 0.0f;
}

void AmbientWeatherSystem::SetPreset(WeatherPreset preset) {
	if (preset_ != preset) {
		preset_ = preset;
		needsInitialSeed_ = (preset_ != WeatherPreset::Off);
		crystalEmitCarry_ = 0.0f;
		fogEmitCarry_ = 0.0f;
	}
}

void AmbientWeatherSystem::SubmitWakeSource(uint32_t slot, const WakeSource& source) const {
	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	GpuWakeField field{};
	if (!source.active) {
		field.active = 0u;
		gpu->SetWakeField(slot, field);
		return;
	}

	const float speed = Length(source.velocity);
	const float speedFactor = std::clamp(speed / 120.0f, 0.25f, 2.2f);
	const float gFactor = std::clamp(std::fabs(source.currentG) * 0.45f, 0.6f, 3.6f);

	field.position = source.position;
	field.halfWingspan = (std::max)(1.5f, source.halfWingspan);
	field.forward = (Length(source.forward) > 1e-4f) ? Normalize(source.forward) : Vector3{ 0.0f, 0.0f, 1.0f };
	field.right = (Length(source.right) > 1e-4f) ? Normalize(source.right) : Vector3{ 1.0f, 0.0f, 0.0f };
	field.velocity = source.velocity;
	// 高G旋回・高速飛行ほど翼端渦の循環強度が劇的に増大する
	field.vortexStrength = 95.0f * speedFactor * gFactor;
	// スロットル＋アフターバーナー出力に応じてノズル後流のブラストが増大
	field.exhaustThrust = (65.0f * std::clamp(source.throttle, 0.0f, 1.2f)) +
	                      (145.0f * std::clamp(source.afterburnerIntensity, 0.0f, 1.0f));
	field.active = 1u;

	gpu->SetWakeField(slot, field);
}

void AmbientWeatherSystem::Update(float dt, const Vector3& cameraPosition) {
	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	if (preset_ == WeatherPreset::Off) {
		GpuAmbientVolume offVol{};
		offVol.active = 0u;
		gpu->SetAmbientVolume(offVol);
		return;
	}

	const PresetSpec spec = GetSpec(preset_);
	const float effectiveHalfExtent = (halfExtent_ > 5.0f) ? halfExtent_ : spec.halfExtent;
	const Vector3 wind = useCustomWind_ ? customWind_ : spec.windVelocity;

	GpuAmbientVolume vol{};
	vol.center = cameraPosition;
	vol.halfExtent = effectiveHalfExtent;
	vol.windVelocity = wind;
	vol.active = 1u;
	gpu->SetAmbientVolume(vol);

	if (dt <= 0.0f) {
		return;
	}
	const float clampedDt = std::clamp(dt, 0.0f, 0.1f);

	// 初回シード：寿命を [1.2s, maxLife] に分散させて一括展開（定常補充時の総数スパイク＆一斉消滅を防止）
	if (needsInitialSeed_) {
		PresetSpec effectiveSpec = spec;
		effectiveSpec.halfExtent = effectiveHalfExtent;
		effectiveSpec.windVelocity = wind;
		EmitBatch(cameraPosition, effectiveSpec, spec.targetCrystalCount, spec.targetFogCount, true);
		needsInitialSeed_ = false;
		return;
	}

	// 定常補充：寿命消滅分と同率で穏やかに補充し総数を安定維持
	crystalEmitCarry_ += (static_cast<float>(spec.targetCrystalCount) / kCrystalAvgLife) * clampedDt;
	fogEmitCarry_ += (static_cast<float>(spec.targetFogCount) / kFogAvgLife) * clampedDt;

	const uint32_t emitCrystals = static_cast<uint32_t>(crystalEmitCarry_);
	const uint32_t emitFog = static_cast<uint32_t>(fogEmitCarry_);
	crystalEmitCarry_ -= static_cast<float>(emitCrystals);
	fogEmitCarry_ -= static_cast<float>(emitFog);

	if (emitCrystals > 0u || emitFog > 0u) {
		PresetSpec effectiveSpec = spec;
		effectiveSpec.halfExtent = effectiveHalfExtent;
		effectiveSpec.windVelocity = wind;
		EmitBatch(cameraPosition, effectiveSpec, emitCrystals, emitFog);
	}
}

void AmbientWeatherSystem::EmitBatch(
	const Vector3& center,
	const PresetSpec& spec,
	uint32_t crystalCount,
	uint32_t fogCount,
	bool staggerInitialLife) {

	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	const EffectPreset* weatherPreset = EffectManager::GetInstance()
		? EffectManager::GetInstance()->FindPreset("polar_blizzard")
		: nullptr;
	const EffectParticleLayer* crystalLayer = nullptr;
	const EffectParticleLayer* fogLayer = nullptr;
	if (weatherPreset) {
		for (const auto& l : weatherPreset->layers) {
			if (l.useGpu && l.gpuRich.useRichGpu) {
				if (l.layerName == "SnowCrystals") { crystalLayer = &l; }
				else if (l.layerName == "PolarFog") { fogLayer = &l; }
			}
		}
	}

	// 1. 氷晶・吹雪スノー粒子（カメラ追従ラップ＋ウェイク渦で劇的に巻かれる）
	if (crystalCount > 0u) {
		if (crystalLayer && crystalLayer->emitCount > 0u) {
			EffectGpuPresetBridge::EmitOverride ov{};
			ov.countMultiplier = static_cast<float>(crystalCount) / static_cast<float>(crystalLayer->emitCount);
			ov.alphaMultiplier = spec.crystalAlpha / 0.82f;
			ov.overrideRandomPositionRange = true;
			ov.randomPositionRange = spec.halfExtent;
			ov.overrideVelocityRange = true;
			ov.minVelocity = Subtract(spec.windVelocity, { 2.0f, 1.5f, 2.0f });
			ov.maxVelocity = Add(spec.windVelocity, { 2.0f, 1.5f, 2.0f });
			ov.overrideLifeTime = true;
			ov.minLifeTime = staggerInitialLife ? 1.2f : kCrystalMinLife;
			ov.maxLifeTime = kCrystalMaxLife;
			ov.overrideFlagsMask = true;
			ov.additionalFlags = GPUParticleFlags::kCameraWrap | GPUParticleFlags::kWakeInteract | GPUParticleFlags::kWorldUnitsPerSec;
			EffectGpuPresetBridge::EmitLayer(*crystalLayer, center, ov);
		} else {
			GpuParticleEmitDesc snow{};
			snow.origin = center;
			snow.randomPositionRange = spec.halfExtent;
			snow.minVelocity = Subtract(spec.windVelocity, { 2.0f, 1.5f, 2.0f });
			snow.maxVelocity = Add(spec.windVelocity, { 2.0f, 1.5f, 2.0f });
			snow.minLifeTime = staggerInitialLife ? 1.2f : kCrystalMinLife;
			snow.maxLifeTime = kCrystalMaxLife;
			snow.minScale = 0.22f;
			snow.maxScale = 0.62f;
			snow.endScale = 0.35f;
			snow.scaleEasing = 1.0f;
			snow.minRotationSpeed = -2.2f;
			snow.maxRotationSpeed = 2.2f;
			snow.minColor = { 0.80f, 0.91f, 1.00f, spec.crystalAlpha * 0.72f };
			snow.maxColor = { 1.00f, 1.00f, 1.00f, spec.crystalAlpha };
			snow.count = crystalCount;
			snow.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
			             GPUParticleFlags::kWakeInteract | GPUParticleFlags::kCameraWrap |
			             GPUParticleFlags::kWorldUnitsPerSec;
			snow.shape = GPUParticleShape::SnowCrystal;
			snow.drag = 1.45f; // ウェイクで巻き上げられた後、滑らかに吹雪風速へ戻る
			snow.curlStrength = 4.2f;
			snow.curlFrequency = 0.075f;
			snow.emissiveIntensity = 1.25f;
			snow.wakeSensitivity = 1.45f;
			gpu->EnqueueRichEmit(snow);
		}
	}

	// 2. 極地凍結霧・吹雪の雲筋（ソフトパーティクル＋カールノイズ＋ウェイク連動）
	if (fogCount > 0u) {
		if (fogLayer && fogLayer->emitCount > 0u) {
			EffectGpuPresetBridge::EmitOverride ov{};
			ov.countMultiplier = static_cast<float>(fogCount) / static_cast<float>(fogLayer->emitCount);
			ov.alphaMultiplier = spec.fogAlpha / 0.115f;
			ov.overrideRandomPositionRange = true;
			ov.randomPositionRange = spec.halfExtent;
			ov.overrideVelocityRange = true;
			ov.minVelocity = Subtract(spec.windVelocity, { 1.5f, 0.8f, 1.5f });
			ov.maxVelocity = Add(spec.windVelocity, { 1.5f, 0.8f, 1.5f });
			ov.overrideLifeTime = true;
			ov.minLifeTime = staggerInitialLife ? 1.5f : kFogMinLife;
			ov.maxLifeTime = kFogMaxLife;
			ov.overrideFlagsMask = true;
			ov.additionalFlags = GPUParticleFlags::kCameraWrap | GPUParticleFlags::kWakeInteract | GPUParticleFlags::kWorldUnitsPerSec;
			EffectGpuPresetBridge::EmitLayer(*fogLayer, center, ov);
		} else {
			GpuParticleEmitDesc fog{};
			fog.origin = center;
			fog.randomPositionRange = spec.halfExtent;
			fog.minVelocity = Subtract(spec.windVelocity, { 1.5f, 0.8f, 1.5f });
			fog.maxVelocity = Add(spec.windVelocity, { 1.5f, 0.8f, 1.5f });
			fog.minLifeTime = staggerInitialLife ? 1.5f : kFogMinLife;
			fog.maxLifeTime = kFogMaxLife;
			fog.minScale = 5.5f;
			fog.maxScale = 12.5f;
			fog.endScale = 14.0f;
			fog.scaleEasing = 0.75f;
			fog.minRotationSpeed = -0.35f;
			fog.maxRotationSpeed = 0.35f;
			fog.minColor = { 0.76f, 0.85f, 0.95f, spec.fogAlpha * 0.65f };
			fog.maxColor = { 0.92f, 0.96f, 1.00f, spec.fogAlpha };
			fog.count = fogCount;
			fog.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
			            GPUParticleFlags::kWakeInteract | GPUParticleFlags::kCameraWrap |
			            GPUParticleFlags::kSoftParticle | GPUParticleFlags::kWorldUnitsPerSec;
			fog.shape = GPUParticleShape::SmokeCloud;
			fog.drag = 1.25f;
			fog.curlStrength = 3.2f;
			fog.curlFrequency = 0.045f;
			fog.softDistance = 8.0f;
			fog.wakeSensitivity = 1.20f;
			gpu->EnqueueRichEmit(fog);
		}
	}
}
