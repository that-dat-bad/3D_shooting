#include "AirFrame.h"
#include <algorithm>
#include <cmath>

void Airframe::Initialize(const AirframeData& data) {
	// --- 既存パラメータ ---
	emptyFrameMass_ = data.emptyFrameMass;
	maxInternalFuel_ = data.maxInternalFuel;
	baseDrag_ = data.baseDrag;
	liftCoefficient_ = data.liftCoefficient;
	wingArea_ = data.wingArea;
	maxHealth_ = data.maxHealth;
	centerOfGravityZ_ = data.centerOfGravityZ;

	// --- 揚力・失速 ---
	criticalAoA_ = data.criticalAoA;
	maxLiftCoefficient_ = data.maxLiftCoefficient;
	stallLiftCoefficient_ = data.stallLiftCoefficient;

	// --- 誘導抵抗 ---
	aspectRatio_ = data.aspectRatio;
	oswaldEfficiency_ = data.oswaldEfficiency;

	// --- G制限 ---
	positiveGLimit_ = data.positiveGLimit;
	negativeGLimit_ = data.negativeGLimit;

	// --- フラップ ---
	flapLiftBonus_ = data.flapLiftBonus;
	flapDragBonus_ = data.flapDragBonus;
	flapMaxSpeed_ = data.flapMaxSpeed;
	flapDeploySpeed_ = data.flapDeploySpeed;

	// --- エアブレーキ ---
	airBrakeDragBonus_ = data.airBrakeDragBonus;
	airBrakeDeploySpeed_ = data.airBrakeDeploySpeed;

	// --- 状態の初期化 ---
	currentInternalFuel_ = maxInternalFuel_; // 初期状態では満タン
	currentHealth_ = maxHealth_;             // 初期状態ではフル耐久

	flapPosition_ = 0.0f;
	flapDesired_ = false;
	airBrakePosition_ = 0.0f;
	airBrakeDesired_ = false;

	for (int i = 0; i < static_cast<int>(DamageZone::Count); ++i) {
		damageState_[i] = 0.0f;
	}

	damageModel_.Initialize();
}

void Airframe::ConsumeInternalFuel(float amount) {
	currentInternalFuel_ -= amount;
	currentInternalFuel_ = (std::max)(currentInternalFuel_, 0.0f);
}

void Airframe::TakeDamage(float damage) {
	currentHealth_ -= damage;
	currentHealth_ = (std::max)(currentHealth_, 0.0f);
}

float Airframe::GetTotalMass() const {
	return emptyFrameMass_ + currentInternalFuel_;
}

bool Airframe::IsDestroyed() const {
	return damageModel_.IsCriticallyDamaged() || currentHealth_ <= 0.0f;
}


// ===========================================================
// フラップの更新
// ===========================================================
void Airframe::UpdateFlap(float deltaTime, float speed) {
	// 速度超過時は強制収納（フラップ保護）
	bool canDeploy = flapDesired_ && !IsFlapOverspeed(speed);

	if (canDeploy) {
		// 展開
		flapPosition_ += flapDeploySpeed_ * deltaTime;
		if (flapPosition_ > 1.0f) { flapPosition_ = 1.0f; }
	} else {
		// 収納
		flapPosition_ -= flapDeploySpeed_ * deltaTime;
		if (flapPosition_ < 0.0f) { flapPosition_ = 0.0f; }
	}
}


// ===========================================================
// エアブレーキの更新
// ===========================================================
void Airframe::UpdateAirBrake(float deltaTime) {
	if (airBrakeDesired_) {
		airBrakePosition_ += airBrakeDeploySpeed_ * deltaTime;
		if (airBrakePosition_ > 1.0f) { airBrakePosition_ = 1.0f; }
	} else {
		airBrakePosition_ -= airBrakeDeploySpeed_ * deltaTime;
		if (airBrakePosition_ < 0.0f) { airBrakePosition_ = 0.0f; }
	}
}


// ===========================================================
// 部位ダメージの適用
// ===========================================================
void Airframe::ApplyZoneDamage(DamageZone zone, float amount) {
	int idx = static_cast<int>(zone);
	if (idx < 0 || idx >= static_cast<int>(DamageZone::Count)) { return; }

	damageState_[idx] += amount;
	if (damageState_[idx] > 1.0f) { damageState_[idx] = 1.0f; }

	// DamageModel にもブリッジ適用
	switch (zone) {
	case DamageZone::Engine:
		damageModel_.ProcessHit(DamagePart::Engine1, amount * 20.0f);
		break;
	case DamageZone::LeftWing:
		damageModel_.ProcessHit(DamagePart::Wing1_L, amount * 15.0f);
		break;
	case DamageZone::RightWing:
		damageModel_.ProcessHit(DamagePart::Wing1_R, amount * 15.0f);
		break;
	case DamageZone::Tail:
		damageModel_.ProcessHit(DamagePart::Tail, amount * 20.0f);
		break;
	case DamageZone::FuelTank:
		damageModel_.ProcessHit(DamagePart::Tank1, amount * 15.0f);
		break;
	default:
		break;
	}
}

float Airframe::GetDamageState(DamageZone zone) const {
	switch (zone) {
	case DamageZone::Engine:
		return 1.0f - damageModel_.GetEnginePowerFactor();
	case DamageZone::LeftWing:
		return 1.0f - damageModel_.GetLeftWingLiftFactor();
	case DamageZone::RightWing:
		return 1.0f - damageModel_.GetRightWingLiftFactor();
	case DamageZone::Tail:
		return 1.0f - damageModel_.GetPitchControlFactor();
	case DamageZone::FuelTank:
		return damageModel_.IsPartLeaking(DamagePart::Tank1) || damageModel_.IsPartLeaking(DamagePart::Tank2) ? 1.0f : 0.0f;
	default:
		return damageState_[static_cast<int>(zone)];
	}
}


// ===========================================================
// ダメージ補正値
// ===========================================================
float Airframe::GetEffectiveLiftCoefficient() const {
	float avgWingLift = (damageModel_.GetLeftWingLiftFactor() + damageModel_.GetRightWingLiftFactor()) * 0.5f;
	return liftCoefficient_ * avgWingLift;
}

float Airframe::GetEffectiveBaseDrag() const {
	return baseDrag_ * (1.0f + damageModel_.GetDragIncreaseFactor());
}

float Airframe::GetControlDamageFactor() const {
	return damageModel_.GetPitchControlFactor() * damageModel_.GetRollControlFactor();
}

float Airframe::GetFuelLeakRate() const {
	return damageModel_.GetTotalFuelLeakRate();
}

