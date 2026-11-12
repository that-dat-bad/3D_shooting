#include "Engine.h"
#include <algorithm>
#include <cmath>

// --- 初期化処理（外部のDTOデータを受け取って自分の変数に入れる） ---
void Engine::Initialize(const EngineData& data) {
	mass_ = data.mass;
	baseThrust_ = data.baseThrust;
	normalThrottleLimit_ = data.normalThrottleLimit;
	wepThrottleLimit_ = data.wepThrottleLimit;
	enginePhysicalSpoolSpeed_ = data.physicalSpoolSpeed;
	baseFuelFlowRate_ = data.baseFuelFlowRate;
	altitudeThrottleFactor_ = data.altitudeThrottleFactor;

	currentThrottle_ = 0.0f; // エンジン停止状態でスタート
}

// --- 毎フレームの更新処理（スロットルの物理的な遅延） ---
void Engine::Update(float dt, float targetThrottle) {
	// 1. 目標値が限界（WEP上限）を超えないように安全対策（クランプ）
	if (targetThrottle > wepThrottleLimit_) {
		targetThrottle = wepThrottleLimit_;
	}

	// 2. 物理的な反応速度に従って、現在のスロットルを目標値に近づける
	if (currentThrottle_ < targetThrottle) {
		// スロットルを上げる時（スプールアップ）
		currentThrottle_ += enginePhysicalSpoolSpeed_ * dt;

		// 上がりすぎたら目標値で止める
		if (currentThrottle_ > targetThrottle) {
			currentThrottle_ = targetThrottle;
		}
	} else if (currentThrottle_ > targetThrottle) {
		// スロットルを下げる時（スプールダウン）
		currentThrottle_ -= enginePhysicalSpoolSpeed_ * dt;

		// 下がりすぎたら目標値でピタッと止める
		if (currentThrottle_ < targetThrottle) {
			currentThrottle_ = targetThrottle;
		}
	}
}

// --- 高度とエンジンダメージを考慮した推力 ---
float Engine::GetThrustAtAltitude(float altitude, float engineDamageFactor) const {
	// 高度補正: 高度が上がるほど推力低下（指数関数的）
	float altFactor = 1.0f - altitudeThrottleFactor_ * altitude;
	altFactor = (std::max)(altFactor, 0.1f); // 最低10%は維持

	// エンジンダメージで推力低下（完全破壊で90%推力喪失）
	float damageFactor = 1.0f - engineDamageFactor * 0.9f;
	damageFactor = (std::max)(damageFactor, 0.1f);

	return baseThrust_ * currentThrottle_ * altFactor * damageFactor;
}