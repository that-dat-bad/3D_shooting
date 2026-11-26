#include "GpuExplosionEmitter.h"
#include "../../engine/Graphics/Particle/EffectGpuPresetBridge.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Graphics/Particle/GPUParticleTypes.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

void GpuExplosionEmitter::EmitMultiStage(
	const Vector3& position,
	float floorY,
	float scale,
	ScalePreset preset) {

	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	const float s = (std::max)(0.25f, scale);
	const bool isMassive = (preset == ScalePreset::MassiveShowcase);
	const float countMult = isMassive ? 3.5f : 1.0f;
	const float safeFloorY = (std::min)(floorY, position.y - 0.1f);
	const float heightAboveGround = position.y - safeFloorY;

	auto scaledCount = [countMult](uint32_t base) -> uint32_t {
		return static_cast<uint32_t>(std::round(static_cast<float>(base) * countMult));
	};

	auto triggerPointLight = [&]() {
		if (PointLight* pLight = Object3dCommon::GetInstance()->GetPointLightData()) {
			pLight->position = position;
			pLight->color = { 1.0f, 0.58f, 0.18f, 1.0f };
			pLight->intensity = isMassive ? 420.0f : 280.0f;
			pLight->radius = 480.0f * s;
			pLight->decay = 2.2f;
		}
	};

	if (EffectManager* em = EffectManager::GetInstance()) {
		if (const EffectPreset* preset = em->FindPreset("gpu_explosion")) {
			bool emittedAny = false;
			for (const auto& layer : preset->layers) {
				if (!layer.useGpu || !layer.gpuRich.useRichGpu) {
					continue;
				}
				if (layer.layerName == "GroundDust" && heightAboveGround >= 35.0f * s) {
					continue;
				}
				EffectGpuPresetBridge::EmitOverride ov{};
				ov.scale = s;
				ov.countMultiplier = countMult;
				ov.overrideFloorY = true;
				ov.floorY = safeFloorY;
				if (layer.layerName == "GroundDust") {
					ov.overrideOrigin = true;
					ov.origin = { position.x, safeFloorY + 1.2f * s, position.z };
				} else if (layer.layerName == "MushroomSmoke") {
					ov.overrideOrigin = true;
					ov.origin = { position.x, position.y + 1.2f * s, position.z };
				} else if (layer.layerName == "BounceSparks") {
					ov.overrideOrigin = true;
					ov.origin = { position.x, position.y + 0.8f * s, position.z };
				}
				if (EffectGpuPresetBridge::EmitLayer(layer, position, ov)) {
					emittedAny = true;
				}
			}
			if (emittedAny) {
				triggerPointLight();
				return;
			}
		}
	}

	// =========================================================================
	// Layer 1: 超高温プラズマ閃光コア（Additive HDR + 黒体放射）
	// =========================================================================
	{
		GpuParticleEmitDesc flash{};
		flash.origin = position;
		flash.randomPositionRange = 1.2f * s;
		flash.minLifeTime = 0.14f;
		flash.maxLifeTime = 0.28f;
		flash.minScale = 10.0f * s;
		flash.maxScale = 16.0f * s;
		flash.endScale = 28.0f * s;
		flash.scaleEasing = 0.35f;
		flash.minColor = { 1.0f, 0.96f, 0.85f, 0.92f };
		flash.maxColor = { 1.0f, 1.00f, 0.95f, 1.00f };
		flash.count = scaledCount(36u);
		flash.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		              GPUParticleFlags::kBlackbody | GPUParticleFlags::kSoftParticle |
		              GPUParticleFlags::kSphereEmit | GPUParticleFlags::kWorldUnitsPerSec;
		flash.shape = GPUParticleShape::SoftCircle;
		flash.radialSpeedMin = 4.0f * s;
		flash.radialSpeedMax = 14.0f * s;
		flash.drag = 4.5f;
		flash.minTemperature = 1.15f;
		flash.maxTemperature = 1.32f;
		flash.coolingRate = 2.6f;
		flash.emissiveIntensity = 2.6f;
		flash.softDistance = 5.0f * s;
		gpu->EnqueueRichEmit(flash);
	}

	// =========================================================================
	// Layer 2: 黒体放射サーマル火球（白熱→黄→橙→深紅へ冷却しながらカール上昇）
	// =========================================================================
	{
		GpuParticleEmitDesc fireball{};
		fireball.origin = position;
		fireball.randomPositionRange = 2.6f * s;
		fireball.minVelocity = { -2.0f * s, 2.5f * s, -2.0f * s };
		fireball.maxVelocity = {  2.0f * s, 8.5f * s,  2.0f * s };
		fireball.acceleration = { 0.0f, 6.2f * s, 0.0f }; // 高温浮力
		fireball.minLifeTime = 0.75f;
		fireball.maxLifeTime = 1.65f;
		fireball.minScale = 4.2f * s;
		fireball.maxScale = 8.2f * s;
		fireball.endScale = 16.5f * s;
		fireball.scaleEasing = 0.45f;
		fireball.minRotationSpeed = -1.6f;
		fireball.maxRotationSpeed = 1.6f;
		fireball.minColor = { 1.0f, 0.65f, 0.25f, 0.55f };
		fireball.maxColor = { 1.0f, 0.90f, 0.50f, 0.85f };
		fireball.count = scaledCount(900u);
		fireball.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		                 GPUParticleFlags::kBlackbody | GPUParticleFlags::kCurlNoise |
		                 GPUParticleFlags::kSoftParticle | GPUParticleFlags::kSphereEmit |
		                 GPUParticleFlags::kWorldUnitsPerSec;
		fireball.shape = GPUParticleShape::SmokeCloud;
		fireball.radialSpeedMin = 10.0f * s;
		fireball.radialSpeedMax = 34.0f * s;
		fireball.drag = 2.15f;
		fireball.curlStrength = 16.0f * s;
		fireball.curlFrequency = 0.085f;
		fireball.minTemperature = 0.78f;
		fireball.maxTemperature = 1.18f;
		fireball.coolingRate = 0.68f;
		fireball.emissiveIntensity = 1.85f;
		fireball.softDistance = 4.5f * s;
		gpu->EnqueueRichEmit(fireball);
	}

	// =========================================================================
	// Layer 3: 高速膨張マッハ衝撃波リング（水平＋斜め2リング）
	// =========================================================================
	{
		GpuParticleEmitDesc ring{};
		ring.origin = position;
		ring.randomPositionRange = 0.6f * s;
		ring.minLifeTime = 0.32f;
		ring.maxLifeTime = 0.55f;
		ring.minScale = 3.2f * s;
		ring.maxScale = 5.4f * s;
		ring.endScale = 13.0f * s;
		ring.scaleEasing = 0.55f;
		ring.minColor = { 1.0f, 0.82f, 0.52f, 0.55f };
		ring.maxColor = { 1.0f, 0.96f, 0.84f, 0.82f };
		ring.count = scaledCount(220u);
		ring.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		             GPUParticleFlags::kSoftParticle | GPUParticleFlags::kRingEmit |
		             GPUParticleFlags::kWorldUnitsPerSec;
		ring.shape = GPUParticleShape::ShockRing;
		ring.emitAxis = { 0.0f, 1.0f, 0.0f };
		ring.ringRadius = 2.2f * s;
		ring.radialSpeedMin = 48.0f * s;
		ring.radialSpeedMax = 68.0f * s;
		ring.drag = 2.8f;
		ring.emissiveIntensity = 1.65f;
		ring.softDistance = 3.0f * s;
		gpu->EnqueueRichEmit(ring);
	}

	// =========================================================================
	// Layer 4: 3Dカールノイズ・キノコ雲状の高密度爆煙（Normal Alpha バケット）
	// =========================================================================
	{
		GpuParticleEmitDesc smoke{};
		smoke.origin = { position.x, position.y + 1.2f * s, position.z };
		smoke.randomPositionRange = 4.0f * s;
		smoke.minVelocity = { -3.5f * s, 3.0f * s, -3.5f * s };
		smoke.maxVelocity = {  3.5f * s, 11.5f * s, 3.5f * s };
		smoke.acceleration = { 0.0f, 3.8f * s, 0.0f }; // サーマル上昇気流
		smoke.minLifeTime = 2.4f;
		smoke.maxLifeTime = 4.6f;
		smoke.minScale = 6.0f * s;
		smoke.maxScale = 11.5f * s;
		smoke.endScale = 27.0f * s;
		smoke.scaleEasing = 0.50f;
		smoke.minRotationSpeed = -0.85f;
		smoke.maxRotationSpeed = 0.85f;
		smoke.minColor = { 0.07f, 0.07f, 0.08f, 0.36f };
		smoke.maxColor = { 0.22f, 0.19f, 0.17f, 0.62f };
		smoke.count = scaledCount(1300u);
		smoke.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
		              GPUParticleFlags::kSoftParticle | GPUParticleFlags::kSphereEmit |
		              GPUParticleFlags::kWakeInteract | GPUParticleFlags::kWorldUnitsPerSec;
		smoke.shape = GPUParticleShape::SmokeCloud;
		smoke.radialSpeedMin = 8.0f * s;
		smoke.radialSpeedMax = 28.0f * s;
		smoke.drag = 1.35f;
		smoke.curlStrength = 14.5f * s;
		smoke.curlFrequency = 0.065f;
		smoke.emissiveIntensity = 1.0f;
		smoke.softDistance = 6.0f * s;
		smoke.wakeSensitivity = 0.75f;
		gpu->EnqueueRichEmit(smoke);
	}

	// =========================================================================
	// Layer 5: 地表爆風ダストスカート（地形近傍爆発時・地面沿いに広がる巻き上げ煙）
	// =========================================================================
	if (heightAboveGround < 35.0f * s) {
		GpuParticleEmitDesc dust{};
		dust.origin = { position.x, safeFloorY + 1.2f * s, position.z };
		dust.randomPositionRange = 2.0f * s;
		dust.minVelocity = { 0.0f, 0.5f * s, 0.0f };
		dust.maxVelocity = { 0.0f, 3.2f * s, 0.0f };
		dust.acceleration = { 0.0f, 1.2f * s, 0.0f };
		dust.minLifeTime = 1.8f;
		dust.maxLifeTime = 3.4f;
		dust.minScale = 4.5f * s;
		dust.maxScale = 8.5f * s;
		dust.endScale = 20.0f * s;
		dust.scaleEasing = 0.45f;
		dust.minRotationSpeed = -1.4f;
		dust.maxRotationSpeed = 1.4f;
		dust.minColor = { 0.38f, 0.34f, 0.29f, 0.30f };
		dust.maxColor = { 0.58f, 0.53f, 0.47f, 0.52f };
		dust.count = scaledCount(450u);
		dust.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
		             GPUParticleFlags::kSoftParticle | GPUParticleFlags::kRingEmit |
		             GPUParticleFlags::kWorldUnitsPerSec;
		dust.shape = GPUParticleShape::SmokeCloud;
		dust.emitAxis = { 0.0f, 1.0f, 0.0f };
		dust.ringRadius = 3.0f * s;
		dust.radialSpeedMin = 24.0f * s;
		dust.radialSpeedMax = 52.0f * s;
		dust.drag = 1.85f;
		dust.curlStrength = 10.0f * s;
		dust.curlFrequency = 0.09f;
		dust.softDistance = 4.0f * s;
		gpu->EnqueueRichEmit(dust);
	}

	// =========================================================================
	// Layer 6: 地面バウンド＆速度ストレッチ火花シャワー（Additive HDR + 黒体放射）
	// =========================================================================
	{
		GpuParticleEmitDesc sparks{};
		sparks.origin = { position.x, position.y + 0.8f * s, position.z };
		sparks.randomPositionRange = 1.6f * s;
		sparks.minVelocity = { -6.0f * s, 8.0f * s, -6.0f * s };
		sparks.maxVelocity = {  6.0f * s, 26.0f * s, 6.0f * s };
		sparks.acceleration = { 0.0f, -26.0f, 0.0f }; // 強い重力で放物線を描き地面で跳ねる
		sparks.minLifeTime = 1.1f;
		sparks.maxLifeTime = 2.8f;
		sparks.minScale = 0.22f * s;
		sparks.maxScale = 0.48f * s;
		sparks.endScale = 0.05f * s;
		sparks.scaleEasing = 0.7f;
		sparks.minColor = { 1.0f, 0.80f, 0.35f, 0.90f };
		sparks.maxColor = { 1.0f, 0.98f, 0.75f, 1.00f };
		sparks.count = scaledCount(1500u);
		sparks.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		               GPUParticleFlags::kBlackbody | GPUParticleFlags::kStretched |
		               GPUParticleFlags::kFloorBounce | GPUParticleFlags::kSphereEmit |
		               GPUParticleFlags::kWorldUnitsPerSec;
		sparks.shape = GPUParticleShape::SparkStreak;
		sparks.radialSpeedMin = 22.0f * s;
		sparks.radialSpeedMax = 68.0f * s;
		sparks.drag = 0.45f;
		sparks.stretchFactor = 0.075f;
		sparks.minTemperature = 0.82f;
		sparks.maxTemperature = 1.18f;
		sparks.coolingRate = 0.32f;
		sparks.floorY = safeFloorY;
		sparks.useCustomFloorY = true;
		sparks.restitution = 0.50f;
		sparks.emissiveIntensity = 2.15f;
		gpu->EnqueueRichEmit(sparks);
	}

	// =========================================================================
	// Layer 7: 爆煙の渦中を舞い上がる残り火（カールノイズ浮遊エンバー）
	// =========================================================================
	{
		GpuParticleEmitDesc embers{};
		embers.origin = position;
		embers.randomPositionRange = 5.0f * s;
		embers.minVelocity = { -4.0f * s, 2.0f * s, -4.0f * s };
		embers.maxVelocity = {  4.0f * s, 9.0f * s,  4.0f * s };
		embers.acceleration = { 0.0f, 2.6f * s, 0.0f };
		embers.minLifeTime = 2.6f;
		embers.maxLifeTime = 5.2f;
		embers.minScale = 0.25f * s;
		embers.maxScale = 0.65f * s;
		embers.endScale = 0.04f * s;
		embers.scaleEasing = 0.8f;
		embers.minColor = { 1.0f, 0.55f, 0.15f, 0.80f };
		embers.maxColor = { 1.0f, 0.90f, 0.40f, 1.00f };
		embers.count = scaledCount(520u);
		embers.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		               GPUParticleFlags::kBlackbody | GPUParticleFlags::kCurlNoise |
		               GPUParticleFlags::kSphereEmit | GPUParticleFlags::kWakeInteract |
		               GPUParticleFlags::kWorldUnitsPerSec;
		embers.shape = GPUParticleShape::SoftCircle;
		embers.radialSpeedMin = 4.0f * s;
		embers.radialSpeedMax = 18.0f * s;
		embers.drag = 0.95f;
		embers.curlStrength = 18.0f * s;
		embers.curlFrequency = 0.14f;
		embers.minTemperature = 0.45f;
		embers.maxTemperature = 0.92f;
		embers.coolingRate = 0.12f;
		embers.emissiveIntensity = 1.95f;
		embers.wakeSensitivity = 1.1f;
		gpu->EnqueueRichEmit(embers);
	}

	// 周辺地形・機体を照らす動的ポイントライト閃光
	if (PointLight* pLight = Object3dCommon::GetInstance()->GetPointLightData()) {
		pLight->position = position;
		pLight->color = { 1.0f, 0.58f, 0.18f, 1.0f };
		pLight->intensity = isMassive ? 420.0f : 280.0f;
		pLight->radius = 480.0f * s;
		pLight->decay = 2.2f;
	}
}
