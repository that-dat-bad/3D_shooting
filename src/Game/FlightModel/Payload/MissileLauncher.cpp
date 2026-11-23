#include "MissileLauncher.h"
#include <algorithm>

void MissileLauncher::Initialize(const MissileLauncherData& data) {
	data_ = data;
	maxMissiles_ = std::clamp(data.maxMissiles, 0, static_cast<int>(pylonLoaded_.size()));
	currentMissiles_ = maxMissiles_;
	cooldownTimer_ = 0.0f;
	isAttached_ = true;
	nextPylonIndex_ = 0;

	for (size_t i = 0; i < pylonLoaded_.size(); ++i) {
		pylonLoaded_[i] = (static_cast<int>(i) < maxMissiles_);
	}
}

void MissileLauncher::Update(float deltaTime) {
	if (cooldownTimer_ > 0.0f) {
		cooldownTimer_ = (std::max)(0.0f, cooldownTimer_ - deltaTime);
	}
}

void MissileLauncher::Fire() {
	MyMath::Vector3 unusedOffset{};
	TryConsumeMissile(unusedOffset);
}

bool MissileLauncher::TryConsumeMissile(MyMath::Vector3& outLocalPylonOffset) {
	if (!IsReadyToFire()) {
		return false;
	}

	const int count = static_cast<int>(pylonLoaded_.size());
	int consumedIndex = -1;
	for (int offset = 0; offset < count; ++offset) {
		const int idx = (nextPylonIndex_ + offset) % count;
		if (pylonLoaded_[static_cast<size_t>(idx)]) {
			consumedIndex = idx;
			break;
		}
	}

	if (consumedIndex < 0) {
		return false;
	}

	pylonLoaded_[static_cast<size_t>(consumedIndex)] = false;
	currentMissiles_ = (std::max)(0, currentMissiles_ - 1);
	cooldownTimer_ = data_.fireCooldown;
	nextPylonIndex_ = (consumedIndex + 1) % count;
	outLocalPylonOffset = data_.pylonLocalOffsets[static_cast<size_t>(consumedIndex)];
	return true;
}

void MissileLauncher::ReplenishMissiles() {
	currentMissiles_ = maxMissiles_;
	cooldownTimer_ = 0.0f;
	nextPylonIndex_ = 0;
	for (size_t i = 0; i < pylonLoaded_.size(); ++i) {
		pylonLoaded_[i] = (static_cast<int>(i) < maxMissiles_);
	}
}

float MissileLauncher::GetWeight() const {
	if (!isAttached_) {
		return 0.0f;
	}
	return data_.baseMass + (data_.missileMass * static_cast<float>(currentMissiles_));
}

float MissileLauncher::GetDragCoeff() const {
	if (!isAttached_) {
		return 0.0f;
	}
	return data_.drag;
}

float MissileLauncher::GetCooldownRatio() const {
	if (data_.fireCooldown <= 0.0001f) {
		return 0.0f;
	}
	return std::clamp(cooldownTimer_ / data_.fireCooldown, 0.0f, 1.0f);
}

bool MissileLauncher::IsPylonLoaded(int index) const {
	if (!isAttached_ || index < 0 || index >= static_cast<int>(pylonLoaded_.size())) {
		return false;
	}
	return pylonLoaded_[static_cast<size_t>(index)];
}

MyMath::Vector3 MissileLauncher::GetPylonLocalOffset(int index) const {
	if (index < 0 || index >= static_cast<int>(data_.pylonLocalOffsets.size())) {
		return { 0.0f, 0.0f, 0.0f };
	}
	return data_.pylonLocalOffsets[static_cast<size_t>(index)];
}
