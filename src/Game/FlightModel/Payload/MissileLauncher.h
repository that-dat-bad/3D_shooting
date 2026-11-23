#pragma once
#include "IPayload.h"
#include <array>
#include "../../../engine/base/Math/Vector3.h"

struct MissileLauncherData {
	float baseMass = 40.0f;      // ランチャー・パイロン本体の重さ (kg)
	float drag = 0.015f;         // 空気抵抗係数
	float missileMass = 85.0f;   // ミサイル1発あたりの重さ (kg)
	int maxMissiles = 4;         // 最大搭載数
	float fireCooldown = 1.0f;   // 連続発射クールダウン (秒)
	std::array<MyMath::Vector3, 4> pylonLocalOffsets = { {
		{ -2.4f, -0.55f, -0.6f }, // 左内翼
		{  2.4f, -0.55f, -0.6f }, // 右内翼
		{ -3.6f, -0.55f, -1.4f }, // 左外翼
		{  3.6f, -0.55f, -1.4f }  // 右外翼
	} };
};

// 翼下ミサイルランチャー（IPayload派生）
class MissileLauncher : public IPayload {
public:
	MissileLauncher() = default;
	~MissileLauncher() override = default;

	// 初期化
	void Initialize(const MissileLauncherData& data = MissileLauncherData{});

	// IPayload 実装
	void Update(float deltaTime) override;
	void Fire() override;
	float GetWeight() const override;
	float GetDragCoeff() const override;

	// 発射を試行し、消費したパイロンのローカル座標オフセットを返す
	bool TryConsumeMissile(MyMath::Vector3& outLocalPylonOffset);

	// 補給・投棄
	void ReplenishMissiles();
	void Jettison() { isAttached_ = false; }

	// アクセッサ
	int GetCurrentMissiles() const { return currentMissiles_; }
	int GetMaxMissiles() const { return maxMissiles_; }
	bool IsReadyToFire() const { return isAttached_ && currentMissiles_ > 0 && cooldownTimer_ <= 0.0f; }
	float GetCooldownRatio() const;
	bool IsAttached() const { return isAttached_; }
	bool IsPylonLoaded(int index) const;
	MyMath::Vector3 GetPylonLocalOffset(int index) const;
	int GetPylonCount() const { return static_cast<int>(pylonLoaded_.size()); }

private:
	MissileLauncherData data_{};
	int currentMissiles_ = 0;
	int maxMissiles_ = 4;
	float cooldownTimer_ = 0.0f;
	bool isAttached_ = true;
	int nextPylonIndex_ = 0;
	std::array<bool, 4> pylonLoaded_{ { false, false, false, false } };
};
