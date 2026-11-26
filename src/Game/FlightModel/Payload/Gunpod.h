#pragma once
#include "IPayload.h"
#include <vector>
#include "../../../engine/base/Math/Vector3.h"

struct GunPodData {
	static constexpr int kMaxMuzzles = 4;

	float baseMass = 50.0f;     // ポッド本体の重さ
	float drag = 0.005f;        // 空気抵抗係数
	float ammoWeight = 0.1f;    // 弾丸1発の重さ
	int maxAmmo = 500;          // 最大弾数
	float fireRate = 25.0f;     // 1秒間に何発撃てるか
	std::vector<MyMath::Vector3> muzzleOffsets = { { 0.0f, 0.0f, 8.0f } }; // 機体ローカル座標の発射位置
};

// ガンポッド
class GunPod : public IPayload {
public:
	GunPod() : baseMass_(0.0f), drag_(0.0f), ammoWeight_(0.0f),
	           currentAmmo_(0), maxAmmo_(0),
	           fireInterval_(0.0f), cooldownTimer_(0.0f), isAttached_(true) {}
	~GunPod() override = default;

	// 初期化
	void Initialize(const GunPodData& data);

	// 更新
	void Update(float dt) override;
	float GetWeight() const override;
	float GetDragCoeff() const override;

	// 発射
	void Fire() override;

	// --- アクセッサ ---
	int  GetCurrentAmmo() const { return currentAmmo_; }
	int  GetMaxAmmo() const { return maxAmmo_; }
	void ReplenishAmmo() { currentAmmo_ = maxAmmo_; }
	bool IsAttached() const { return isAttached_; }
	const MyMath::Vector3& GetLastFiredMuzzleOffset() const { return lastFiredMuzzleOffset_; }
	const std::vector<MyMath::Vector3>& GetMuzzleOffsets() const { return muzzleOffsets_; }

	// ペイロードを投棄する
	void Jettison() { isAttached_ = false; }

private:
	float baseMass_;
	float drag_;
	float ammoWeight_;
	int currentAmmo_;
	int maxAmmo_;

	float fireInterval_;  // 1発撃つための間隔（秒）
	float cooldownTimer_; // 次に撃てるようになるまでのタイマー

	bool isAttached_;

	std::vector<MyMath::Vector3> muzzleOffsets_{ { 0.0f, 0.0f, 8.0f } };
	size_t nextMuzzleIndex_ = 0;
	MyMath::Vector3 lastFiredMuzzleOffset_{ 0.0f, 0.0f, 8.0f };
};
