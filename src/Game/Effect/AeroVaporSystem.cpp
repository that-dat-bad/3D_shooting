#include "AeroVaporSystem.h"
#include "../../engine/Graphics/Particle/EffectGpuPresetBridge.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Graphics/Particle/GPUParticleTypes.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

void AeroVaporSystem::Reset() {
	currentMach_ = 0.0f;
	wingVaporIntensity_ = 0.0f;
	stallVaporIntensity_ = 0.0f;
	transonicIntensity_ = 0.0f;
	wingEmitAccum_ = 0.0f;
	stallEmitAccum_ = 0.0f;
	coneEmitAccum_ = 0.0f;
	sonicBoomCooldown_ = 0.0f;
	wasSupersonic_ = false;
	sonicBoomTriggered_ = false;
	hasPrevTips_ = false;
}

bool AeroVaporSystem::ConsumeSonicBoomTriggered() {
	const bool fired = sonicBoomTriggered_;
	sonicBoomTriggered_ = false;
	return fired;
}

void AeroVaporSystem::Update(float dt, const AircraftAeroState& state) {
	if (dt <= 0.0f) {
		return;
	}
	const float clampedDt = std::clamp(dt, 0.0f, 0.1f);
	if (sonicBoomCooldown_ > 0.0f) {
		sonicBoomCooldown_ = (std::max)(0.0f, sonicBoomCooldown_ - clampedDt);
	}

	currentMach_ = state.speed / kSpeedOfSound;

	const float aoaRatio = std::fabs(state.aoaRad) / (std::max)(0.15f, state.criticalAoARad);
	const float speedGate = std::clamp((state.speed - 32.0f) / 48.0f, 0.0f, 1.0f);

	// 1. 高G / 高迎角 主翼前縁〜上面の薄透明凝縮シート強度
	{
		const float gTerm = std::clamp((std::fabs(state.currentG) - 2.2f) / 3.5f, 0.0f, 1.0f);
		const float aoaTerm = std::clamp((aoaRatio - 0.28f) / 0.45f, 0.0f, 1.0f);
		wingVaporIntensity_ = (std::max)(gTerm, aoaTerm) * speedGate;
	}

	if (wingVaporIntensity_ > 0.04f) {
		EmitHighGWingVaporStep(state, wingVaporIntensity_);
	} else {
		wingEmitAccum_ = 0.0f;
		hasPrevTips_ = false;
	}

	// 2. 失速・大迎角の翼面剥離乱流ベイパー（Vortex Breakdown）
	{
		const float preStallTerm = std::clamp((aoaRatio - 0.56f) / 0.40f, 0.0f, 1.0f);
		const float stallBoost = state.isStalling ? std::clamp(0.75f + (aoaRatio - 1.0f) * 0.35f, 0.75f, 1.0f) : 0.0f;
		stallVaporIntensity_ = (std::max)(preStallTerm, stallBoost) * speedGate;
	}

	if (stallVaporIntensity_ > 0.05f) {
		stallEmitAccum_ += clampedDt;
		constexpr float kStallEmitInterval = 0.018f;
		int steps = 0;
		while (stallEmitAccum_ >= kStallEmitInterval && steps < 2) {
			stallEmitAccum_ -= kStallEmitInterval;
			EmitStallSeparationStep(state, stallVaporIntensity_);
			steps++;
		}
	} else {
		stallEmitAccum_ = 0.0f;
	}

	// 3. 遷音速ベイパーコーン雲（Mach 0.84〜1.08）
	float coneFactor = 0.0f;
	if (currentMach_ >= 0.84f && currentMach_ <= 1.08f) {
		if (currentMach_ < 0.98f) {
			coneFactor = (currentMach_ - 0.84f) / 0.14f;
		} else {
			coneFactor = (1.08f - currentMach_) / 0.10f;
		}
		coneFactor = std::clamp(coneFactor, 0.0f, 1.0f);
	}
	transonicIntensity_ = coneFactor;

	if (transonicIntensity_ > 0.04f) {
		coneEmitAccum_ += clampedDt;
		constexpr float kConeEmitInterval = 0.018f;
		int steps = 0;
		while (coneEmitAccum_ >= kConeEmitInterval && steps < 2) {
			coneEmitAccum_ -= kConeEmitInterval;
			EmitTransonicConeStep(state, transonicIntensity_);
			steps++;
		}
	} else {
		coneEmitAccum_ = 0.0f;
	}

	// 4. マッハ 1.0 音速突破ソニックブーム衝撃波の自動検知（ヒステリシス付き）
	if (!wasSupersonic_ && currentMach_ >= 0.995f && sonicBoomCooldown_ <= 0.0f) {
		wasSupersonic_ = true;
		sonicBoomCooldown_ = 2.6f;
		sonicBoomTriggered_ = true;
		TriggerSonicBoom(state.position, state.forward, state.velocity, 1.0f);
	} else if (wasSupersonic_ && currentMach_ < 0.92f) {
		wasSupersonic_ = false;
	}
}

void AeroVaporSystem::EmitHighGWingVaporStep(const AircraftAeroState& state, float intensity) {
	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized() || intensity <= 0.01f) {
		return;
	}

	const float clamped = std::clamp(intensity, 0.0f, 1.0f);
	const Vector3 aftDrift = Add(Multiply(0.82f, state.velocity), Multiply(-8.0f, state.forward));

	// m21.gltf のデルタ翼前縁スウィープラインに沿った左右6ステーション（半スパン 4.50m, 後縁 z=-4.75m 基準）
	constexpr float kSpanFractions[6] = { 0.28f, 0.42f, 0.56f, 0.70f, 0.84f, 0.96f };
	constexpr float kChordOffsets[6]  = { -0.2f, -1.0f, -1.8f, -2.6f, -3.4f, -4.1f };

	const EffectPreset* wingPreset = EffectManager::GetInstance()
		? EffectManager::GetInstance()->FindPreset("wing_vapor")
		: nullptr;
	const EffectParticleLayer* cloudLayer = nullptr;
	const EffectParticleLayer* tipLayer = nullptr;
	if (wingPreset) {
		for (const auto& l : wingPreset->layers) {
			if (l.useGpu && l.gpuRich.useRichGpu) {
				if (l.layerName == "WingCondensationCloud") { cloudLayer = &l; }
				else if (l.layerName == "TipVortexRibbon") { tipLayer = &l; }
			}
		}
	}

	for (int side = -1; side <= 1; side += 2) {
		const float s = static_cast<float>(side);
		for (int st = 0; st < 6; ++st) {
			const float span = s * state.halfWingspan * kSpanFractions[st];
			const Vector3 spawnPos = Add(
				state.position,
				Add(
					Add(Multiply(span, state.right), Multiply(kChordOffsets[st], state.forward)),
					Multiply(0.16f, state.up)));

			if (cloudLayer) {
				const float stScale = (0.65f + 0.35f * (1.0f - kSpanFractions[st])) * (0.72f + 0.42f * clamped);
				EffectGpuPresetBridge::EmitOverride ov{};
				ov.scale = stScale;
				ov.alphaMultiplier = clamped * 1.15f;
				ov.countMultiplier = 0.25f + 0.35f * clamped;
				ov.overrideRandomPositionRange = true;
				ov.randomPositionRange = 0.45f + 0.25f * clamped;
				ov.overrideVelocityRange = true;
				ov.minVelocity = Subtract(aftDrift, { 1.8f, 1.2f, 1.8f });
				ov.maxVelocity = Add(aftDrift, { 1.8f, 1.6f, 1.8f });
				ov.overrideLifeTime = true;
				ov.minLifeTime = 0.06f + 0.04f * clamped;
				ov.maxLifeTime = 0.11f + 0.06f * clamped;
				EffectGpuPresetBridge::EmitLayer(*cloudLayer, spawnPos, ov);
			} else {
				GpuParticleEmitDesc cloud{};
				cloud.origin = spawnPos;
				cloud.randomPositionRange = 0.45f + 0.25f * clamped;
				cloud.minVelocity = Subtract(aftDrift, { 1.8f, 1.2f, 1.8f });
				cloud.maxVelocity = Add(aftDrift, { 1.8f, 1.6f, 1.8f });
				cloud.minLifeTime = 0.06f + 0.04f * clamped;
				cloud.maxLifeTime = 0.11f + 0.06f * clamped;
				cloud.minScale = (0.95f + 0.45f * (1.0f - kSpanFractions[st])) * (0.72f + 0.42f * clamped);
				cloud.maxScale = (1.55f + 0.65f * (1.0f - kSpanFractions[st])) * (0.72f + 0.42f * clamped);
				cloud.endScale = 2.6f * (0.75f + 0.40f * clamped);
				cloud.scaleEasing = 0.55f;
				cloud.minRotationSpeed = -1.8f;
				cloud.maxRotationSpeed = 1.8f;
				cloud.minColor = { 0.88f, 0.94f, 1.00f, 0.06f * clamped };
				cloud.maxColor = { 0.98f, 0.99f, 1.00f, 0.14f * clamped };
				cloud.count = static_cast<uint32_t>(2 + static_cast<int>(clamped * 3.0f));
				cloud.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
				              GPUParticleFlags::kSoftParticle | GPUParticleFlags::kWakeInteract |
				              GPUParticleFlags::kWorldUnitsPerSec;
				cloud.shape = GPUParticleShape::VaporMist;
				cloud.drag = 2.6f;
				cloud.curlStrength = 5.5f;
				cloud.curlFrequency = 0.22f;
				cloud.softDistance = 2.0f;
				cloud.wakeSensitivity = 0.65f;
				gpu->EnqueueRichEmit(cloud);
			}
		}
	}

	// 翼端ボルテックスリボン（m21.gltf 外翼後縁: ±4.50m, z=-4.72m, y=+0.09m）
	const Vector3 tipOffset = Add(Multiply(-4.72f, state.forward), Multiply(0.09f, state.up));
	const Vector3 leftTipPos = Add(state.position, Add(Multiply(-state.halfWingspan, state.right), tipOffset));
	const Vector3 rightTipPos = Add(state.position, Add(Multiply(state.halfWingspan, state.right), tipOffset));

	auto emitTipRibbon = [&](const Vector3& tipPos, const Vector3& prevTipPos) {
		int steps = 1;
		if (hasPrevTips_) {
			const float dist = Length(Subtract(tipPos, prevTipPos));
			steps = std::clamp(static_cast<int>(std::ceil(dist / 0.50f)), 1, 10);
		}
		for (int i = 0; i < steps; ++i) {
			const float t = hasPrevTips_ ? ((static_cast<float>(i) + 0.5f) / static_cast<float>(steps)) : 1.0f;
			const Vector3 samplePos = hasPrevTips_ ? Lerp(prevTipPos, tipPos, t) : tipPos;
			if (tipLayer) {
				EffectGpuPresetBridge::EmitOverride ov{};
				ov.alphaMultiplier = clamped * 1.15f;
				ov.countMultiplier = 0.25f + 0.35f * clamped;
				ov.overrideVelocityRange = true;
				ov.minVelocity = { 0.0f, 0.0f, 0.0f };
				ov.maxVelocity = { 0.0f, 0.0f, 0.0f };
				EffectGpuPresetBridge::EmitLayer(*tipLayer, samplePos, ov);
			} else {
				GpuParticleEmitDesc tipRibbon{};
				tipRibbon.origin = samplePos;
				tipRibbon.randomPositionRange = 0.03f;
				tipRibbon.minVelocity = { 0.0f, 0.0f, 0.0f };
				tipRibbon.maxVelocity = { 0.0f, 0.0f, 0.0f };
				tipRibbon.minLifeTime = 0.50f;
				tipRibbon.maxLifeTime = 0.85f;
				tipRibbon.minScale = 0.16f;
				tipRibbon.maxScale = 0.28f;
				tipRibbon.endScale = 0.95f;
				tipRibbon.scaleEasing = 0.65f;
				tipRibbon.minColor = { 0.90f, 0.95f, 1.00f, 0.16f * clamped };
				tipRibbon.maxColor = { 1.00f, 1.00f, 1.00f, 0.30f * clamped };
				tipRibbon.count = 2u;
				tipRibbon.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
				                  GPUParticleFlags::kSoftParticle | GPUParticleFlags::kWakeInteract |
				                  GPUParticleFlags::kWorldUnitsPerSec;
				tipRibbon.shape = GPUParticleShape::VaporMist;
				tipRibbon.drag = 3.4f;
				tipRibbon.curlStrength = 0.6f;
				tipRibbon.curlFrequency = 0.22f;
				tipRibbon.softDistance = 1.6f;
				tipRibbon.wakeSensitivity = 0.15f;
				gpu->EnqueueRichEmit(tipRibbon);
			}
		}
	};

	emitTipRibbon(leftTipPos, prevLeftTip_);
	emitTipRibbon(rightTipPos, prevRightTip_);
	prevLeftTip_ = leftTipPos;
	prevRightTip_ = rightTipPos;
	hasPrevTips_ = true;
}

void AeroVaporSystem::EmitStallSeparationStep(const AircraftAeroState& state, float stallIntensity) {
	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized() || stallIntensity <= 0.01f) {
		return;
	}

	const float clamped = std::clamp(stallIntensity, 0.0f, 1.0f);
	// 失速時は主翼上面から剥離した渦が機体上後方へ遅い速度で巻き上がる
	const Vector3 sepVelBase = Add(
		Add(Multiply(0.52f, state.velocity), Multiply(-16.0f, state.forward)),
		Multiply(4.5f, state.up));

	const EffectPreset* wingPreset = EffectManager::GetInstance()
		? EffectManager::GetInstance()->FindPreset("wing_vapor")
		: nullptr;
	const EffectParticleLayer* stallLayer = nullptr;
	if (wingPreset) {
		for (const auto& l : wingPreset->layers) {
			if (l.useGpu && l.gpuRich.useRichGpu && l.layerName == "StallSeparationVapor") {
				stallLayer = &l;
				break;
			}
		}
	}

	constexpr float kStallStations[2] = { 0.42f, 0.76f };
	for (int side = -1; side <= 1; side += 2) {
		const float s = static_cast<float>(side);
		for (int st = 0; st < 2; ++st) {
			const float span = s * state.halfWingspan * kStallStations[st];
			const Vector3 spawnPos = Add(
				state.position,
				Add(
					Add(Multiply(span, state.right), Multiply(-3.0f - 0.6f * static_cast<float>(st), state.forward)),
					Multiply(0.22f, state.up)));

			if (stallLayer) {
				EffectGpuPresetBridge::EmitOverride ov{};
				ov.scale = 0.78f + 0.45f * clamped;
				ov.alphaMultiplier = clamped * 1.15f;
				ov.countMultiplier = 0.45f + 0.55f * clamped;
				ov.overrideVelocityRange = true;
				ov.minVelocity = Subtract(sepVelBase, { 4.0f, 2.5f, 4.0f });
				ov.maxVelocity = Add(sepVelBase, { 4.0f, 5.5f, 4.0f });
				EffectGpuPresetBridge::EmitLayer(*stallLayer, spawnPos, ov);
			} else {
				GpuParticleEmitDesc sep{};
				sep.origin = spawnPos;
				sep.randomPositionRange = 0.85f;
				sep.minVelocity = Subtract(sepVelBase, { 4.0f, 2.5f, 4.0f });
				sep.maxVelocity = Add(sepVelBase, { 4.0f, 5.5f, 4.0f });
				sep.minLifeTime = 0.24f;
				sep.maxLifeTime = 0.52f;
				sep.minScale = 1.1f * (0.78f + 0.45f * clamped);
				sep.maxScale = 2.0f * (0.78f + 0.45f * clamped);
				sep.endScale = 4.6f * (0.80f + 0.40f * clamped);
				sep.scaleEasing = 0.50f;
				sep.minRotationSpeed = -2.8f;
				sep.maxRotationSpeed = 2.8f;
				sep.minColor = { 0.86f, 0.92f, 0.99f, 0.08f * clamped };
				sep.maxColor = { 0.98f, 0.99f, 1.00f, 0.18f * clamped };
				sep.count = static_cast<uint32_t>(3 + static_cast<int>(clamped * 4.0f));
				sep.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
				            GPUParticleFlags::kSoftParticle | GPUParticleFlags::kWakeInteract |
				            GPUParticleFlags::kWorldUnitsPerSec;
				sep.shape = GPUParticleShape::VaporMist;
				sep.drag = 2.1f;
				sep.curlStrength = 13.5f;
				sep.curlFrequency = 0.17f;
				sep.softDistance = 2.8f;
				sep.wakeSensitivity = 1.15f;
				gpu->EnqueueRichEmit(sep);
			}
		}
	}
}

void AeroVaporSystem::EmitTransonicConeStep(const AircraftAeroState& state, float intensity) {
	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized() || intensity <= 0.01f) {
		return;
	}

	const float clamped = std::clamp(intensity, 0.0f, 1.0f);
	const Vector3 coneCenter = Add(state.position, Multiply(-1.4f, state.forward));
	const Vector3 coneVel = Add(Multiply(0.76f, state.velocity), Multiply(-24.0f, state.forward));

	// プラントル＝グロワート遷音速ベイパーコーンリング（薄透明ベール霧）
	GpuParticleEmitDesc cone{};
	cone.origin = coneCenter;
	cone.randomPositionRange = 0.85f;
	cone.minVelocity = Subtract(coneVel, { 3.0f, 3.0f, 3.0f });
	cone.maxVelocity = Add(coneVel, { 3.0f, 3.0f, 3.0f });
	cone.minLifeTime = 0.10f;
	cone.maxLifeTime = 0.20f;
	cone.minScale = 1.8f * (0.75f + 0.45f * clamped);
	cone.maxScale = 3.1f * (0.75f + 0.45f * clamped);
	cone.endScale = 5.6f * (0.75f + 0.45f * clamped);
	cone.scaleEasing = 0.45f;
	cone.minRotationSpeed = -2.5f;
	cone.maxRotationSpeed = 2.5f;
	cone.minColor = { 0.90f, 0.95f, 1.00f, 0.10f * clamped };
	cone.maxColor = { 1.00f, 1.00f, 1.00f, 0.22f * clamped };
	cone.count = static_cast<uint32_t>(18 + static_cast<int>(clamped * 28.0f));
	cone.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
	             GPUParticleFlags::kSoftParticle | GPUParticleFlags::kRingEmit |
	             GPUParticleFlags::kWorldUnitsPerSec;
	cone.shape = GPUParticleShape::VaporMist;
	cone.emitAxis = state.forward;
	cone.ringRadius = 3.6f * (0.8f + 0.35f * clamped);
	cone.radialSpeedMin = 8.0f;
	cone.radialSpeedMax = 20.0f;
	cone.drag = 4.2f;
	cone.curlStrength = 9.0f;
	cone.curlFrequency = 0.16f;
	cone.softDistance = 3.2f;
	gpu->EnqueueRichEmit(cone);
}

void AeroVaporSystem::TriggerSonicBoom(
	const Vector3& position,
	const Vector3& forward,
	const Vector3& velocity,
	float scale) {

	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	const float s = (std::max)(0.3f, scale);
	const Vector3 fwd = (Length(forward) > 1e-4f) ? Normalize(forward) : Vector3{ 0.0f, 0.0f, 1.0f };
	const Vector3 ringOrigin = Add(position, Multiply(-0.8f * s, fwd));
	const Vector3 inheritVel = Multiply(0.22f, velocity);

	if (EffectManager* em = EffectManager::GetInstance()) {
		if (const EffectPreset* preset = em->FindPreset("sonic_boom")) {
			bool emittedAny = false;
			for (const auto& layer : preset->layers) {
				if (!layer.useGpu || !layer.gpuRich.useRichGpu) {
					continue;
				}
				EffectGpuPresetBridge::EmitOverride ov{};
				ov.scale = s;
				ov.overrideEmitAxis = true;
				ov.emitAxis = fwd;
				ov.addVelocityOffset = true;
				if (layer.layerName == "VaporBurstCloud") {
					ov.velocityOffset = Subtract(inheritVel, Multiply(5.0f * s, fwd));
				} else if (layer.layerName == "MistStreaks") {
					ov.velocityOffset = Subtract(inheritVel, Multiply(12.0f * s, fwd));
				} else {
					ov.velocityOffset = inheritVel;
				}
				if (EffectGpuPresetBridge::EmitLayer(layer, ringOrigin, ov)) {
					emittedAny = true;
				}
			}
			if (emittedAny) {
				return;
			}
		}
	}

	// 1. 鋭利なマッハ衝撃波グローリング（Additive HDR + ShockRing）
	{
		GpuParticleEmitDesc shockRing{};
		shockRing.origin = ringOrigin;
		shockRing.randomPositionRange = 0.45f * s;
		shockRing.minVelocity = inheritVel;
		shockRing.maxVelocity = inheritVel;
		shockRing.minLifeTime = 0.35f;
		shockRing.maxLifeTime = 0.58f;
		shockRing.minScale = 2.4f * s;
		shockRing.maxScale = 4.2f * s;
		shockRing.endScale = 9.5f * s;
		shockRing.scaleEasing = 0.45f;
		shockRing.minColor = { 0.72f, 0.90f, 1.00f, 0.60f };
		shockRing.maxColor = { 0.96f, 0.99f, 1.00f, 0.90f };
		shockRing.count = 320u;
		shockRing.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		                  GPUParticleFlags::kSoftParticle | GPUParticleFlags::kRingEmit |
		                  GPUParticleFlags::kWorldUnitsPerSec;
		shockRing.shape = GPUParticleShape::ShockRing;
		shockRing.emitAxis = fwd;
		shockRing.ringRadius = 3.0f * s;
		shockRing.radialSpeedMin = 42.0f * s;
		shockRing.radialSpeedMax = 68.0f * s;
		shockRing.drag = 3.0f;
		shockRing.emissiveIntensity = 1.85f;
		shockRing.softDistance = 3.5f * s;
		gpu->EnqueueRichEmit(shockRing);
	}

	// 2. 音速突破の純白ベイパーバースト雲（Normal Alpha + VaporMist + 3D Curl Noise）
	{
		GpuParticleEmitDesc burstCloud{};
		burstCloud.origin = ringOrigin;
		burstCloud.randomPositionRange = 1.4f * s;
		burstCloud.minVelocity = Subtract(inheritVel, Multiply(8.0f * s, fwd));
		burstCloud.maxVelocity = Subtract(inheritVel, Multiply(2.0f * s, fwd));
		burstCloud.minLifeTime = 0.45f;
		burstCloud.maxLifeTime = 0.95f;
		burstCloud.minScale = 3.2f * s;
		burstCloud.maxScale = 5.8f * s;
		burstCloud.endScale = 13.5f * s;
		burstCloud.scaleEasing = 0.48f;
		burstCloud.minRotationSpeed = -2.4f;
		burstCloud.maxRotationSpeed = 2.4f;
		burstCloud.minColor = { 0.88f, 0.94f, 1.00f, 0.24f };
		burstCloud.maxColor = { 1.00f, 1.00f, 1.00f, 0.42f };
		burstCloud.count = 950u;
		burstCloud.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
		                   GPUParticleFlags::kSoftParticle | GPUParticleFlags::kRingEmit |
		                   GPUParticleFlags::kWakeInteract | GPUParticleFlags::kWorldUnitsPerSec;
		burstCloud.shape = GPUParticleShape::VaporMist;
		burstCloud.emitAxis = fwd;
		burstCloud.ringRadius = 2.6f * s;
		burstCloud.radialSpeedMin = 22.0f * s;
		burstCloud.radialSpeedMax = 54.0f * s;
		burstCloud.drag = 2.7f;
		burstCloud.curlStrength = 14.0f * s;
		burstCloud.curlFrequency = 0.12f;
		burstCloud.softDistance = 4.2f * s;
		burstCloud.wakeSensitivity = 0.85f;
		gpu->EnqueueRichEmit(burstCloud);
	}

	// 3. 超音速圧縮ミストの高速放射ストリーク（Additive + 速度ストレッチ）
	{
		GpuParticleEmitDesc mistStreaks{};
		mistStreaks.origin = ringOrigin;
		mistStreaks.randomPositionRange = 1.0f * s;
		mistStreaks.minVelocity = Subtract(inheritVel, Multiply(18.0f * s, fwd));
		mistStreaks.maxVelocity = Subtract(inheritVel, Multiply(6.0f * s, fwd));
		mistStreaks.minLifeTime = 0.22f;
		mistStreaks.maxLifeTime = 0.45f;
		mistStreaks.minScale = 0.16f * s;
		mistStreaks.maxScale = 0.34f * s;
		mistStreaks.endScale = 0.04f * s;
		mistStreaks.scaleEasing = 0.6f;
		mistStreaks.minColor = { 0.80f, 0.93f, 1.00f, 0.72f };
		mistStreaks.maxColor = { 1.00f, 1.00f, 1.00f, 0.96f };
		mistStreaks.count = 420u;
		mistStreaks.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		                    GPUParticleFlags::kStretched | GPUParticleFlags::kRingEmit |
		                    GPUParticleFlags::kWorldUnitsPerSec;
		mistStreaks.shape = GPUParticleShape::SparkStreak;
		mistStreaks.emitAxis = fwd;
		mistStreaks.ringRadius = 2.0f * s;
		mistStreaks.radialSpeedMin = 38.0f * s;
		mistStreaks.radialSpeedMax = 82.0f * s;
		mistStreaks.drag = 2.2f;
		mistStreaks.stretchFactor = 0.065f;
		mistStreaks.emissiveIntensity = 1.65f;
		gpu->EnqueueRichEmit(mistStreaks);
	}
}
