#pragma once
#include "IPayload.h"

struct GunPodData {
	float baseMass;     // ポッド本体の重さ
	float drag;         // 空気抵抗係数
	float ammoWeight;   // 弾丸1発の重さ
	int maxAmmo;        // 最大弾数
	float fireRate;     // 1秒間に何発撃てるか (例: 10.0f なら秒間10発)
};

/// <summary>
/// 航空機に搭載するガンポッド（武装）を管理するクラス
/// </summary>
class GunPod : public IPayload {
public:
	GunPod() : baseMass_(0.0f), drag_(0.0f), ammoWeight_(0.0f),
	           currentAmmo_(0), maxAmmo_(0),
	           fireInterval_(0.0f), cooldownTimer_(0.0f), isAttached_(true) {}
	~GunPod() override = default;

	/// <summary>初期化処理</summary>
	void Initialize(const GunPodData& data);

	/// <summary>更新処理</summary>
	void Update(float dt) override;
	float GetWeight() const override;
	float GetDragCoeff() const override;

	/// <summary>トリガーを引いている間に呼ばれる関数</summary>
	void Fire() override;

	// --- アクセッサ ---
	int  GetCurrentAmmo() const { return currentAmmo_; }
	int  GetMaxAmmo() const { return maxAmmo_; }
	void ReplenishAmmo() { currentAmmo_ = maxAmmo_; }
	bool IsAttached() const { return isAttached_; }

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
};
