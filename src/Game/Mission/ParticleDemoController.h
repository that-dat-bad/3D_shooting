#pragma once
#include <vector>
#include "../../engine/base/Math/MyMath.h"

class Enemy;
class EnemyManager;

/// @brief パーティクルデモ用オートパイロット＆兵装実演コントローラー
class ParticleDemoController {
public:
	struct DemoContext {
		float dt = 0.0f;
		MyMath::Vector3 position{};
		MyMath::Vector3 forward{ 0.0f, 0.0f, 1.0f };
		float speed = 0.0f;
		float normalThrottleLimit = 1.0f;
		float wepThrottleLimit = 1.15f;
		const EnemyManager* enemyManager = nullptr;
		bool missileLocked = false;
		bool missileReadyToFire = false;
		int remainingMissiles = 0;
	};

	struct DemoOutput {
		MyMath::Vector3 desiredDirection{ 0.0f, 0.0f, 1.0f };
		float throttle = 1.0f;
		bool useMissileMode = false;
		bool fireGun = false;
		bool fireMissile = false;
		bool triggerFlare = false;
		bool flapInput = false;
		bool airBrakeInput = false;
	};

	/// @brief 状態をリセットし、デモ制御の有効/無効を設定
	void Reset(bool enabled);

	/// @brief 自動操縦・兵装制御を1フレーム更新
	void Update(const DemoContext& ctx);

	bool IsActive() const { return enabled_ && !finaleTriggered_; }
	const DemoOutput& GetOutput() const { return output_; }
	float GetElapsedTime() const { return elapsed_; }
	bool HasActiveTarget() const { return hasActiveTarget_; }
	const MyMath::Vector3& GetActiveTargetPosition() const { return activeTargetPos_; }
	bool IsExplosionHoldActive() const { return explosionHoldTimer_ > 0.0f; }
	bool IsFlareFocusActive() const { return flareFocusTimer_ > 0.0f; }

	/// @brief フィナーレ撃墜トリガーを一度だけ消費して返す
	bool ConsumeFinaleDeathRequested();

private:
	struct TargetCandidate {
		bool found = false;
		MyMath::Vector3 position{};
		float distance = 0.0f;
		float forwardDot = -1.0f;
	};

	static TargetCandidate FindBestTarget(
		const MyMath::Vector3& pos,
		const MyMath::Vector3& forward,
		const EnemyManager* enemyManager,
		bool preferAir);

	static MyMath::Vector3 ApplyAltitudeSafetyAndBank(
		const MyMath::Vector3& pos,
		const MyMath::Vector3& rawDir,
		float bankLateralOffset,
		float minPitchBias);

	bool enabled_ = false;
	float elapsed_ = 0.0f;
	float missileShotCooldown_ = 0.0f;
	int flareStageTriggered_ = 0;
	bool finaleDeathRequested_ = false;
	bool finaleTriggered_ = false;
	bool hasActiveTarget_ = false;
	MyMath::Vector3 activeTargetPos_{};
	int prevDestroyedCount_ = 0;
	std::vector<bool> wasEnemyAlive_;
	float explosionHoldTimer_ = 0.0f;
	MyMath::Vector3 explosionHoldPos_{};
	float flareFocusTimer_ = 0.0f;
	bool hasFocusedFlare_ = false;
	DemoOutput output_{};
};
