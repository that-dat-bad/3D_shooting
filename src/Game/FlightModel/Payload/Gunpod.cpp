#include "Gunpod.h"

void GunPod::Initialize(const GunPodData& data) {
	baseMass_ = data.baseMass;
	drag_ = data.drag;
	ammoWeight_ = data.ammoWeight;
	maxAmmo_ = data.maxAmmo;
	currentAmmo_ = data.maxAmmo; // 初期状態ではフル装填

	// 発射間隔の計算
	if (data.fireRate > 0.0f) {
		fireInterval_ = 1.0f / data.fireRate;
	} else {
		fireInterval_ = 1.0f; // フォールバック: 秒間1発
	}

	cooldownTimer_ = 0.0f;
	isAttached_ = true;

	muzzleOffsets_ = data.muzzleOffsets;
	if (muzzleOffsets_.empty()) {
		muzzleOffsets_.push_back({ 0.0f, 0.0f, 8.0f });
	}
	nextMuzzleIndex_ = 0;
	lastFiredMuzzleOffset_ = muzzleOffsets_.front();
}

void GunPod::Update(float dt) {
	// クールダウンタイマーの更新
	if (cooldownTimer_ > 0.0f) {
		cooldownTimer_ -= dt;
		if (cooldownTimer_ < 0.0f) {
			cooldownTimer_ = 0.0f;
		}
	}
}

void GunPod::Fire() {
	// 投棄済み or 弾切れ or クールダウン中なら何もしない
	if (!isAttached_ || currentAmmo_ <= 0 || cooldownTimer_ > 0.0f) {
		return;
	}

	// 弾を1発消費
	currentAmmo_--;

	// 発射砲口を記録し、複数門の場合は交互にローテーション
	if (!muzzleOffsets_.empty()) {
		lastFiredMuzzleOffset_ = muzzleOffsets_[nextMuzzleIndex_ % muzzleOffsets_.size()];
		nextMuzzleIndex_ = (nextMuzzleIndex_ + 1) % muzzleOffsets_.size();
	}

	// クールダウンを発射間隔分セット
	cooldownTimer_ = fireInterval_;

}

float GunPod::GetWeight() const {
	if (!isAttached_) {
		return 0.0f; // 投棄済みなら重量0
	}
	return baseMass_ + (ammoWeight_ * static_cast<float>(currentAmmo_));
}

float GunPod::GetDragCoeff() const {
	if (!isAttached_) {
		return 0.0f; // 投棄済みなら抵抗0
	}
	return drag_;
}
