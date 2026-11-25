#include "GunCombatEffectEmitter.h"
#include "../../engine/Graphics/Particle/EffectGpuPresetBridge.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Graphics/Particle/GPUParticleTypes.h"

using namespace MyMath;

void GunCombatEffectEmitter::EmitMuzzleGas(
	const Vector3& muzzlePos,
	const Vector3& fireDir,
	const Vector3& aircraftVelocity,
	const Vector3& aircraftUp)
{
	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	const Vector3 flashMinVel = Add(Add(Multiply(0.88f, aircraftVelocity), Multiply(8.0f, fireDir)), { -2.5f, -2.5f, -2.5f });
	const Vector3 flashMaxVel = Add(Add(Multiply(0.88f, aircraftVelocity), Multiply(18.0f, fireDir)), {  2.5f,  2.5f,  2.5f });

	const Vector3 streamMinVel = Add(
		Add(Multiply(0.76f, aircraftVelocity), Multiply(-16.0f, fireDir)),
		Add(Multiply(0.8f, aircraftUp), { -2.2f, -1.0f, -2.2f }));
	const Vector3 streamMaxVel = Add(
		Add(Multiply(0.84f, aircraftVelocity), Multiply(-8.0f, fireDir)),
		Add(Multiply(2.6f, aircraftUp), {  2.2f,  2.0f,  2.2f }));

	const EffectPreset* preset = EffectManager::GetInstance()
		? EffectManager::GetInstance()->FindPreset("gun_smoke")
		: nullptr;
	const EffectParticleLayer* flashLayer = (preset && preset->layers.size() >= 1) ? &preset->layers[0] : nullptr;
	const EffectParticleLayer* streamLayer = (preset && preset->layers.size() >= 2) ? &preset->layers[1] : nullptr;

	if (flashLayer) {
		EffectGpuPresetBridge::EmitOverride ov{};
		ov.overrideVelocityRange = true;
		ov.minVelocity = flashMinVel;
		ov.maxVelocity = flashMaxVel;
		EffectGpuPresetBridge::EmitLayer(*flashLayer, muzzlePos, ov);
	} else {
		GpuParticleEmitDesc flash{};
		flash.origin = muzzlePos;
		flash.randomPositionRange = 0.08f;
		flash.minVelocity = flashMinVel;
		flash.maxVelocity = flashMaxVel;
		flash.minLifeTime = 0.05f;
		flash.maxLifeTime = 0.10f;
		flash.minScale = 0.26f;
		flash.maxScale = 0.44f;
		flash.endScale = 1.15f;
		flash.scaleEasing = 0.55f;
		flash.minColor = { 1.00f, 0.52f, 0.14f, 0.35f };
		flash.maxColor = { 1.00f, 0.78f, 0.35f, 0.55f };
		flash.count = 2u;
		flash.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		              GPUParticleFlags::kCurlNoise | GPUParticleFlags::kSoftParticle |
		              GPUParticleFlags::kWorldUnitsPerSec;
		flash.shape = GPUParticleShape::SmokeCloud;
		flash.drag = 3.5f;
		flash.curlStrength = 2.5f;
		flash.curlFrequency = 0.30f;
		flash.softDistance = 1.2f;
		flash.emissiveIntensity = 2.0f;
		gpu->EnqueueRichEmit(flash);
	}

	if (streamLayer) {
		EffectGpuPresetBridge::EmitOverride ov{};
		ov.overrideVelocityRange = true;
		ov.minVelocity = streamMinVel;
		ov.maxVelocity = streamMaxVel;
		EffectGpuPresetBridge::EmitLayer(*streamLayer, muzzlePos, ov);
	} else {
		GpuParticleEmitDesc smoke{};
		smoke.origin = muzzlePos;
		smoke.randomPositionRange = 0.10f;
		smoke.minVelocity = streamMinVel;
		smoke.maxVelocity = streamMaxVel;
		smoke.minLifeTime = 0.28f;
		smoke.maxLifeTime = 0.56f;
		smoke.minScale = 0.22f;
		smoke.maxScale = 0.38f;
		smoke.endScale = 1.90f;
		smoke.scaleEasing = 0.50f;
		smoke.minColor = { 0.76f, 0.78f, 0.82f, 0.18f };
		smoke.maxColor = { 0.92f, 0.94f, 0.96f, 0.30f };
		smoke.count = 3u;
		smoke.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
		              GPUParticleFlags::kWakeInteract | GPUParticleFlags::kSoftParticle |
		              GPUParticleFlags::kWorldUnitsPerSec;
		smoke.shape = GPUParticleShape::VaporMist;
		smoke.drag = 3.0f;
		smoke.curlStrength = 4.0f;
		smoke.curlFrequency = 0.25f;
		smoke.wakeSensitivity = 0.85f;
		smoke.softDistance = 1.5f;
		gpu->EnqueueRichEmit(smoke);
	}
}

void GunCombatEffectEmitter::EmitBulletRicochet(const Vector3& hitPos, float floorY) {
	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	const EffectPreset* preset = EffectManager::GetInstance()
		? EffectManager::GetInstance()->FindPreset("bullet_ricochet")
		: nullptr;
	if (preset && !preset->layers.empty()) {
		EffectGpuPresetBridge::EmitOverride ov{};
		ov.overrideFloorY = true;
		ov.floorY = floorY;
		for (const auto& layer : preset->layers) {
			EffectGpuPresetBridge::EmitLayer(layer, hitPos, ov);
		}
		return;
	}

	GpuParticleEmitDesc sparks{};
	sparks.origin = hitPos;
	sparks.randomPositionRange = 0.22f;
	sparks.acceleration = { 0.0f, -24.0f, 0.0f };
	sparks.minLifeTime = 0.18f;
	sparks.maxLifeTime = 0.48f;
	sparks.minScale = 0.14f;
	sparks.maxScale = 0.28f;
	sparks.endScale = 0.03f;
	sparks.scaleEasing = 0.50f;
	sparks.minColor = { 1.00f, 0.75f, 0.30f, 0.95f };
	sparks.maxColor = { 1.00f, 0.98f, 0.85f, 1.00f };
	sparks.count = 28u;
	sparks.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
	               GPUParticleFlags::kCurlNoise | GPUParticleFlags::kBlackbody |
	               GPUParticleFlags::kStretched | GPUParticleFlags::kFloorBounce |
	               GPUParticleFlags::kSoftParticle | GPUParticleFlags::kSphereEmit |
	               GPUParticleFlags::kWorldUnitsPerSec;
	sparks.shape = GPUParticleShape::SparkStreak;
	sparks.drag = 2.4f;
	sparks.curlStrength = 3.0f;
	sparks.curlFrequency = 0.28f;
	sparks.stretchFactor = 0.065f;
	sparks.minTemperature = 0.72f;
	sparks.maxTemperature = 1.00f;
	sparks.coolingRate = 1.9f;
	sparks.floorY = floorY;
	sparks.restitution = 0.52f;
	sparks.softDistance = 1.4f;
	sparks.radialSpeedMin = 22.0f;
	sparks.radialSpeedMax = 78.0f;
	sparks.emissiveIntensity = 2.8f;
	gpu->EnqueueRichEmit(sparks);

	GpuParticleEmitDesc flash{};
	flash.origin = hitPos;
	flash.randomPositionRange = 0.10f;
	flash.minLifeTime = 0.04f;
	flash.maxLifeTime = 0.075f;
	flash.minScale = 1.6f;
	flash.maxScale = 2.6f;
	flash.endScale = 0.4f;
	flash.scaleEasing = 0.60f;
	flash.minColor = { 1.00f, 0.72f, 0.28f, 0.90f };
	flash.maxColor = { 1.00f, 0.92f, 0.65f, 1.00f };
	flash.count = 2u;
	flash.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
	              GPUParticleFlags::kSoftParticle | GPUParticleFlags::kWorldUnitsPerSec;
	flash.shape = GPUParticleShape::FlareCore;
	flash.drag = 2.0f;
	flash.softDistance = 1.5f;
	flash.emissiveIntensity = 2.8f;
	gpu->EnqueueRichEmit(flash);

	GpuParticleEmitDesc debris{};
	debris.origin = hitPos;
	debris.randomPositionRange = 0.28f;
	debris.minLifeTime = 0.35f;
	debris.maxLifeTime = 0.75f;
	debris.minScale = 0.45f;
	debris.maxScale = 0.85f;
	debris.endScale = 2.6f;
	debris.scaleEasing = 0.50f;
	debris.minColor = { 0.28f, 0.26f, 0.24f, 0.38f };
	debris.maxColor = { 0.54f, 0.50f, 0.46f, 0.55f };
	debris.count = 6u;
	debris.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
	               GPUParticleFlags::kSoftParticle | GPUParticleFlags::kSphereEmit |
	               GPUParticleFlags::kWorldUnitsPerSec;
	debris.shape = GPUParticleShape::SmokeCloud;
	debris.drag = 3.2f;
	debris.curlStrength = 3.5f;
	debris.curlFrequency = 0.22f;
	debris.softDistance = 2.0f;
	debris.radialSpeedMin = 3.0f;
	debris.radialSpeedMax = 11.0f;
	gpu->EnqueueRichEmit(debris);
}
