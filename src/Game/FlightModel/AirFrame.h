#pragma once
#include <algorithm>
#include "DamageModel.h"

// --- 外部ファイルから読み込むためのデータ構造体（DTO） ---
struct AirframeData {
	float emptyFrameMass;   // 骨組みのみの空虚重量 (kg)
	float maxInternalFuel;  // 内蔵燃料の最大容量 (kg)
	float baseDrag;         // 基本の空気抵抗係数
	float liftCoefficient;  // 揚力係数（翼の性能）
	float wingArea;         // 翼面積 (m^2)
	float maxHealth;        // 最大耐久値
	float centerOfGravityZ; // 重心のZ位置 (正: 機首側, 負: 尾翼側)

	// --- 揚力・失速 ---
	float criticalAoA;          // 臨界迎え角（rad）
	float maxLiftCoefficient;   // 臨界AoA時の最大CL
	float stallLiftCoefficient; // 失速後の残留CL

	// --- 誘導抵抗 ---
	float aspectRatio;          // 翼アスペクト比
	float oswaldEfficiency;     // オズワルド効率

	// --- G制限 ---
	float positiveGLimit;       // +Gリミット
	float negativeGLimit;       // -Gリミット

	// --- フラップ ---
	float flapLiftBonus;        // フラップ展開時のCL増加量
	float flapDragBonus;        // フラップ展開時のCd増加量
	float flapMaxSpeed;         // フラップが使用可能な最大速度 (m/s)
	float flapDeploySpeed;      // フラップ展開/収納速度 (0→1の速度/sec)

	// --- エアブレーキ ---
	float airBrakeDragBonus;    // エアブレーキ展開時のCd増加量
	float airBrakeDeploySpeed;  // エアブレーキ展開/収納速度
};

// --- ダメージ部位 ---
enum class DamageZone {
	Engine,
	LeftWing,
	RightWing,
	Tail,
	FuelTank,
	Count
};

// --- 機体フレームクラス本体 ---
// 機体のフレームや物理的特性を管理するクラス
class Airframe {
public:
	Airframe() : currentInternalFuel_(0.0f), currentHealth_(0.0f),
		flapPosition_(0.0f), flapDesired_(false),
		airBrakePosition_(0.0f), airBrakeDesired_(false) {
		for (int i = 0; i < static_cast<int>(DamageZone::Count); ++i) {
			damageState_[i] = 0.0f;
		}
	}
	~Airframe() = default;

	// 初期化
	// data: 機体データ
	void Initialize(const AirframeData& data);

	// 内蔵燃料を消費
	// amount: 消費量
	void ConsumeInternalFuel(float amount);

	// ダメージ処理
	// damage: ダメージ量
	void TakeDamage(float damage);

	// --- フラップ ---
	void SetFlapDesired(bool desired) { flapDesired_ = desired; }
	void UpdateFlap(float deltaTime, float speed);
	float GetFlapPosition() const { return flapPosition_; }
	bool  IsFlapOverspeed(float speed) const { return speed > flapMaxSpeed_; }
	float GetFlapLiftBonus() const { return flapLiftBonus_; }
	float GetFlapDragBonus() const { return flapDragBonus_; }

	// --- エアブレーキ ---
	void SetAirBrakeDesired(bool desired) { airBrakeDesired_ = desired; }
	void UpdateAirBrake(float deltaTime);
	float GetAirBrakePosition() const { return airBrakePosition_; }
	float GetAirBrakeDragBonus() const { return airBrakeDragBonus_; }

	// --- ダメージ連動 ---
	void ApplyZoneDamage(DamageZone zone, float amount);
	float GetDamageState(DamageZone zone) const;

	DamageModel& GetDamageModel() { return damageModel_; }
	const DamageModel& GetDamageModel() const { return damageModel_; }

	// ダメージ補正値
	float GetEffectiveLiftCoefficient() const;   // 翼ダメージで揚力低下
	float GetEffectiveBaseDrag() const;           // ダメージで抵抗増加
	float GetControlDamageFactor() const;         // 尾翼ダメージで操舵低下
	float GetFuelLeakRate() const;                // 燃料タンクダメージで燃料漏れ

	// --- アクセッサ（既存 + 新規） ---
	float GetTotalMass() const;

	float GetCurrentInternalFuel() const { return currentInternalFuel_; }
	float GetMaxInternalFuel() const { return maxInternalFuel_; }
	float GetBaseDrag() const { return baseDrag_; }
	float GetLiftCoefficient() const { return liftCoefficient_; }
	float GetWingArea() const { return wingArea_; }
	float GetCurrentHealth() const { return currentHealth_; }
	float GetMaxHealth() const { return maxHealth_; }
	float GetCenterOfGravityZ() const { return centerOfGravityZ_; }
	bool  IsDestroyed() const;

	// 揚力・失速
	float GetCriticalAoA() const { return criticalAoA_; }
	float GetMaxLiftCoefficient() const { return maxLiftCoefficient_; }
	float GetStallLiftCoefficient() const { return stallLiftCoefficient_; }

	// 誘導抵抗
	float GetAspectRatio() const { return aspectRatio_; }
	float GetOswaldEfficiency() const { return oswaldEfficiency_; }

	float GetPositiveGLimit() const { return positiveGLimit_; }
	float GetNegativeGLimit() const { return negativeGLimit_; }

private:
	// --- 物理スペック（不変データ） ---
	float emptyFrameMass_ = 0.0f;      // 骨組みのみの空虚重量 (kg)
	float maxInternalFuel_ = 0.0f;     // 内蔵燃料の最大容量 (kg)
	float baseDrag_ = 0.0f;            // 基本空気抵抗係数
	float liftCoefficient_ = 0.0f;     // 揚力係数
	float wingArea_ = 0.0f;            // 翼面積 (m^2)
	float maxHealth_ = 0.0f;           // 最大耐久値
	float centerOfGravityZ_ = 0.0f;    // 重心位置

	// 揚力・失速
	float criticalAoA_ = 0.0f;
	float maxLiftCoefficient_ = 0.0f;
	float stallLiftCoefficient_ = 0.0f;

	// 誘導抵抗
	float aspectRatio_ = 0.0f;
	float oswaldEfficiency_ = 0.0f;

	float positiveGLimit_ = 0.0f;
	float negativeGLimit_ = 0.0f;

	// フラップ
	float flapLiftBonus_ = 0.0f;
	float flapDragBonus_ = 0.0f;
	float flapMaxSpeed_ = 0.0f;
	float flapDeploySpeed_ = 0.0f;

	// エアブレーキ
	float airBrakeDragBonus_ = 0.0f;
	float airBrakeDeploySpeed_ = 0.0f;

	// --- 現在の状態（変動データ） ---
	float currentInternalFuel_; // 現在の内蔵燃料 (kg)
	float currentHealth_;       // 現在の耐久値

	// フラップ状態
	float flapPosition_;        // 0.0 (収納) ～ 1.0 (全展開)
	bool  flapDesired_;

	// エアブレーキ状態
	float airBrakePosition_;    // 0.0 (収納) ～ 1.0 (全展開)
	bool  airBrakeDesired_;

	// ダメージ状態（各部位 0.0～1.0）
	float damageState_[static_cast<int>(DamageZone::Count)];

	// 詳細パーツダメージモデル
	DamageModel damageModel_;
};

