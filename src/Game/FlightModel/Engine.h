#pragma once

// --- 外部ファイルから読み込むためのデータ構造体（DTO） ---
struct EngineData {
	float mass;                 // エンジンの重量
	float baseThrust;           // 定格推力（スロットル1.0時の推力）
	float normalThrottleLimit;  // 通常のスロットル上限（1.0）
	float wepThrottleLimit;     // WEP時のスロットル上限k
	float physicalSpoolSpeed;   // エンジンの物理的な反応速度
	float baseFuelFlowRate;     // 定格推力時(1.0)の燃料消費量
	float altitudeThrottleFactor; // 高度による推力低下率。典型: 0.00004
};

// --- エンジンクラス本体 ---
// エンジン
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

	// --- アクセッサ（外部から情報を取得） ---
	float GetMass() const { return mass_; }
	float GetCurrentThrust() const { return baseThrust_ * currentThrottle_; }
	float GetThrustAtAltitude(float altitude, float engineDamageFactor) const;
	float GetFuelConsumptionRate() const { return baseFuelFlowRate_ * currentThrottle_; }
	float GetCurrentThrottle() const { return currentThrottle_; }
	void SetThrottle(float throttle) { currentThrottle_ = throttle; }

private:
	// --- 物理スペック（不変データ） ---
	float mass_ = 0.0f;                        // エンジン自体の重量
	float baseThrust_ = 0.0f;                  // 定格推力（1.0時の推力）
	float normalThrottleLimit_ = 0.0f;         // 通常スロットル上限（基本1.0）
	float wepThrottleLimit_ = 0.0f;            // WEP時スロットル上限（例: 1.1）
	float enginePhysicalSpoolSpeed_ = 0.0f;    // エンジンの物理的な反応速度
	float baseFuelFlowRate_ = 0.0f;            // 定格推力時(1.0)の燃料消費率
	float altitudeThrottleFactor_ = 0.0f;      // 高度による推力低下率

	// --- 現在の状態（変動データ） ---
	float currentThrottle_;             // 現在の実際のエンジン出力（0.0 ～ WEP上限）
};