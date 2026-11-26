#include "AfterburnerParticleEmitter.h"
#include "../Graphics/Afterburner.h"
#include "../../engine/Graphics/Particle/EffectGpuPresetBridge.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Graphics/Particle/GPUParticleTypes.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

namespace {
	constexpr float kPi = 3.14159265358979323846f;
}

void AfterburnerParticleEmitter::Reset() {
	wasActive_ = false;
	ignitionCooldown_ = 0.0f;
	emitAccum_ = 0.0f;
}

void AfterburnerParticleEmitter::ExtractNozzlePose(
	const Vector3& localOffset,
	const Vector3& nozzleRot,
	const Matrix4x4& visualWorld,
	Vector3& outPos,
	Vector3& outExhaustDir)
{
	const Matrix4x4 local = MakeAffineMatrix(Vector3{ 1.0f, 1.0f, 1.0f }, nozzleRot, localOffset);
	const Matrix4x4 world = local * visualWorld;
	outPos = { world.m[3][0], world.m[3][1], world.m[3][2] };
	const Vector3 zAxis = { world.m[2][0], world.m[2][1], world.m[2][2] };
	outExhaustDir = (Length(zAxis) > 0.0001f) ? Normalize(zAxis) : Vector3{ 0.0f, 0.0f, 1.0f };
}

void AfterburnerParticleEmitter::Update(
	float dt,
	const Afterburner& afterburner,
	const Matrix4x4& aircraftWorldMatrix,
	const Vector3& aircraftVelocity,
	float intensity)
{
	if (dt <= 0.0f) {
		return;
	}
	const float clampedDt = std::clamp(dt, 0.0f, 0.1f);
	if (ignitionCooldown_ > 0.0f) {
		ignitionCooldown_ = (std::max)(0.0f, ignitionCooldown_ - clampedDt);
	}

	const float clampedIntensity = std::clamp(intensity, 0.0f, 1.0f);
	const bool isActive = (clampedIntensity > 0.05f);
	if (!isActive) {
		wasActive_ = false;
		emitAccum_ = 0.0f;
		return;
	}

	const Matrix4x4 baseRotate = MakeAffineMatrix(Vector3{ 1.0f, 1.0f, 1.0f }, Vector3{ 0.0f, kPi, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f });
	const Matrix4x4 visualWorld = Multiply(baseRotate, aircraftWorldMatrix);
	const Vector3 nozzleRot = afterburner.GetNozzleRotation();
	const float flameLength = afterburner.GetFlameLength();

	auto emitForNozzle = [&](const Vector3& localOffset, bool triggerIgnition, int streamSteps) {
		Vector3 nozzlePos{};
		Vector3 exhaustDir{};
		ExtractNozzlePose(localOffset, nozzleRot, visualWorld, nozzlePos, exhaustDir);
		if (triggerIgnition) {
			EmitIgnitionBurst(nozzlePos, exhaustDir, aircraftVelocity);
		}
		for (int i = 0; i < streamSteps; ++i) {
			EmitSupersonicStreamStep(nozzlePos, exhaustDir, aircraftVelocity, flameLength, clampedIntensity);
		}
	};

	const bool triggerIgnition = (!wasActive_ && ignitionCooldown_ <= 0.0f);
	if (triggerIgnition) {
		ignitionCooldown_ = 0.35f;
	}
	wasActive_ = true;

	emitAccum_ += clampedDt;
	constexpr float kStepInterval = 0.016f;
	int streamSteps = 0;
	while (emitAccum_ >= kStepInterval && streamSteps < 2) {
		emitAccum_ -= kStepInterval;
		++streamSteps;
	}

	if (!triggerIgnition && streamSteps == 0) {
		return;
	}

	if (afterburner.IsTwinEngine()) {
		emitForNozzle(afterburner.GetLeftNozzleOffset(), triggerIgnition, streamSteps);
		emitForNozzle(afterburner.GetRightNozzleOffset(), triggerIgnition, streamSteps);
	} else {
		emitForNozzle(afterburner.GetSingleNozzleOffset(), triggerIgnition, streamSteps);
	}
}

void AfterburnerParticleEmitter::EmitIgnitionBurst(
	const Vector3& nozzlePos,
	const Vector3& exhaustDir,
	const Vector3& aircraftVelocity) const
{
	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	const Vector3 minVel = Add(aircraftVelocity, Add(Multiply(70.0f, exhaustDir), { -18.0f, -18.0f, -18.0f }));
	const Vector3 maxVel = Add(aircraftVelocity, Add(Multiply(145.0f, exhaustDir), {  18.0f,  18.0f,  18.0f }));

	const EffectPreset* preset = EffectManager::GetInstance()
		? EffectManager::GetInstance()->FindPreset("afterburner_sparks")
		: nullptr;
	if (preset && !preset->layers.empty()) {
		EffectGpuPresetBridge::EmitOverride ov{};
		ov.overrideVelocityRange = true;
		ov.minVelocity = minVel;
		ov.maxVelocity = maxVel;
		EffectGpuPresetBridge::EmitLayer(preset->layers[0], nozzlePos, ov);
	} else {
		GpuParticleEmitDesc spark{};
		spark.origin = nozzlePos;
		spark.randomPositionRange = 0.12f;
		spark.minVelocity = minVel;
		spark.maxVelocity = maxVel;
		spark.minLifeTime = 0.10f;
		spark.maxLifeTime = 0.24f;
		spark.minScale = 0.14f;
		spark.maxScale = 0.26f;
		spark.endScale = 0.03f;
		spark.scaleEasing = 0.50f;
		spark.minColor = { 0.55f, 0.82f, 1.00f, 0.95f };
		spark.maxColor = { 1.00f, 0.96f, 0.85f, 1.00f };
		spark.count = 34u;
		spark.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		              GPUParticleFlags::kCurlNoise | GPUParticleFlags::kBlackbody |
		              GPUParticleFlags::kStretched | GPUParticleFlags::kSoftParticle |
		              GPUParticleFlags::kWorldUnitsPerSec;
		spark.shape = GPUParticleShape::SparkStreak;
		spark.drag = 2.2f;
		spark.curlStrength = 5.5f;
		spark.curlFrequency = 0.35f;
		spark.stretchFactor = 0.055f;
		spark.minTemperature = 0.82f;
		spark.maxTemperature = 1.00f;
		spark.coolingRate = 2.4f;
		spark.softDistance = 1.2f;
		spark.emissiveIntensity = 3.0f;
		gpu->EnqueueRichEmit(spark);
	}

	// 点火瞬間の鋭いショックリング
	GpuParticleEmitDesc ring{};
	ring.origin = nozzlePos;
	ring.randomPositionRange = 0.04f;
	ring.minVelocity = Add(aircraftVelocity, Multiply(18.0f, exhaustDir));
	ring.maxVelocity = ring.minVelocity;
	ring.minLifeTime = 0.08f;
	ring.maxLifeTime = 0.14f;
	ring.minScale = 0.55f;
	ring.maxScale = 0.85f;
	ring.endScale = 2.20f;
	ring.scaleEasing = 0.55f;
	ring.minColor = { 0.55f, 0.82f, 1.00f, 0.75f };
	ring.maxColor = { 0.95f, 0.98f, 1.00f, 0.95f };
	ring.count = 10u;
	ring.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
	             GPUParticleFlags::kRingEmit | GPUParticleFlags::kSoftParticle |
	             GPUParticleFlags::kWorldUnitsPerSec;
	ring.shape = GPUParticleShape::ShockRing;
	ring.emitAxis = exhaustDir;
	ring.ringRadius = 0.35f;
	ring.radialSpeedMin = 8.0f;
	ring.radialSpeedMax = 14.0f;
	ring.softDistance = 1.2f;
	ring.emissiveIntensity = 2.4f;
	gpu->EnqueueRichEmit(ring);
}

void AfterburnerParticleEmitter::EmitSupersonicStreamStep(
	const Vector3& nozzlePos,
	const Vector3& exhaustDir,
	const Vector3& aircraftVelocity,
	float flameLength,
	float intensity) const
{
	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	const Vector3 sparkOrigin = Add(nozzlePos, Multiply(0.35f, exhaustDir));
	const Vector3 sparkMinVel = Add(aircraftVelocity, Add(Multiply(95.0f, exhaustDir), { -8.0f, -8.0f, -8.0f }));
	const Vector3 sparkMaxVel = Add(aircraftVelocity, Add(Multiply(165.0f, exhaustDir), {  8.0f,  8.0f,  8.0f }));

	const Vector3 veilOrigin = Add(nozzlePos, Multiply(flameLength * 0.55f, exhaustDir));
	const Vector3 veilMinVel = Add(aircraftVelocity, Add(Multiply(45.0f, exhaustDir), { -4.0f, -4.0f, -4.0f }));
	const Vector3 veilMaxVel = Add(aircraftVelocity, Add(Multiply(80.0f, exhaustDir), {  4.0f,  4.0f,  4.0f }));

	const EffectPreset* preset = EffectManager::GetInstance()
		? EffectManager::GetInstance()->FindPreset("afterburner_sparks")
		: nullptr;
	const EffectParticleLayer* sparkLayer = (preset && preset->layers.size() >= 2) ? &preset->layers[1] : nullptr;
	const EffectParticleLayer* veilLayer = (preset && preset->layers.size() >= 3) ? &preset->layers[2] : nullptr;

	if (sparkLayer) {
		EffectGpuPresetBridge::EmitOverride ov{};
		ov.alphaMultiplier = intensity;
		ov.overrideVelocityRange = true;
		ov.minVelocity = sparkMinVel;
		ov.maxVelocity = sparkMaxVel;
		EffectGpuPresetBridge::EmitLayer(*sparkLayer, sparkOrigin, ov);
	} else {
		GpuParticleEmitDesc spark{};
		spark.origin = sparkOrigin;
		spark.randomPositionRange = 0.14f;
		spark.minVelocity = sparkMinVel;
		spark.maxVelocity = sparkMaxVel;
		spark.minLifeTime = 0.06f;
		spark.maxLifeTime = 0.15f;
		spark.minScale = 0.10f;
		spark.maxScale = 0.20f;
		spark.endScale = 0.02f;
		spark.scaleEasing = 0.55f;
		spark.minColor = { 0.45f, 0.78f, 1.00f, 0.85f * intensity };
		spark.maxColor = { 1.00f, 0.88f, 0.65f, 1.00f * intensity };
		spark.count = 4u;
		spark.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		              GPUParticleFlags::kCurlNoise | GPUParticleFlags::kBlackbody |
		              GPUParticleFlags::kStretched | GPUParticleFlags::kSoftParticle |
		              GPUParticleFlags::kWorldUnitsPerSec;
		spark.shape = GPUParticleShape::SparkStreak;
		spark.drag = 1.8f;
		spark.curlStrength = 4.2f;
		spark.curlFrequency = 0.40f;
		spark.stretchFactor = 0.050f;
		spark.minTemperature = 0.76f;
		spark.maxTemperature = 1.00f;
		spark.coolingRate = 3.2f;
		spark.softDistance = 1.0f;
		spark.emissiveIntensity = 2.6f;
		gpu->EnqueueRichEmit(spark);
	}

	if (veilLayer) {
		EffectGpuPresetBridge::EmitOverride ov{};
		ov.alphaMultiplier = intensity;
		ov.overrideVelocityRange = true;
		ov.minVelocity = veilMinVel;
		ov.maxVelocity = veilMaxVel;
		EffectGpuPresetBridge::EmitLayer(*veilLayer, veilOrigin, ov);
	} else {
		GpuParticleEmitDesc veil{};
		veil.origin = veilOrigin;
		veil.randomPositionRange = 0.16f;
		veil.minVelocity = veilMinVel;
		veil.maxVelocity = veilMaxVel;
		veil.minLifeTime = 0.10f;
		veil.maxLifeTime = 0.22f;
		veil.minScale = 0.34f;
		veil.maxScale = 0.52f;
		veil.endScale = 1.85f;
		veil.scaleEasing = 0.55f;
		veil.minColor = { 0.22f, 0.50f, 0.95f, 0.10f * intensity };
		veil.maxColor = { 0.95f, 0.55f, 0.25f, 0.18f * intensity };
		veil.count = 2u;
		veil.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		             GPUParticleFlags::kCurlNoise | GPUParticleFlags::kWakeInteract |
		             GPUParticleFlags::kSoftParticle | GPUParticleFlags::kWorldUnitsPerSec;
		veil.shape = GPUParticleShape::VaporMist;
		veil.drag = 2.8f;
		veil.curlStrength = 5.0f;
		veil.curlFrequency = 0.28f;
		veil.wakeSensitivity = 0.55f;
		veil.softDistance = 1.6f;
		veil.emissiveIntensity = 1.4f;
		gpu->EnqueueRichEmit(veil);
	}
}
