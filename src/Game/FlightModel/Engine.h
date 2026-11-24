#pragma once

// --- 外部ファイルから読み込むためのデータ構造体 ---
struct EngineData {
	float mass;                 // エンジンの重量
	float baseThrust;           // 定格推力
	float normalThrottleLimit;  // 通常のスロットル上限
	float wepThrottleLimit;     // WEP時のスロットル上限k
	float physicalSpoolSpeed;   // エンジンの物理的な反応速度
	float baseFuelFlowRate;     // 定格推力時の燃料消費量
	float altitudeThrottleFactor; // 高度による推力低下率。典型: 0.00004
};

// エンジンクラス
class Engine {
public:
	Engine() : currentThrottle_(0.0f), altitudeThrottleFactor_(0.0f) {}
	~Engine() = default;

	// 初期化
	// data: エンジンパラメータ
	void Initialize(const EngineData& data);

	// 更新
	// dt: デルタタイム
	// targetThrottle: 目標スロットル値
	void Update(float dt, float targetThrottle);

	// --- アクセッサ ---
	float GetMass() const { return mass_; }
	float GetCurrentThrust() const { return baseThrust_ * currentThrottle_; }
	float GetThrustAtAltitude(float altitude, float engineDamageFactor) const;
	float GetFuelConsumptionRate() const { return baseFuelFlowRate_ * currentThrottle_; }
	float GetCurrentThrottle() const { return currentThrottle_; }
	float GetNormalThrottleLimit() const { return normalThrottleLimit_; }
	float GetWepThrottleLimit() const { return wepThrottleLimit_; }
	bool HasWep() const { return wepThrottleLimit_ > normalThrottleLimit_ + 0.005f; }
	bool IsWepActive() const { return HasWep() && currentThrottle_ > normalThrottleLimit_ + 0.005f; }
	float GetAfterburnerRatio() const;
	void SetThrottle(float throttle) { currentThrottle_ = throttle; }

private:
	// --- 物理スペック ---
	float mass_ = 0.0f;                        // エンジン自体の重量
	float baseThrust_ = 0.0f;                  // 定格推力
	float normalThrottleLimit_ = 0.0f;         // 通常スロットル上限
	float wepThrottleLimit_ = 0.0f;            // WEP時スロットル上限
	float enginePhysicalSpoolSpeed_ = 0.0f;    // エンジンの物理的な反応速度
	float baseFuelFlowRate_ = 0.0f;            // 定格推力時の燃料消費率
	float altitudeThrottleFactor_ = 0.0f;      // 高度による推力低下率

	// --- 現在の状態 ---
	float currentThrottle_;             // 現在の実際のエンジン出力
};