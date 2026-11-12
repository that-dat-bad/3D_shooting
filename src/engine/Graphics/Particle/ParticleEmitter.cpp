#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include "../base/Math/MyMath.h"
using namespace MyMath;

ParticleEmitter::ParticleEmitter(const std::string& name, const Transform& transform, uint32_t count, float frequency)
	: name_(name), transform_(transform), count_(count), frequency_(frequency), frequencyTime_(0.0f) {
}

void ParticleEmitter::Update() {
	frequencyTime_ += 1.0f / 60.0f;

	if (frequency_ <= frequencyTime_) {
		// パーティクル発生
		ParticleManager::GetInstance()->Emit(name_, transform_.translate, params_, count_);

		frequencyTime_ -= frequency_;
	}
}

void ParticleEmitter::Emit() {
	ParticleManager::GetInstance()->Emit(name_, transform_.translate, params_, count_);
}