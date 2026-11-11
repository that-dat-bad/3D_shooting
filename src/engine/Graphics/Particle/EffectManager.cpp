#include "EffectManager.h"
#include "../System/TextureManager.h"
#include "../Model/Object3dCommon.h"
#include "../../io/ConfigFile.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>

#if defined(_DEBUG) && defined(USE_IMGUI)
#include "../../../../external/imgui/imgui.h"
#endif

namespace fs = std::filesystem;

namespace {
	BlendMode ParseBlendMode(const std::string& str) {
		std::string lower = str;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (lower == "add" || lower == "additive") { return BlendMode::kAdd; }
		if (lower == "subtract" || lower == "sub") { return BlendMode::kSubtract; }
		if (lower == "multiply" || lower == "mul") { return BlendMode::kMultiply; }
		if (lower == "screen") { return BlendMode::kScreen; }
		if (lower == "opaque" || lower == "none") { return BlendMode::kOpaque; }
		return BlendMode::kNormal;
	}

	const char* BlendModeToString(BlendMode mode) {
		switch (mode) {
		case BlendMode::kOpaque: return "Opaque";
		case BlendMode::kNormal: return "Normal";
		case BlendMode::kAdd: return "Add";
		case BlendMode::kSubtract: return "Subtract";
		case BlendMode::kMultiply: return "Multiply";
		case BlendMode::kScreen: return "Screen";
		default: return "Normal";
		}
	}

	EffectStopCondition ParseStopCondition(const std::string& str) {
		std::string lower = str;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (lower == "byduration" || lower == "duration" || lower == "time" || lower == "1") {
			return EffectStopCondition::ByDuration;
		}
		if (lower == "bycount" || lower == "count" || lower == "2") {
			return EffectStopCondition::ByCount;
		}
		return EffectStopCondition::Manual;
	}

	const char* StopConditionToString(EffectStopCondition cond) {
		switch (cond) {
		case EffectStopCondition::Manual: return "Manual";
		case EffectStopCondition::ByDuration: return "ByDuration";
		case EffectStopCondition::ByCount: return "ByCount";
		default: return "Manual";
		}
	}
}

bool EffectPreset::LoadFromEffectFile(const std::string& path) {
	ConfigFile cfg;
	if (!cfg.LoadFromFile(path)) {
		return false;
	}

	filePath = path;
	const std::string stemName = fs::path(path).stem().string();
	name = cfg.GetString("General", "name", stemName);
	stopCondition = ParseStopCondition(cfg.GetString("General", "stopCondition", "Manual"));
	emitInterval = (std::max)(0.016f, cfg.GetFloat("General", "emitInterval", 0.12f));
	maxDuration = (std::max)(0.05f, cfg.GetFloat("General", "maxDuration", 5.0f));
	maxEmitCount = static_cast<uint32_t>((std::max)(1, cfg.GetInt("General", "maxEmitCount", 30)));

	const int layerCount = (std::max)(0, cfg.GetInt("General", "layerCount", 1));
	layers.clear();

	for (int i = 0; i < layerCount; ++i) {
		const std::string sec = "Layer" + std::to_string(i);
		if (!cfg.HasSection(sec)) {
			continue;
		}

		EffectParticleLayer layer{};
		layer.layerName = cfg.GetString(sec, "layerName", sec);
		layer.groupName = cfg.GetString(sec, "groupName", "ExplosionSmoke");
		layer.texturePath = cfg.GetString(sec, "texturePath", "assets/textures/smoke_particle.png");
		layer.blendMode = ParseBlendMode(cfg.GetString(sec, "blendMode", "Normal"));
		layer.emitCount = static_cast<uint32_t>((std::max)(1, cfg.GetInt(sec, "emitCount", 3)));

		ParticleParameters& p = layer.params;
		p.maxRiseHeight = cfg.GetFloat(sec, "maxRiseHeight", 5.5f);
		p.minLifeTime = cfg.GetFloat(sec, "minLifeTime", 1.5f);
		p.maxLifeTime = cfg.GetFloat(sec, "maxLifeTime", 2.5f);

		p.minVelocity.x = cfg.GetFloat(sec, "minVelocityX", -0.03f);
		p.minVelocity.y = cfg.GetFloat(sec, "minVelocityY", 0.015f);
		p.minVelocity.z = cfg.GetFloat(sec, "minVelocityZ", -0.03f);

		p.maxVelocity.x = cfg.GetFloat(sec, "maxVelocityX", 0.03f);
		p.maxVelocity.y = cfg.GetFloat(sec, "maxVelocityY", 0.04f);
		p.maxVelocity.z = cfg.GetFloat(sec, "maxVelocityZ", 0.03f);

		p.acceleration.x = cfg.GetFloat(sec, "accelerationX", 0.0f);
		p.acceleration.y = cfg.GetFloat(sec, "accelerationY", 0.0f);
		p.acceleration.z = cfg.GetFloat(sec, "accelerationZ", 0.0f);

		p.randomPositionRange = cfg.GetFloat(sec, "randomPositionRange", 1.0f);

		p.minScale = cfg.GetFloat(sec, "minScale", 4.0f);
		p.maxScale = cfg.GetFloat(sec, "maxScale", 7.0f);
		p.endScale = cfg.GetFloat(sec, "endScale", 14.0f);
		p.scaleEasing = cfg.GetFloat(sec, "scaleEasing", 0.45f);
		p.fadeOut = cfg.GetBool(sec, "fadeOut", true);

		p.minColor.x = cfg.GetFloat(sec, "minColorR", 0.1f);
		p.minColor.y = cfg.GetFloat(sec, "minColorG", 0.1f);
		p.minColor.z = cfg.GetFloat(sec, "minColorB", 0.1f);
		p.minColor.w = cfg.GetFloat(sec, "minColorA", 0.25f);

		p.maxColor.x = cfg.GetFloat(sec, "maxColorR", 0.2f);
		p.maxColor.y = cfg.GetFloat(sec, "maxColorG", 0.2f);
		p.maxColor.z = cfg.GetFloat(sec, "maxColorB", 0.2f);
		p.maxColor.w = cfg.GetFloat(sec, "maxColorA", 0.40f);

		p.minRotation = cfg.GetFloat(sec, "minRotation", 0.0f);
		p.maxRotation = cfg.GetFloat(sec, "maxRotation", 6.283185f);
		p.minRotationSpeed = cfg.GetFloat(sec, "minRotationSpeed", -0.15f);
		p.maxRotationSpeed = cfg.GetFloat(sec, "maxRotationSpeed", 0.15f);

		p.isStretched = cfg.GetBool(sec, "isStretched", false);
		p.stretchFactor = cfg.GetFloat(sec, "stretchFactor", 1.0f);

		EffectManager::GetInstance()->ApplyLayerParticleGroup(layer);
		layers.push_back(layer);
	}

	return true;
}

bool EffectPreset::SaveToEffectFile(const std::string& path) const {
	fs::path fsPath(path);
	if (fsPath.has_parent_path() && !fs::exists(fsPath.parent_path())) {
		fs::create_directories(fsPath.parent_path());
	}

	std::ofstream file(path);
	if (!file.is_open()) {
		return false;
	}

	file << "# ========================================================\n";
	file << "# Effect Configuration File (.effect)\n";
	file << "# ========================================================\n\n";

	file << "[General]\n";
	file << "name = " << name << "\n";
	file << "stopCondition = " << StopConditionToString(stopCondition) << "          # Manual / ByDuration / ByCount\n";
	file << "emitInterval = " << emitInterval << "             # 発生間隔 (秒)\n";
	file << "maxDuration = " << maxDuration << "               # 時間指定停止時の最大継続時間 (秒)\n";
	file << "maxEmitCount = " << maxEmitCount << "               # 回数指定停止時の最大発生回数 (回)\n";
	file << "layerCount = " << layers.size() << "\n\n";

	for (size_t i = 0; i < layers.size(); ++i) {
		const EffectParticleLayer& layer = layers[i];
		const ParticleParameters& p = layer.params;

		file << "[Layer" << i << "]\n";
		file << "layerName = " << layer.layerName << "\n";
		file << "groupName = " << layer.groupName << "\n";
		file << "texturePath = " << layer.texturePath << "\n";
		file << "blendMode = " << BlendModeToString(layer.blendMode) << "\n";
		file << "emitCount = " << layer.emitCount << "\n";
		file << "maxRiseHeight = " << p.maxRiseHeight << "\n";
		file << "minLifeTime = " << p.minLifeTime << "\n";
		file << "maxLifeTime = " << p.maxLifeTime << "\n";
		file << "minVelocityX = " << p.minVelocity.x << "\n";
		file << "minVelocityY = " << p.minVelocity.y << "\n";
		file << "minVelocityZ = " << p.minVelocity.z << "\n";
		file << "maxVelocityX = " << p.maxVelocity.x << "\n";
		file << "maxVelocityY = " << p.maxVelocity.y << "\n";
		file << "maxVelocityZ = " << p.maxVelocity.z << "\n";
		file << "accelerationX = " << p.acceleration.x << "\n";
		file << "accelerationY = " << p.acceleration.y << "\n";
		file << "accelerationZ = " << p.acceleration.z << "\n";
		file << "randomPositionRange = " << p.randomPositionRange << "\n";
		file << "minScale = " << p.minScale << "\n";
		file << "maxScale = " << p.maxScale << "\n";
		file << "endScale = " << p.endScale << "\n";
		file << "scaleEasing = " << p.scaleEasing << "\n";
		file << "fadeOut = " << (p.fadeOut ? "true" : "false") << "\n";
		file << "minColorR = " << p.minColor.x << "\n";
		file << "minColorG = " << p.minColor.y << "\n";
		file << "minColorB = " << p.minColor.z << "\n";
		file << "minColorA = " << p.minColor.w << "\n";
		file << "maxColorR = " << p.maxColor.x << "\n";
		file << "maxColorG = " << p.maxColor.y << "\n";
		file << "maxColorB = " << p.maxColor.z << "\n";
		file << "maxColorA = " << p.maxColor.w << "\n";
		file << "minRotation = " << p.minRotation << "\n";
		file << "maxRotation = " << p.maxRotation << "\n";
		file << "minRotationSpeed = " << p.minRotationSpeed << "\n";
		file << "maxRotationSpeed = " << p.maxRotationSpeed << "\n";
		file << "isStretched = " << (p.isStretched ? "true" : "false") << "\n";
		file << "stretchFactor = " << p.stretchFactor << "\n\n";
	}

	return true;
}

EffectHandle::EffectHandle(const std::string& effectName)
	: effectName_(effectName) {
}

const EffectPreset* EffectHandle::GetResolvedPreset() const {
	return EffectManager::GetInstance()->FindPreset(effectName_);
}

EffectStopCondition EffectHandle::GetEffectiveStopCondition() const {
	if (overrideStopCondition_) {
		return customStopCondition_;
	}
	const EffectPreset* preset = GetResolvedPreset();
	return preset ? preset->stopCondition : EffectStopCondition::Manual;
}

float EffectHandle::GetEffectiveMaxDuration() const {
	if (overrideStopCondition_ && customStopCondition_ == EffectStopCondition::ByDuration) {
		return customMaxDuration_;
	}
	const EffectPreset* preset = GetResolvedPreset();
	return preset ? preset->maxDuration : 5.0f;
}

uint32_t EffectHandle::GetEffectiveMaxEmitCount() const {
	if (overrideStopCondition_ && customStopCondition_ == EffectStopCondition::ByCount) {
		return customMaxEmitCount_;
	}
	const EffectPreset* preset = GetResolvedPreset();
	return preset ? preset->maxEmitCount : 30u;
}

float EffectHandle::GetEffectiveEmitInterval() const {
	const EffectPreset* preset = GetResolvedPreset();
	return preset ? (std::max)(0.016f, preset->emitInterval) : 0.12f;
}

void EffectHandle::emit(const Vector3& position) {
	position_ = position;
	emit();
}

void EffectHandle::emit() {
	if (pendingDelete_) {
		return;
	}
	calledThisFrame_ = true;

	// 停止条件に達している場合は再発生しない
	if (isCompleted_ && wasCalledLastFrame_) {
		return;
	}

	if (!isEmitting_) {
		isEmitting_ = true;
		isCompleted_ = false;
		elapsedSeconds_ = 0.0f;
		intervalTimer_ = 0.0f;
		emittedBurstCount_ = 0u;
		totalEmittedParticles_ = 0u;

		EmitBurstNow();
		CheckAutoStopCondition();
	}
}

void EffectHandle::stop() {
	isEmitting_ = false;
	isCompleted_ = false;
	calledThisFrame_ = false;
	wasCalledLastFrame_ = false;
	elapsedSeconds_ = 0.0f;
	intervalTimer_ = 0.0f;
}

void EffectHandle::Delete() {
	stop();
	pendingDelete_ = true;
}

void EffectHandle::EmitBurstNow() {
	const EffectPreset* preset = GetResolvedPreset();
	ParticleManager* pm = ParticleManager::GetInstance();
	if (!preset || !pm) {
		return;
	}

	for (const auto& layer : preset->layers) {
		if (layer.emitCount > 0u && !layer.groupName.empty()) {
			EffectManager::GetInstance()->ApplyLayerParticleGroup(layer);
			pm->Emit(layer.groupName, position_, layer.params, layer.emitCount);
			totalEmittedParticles_ += layer.emitCount;
		}
	}
	emittedBurstCount_ += 1u;
}

void EffectHandle::CheckAutoStopCondition() {
	const EffectStopCondition cond = GetEffectiveStopCondition();
	if (cond == EffectStopCondition::ByDuration) {
		if (elapsedSeconds_ >= GetEffectiveMaxDuration()) {
			isEmitting_ = false;
			isCompleted_ = true;
		}
	} else if (cond == EffectStopCondition::ByCount) {
		if (emittedBurstCount_ >= (std::max)(1u, GetEffectiveMaxEmitCount())) {
			isEmitting_ = false;
			isCompleted_ = true;
		}
	}
}

void EffectHandle::Update(float dt) {
	if (pendingDelete_) {
		return;
	}

	if (!calledThisFrame_ && isCompleted_) {
		isCompleted_ = false;
	}

	if (isEmitting_) {
		elapsedSeconds_ += dt;
		intervalTimer_ += dt;

		CheckAutoStopCondition();

		const float interval = GetEffectiveEmitInterval();
		while (isEmitting_ && intervalTimer_ >= interval) {
			intervalTimer_ -= interval;
			EmitBurstNow();
			CheckAutoStopCondition();
		}
	}

	wasCalledLastFrame_ = calledThisFrame_;
	calledThisFrame_ = false;
}

RingEffect::RingEffect(const Vector3& pos, const Vector4& color, float lifeTime, float maxRadius) {
	position_ = pos;
	color_ = color;
	lifeTime_ = lifeTime;
	currentTime_ = 0.0f;
	maxRadius_ = maxRadius;
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");
}

void RingEffect::Update() {
	currentTime_ += 1.0f / 60.0f;
	if (currentTime_ >= lifeTime_) {
		isDead_ = true;
	}

	float t = currentTime_ / lifeTime_;
	currentRadius_ = maxRadius_ * (1.0f - std::powf(1.0f - t, 3.0f));
	color_.w = (1.0f - t);

	scale_ = { currentRadius_, currentRadius_, currentRadius_ };
}

void RingEffect::Draw(Camera* camera) {
	if (isDead_) { return; }
	Vector3 rot = { 3.141592f / 2.0f, 0.0f, 0.0f };
	PrimitiveModel::GetInstance()->DrawRing(scale_, rot, position_, color_, textureIndex_, camera, BlendMode::kAdd);
}

CylinderEffect::CylinderEffect(const Vector3& pos, const Vector4& color, float lifeTime, float maxScaleY, float maxScaleX) {
	position_ = pos;
	color_ = color;
	lifeTime_ = lifeTime;
	currentTime_ = 0.0f;
	maxScaleY_ = maxScaleY;
	maxScaleX_ = maxScaleX;
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");
}

void CylinderEffect::Update() {
	currentTime_ += 1.0f / 60.0f;
	if (currentTime_ >= lifeTime_) {
		isDead_ = true;
	}

	float t = currentTime_ / lifeTime_;
	scale_.x = maxScaleX_ * (1.0f - t);
	scale_.z = maxScaleX_ * (1.0f - t);
	scale_.y = maxScaleY_ * std::powf(t, 0.5f);
	color_.w = (1.0f - t);
}

void CylinderEffect::Draw(Camera* camera) {
	if (isDead_) { return; }
	PrimitiveModel::GetInstance()->DrawCylinder(scale_, { 0,0,0 }, position_, color_, textureIndex_, camera, BlendMode::kAdd);
}

HitRotPlaneEffect::HitRotPlaneEffect(const Vector3& pos, const Vector4& color, float lifeTime, float size) {
	position_ = pos;
	color_ = color;
	lifeTime_ = lifeTime;
	currentTime_ = 0.0f;
	targetSize_ = size;
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");

	rotationEulers_ = { (float)(rand() % 314) / 100.0f, (float)(rand() % 314) / 100.0f, (float)(rand() % 314) / 100.0f };
	rotationSpeed_ = {
		((float)(rand() % 100) / 1000.0f) - 0.05f,
		((float)(rand() % 100) / 1000.0f) - 0.05f,
		((float)(rand() % 100) / 1000.0f) - 0.05f
	};
}

void HitRotPlaneEffect::Update() {
	currentTime_ += 1.0f / 60.0f;
	if (currentTime_ >= lifeTime_) {
		isDead_ = true;
	}

	float t = currentTime_ / lifeTime_;
	rotationEulers_.x += rotationSpeed_.x;
	rotationEulers_.y += rotationSpeed_.y;
	rotationEulers_.z += rotationSpeed_.z;

	scale_.x = targetSize_ * 0.15f * (1.0f - t);
	scale_.y = targetSize_ * 1.5f * (1.0f - t);
	scale_.z = targetSize_ * 0.15f * (1.0f - t);
	color_.w = (1.0f - t);
}

void HitRotPlaneEffect::Draw(Camera* camera) {
	if (isDead_) { return; }
	PrimitiveModel::GetInstance()->DrawPlane(scale_, rotationEulers_, position_, color_, textureIndex_, camera, BlendMode::kAdd);
}

BillboardParticleEffect::BillboardParticleEffect(const Vector3& pos, const std::string& groupName, uint32_t count) {
	position_ = pos;
	lifeTime_ = 0.1f;
	ParticleManager::GetInstance()->Emit(groupName, position_, count);
}
void BillboardParticleEffect::Update() {
	currentTime_ += 1.0f / 60.0f;
	if (currentTime_ >= lifeTime_) { isDead_ = true; }
}
void BillboardParticleEffect::Draw(Camera* camera) {
	(void)camera;
}

ExplosionParticleEffect::ExplosionParticleEffect(const Vector3& pos) {
	position_ = pos;
	lifeTime_ = 2.0f;
	currentTime_ = 0.0f;

	ParticleManager* pm = ParticleManager::GetInstance();
	flashScale_ = 30.0f;
	flashAlpha_ = 1.0f;

	for (int i = 0; i < 48; i++) {
		float angle = ((float)(rand() % 360)) * 3.141592f / 180.0f;
		Vector3 dir = { cosf(angle), 0.0f, sinf(angle) };

		ParticleParameters waveParams;
		float speed = 0.16f + (rand() % 100) / 550.0f;
		Vector3 baseVel = { dir.x * speed, 0.008f, dir.z * speed };
		Vector3 spread = { std::abs(dir.x * 0.04f), 0.008f, std::abs(dir.z * 0.04f) };

		waveParams.minVelocity = { baseVel.x - spread.x, baseVel.y - spread.y, baseVel.z - spread.z };
		waveParams.maxVelocity = { baseVel.x + spread.x, baseVel.y + spread.y, baseVel.z + spread.z };

		float shade = 0.5f + ((rand() % 100) / 100.0f) * 0.3f;
		waveParams.minColor = { shade, shade * 0.9f, shade * 0.8f, 0.02f };
		waveParams.maxColor = { shade + 0.1f, (shade + 0.1f) * 0.9f, (shade + 0.1f) * 0.8f, 0.08f };
		waveParams.minLifeTime = 1.2f;
		waveParams.maxLifeTime = 2.0f;
		waveParams.acceleration = { 0.0f, -0.0002f, 0.0f };
		waveParams.maxRiseHeight = 3.0f;
		waveParams.minScale = 5.0f;
		waveParams.maxScale = 9.5f;
		waveParams.endScale = 20.0f;
		waveParams.fadeOut = true;
		waveParams.scaleEasing = 0.5f;

		waveParams.minRotation = 0.0f;
		waveParams.maxRotation = 6.28f;
		waveParams.minRotationSpeed = -2.0f;
		waveParams.maxRotationSpeed = 2.0f;

		pm->Emit("ExplosionSmoke", position_, waveParams, 2);
	}

	lightIntensity_ = 300.0f;

	for (int i = 0; i < 32; i++) {
		Vector3 offset = {
			((rand() % 100) / 50.0f - 1.0f),
			((rand() % 100) / 50.0f - 1.0f),
			((rand() % 100) / 50.0f - 1.0f)
		};
		offset = Normalize(offset);

		ParticleParameters fireParams;
		float speed = 0.12f + (rand() % 100) / 450.0f;
		Vector3 baseVel = { offset.x * speed, offset.y * speed, offset.z * speed };
		Vector3 spread = { std::abs(baseVel.x * 0.2f), std::abs(baseVel.y * 0.2f), std::abs(baseVel.z * 0.2f) };
		fireParams.minVelocity = { baseVel.x - spread.x, baseVel.y - spread.y, baseVel.z - spread.z };
		fireParams.maxVelocity = { baseVel.x + spread.x, baseVel.y + spread.y, baseVel.z + spread.z };

		fireParams.minColor = { 1.0f, 0.4f, 0.0f, 1.0f };
		fireParams.maxColor = { 1.0f, 0.8f, 0.2f, 1.0f };
		fireParams.minLifeTime = 0.2f;
		fireParams.maxLifeTime = 0.55f;
		fireParams.acceleration = { 0.0f, 0.0f, 0.0f };
		fireParams.maxRiseHeight = 5.0f;
		fireParams.minScale = 4.5f;
		fireParams.maxScale = 10.0f;
		fireParams.endScale = 0.0f;
		fireParams.fadeOut = true;
		fireParams.scaleEasing = 0.2f;

		fireParams.minRotation = 0.0f;
		fireParams.maxRotation = 6.28f;
		fireParams.minRotationSpeed = -0.5f;
		fireParams.maxRotationSpeed = 0.5f;

		pm->Emit("ExplosionFire", position_, fireParams, 1);
	}

	for (int i = 0; i < 45; i++) {
		Vector3 offset = {
			((rand() % 100) / 50.0f - 1.0f) * 3.5f,
			((rand() % 100) / 50.0f - 0.5f) * 1.5f,
			((rand() % 100) / 50.0f - 1.0f) * 3.5f
		};

		ParticleParameters smokeParams;
		Vector3 baseVel = { offset.x * 0.035f, 0.015f, offset.z * 0.035f };
		Vector3 sSpread = { std::abs(offset.x * 0.02f) + 0.01f, 0.015f, std::abs(offset.z * 0.02f) + 0.01f };
		smokeParams.minVelocity = { baseVel.x - sSpread.x, baseVel.y - sSpread.y, baseVel.z - sSpread.z };
		smokeParams.maxVelocity = { baseVel.x + sSpread.x, baseVel.y + sSpread.y, baseVel.z + sSpread.z };

		float shade = 0.1f + ((rand() % 100) / 100.0f) * 0.18f;
		smokeParams.minColor = { shade, shade, shade, 0.02f };
		smokeParams.maxColor = { shade + 0.08f, shade + 0.08f, shade + 0.08f, 0.08f };
		smokeParams.minLifeTime = 1.6f;
		smokeParams.maxLifeTime = 2.6f;
		smokeParams.acceleration = { 0.0f, 0.0001f, 0.0f };
		smokeParams.maxRiseHeight = 6.5f;

		smokeParams.minScale = 8.0f;
		smokeParams.maxScale = 14.0f;
		smokeParams.endScale = 24.0f;
		smokeParams.fadeOut = true;
		smokeParams.scaleEasing = 0.55f;

		smokeParams.minRotation = 0.0f;
		smokeParams.maxRotation = 6.28f;
		smokeParams.minRotationSpeed = -0.4f;
		smokeParams.maxRotationSpeed = 0.4f;

		pm->Emit("ExplosionSmoke", Add(position_, offset), smokeParams, 2);
	}

	int debrisCount = 10 + (rand() % 5);
	for (int i = 0; i < debrisCount; i++) {
		Debris d;
		d.pos = position_;
		Vector3 dir = {
			((rand() % 100) / 50.0f - 1.0f),
			((rand() % 100) / 100.0f) + 0.2f,
			((rand() % 100) / 50.0f - 1.0f)
		};
		dir = Normalize(dir);
		float dSpeed = 20.0f + (rand() % 300) / 10.0f;
		d.vel = Multiply(dSpeed, dir);
		debris_.push_back(d);
	}
}

void ExplosionParticleEffect::Update() {
	float dt = 1.0f / 60.0f;
	currentTime_ += dt;
	if (currentTime_ >= lifeTime_) {
		isDead_ = true;
		return;
	}

	if (flashAlpha_ > 0.0f) {
		flashScale_ += 80.0f * dt;
		flashAlpha_ -= 8.0f * dt;
		if (flashAlpha_ < 0.0f) { flashAlpha_ = 0.0f; }
	}

	if (lightIntensity_ > 0.0f) {
		lightIntensity_ -= 600.0f * dt;
		if (lightIntensity_ < 0.0f) { lightIntensity_ = 0.0f; }
	}

	ParticleManager* pm = ParticleManager::GetInstance();
	for (auto& d : debris_) {
		if (d.pos.y < -10.0f) { continue; }

		d.pos = Add(d.pos, Multiply(dt, d.vel));
		d.vel.y -= 9.81f * 4.0f * dt;

		ParticleParameters trailParams;
		trailParams.minVelocity = { -0.02f, -0.02f, -0.02f };
		trailParams.maxVelocity = {  0.02f,  0.02f,  0.02f };
		trailParams.minColor = { 0.1f, 0.1f, 0.1f, 0.05f };
		trailParams.maxColor = { 0.2f, 0.2f, 0.2f, 0.15f };
		trailParams.minLifeTime = 0.5f;
		trailParams.maxLifeTime = 1.0f;
		trailParams.maxRiseHeight = 2.0f;
		trailParams.minScale = 3.5f;
		trailParams.maxScale = 5.5f;
		trailParams.endScale = 10.0f;
		trailParams.fadeOut = true;
		trailParams.scaleEasing = 0.5f;

		trailParams.minRotation = 0.0f;
		trailParams.maxRotation = 6.28f;
		trailParams.minRotationSpeed = -1.0f;
		trailParams.maxRotationSpeed = 1.0f;

		pm->Emit("ExplosionSmoke", d.pos, trailParams, 2);
	}
}

void ExplosionParticleEffect::Draw(Camera* camera) {
	PrimitiveModel* prim = PrimitiveModel::GetInstance();

	if (flashAlpha_ > 0.0f) {
		uint32_t flashTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");
		Vector4 flashColor = { 1.0f, 0.8f, 0.3f, flashAlpha_ };
		Vector3 scale = { flashScale_, flashScale_, flashScale_ };
		Vector3 rotate = camera->GetRotate();
		prim->DrawPlane(scale, rotate, position_, flashColor, flashTex, camera, BlendMode::kAdd);
	}

	if (lightIntensity_ > 0.0f) {
		PointLight* pLight = Object3dCommon::GetInstance()->GetPointLightData();
		if (pLight) {
			pLight->position = position_;
			pLight->color = { 1.0f, 0.5f, 0.1f, 1.0f };
			pLight->intensity = lightIntensity_;
			pLight->radius = 400.0f;
			pLight->decay = 2.5f;
		}
	}
}

MuzzleFlashEffect::MuzzleFlashEffect(const Vector3& pos, const Vector3& direction) {
	position_ = pos;
	direction_ = Normalize(direction);
	lifeTime_ = 0.1f;

	ParticleManager* pm = ParticleManager::GetInstance();

	// 火花
	{
		ParticleParameters sparkParams;
		float sparkBase = 0.12f;
		float sparkSpread = 0.08f;
		sparkParams.minVelocity = {
			direction_.x * sparkBase - sparkSpread,
			direction_.y * sparkBase - sparkSpread,
			direction_.z * sparkBase - sparkSpread
		};
		sparkParams.maxVelocity = {
			direction_.x * sparkBase + sparkSpread,
			direction_.y * sparkBase + sparkSpread,
			direction_.z * sparkBase + sparkSpread
		};
		sparkParams.minColor = { 1.0f, 0.6f, 0.1f, 1.0f };
		sparkParams.maxColor = { 1.0f, 0.8f, 0.2f, 1.0f };
		sparkParams.minLifeTime = 0.03f;
		sparkParams.maxLifeTime = 0.12f;
		sparkParams.acceleration = { 0.0f, -0.0005f, 0.0f };
		sparkParams.minScale = 0.02f;
		sparkParams.maxScale = 0.08f;
		sparkParams.endScale = 0.0f;
		sparkParams.fadeOut = true;
		sparkParams.scaleEasing = 0.5f;
		sparkParams.isStretched = true;
		sparkParams.stretchFactor = 0.08f;

		pm->Emit("ExplosionSpark", position_, sparkParams, 60);
	}

	// フラッシュ
	{
		ParticleParameters flashParams;
		flashParams.minVelocity = { 0.0f, 0.0f, 0.0f };
		flashParams.maxVelocity = { 0.0f, 0.0f, 0.0f };
		flashParams.minColor = { 1.0f, 0.5f, 0.05f, 1.0f };
		flashParams.maxColor = { 1.0f, 0.7f, 0.1f, 1.0f };
		flashParams.minLifeTime = 0.03f;
		flashParams.maxLifeTime = 0.06f;
		flashParams.acceleration = { 0.0f, 0.0f, 0.0f };
		flashParams.minScale = 1.0f;
		flashParams.maxScale = 2.0f;
		flashParams.endScale = 0.0f;
		flashParams.fadeOut = true;
		flashParams.scaleEasing = 0.2f;
		pm->Emit("ExplosionFire", position_, flashParams, 10);
	}

	// スモーク
	{
		ParticleParameters smokeParams;
		smokeParams.minVelocity = {
			-0.03f - direction_.x * 0.05f,
			-0.03f - direction_.y * 0.05f,
			-0.03f - direction_.z * 0.05f
		};
		smokeParams.maxVelocity = {
			0.03f - direction_.x * 0.05f,
			0.03f - direction_.y * 0.05f,
			0.03f - direction_.z * 0.05f
		};
		smokeParams.minColor = { 0.4f, 0.4f, 0.4f, 0.02f };
		smokeParams.maxColor = { 0.6f, 0.6f, 0.6f, 0.08f };
		smokeParams.minLifeTime = 0.25f;
		smokeParams.maxLifeTime = 0.50f;
		smokeParams.acceleration = { 0.0f, 0.0005f, 0.0f };
		smokeParams.maxRiseHeight = 1.5f;
		smokeParams.minScale = 1.8f;
		smokeParams.maxScale = 3.5f;
		smokeParams.endScale = 6.5f;
		smokeParams.fadeOut = true;
		smokeParams.scaleEasing = 0.3f;

		pm->Emit("ExplosionSmoke", position_, smokeParams, 24);
	}
}

void MuzzleFlashEffect::Update() {
	currentTime_ += 1.0f / 60.0f;
	if (currentTime_ >= lifeTime_) { isDead_ = true; }
}

void MuzzleFlashEffect::Draw(Camera* camera) {
	(void)camera;
}

std::unique_ptr<EffectManager> EffectManager::instance_ = nullptr;

EffectManager* EffectManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique<EffectManager>();
	}
	return instance_.get();
}

void EffectManager::EnsureCoreParticleGroupsCreated() {
	ParticleManager* pm = ParticleManager::GetInstance();
	if (!pm) {
		return;
	}
	pm->CreateParticleGroup("HitSpark", "assets/textures/white1x1.png");
	pm->CreateParticleGroup("Vortex", "assets/textures/circle2.png");
	pm->CreateParticleGroup("Casing", "assets/textures/white1x1.png");
	pm->CreateParticleGroup("ExplosionSpark", "assets/textures/circle.png");
	pm->SetBlendMode("ExplosionSpark", BlendMode::kAdd);
	pm->CreateParticleGroup("ExplosionFire", "assets/textures/flame_particle.png");
	pm->SetBlendMode("ExplosionFire", BlendMode::kAdd);
	pm->CreateParticleGroup("ExplosionSmoke", "assets/textures/smoke_particle.png");
	pm->SetBlendMode("ExplosionSmoke", BlendMode::kNormal);
}

void EffectManager::ApplyLayerParticleGroup(const EffectParticleLayer& layer) {
	ParticleManager* pm = ParticleManager::GetInstance();
	if (!pm || layer.groupName.empty() || layer.texturePath.empty()) {
		return;
	}
	pm->CreateParticleGroup(layer.groupName, layer.texturePath);
	pm->SetBlendMode(layer.groupName, layer.blendMode);
}

std::vector<std::string> EffectManager::GetAvailableEffectFiles(const std::string& directory) {
	std::vector<std::string> result;
	if (!fs::exists(directory) || !fs::is_directory(directory)) {
		return result;
	}
	for (const auto& entry : fs::directory_iterator(directory)) {
		if (entry.is_regular_file() && entry.path().extension() == ".effect") {
			result.push_back(entry.path().generic_string());
		}
	}
	std::sort(result.begin(), result.end());
	return result;
}

void EffectManager::LoadAllEffectsFromDirectory(const std::string& directory) {
	EnsureCoreParticleGroupsCreated();
	presets_.clear();

	const std::vector<std::string> files = GetAvailableEffectFiles(directory);
	for (const auto& file : files) {
		EffectPreset preset{};
		if (preset.LoadFromEffectFile(file)) {
			presets_.push_back(std::move(preset));
		}
	}

	// ファイルがない場合はデフォルトを生成
	if (presets_.empty()) {
		EffectPreset fallback{};
		fallback.name = "default_fire";
		fallback.filePath = directory + "/default_fire.effect";
		EffectParticleLayer layer{};
		layer.layerName = "CoreFire";
		layer.groupName = "ExplosionFire";
		layer.texturePath = "assets/textures/flame_particle.png";
		layer.blendMode = BlendMode::kAdd;
		layer.emitCount = 2u;
		layer.params.maxRiseHeight = 2.8f;
		layer.params.minLifeTime = 0.45f;
		layer.params.maxLifeTime = 0.85f;
		layer.params.minVelocity = { -0.02f, 0.015f, -0.02f };
		layer.params.maxVelocity = {  0.02f, 0.035f,  0.02f };
		layer.params.minScale = 4.0f;
		layer.params.maxScale = 6.5f;
		layer.params.endScale = 10.5f;
		layer.params.minColor = { 1.0f, 0.38f, 0.06f, 0.85f };
		layer.params.maxColor = { 1.0f, 0.88f, 0.30f, 0.95f };
		fallback.layers.push_back(layer);
		fallback.SaveToEffectFile(fallback.filePath);
		presets_.push_back(std::move(fallback));
	}
}

void EffectManager::Initialize() {
	EnsureCoreParticleGroupsCreated();
	effects_.clear();
	StopAllHandles();

	if (!isInitialized_) {
		LoadAllEffectsFromDirectory("Resources/effects");
		isInitialized_ = true;
	}
}

void EffectManager::Update() {
	for (auto it = effects_.begin(); it != effects_.end();) {
		(*it)->Update();
		if ((*it)->IsDead()) {
			it = effects_.erase(it);
		} else {
			++it;
		}
	}

	const float dt = 1.0f / 60.0f;
	for (auto it = namedHandles_.begin(); it != namedHandles_.end();) {
		if (!it->second || it->second->IsPendingDelete()) {
			it = namedHandles_.erase(it);
		} else {
			it->second->Update(dt);
			++it;
		}
	}

	for (auto it = handles_.begin(); it != handles_.end();) {
		if (!(*it) || (*it)->IsPendingDelete()) {
			it = handles_.erase(it);
		} else {
			(*it)->Update(dt);
			++it;
		}
	}
}

void EffectManager::Draw(Camera* camera) {
	for (auto& effect : effects_) {
		effect->Draw(camera);
	}
}

void EffectManager::Finalize() {
	effects_.clear();
	namedHandles_.clear();
	handles_.clear();
	instance_.reset();
}

void EffectManager::AddEffect(std::unique_ptr<IEffect> effect) {
	effects_.push_back(std::move(effect));
}

EffectPreset* EffectManager::FindPreset(const std::string& nameOrStem) {
	const std::string queryStem = fs::path(nameOrStem).stem().string();
	for (auto& p : presets_) {
		if (p.name == nameOrStem || p.name == queryStem || fs::path(p.filePath).stem().string() == queryStem) {
			return &p;
		}
	}
	return nullptr;
}

const EffectPreset* EffectManager::FindPreset(const std::string& nameOrStem) const {
	const std::string queryStem = fs::path(nameOrStem).stem().string();
	for (const auto& p : presets_) {
		if (p.name == nameOrStem || p.name == queryStem || fs::path(p.filePath).stem().string() == queryStem) {
			return &p;
		}
	}
	return nullptr;
}

EffectPreset& EffectManager::LoadOrCreatePreset(const std::string& filepath) {
	const std::string stem = fs::path(filepath).stem().string();
	if (EffectPreset* existing = FindPreset(stem)) {
		existing->LoadFromEffectFile(filepath);
		return *existing;
	}

	EffectPreset created{};
	created.name = stem;
	created.filePath = filepath;
	if (fs::exists(filepath)) {
		created.LoadFromEffectFile(filepath);
	} else {
		if (!presets_.empty()) {
			created.layers = presets_.front().layers;
		}
		created.SaveToEffectFile(filepath);
	}
	presets_.push_back(std::move(created));
	return presets_.back();
}

EffectHandle* EffectManager::GetHandle(const std::string& handleName, const std::string& effectName) {
	if (!isInitialized_) {
		Initialize();
	}
	const std::string resolvedEffect = effectName.empty() ? handleName : effectName;
	auto it = namedHandles_.find(handleName);
	if (it == namedHandles_.end() || !it->second) {
		namedHandles_[handleName] = std::make_unique<EffectHandle>(resolvedEffect);
	} else {
		it->second->SetEffectName(resolvedEffect);
	}
	return namedHandles_[handleName].get();
}

EffectHandle* EffectManager::CreateHandle(const std::string& effectName) {
	if (!isInitialized_) {
		Initialize();
	}
	handles_.push_back(std::make_unique<EffectHandle>(effectName));
	return handles_.back().get();
}

void EffectManager::DeleteHandle(EffectHandle*& handle) {
	if (!handle) {
		return;
	}
	handle->Delete();

	for (auto it = namedHandles_.begin(); it != namedHandles_.end(); ++it) {
		if (it->second.get() == handle) {
			namedHandles_.erase(it);
			handle = nullptr;
			return;
		}
	}
	for (auto it = handles_.begin(); it != handles_.end(); ++it) {
		if (it->get() == handle) {
			handles_.erase(it);
			handle = nullptr;
			return;
		}
	}
	handle = nullptr;
}

void EffectManager::StopAllHandles() {
	for (auto& [name, handle] : namedHandles_) {
		if (handle) {
			handle->stop();
		}
	}
	for (auto& handle : handles_) {
		if (handle) {
			handle->stop();
		}
	}
}

void EffectManager::EmitHitEffect(const Vector3& position) {
	Vector4 color = { 1.0f, 0.8f, 0.2f, 1.0f };
	AddEffect(std::make_unique<RingEffect>(position, color, 0.5f, 4.0f));
	AddEffect(std::make_unique<CylinderEffect>(position, color, 0.4f, 8.0f, 1.5f));

	Vector4 planeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	for (int i = 0; i < 3; i++) {
		AddEffect(std::make_unique<HitRotPlaneEffect>(position, planeColor, 0.3f, 3.0f));
	}

	AddEffect(std::make_unique<BillboardParticleEffect>(position, "HitSpark", 20));
}

void EffectManager::EmitHitPlaneEffect(const Vector3& position) {
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	for (int i = 0; i < 3; i++) {
		AddEffect(std::make_unique<HitRotPlaneEffect>(position, color, 0.3f, 3.0f));
	}
}

void EffectManager::EmitDestroyEffect(const Vector3& position) {
	AddEffect(std::make_unique<ExplosionParticleEffect>(position));
}

void EffectManager::EmitMuzzleFlash(const Vector3& position, const Vector3& direction) {
	AddEffect(std::make_unique<MuzzleFlashEffect>(position, direction));
}

#ifdef _DEBUG
void EffectManager::DrawDebugGui(EffectHandle* previewHandle) {
#ifdef USE_IMGUI
	if (!isInitialized_) {
		Initialize();
	}

	ImGui::Begin("Effect Manager (.effect Editor) [DEBUG]");

	// ファイル管理
	if (ImGui::CollapsingHeader("1. Effect File (.effect) Management", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (presets_.empty()) {
			LoadAllEffectsFromDirectory("Resources/effects");
		}

		std::vector<const char*> presetNames;
		presetNames.reserve(presets_.size());
		for (const auto& p : presets_) {
			presetNames.push_back(p.name.c_str());
		}

		if (selectedPresetIndex_ < 0 || selectedPresetIndex_ >= static_cast<int>(presets_.size())) {
			selectedPresetIndex_ = 0;
		}

		if (ImGui::Combo("Select .effect", &selectedPresetIndex_, presetNames.data(), static_cast<int>(presetNames.size()))) {
			selectedLayerIndex_ = 0;
			if (previewHandle && !presets_.empty()) {
				previewHandle->SetEffectName(presets_[selectedPresetIndex_].name);
			}
		}

		EffectPreset& activePreset = presets_[selectedPresetIndex_];
		ImGui::Text("File: %s", activePreset.filePath.c_str());

		if (ImGui::Button("Reload from .effect")) {
			if (activePreset.LoadFromEffectFile(activePreset.filePath)) {
				statusMessage_ = "Reloaded: " + activePreset.filePath;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Save (Overwrite .effect)")) {
			if (activePreset.SaveToEffectFile(activePreset.filePath)) {
				statusMessage_ = "Saved: " + activePreset.filePath;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload All Folder")) {
			LoadAllEffectsFromDirectory("Resources/effects");
			selectedPresetIndex_ = 0;
			selectedLayerIndex_ = 0;
			statusMessage_ = "Reloaded all .effect files from Resources/effects/";
		}

		ImGui::Separator();
		ImGui::InputText("New Effect Name", newEffectFileName_, sizeof(newEffectFileName_));
		ImGui::SameLine();
		if (ImGui::Button("Save As New .effect")) {
			std::string stem = newEffectFileName_;
			if (stem.empty()) {
				stem = "new_effect";
			}
			const size_t dotPos = stem.find(".effect");
			if (dotPos != std::string::npos) {
				stem = stem.substr(0, dotPos);
			}
			std::string newPath = "Resources/effects/" + stem + ".effect";
			EffectPreset copyPreset = activePreset;
			copyPreset.name = stem;
			copyPreset.filePath = newPath;
			if (copyPreset.SaveToEffectFile(newPath)) {
				presets_.push_back(std::move(copyPreset));
				selectedPresetIndex_ = static_cast<int>(presets_.size()) - 1;
				selectedLayerIndex_ = 0;
				statusMessage_ = "Created: " + newPath;
			}
		}

		if (!statusMessage_.empty()) {
			ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.55f, 1.0f), "%s", statusMessage_.c_str());
		}
	}

	if (presets_.empty()) {
		ImGui::End();
		return;
	}

	EffectPreset& preset = presets_[selectedPresetIndex_];

	// 停止条件
	if (ImGui::CollapsingHeader("2. Stop Condition & Cycle (停止条件・発生間隔)", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* condItems[] = {
			"Manual (sample->stop() まで継続)",
			"ByDuration (時間指定で自動停止)",
			"ByCount (発生回数指定で自動停止)"
		};
		int condIdx = static_cast<int>(preset.stopCondition);
		if (ImGui::Combo("Stop Condition", &condIdx, condItems, IM_ARRAYSIZE(condItems))) {
			preset.stopCondition = static_cast<EffectStopCondition>(condIdx);
		}

		ImGui::DragFloat("Emit Interval (発生間隔 秒)", &preset.emitInterval, 0.005f, 0.016f, 5.0f, "%.3f s");

		if (preset.stopCondition == EffectStopCondition::ByDuration) {
			ImGui::DragFloat("Max Duration (停止時間 秒)", &preset.maxDuration, 0.1f, 0.1f, 60.0f, "%.2f s");
		} else if (preset.stopCondition == EffectStopCondition::ByCount) {
			int countVal = static_cast<int>(preset.maxEmitCount);
			if (ImGui::DragInt("Max Emit Count (停止回数 回)", &countVal, 1.0f, 1, 1000)) {
				preset.maxEmitCount = static_cast<uint32_t>((std::max)(1, countVal));
			}
		}

		ImGui::TextDisabled("Code Usage: if (cond) { sample->emit(); }  /  sample->stop();  /  sample->Delete();");
	}

	// パーティクル設定
	if (ImGui::CollapsingHeader("3. Particle Parameters (量・上昇速度・上限高さ・加速度・寿命)", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::Button("+ Add Layer")) {
			EffectParticleLayer newLayer{};
			newLayer.layerName = "Layer" + std::to_string(preset.layers.size());
			if (!preset.layers.empty()) {
				newLayer = preset.layers.back();
				newLayer.layerName = "Layer" + std::to_string(preset.layers.size());
			}
			preset.layers.push_back(newLayer);
			selectedLayerIndex_ = static_cast<int>(preset.layers.size()) - 1;
		}
		ImGui::SameLine();
		if (preset.layers.size() > 1 && ImGui::Button("- Remove Layer")) {
			preset.layers.erase(preset.layers.begin() + selectedLayerIndex_);
			if (selectedLayerIndex_ >= static_cast<int>(preset.layers.size())) {
				selectedLayerIndex_ = static_cast<int>(preset.layers.size()) - 1;
			}
		}

		if (!preset.layers.empty()) {
			if (selectedLayerIndex_ < 0 || selectedLayerIndex_ >= static_cast<int>(preset.layers.size())) {
				selectedLayerIndex_ = 0;
			}

			for (int i = 0; i < static_cast<int>(preset.layers.size()); ++i) {
				if (i > 0) { ImGui::SameLine(); }
				char tabLabel[96];
				snprintf(tabLabel, sizeof(tabLabel), "[%d] %s", i, preset.layers[i].layerName.c_str());
				if (ImGui::RadioButton(tabLabel, selectedLayerIndex_ == i)) {
					selectedLayerIndex_ = i;
				}
			}

			EffectParticleLayer& layer = preset.layers[selectedLayerIndex_];
			ParticleParameters& pp = layer.params;

			ImGui::Separator();
			char layerNameBuf[64]{};
			strcpy_s(layerNameBuf, sizeof(layerNameBuf), layer.layerName.c_str());
			if (ImGui::InputText("Layer Name", layerNameBuf, sizeof(layerNameBuf))) {
				layer.layerName = layerNameBuf;
			}

			const char* groupPresets[] = { "ExplosionSmoke", "ExplosionFire", "ExplosionSpark", "HitSpark", "Vortex" };
			int currentGroupIdx = -1;
			for (int g = 0; g < IM_ARRAYSIZE(groupPresets); ++g) {
				if (layer.groupName == groupPresets[g]) {
					currentGroupIdx = g;
					break;
				}
			}
			if (ImGui::Combo("Particle Group", &currentGroupIdx, groupPresets, IM_ARRAYSIZE(groupPresets))) {
				if (currentGroupIdx >= 0) {
					layer.groupName = groupPresets[currentGroupIdx];
					if (layer.groupName == "ExplosionSmoke") {
						layer.texturePath = "assets/textures/smoke_particle.png";
						layer.blendMode = BlendMode::kNormal;
					} else if (layer.groupName == "ExplosionFire") {
						layer.texturePath = "assets/textures/flame_particle.png";
						layer.blendMode = BlendMode::kAdd;
					} else if (layer.groupName == "ExplosionSpark") {
						layer.texturePath = "assets/textures/circle.png";
						layer.blendMode = BlendMode::kAdd;
					} else {
						layer.texturePath = "assets/textures/white1x1.png";
						layer.blendMode = BlendMode::kAdd;
					}
					ApplyLayerParticleGroup(layer);
				}
			}

			int blendIdx = static_cast<int>(layer.blendMode);
			const char* blendItems[] = { "Opaque", "Normal (通常半透明)", "Add (加算発光)", "Subtract", "Multiply", "Screen" };
			if (ImGui::Combo("Blend Mode", &blendIdx, blendItems, IM_ARRAYSIZE(blendItems))) {
				layer.blendMode = static_cast<BlendMode>(blendIdx);
				ApplyLayerParticleGroup(layer);
			}

			ImGui::Separator();
			ImGui::Text("--- 量・寿命・上昇上限高さ ---");
			int emitCnt = static_cast<int>(layer.emitCount);
			if (ImGui::SliderInt("Emit Count (1回の発生量)", &emitCnt, 1, 60)) {
				layer.emitCount = static_cast<uint32_t>((std::max)(1, emitCnt));
			}
			ImGui::SliderFloat("Max Rise Height (上昇上限高さ m)", &pp.maxRiseHeight, 0.0f, 35.0f, "%.2f m (0=無制限)");
			ImGui::DragFloatRange2("LifeTime (寿命 Min/Max 秒)", &pp.minLifeTime, &pp.maxLifeTime, 0.05f, 0.02f, 20.0f, "Min: %.2f s", "Max: %.2f s");
			ImGui::SliderFloat("Spawn Range (発生位置ばらつき m)", &pp.randomPositionRange, 0.0f, 10.0f, "%.2f m");

			ImGui::Separator();
			ImGui::Text("--- 上昇速度・初速 & 与える加速度 ---");
			ImGui::DragFloatRange2("Upward Speed Y (上昇初速 Min/Max)", &pp.minVelocity.y, &pp.maxVelocity.y, 0.001f, -1.0f, 2.0f, "Min: %.4f", "Max: %.4f");
			float horizDrift = (std::max)(std::abs(pp.minVelocity.x), std::abs(pp.maxVelocity.x));
			if (ImGui::SliderFloat("Horizontal Spread X/Z (横広がり速度)", &horizDrift, 0.0f, 0.5f, "%.4f")) {
				pp.minVelocity.x = -horizDrift;
				pp.maxVelocity.x =  horizDrift;
				pp.minVelocity.z = -horizDrift;
				pp.maxVelocity.z =  horizDrift;
			}
			ImGui::DragFloat3("Min Velocity (X, Y, Z)", &pp.minVelocity.x, 0.001f, -5.0f, 5.0f, "%.4f");
			ImGui::DragFloat3("Max Velocity (X, Y, Z)", &pp.maxVelocity.x, 0.001f, -5.0f, 5.0f, "%.4f");
			ImGui::DragFloat3("Acceleration (加速度 X, Y, Z)", &pp.acceleration.x, 0.0001f, -1.0f, 1.0f, "%.5f");

			ImGui::Separator();
			ImGui::Text("--- スケール・カラー・回転 ---");
			ImGui::DragFloatRange2("Start Scale (初期サイズ Min/Max)", &pp.minScale, &pp.maxScale, 0.1f, 0.01f, 50.0f, "Min: %.2f", "Max: %.2f");
			ImGui::SliderFloat("End Scale (終了時サイズ)", &pp.endScale, 0.0f, 60.0f, "%.2f");
			ImGui::SliderFloat("Scale Easing (膨張カーブ)", &pp.scaleEasing, 0.1f, 2.0f, "%.2f");
			ImGui::Checkbox("Alpha Fade Out", &pp.fadeOut);
			ImGui::ColorEdit4("Min Color (RGBA)", &pp.minColor.x);
			ImGui::ColorEdit4("Max Color (RGBA)", &pp.maxColor.x);
			ImGui::DragFloatRange2("Rotation Speed (回転速度)", &pp.minRotationSpeed, &pp.maxRotationSpeed, 0.01f, -10.0f, 10.0f, "%.2f", "%.2f");
			ImGui::Checkbox("Stretched Billboard", &pp.isStretched);
			if (pp.isStretched) {
				ImGui::SliderFloat("Stretch Factor", &pp.stretchFactor, 0.01f, 20.0f);
			}
		}
	}

	// プレビュー操作
	if (ImGui::CollapsingHeader("4. Live Test Controls (emit / stop / clear)", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (previewHandle) {
			previewHandle->SetEffectName(preset.name);
			if (ImGui::Button("sample->emit() [SPACE]", ImVec2(160.0f, 32.0f))) {
				previewHandle->emit();
			}
			ImGui::SameLine();
			if (ImGui::Button("sample->stop() [X]", ImVec2(150.0f, 32.0f))) {
				previewHandle->stop();
			}
			ImGui::SameLine();
		}
		if (ImGui::Button("Clear All Particles [C]", ImVec2(170.0f, 32.0f))) {
			if (ParticleManager::GetInstance()) {
				ParticleManager::GetInstance()->ClearAllParticles();
			}
		}

		if (previewHandle) {
			ImGui::Text(
				"Handle State: %s | Elapsed: %.2f s | Bursts: %u | Emitted Particles: %u",
				previewHandle->IsEmitting() ? "EMITTING" : (previewHandle->IsCompleted() ? "COMPLETED (AUTO-STOP)" : "STOPPED"),
				previewHandle->GetElapsedSeconds(),
				previewHandle->GetEmittedBurstCount(),
				previewHandle->GetTotalEmittedParticles());
		}
		if (ParticleManager::GetInstance()) {
			ImGui::Text("Active Particles in Scene: %u", ParticleManager::GetInstance()->GetTotalActiveParticleCount());
		}
	}

	ImGui::End();
#else
	(void)previewHandle;
#endif
}
#endif
