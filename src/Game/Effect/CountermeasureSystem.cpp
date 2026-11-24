#include "CountermeasureSystem.h"
#include "../../engine/Graphics/Particle/EffectGpuPresetBridge.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Graphics/Particle/GPUParticleTypes.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

namespace {
	constexpr int kPairsPerSalvo = 4;
	constexpr size_t kMaxActiveFlares = 32u;
	constexpr float kPairIntervalSec = 0.095f;
	constexpr float kSalvoCooldownSec = 0.55f;
	constexpr float kFlareGravity = 12.2f;
	constexpr float kFlareDrag = 1.65f;

	float HashSigned01(uint32_t seed, uint32_t salt) {
		uint32_t x = seed * 747796405u + salt * 2891336453u;
		x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
		x = (x >> 22u) ^ x;
		const float u = static_cast<float>(x & 0x00FFFFFFu) / 16777216.0f;
		return u * 2.0f - 1.0f;
	}
}

void CountermeasureSystem::Reset(int maxAmmo) {
	maxAmmo_ = (std::max)(1, maxAmmo);
	currentAmmo_ = maxAmmo_;
	activeFlares_.clear();
	pendingPairs_ = 0;
	emittedPairIndex_ = 0;
	salvoIntervalTimer_ = 0.0f;
	cooldownTimer_ = 0.0f;
	salvoIgnoreAmmo_ = false;
}

bool CountermeasureSystem::TriggerSalvo(bool ignoreAmmo) {
	if (cooldownTimer_ > 0.0f || pendingPairs_ > 0) {
		return false;
	}
	if (!ignoreAmmo && currentAmmo_ <= 0) {
		return false;
	}

	salvoIgnoreAmmo_ = ignoreAmmo;
	pendingPairs_ = kPairsPerSalvo;
	emittedPairIndex_ = 0;
	salvoIntervalTimer_ = 0.0f;
	cooldownTimer_ = kSalvoCooldownSec;
	return true;
}

void CountermeasureSystem::Update(float dt, const AircraftEmitterState& aircraft) {
	if (dt <= 0.0f) {
		return;
	}
	const float clampedDt = std::clamp(dt, 0.0f, 0.1f);

	if (cooldownTimer_ > 0.0f) {
		cooldownTimer_ = (std::max)(0.0f, cooldownTimer_ - clampedDt);
	}

	// 1. 斉射タイマーによる左右ペア順次射出
	if (pendingPairs_ > 0) {
		salvoIntervalTimer_ -= clampedDt;
		while (pendingPairs_ > 0 && salvoIntervalTimer_ <= 0.0f) {
			if (!salvoIgnoreAmmo_ && currentAmmo_ <= 0) {
				pendingPairs_ = 0;
				break;
			}
			EjectPair(aircraft, emittedPairIndex_);
			if (!salvoIgnoreAmmo_) {
				currentAmmo_ = (std::max)(0, currentAmmo_ - 2);
			}
			emittedPairIndex_++;
			pendingPairs_--;
			salvoIntervalTimer_ += kPairIntervalSec;
		}
	}

	// 2. 燃焼中フレア弾の弾道移動と連続補間GPUトレイル発生
	for (auto it = activeFlares_.begin(); it != activeFlares_.end();) {
		FlareDecoy& f = *it;
		f.age += clampedDt;
		if (f.age >= f.lifeTime) {
			it = activeFlares_.erase(it);
			continue;
		}

		f.prevPosition = f.position;

		// 高空気抵抗＋重力によるリアルなフレア弾道（射出直後に減速し美しい放物線を描く）
		const float dragFactor = std::exp(-kFlareDrag * clampedDt);
		f.velocity.x *= dragFactor;
		f.velocity.y = f.velocity.y * dragFactor - kFlareGravity * clampedDt;
		f.velocity.z *= dragFactor;
		f.position = Add(f.position, Multiply(clampedDt, f.velocity));

		// 地面バウンド
		if (f.position.y < f.floorY + 0.15f) {
			f.position.y = f.floorY + 0.15f;
			if (f.velocity.y < 0.0f) {
				f.velocity.y = -f.velocity.y * 0.35f;
				f.velocity.x *= 0.65f;
				f.velocity.z *= 0.65f;
			}
		}

		EmitFlareTrailSegment(f, clampedDt);
		++it;
	}
}

void CountermeasureSystem::EjectPair(const AircraftEmitterState& aircraft, int pairIndex) {
	// ディスペンサ位置：胴体後下部
	const Vector3 baseOrigin = Add(
		aircraft.position,
		Add(Multiply(-3.2f, aircraft.forward), Multiply(-0.75f, aircraft.up)));

	// ペアごとに展開角を変えつつ、微小ランダムゆらぎで自然なバラけを与える
	const float spreadBoost = 1.0f + static_cast<float>(pairIndex) * 0.12f;

	for (int side = -1; side <= 1; side += 2) {
		const float s = static_cast<float>(side);
		const uint32_t seed = decoySerial_++;
		const float jitterR = HashSigned01(seed, 1u) * 0.08f;
		const float jitterU = HashSigned01(seed, 2u) * 0.08f;
		const float jitterF = HashSigned01(seed, 3u) * 0.06f;
		const float speedJitter = 1.0f + HashSigned01(seed, 4u) * 0.12f;
		const float lifeJitter = HashSigned01(seed, 5u) * 0.28f;

		const Vector3 portPos = Add(baseOrigin, Multiply(s * 0.9f, aircraft.right));
		const Vector3 ejectDir = Normalize(Add(
			Add(Multiply(s * (0.76f * spreadBoost) + jitterR, aircraft.right), Multiply(-0.56f + jitterU, aircraft.up)),
			Multiply(-0.32f + jitterF, aircraft.forward)));

		const float ejectSpeed = (25.0f + static_cast<float>(pairIndex) * 2.2f) * speedJitter;
		const Vector3 initialVel = Add(aircraft.velocity, Multiply(ejectSpeed, ejectDir));

		FlareDecoy decoy{};
		decoy.position = portPos;
		decoy.prevPosition = portPos;
		decoy.velocity = initialVel;
		decoy.age = 0.0f;
		decoy.lifeTime = std::clamp(2.75f + static_cast<float>(pairIndex) * 0.12f + lifeJitter, 2.3f, 3.4f);
		decoy.emitAccum = 0.0f;
		decoy.floorY = aircraft.floorY;
		decoy.seed = seed;
		if (activeFlares_.size() >= kMaxActiveFlares) {
			activeFlares_.erase(activeFlares_.begin());
		}
		activeFlares_.push_back(decoy);

		// 射出閃光＆チャフ金属片クラウドを同時展開
		EmitIgnitionFlash(portPos, aircraft.velocity);
		EmitChaffCloud(portPos, aircraft.velocity, ejectDir);
	}
}

void CountermeasureSystem::EmitIgnitionFlash(const Vector3& pos, const Vector3& vel) {
	if (EffectManager* em = EffectManager::GetInstance()) {
		if (const EffectPreset* preset = em->FindPreset("flare_chaff")) {
			for (const auto& layer : preset->layers) {
				if (layer.useGpu && layer.gpuRich.useRichGpu && layer.layerName == "IgnitionFlash") {
					EffectGpuPresetBridge::EmitOverride ov{};
					ov.addVelocityOffset = true;
					ov.velocityOffset = vel;
					EffectGpuPresetBridge::EmitLayer(layer, pos, ov);
					return;
				}
			}
		}
	}

	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	GpuParticleEmitDesc flash{};
	flash.origin = pos;
	flash.randomPositionRange = 0.25f;
	flash.minVelocity = Add(vel, { -2.0f, -2.0f, -2.0f });
	flash.maxVelocity = Add(vel, {  2.0f,  2.0f,  2.0f });
	flash.minLifeTime = 0.06f;
	flash.maxLifeTime = 0.12f;
	flash.minScale = 2.8f;
	flash.maxScale = 4.4f;
	flash.endScale = 0.4f;
	flash.scaleEasing = 0.4f;
	flash.minColor = { 1.0f, 0.40f, 0.04f, 0.95f };
	flash.maxColor = { 1.0f, 0.62f, 0.12f, 1.00f };
	flash.count = 10u;
	flash.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
	              GPUParticleFlags::kSoftParticle | GPUParticleFlags::kWorldUnitsPerSec;
	flash.shape = GPUParticleShape::FlareCore;
	flash.drag = 4.0f;
	flash.emissiveIntensity = 2.8f;
	flash.softDistance = 2.0f;
	gpu->EnqueueRichEmit(flash);
}

void CountermeasureSystem::EmitChaffCloud(const Vector3& pos, const Vector3& vel, const Vector3& ejectDir) {
	const Vector3 chaffVelBase = Add(Multiply(0.55f, vel), Multiply(14.0f, ejectDir));

	if (EffectManager* em = EffectManager::GetInstance()) {
		if (const EffectPreset* preset = em->FindPreset("flare_chaff")) {
			for (const auto& layer : preset->layers) {
				if (layer.useGpu && layer.gpuRich.useRichGpu && layer.layerName == "ChaffFoilCloud") {
					EffectGpuPresetBridge::EmitOverride ov{};
					ov.addVelocityOffset = true;
					ov.velocityOffset = chaffVelBase;
					EffectGpuPresetBridge::EmitLayer(layer, pos, ov);
					return;
				}
			}
		}
	}

	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	GpuParticleEmitDesc chaff{};
	chaff.origin = pos;
	chaff.randomPositionRange = 0.8f;
	chaff.minVelocity = Subtract(chaffVelBase, { 8.0f, 6.0f, 8.0f });
	chaff.maxVelocity = Add(chaffVelBase, { 8.0f, 6.0f, 8.0f });
	chaff.acceleration = { 0.0f, -1.6f, 0.0f };
	chaff.minLifeTime = 2.8f;
	chaff.maxLifeTime = 4.8f;
	chaff.minScale = 0.28f;
	chaff.maxScale = 0.56f;
	chaff.endScale = 0.24f;
	chaff.scaleEasing = 1.0f;
	chaff.minRotationSpeed = -12.0f;
	chaff.maxRotationSpeed = 12.0f;
	chaff.minColor = { 0.82f, 0.88f, 0.96f, 0.85f };
	chaff.maxColor = { 1.00f, 0.98f, 0.92f, 1.00f };
	chaff.count = 140u;
	chaff.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
	              GPUParticleFlags::kWakeInteract | GPUParticleFlags::kSphereEmit |
	              GPUParticleFlags::kWorldUnitsPerSec;
	chaff.shape = GPUParticleShape::ChaffFoil;
	chaff.radialSpeedMin = 3.0f;
	chaff.radialSpeedMax = 11.0f;
	chaff.drag = 2.8f;
	chaff.curlStrength = 7.5f;
	chaff.curlFrequency = 0.16f;
	chaff.emissiveIntensity = 1.45f;
	chaff.wakeSensitivity = 1.35f;
	gpu->EnqueueRichEmit(chaff);
}

void CountermeasureSystem::EmitFlareTrailSegment(const FlareDecoy& flare, float dt) {
	const float remainRatio = std::clamp(1.0f - (flare.age / flare.lifeTime), 0.0f, 1.0f);
	const Vector3 moveDelta = Subtract(flare.position, flare.prevPosition);
	const float dist = Length(moveDelta);
	const int trailSteps = std::clamp(static_cast<int>(std::ceil(dist / 0.75f)), 1, 4);

	const EffectPreset* burnPreset = EffectManager::GetInstance()
		? EffectManager::GetInstance()->FindPreset("flare_burn")
		: nullptr;
	const EffectParticleLayer* coreLayer = nullptr;
	const EffectParticleLayer* litSmokeLayer = nullptr;
	const EffectParticleLayer* whiteSmokeLayer = nullptr;
	const EffectParticleLayer* sparksLayer = nullptr;
	if (burnPreset) {
		for (const auto& l : burnPreset->layers) {
			if (!l.useGpu || !l.gpuRich.useRichGpu) { continue; }
			if (l.layerName == "OrangeFlareCore") { coreLayer = &l; }
			else if (l.layerName == "FlareLitSmoke") { litSmokeLayer = &l; }
			else if (l.layerName == "FlareWhiteSmoke") { whiteSmokeLayer = &l; }
			else if (l.layerName == "OrangeFlareSparks") { sparksLayer = &l; }
		}
	}

	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	// 1. フレア本体：光学ピンポイント白熱芯＋オレンジコロナ＋スターバースト（先端のみ）
	if (coreLayer) {
		EffectGpuPresetBridge::EmitOverride ov{};
		ov.scale = 0.60f + 0.40f * remainRatio;
		ov.addVelocityOffset = true;
		ov.velocityOffset = Multiply(0.88f, flare.velocity);
		ov.overrideLifeTime = true;
		ov.minLifeTime = (std::max)(0.025f, dt * 1.6f);
		ov.maxLifeTime = (std::max)(0.045f, dt * 2.2f);
		EffectGpuPresetBridge::EmitLayer(*coreLayer, flare.position, ov);
	} else {
		GpuParticleEmitDesc core{};
		core.origin = flare.position;
		core.randomPositionRange = 0.04f;
		core.minVelocity = Multiply(0.85f, flare.velocity);
		core.maxVelocity = Multiply(0.92f, flare.velocity);
		core.minLifeTime = (std::max)(0.025f, dt * 1.6f);
		core.maxLifeTime = (std::max)(0.045f, dt * 2.2f);
		core.minScale = 2.6f * (0.60f + 0.40f * remainRatio);
		core.maxScale = 3.8f * (0.60f + 0.40f * remainRatio);
		core.endScale = 1.5f;
		core.scaleEasing = 0.6f;
		core.minColor = { 1.0f, 0.42f, 0.04f, 0.95f };
		core.maxColor = { 1.0f, 0.58f, 0.10f, 1.00f };
		core.count = 2u;
		core.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		             GPUParticleFlags::kSoftParticle | GPUParticleFlags::kWorldUnitsPerSec;
		core.shape = GPUParticleShape::FlareCore;
		core.drag = 2.0f;
		core.emissiveIntensity = 3.2f;
		core.softDistance = 1.6f;
		gpu->EnqueueRichEmit(core);
	}

	// 2. 距離補間による途切れない白煙リボン＆直近煙へのオレンジ照り返し
	for (int i = 0; i < trailSteps; ++i) {
		const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(trailSteps);
		const Vector3 samplePos = Lerp(flare.prevPosition, flare.position, t);

		// (a) フレア直後の煙が光源でオレンジに照らされる光学照り返し
		if (litSmokeLayer) {
			EffectGpuPresetBridge::EmitOverride ov{};
			ov.alphaMultiplier = 0.35f + 0.65f * remainRatio;
			ov.countMultiplier = 1.0f / static_cast<float>(trailSteps);
			EffectGpuPresetBridge::EmitLayer(*litSmokeLayer, samplePos, ov);
		} else {
			GpuParticleEmitDesc lit{};
			lit.origin = samplePos;
			lit.randomPositionRange = 0.16f;
			lit.minVelocity = { -0.6f, 0.2f, -0.6f };
			lit.maxVelocity = {  0.6f, 1.2f,  0.6f };
			lit.acceleration = { 0.0f, 0.8f, 0.0f };
			lit.minLifeTime = 0.18f;
			lit.maxLifeTime = 0.34f;
			lit.minScale = 0.75f;
			lit.maxScale = 1.35f;
			lit.endScale = 2.6f;
			lit.scaleEasing = 0.50f;
			lit.minColor = { 1.00f, 0.42f, 0.06f, 0.22f * remainRatio };
			lit.maxColor = { 1.00f, 0.58f, 0.14f, 0.34f * remainRatio };
			lit.count = 1u;
			lit.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
			            GPUParticleFlags::kCurlNoise | GPUParticleFlags::kSoftParticle |
			            GPUParticleFlags::kWorldUnitsPerSec;
			lit.shape = GPUParticleShape::VaporMist;
			lit.drag = 2.2f;
			lit.curlStrength = 4.2f;
			lit.curlFrequency = 0.15f;
			lit.softDistance = 2.2f;
			gpu->EnqueueRichEmit(lit);
		}

		// (b) 濃密で細く始まり大きく広がる白煙トレイル
		if (whiteSmokeLayer) {
			EffectGpuPresetBridge::EmitOverride ov{};
			ov.alphaMultiplier = 0.30f + 0.70f * remainRatio;
			ov.countMultiplier = (trailSteps > 1) ? 0.65f : 1.0f;
			EffectGpuPresetBridge::EmitLayer(*whiteSmokeLayer, samplePos, ov);
		} else {
			GpuParticleEmitDesc smoke{};
			smoke.origin = samplePos;
			smoke.randomPositionRange = 0.18f;
			smoke.minVelocity = { -0.8f, 0.3f, -0.8f };
			smoke.maxVelocity = {  0.8f, 1.5f,  0.8f };
			smoke.acceleration = { 0.0f, 0.85f, 0.0f };
			smoke.minLifeTime = 2.0f;
			smoke.maxLifeTime = 3.6f;
			smoke.minScale = 0.55f;
			smoke.maxScale = 0.95f;
			smoke.endScale = 4.8f;
			smoke.scaleEasing = 0.48f;
			smoke.minRotationSpeed = -1.1f;
			smoke.maxRotationSpeed = 1.1f;
			smoke.minColor = { 0.84f, 0.87f, 0.92f, 0.26f * remainRatio + 0.05f };
			smoke.maxColor = { 0.96f, 0.96f, 0.98f, 0.42f * remainRatio + 0.08f };
			smoke.count = 2u;
			smoke.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kCurlNoise |
			              GPUParticleFlags::kSoftParticle | GPUParticleFlags::kWakeInteract |
			              GPUParticleFlags::kWorldUnitsPerSec;
			smoke.shape = GPUParticleShape::SmokeCloud;
			smoke.drag = 1.85f;
			smoke.curlStrength = 5.4f;
			smoke.curlFrequency = 0.12f;
			smoke.softDistance = 2.6f;
			smoke.wakeSensitivity = 1.0f;
			gpu->EnqueueRichEmit(smoke);
		}
	}

	// 3. 相対風で後方へたなびく繊細なオレンジ燃えカス火花（先端付近）
	if (sparksLayer) {
		EffectGpuPresetBridge::EmitOverride ov{};
		ov.overrideFloorY = true;
		ov.floorY = flare.floorY;
		ov.addVelocityOffset = true;
		ov.velocityOffset = Multiply(0.30f, flare.velocity);
		EffectGpuPresetBridge::EmitLayer(*sparksLayer, flare.position, ov);
	} else {
		GpuParticleEmitDesc sparks{};
		sparks.origin = flare.position;
		sparks.randomPositionRange = 0.12f;
		sparks.minVelocity = Add(Multiply(0.30f, flare.velocity), { -2.2f, -1.5f, -2.2f });
		sparks.maxVelocity = Add(Multiply(0.30f, flare.velocity), {  2.2f,  2.2f,  2.2f });
		sparks.acceleration = { 0.0f, -16.0f, 0.0f };
		sparks.minLifeTime = 0.24f;
		sparks.maxLifeTime = 0.58f;
		sparks.minScale = 0.05f;
		sparks.maxScale = 0.11f;
		sparks.endScale = 0.015f;
		sparks.scaleEasing = 0.7f;
		sparks.minColor = { 1.0f, 0.40f, 0.04f, 0.95f };
		sparks.maxColor = { 1.0f, 0.62f, 0.10f, 1.00f };
		sparks.count = 5u;
		sparks.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
		               GPUParticleFlags::kBlackbody | GPUParticleFlags::kStretched |
		               GPUParticleFlags::kFloorBounce | GPUParticleFlags::kSphereEmit |
		               GPUParticleFlags::kWorldUnitsPerSec;
		sparks.shape = GPUParticleShape::SparkStreak;
		sparks.radialSpeedMin = 1.2f;
		sparks.radialSpeedMax = 4.2f;
		sparks.drag = 1.35f;
		sparks.stretchFactor = 0.065f;
		sparks.minTemperature = 0.36f;
		sparks.maxTemperature = 0.58f;
		sparks.coolingRate = 0.50f;
		sparks.floorY = flare.floorY;
		sparks.useCustomFloorY = true;
		sparks.restitution = 0.42f;
		sparks.emissiveIntensity = 2.4f;
		gpu->EnqueueRichEmit(sparks);
	}
}
