#include "FlightModel.h"
#include <cmath>
#include <algorithm>

// --- 定数 ---
namespace {
	constexpr float kGravity = 9.81f;            // 重力加速度 (m/s^2)
	constexpr float kSeaLevelDensity = 1.225f;   // 海面気圧での空気密度 (kg/m^3)
	constexpr float kScaleHeight = 8500.0f;      // 大気のスケールハイト (m)
	constexpr float kPi = 3.14159265358979f;

	constexpr float kPitchRate = 1.5f;           // ピッチ角速度 (rad/s)
	constexpr float kRollRate = 2.5f;            // ロール角速度 (rad/s)
	constexpr float kYawRate = 0.8f;             // ヨー角速度 (rad/s)

	constexpr float kGDamageRate = 0.05f;        // G超過時の翼ダメージレート
	constexpr float kBlackoutOnsetG = 5.0f;      // ブラックアウト開始G
	constexpr float kRedoutOnsetG = -2.0f;       // レッドアウト開始G
	constexpr float kGEffectBuildRate = 0.1f;     // G効果の増加速度
	constexpr float kGEffectRecoveryRate = 0.5f;  // G効果の回復速度
}


// ===========================================================
// コンストラクタ
// ===========================================================
FlightModel::FlightModel()
	: position_{ 0.0f, 0.0f, 0.0f }
	, velocity_{ 0.0f, 0.0f, 0.0f }
	, acceleration_{ 0.0f, 0.0f, 0.0f }
	, orientation_{ 0.0f, 0.0f, 0.0f, 1.0f } // 単位クォータニオン（無回転）
	, currentAoA_(0.0f)
	, currentG_(1.0f)
	, isStalling_(false)
	, blackoutFactor_(0.0f)
	, redoutFactor_(0.0f)
	, blackoutToleranceTimer_(0.0f)
	, redoutToleranceTimer_(0.0f)
	, isInvulnerable_(false)
	, gEffectsEnabled_(true)
	, isAutopilot_(false)
	, targetThrottle_(0.0f)
	, inputPitch_(0.0f)
	, inputRoll_(0.0f)
	, inputYaw_(0.0f)
{
}


// ===========================================================
// 初期化
// ===========================================================
void FlightModel::Initialize(const AirframeData& airframeData, const EngineData& engineData)
{
	airframe_.Initialize(airframeData);
	engine_.Initialize(engineData);

	// 物理ステータスをリセット
	position_ = { 0.0f, 0.0f, 0.0f };
	velocity_ = { 0.0f, 0.0f, 0.0f };
	acceleration_ = { 0.0f, 0.0f, 0.0f };
	orientation_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	angularVelocity_ = { 0.0f, 0.0f, 0.0f };

	// 拡張状態のリセット
	currentAoA_ = 0.0f;
	currentG_ = 1.0f;
	isStalling_ = false;
	blackoutFactor_ = 0.0f;
	redoutFactor_ = 0.0f;
	blackoutToleranceTimer_ = 0.0f;
	redoutToleranceTimer_ = 0.0f;
	isInvulnerable_ = false;
	gEffectsEnabled_ = true;
	isAutopilot_ = false;

	// 入力状態をリセット
	targetThrottle_ = 0.0f;
	inputPitch_ = 0.0f;
	inputRoll_ = 0.0f;
	inputYaw_ = 0.0f;
}


// ===========================================================
// 毎フレームの更新処理
// ===========================================================
void FlightModel::Update(float deltaTime)
{
	// 0. ダメージモデルの火災・漏れなどの毎フレーム経過更新
	airframe_.GetDamageModel().UpdateEffects(deltaTime);

	// 1. エンジンの更新（スロットルのスプールアップ/ダウン）
	engine_.Update(deltaTime, targetThrottle_);

	// 2. 燃料消費 + 燃料漏れ（ダメージ連動）
	float fuelConsumption = engine_.GetFuelConsumptionRate() * deltaTime;
	float fuelLeak = airframe_.GetFuelLeakRate() * deltaTime;
	airframe_.ConsumeInternalFuel(fuelConsumption + fuelLeak);

	// 3. フラップ・エアブレーキの更新
	float speed = MyMath::Length(velocity_);
	airframe_.UpdateFlap(deltaTime, speed);
	airframe_.UpdateAirBrake(deltaTime);

	// 4. ペイロードの更新
	for (auto& payload : payloads_) {
		payload->Update(deltaTime);
	}

	// 4.5. オートパイロット（クリア演出・自動操縦）モード時のバイパス
	if (isAutopilot_) {
		position_ = MyMath::Add(position_, MyMath::Multiply(deltaTime, velocity_));
		currentG_ = 1.0f;
		currentAoA_ = 0.0f;
		isStalling_ = false;
		blackoutFactor_ = 0.0f;
		redoutFactor_ = 0.0f;
		blackoutToleranceTimer_ = 0.0f;
		redoutToleranceTimer_ = 0.0f;
		return;
	}

	// 5. AoA計算（姿勢更新前に現在のAoAを知っておく）
	currentAoA_ = CalculateAoA();

	// 失速判定
	isStalling_ = std::fabs(currentAoA_) > airframe_.GetCriticalAoA();

	// 6. 姿勢の更新（速度依存 + ダメージ連動の操舵）
	UpdateOrientation(deltaTime);

	// 7. 合力を計算
	MyMath::Vector3 totalForce = CalculateTotalForce(deltaTime);

	// 8. 加速度 = 力 / 質量 (F = ma → a = F/m)
	float totalMass = GetTotalMass();
	MyMath::Vector3 prevVelocity = velocity_;

	if (totalMass > 0.0f) {
		float invMass = 1.0f / totalMass;
		acceleration_ = MyMath::Multiply(invMass, totalForce);
	}

	// 9. 速度の更新 (v += a * dt)
	velocity_ = MyMath::Add(velocity_, MyMath::Multiply(deltaTime, acceleration_));

	// 10. 位置の更新 (p += v * dt)
	position_ = MyMath::Add(position_, MyMath::Multiply(deltaTime, velocity_));

	// 10.5. 地面との部位別物理衝突判定 & ダメージ計算・転倒トルク
	airframe_.GetDamageModel().ProcessGroundCollision(position_, orientation_, velocity_, angularVelocity_, deltaTime, 0.0f);

	// 11. G計算
	if (deltaTime > 0.0001f) {
		MyMath::Vector3 deltaV = MyMath::Subtract(velocity_, prevVelocity);
		MyMath::Vector3 accelNoGravity = MyMath::Multiply(1.0f / deltaTime, deltaV);
		// 重力を除外（重力は体感Gに含めない: 自由落下 = 0G）
		accelNoGravity = MyMath::Add(accelNoGravity, { 0.0f, kGravity, 0.0f });

		// 機体のUp軸方向のG成分（パイロットが感じるG）
		MyMath::Vector3 up = GetUpDirection();
		currentG_ = MyMath::Dot(accelNoGravity, up) / kGravity;
	}

	// 12. G効果の更新（ブラックアウト/レッドアウト + 構造ダメージ）
	UpdateGEffects(deltaTime);
}


// ===========================================================
// ペイロードの追加
// ===========================================================
void FlightModel::AddPayload(std::unique_ptr<IPayload> payload)
{
	payloads_.push_back(std::move(payload));
}


// ===========================================================
// 全武装を発射
// ===========================================================
void FlightModel::FireWeapons()
{
	for (auto& payload : payloads_) {
		payload->Fire();
	}
}


// ===========================================================
// 総質量の取得（機体 + エンジン + 全ペイロード）
// ===========================================================
float FlightModel::GetTotalMass() const
{
	float mass = airframe_.GetTotalMass() + engine_.GetMass();

	for (const auto& payload : payloads_) {
		mass += payload->GetWeight();
	}

	return mass;
}


// ===========================================================
// ===========================================================
float FlightModel::CalculateAirDensity(float altitude) const
{
	// 海面: 1.225 kg/m³, 指数的に減少
	// 負の高度（海面以下）はクランプ
	float alt = (std::max)(altitude, 0.0f);
	return kSeaLevelDensity * std::exp(-alt / kScaleHeight);
}


// ===========================================================
// 迎え角（AoA）の計算
// ===========================================================
float FlightModel::CalculateAoA() const
{
	float speed = MyMath::Length(velocity_);
	if (speed < 1.0f) { return 0.0f; }

	MyMath::Vector3 velDir = MyMath::Normalize(velocity_);
	MyMath::Vector3 forward = GetForwardDirection();
	MyMath::Vector3 up = GetUpDirection();

	float localForwardVel = MyMath::Dot(velDir, forward);
	float localUpVel = MyMath::Dot(velDir, up);

	// ピッチ方向の迎え角を計算
	// 速度ベクトルが機体の下側(localUpVel < 0)にある場合、正の迎え角となる
	return std::atan2(-localUpVel, localForwardVel);
}


// ===========================================================
// 迎え角に応じた揚力係数の計算
// ===========================================================
float FlightModel::CalculateLiftCoefficient(float aoa) const
{
	float criticalAoA = airframe_.GetCriticalAoA();
	float maxCL = airframe_.GetMaxLiftCoefficient();
	float stallCL = airframe_.GetStallLiftCoefficient();
	float baseCL = airframe_.GetLiftCoefficient(); // 迎え角0での揚力係数（キャンバー翼）

	float cl = 0.0f;
	if (aoa >= 0.0f) {
		if (aoa <= criticalAoA) {
			cl = baseCL + (maxCL - baseCL) * (aoa / criticalAoA);
		} else {
			float overStallRatio = (aoa - criticalAoA) / criticalAoA;
			overStallRatio = (std::min)(overStallRatio, 1.0f);
			cl = maxCL - (maxCL - stallCL) * overStallRatio;
		}
	} else {
		// 負の迎え角（背面飛行や機首下げ）
		float absAoA = -aoa;
		if (absAoA <= criticalAoA) {
			// baseCLから始まり、負方向の最大値(-maxCL)へ向かう
			cl = baseCL - (maxCL + baseCL) * (absAoA / criticalAoA);
		} else {
			float overStallRatio = (absAoA - criticalAoA) / criticalAoA;
			overStallRatio = (std::min)(overStallRatio, 1.0f);
			cl = -maxCL + (maxCL - stallCL) * overStallRatio;
		}
	}

	return cl;
}


// ===========================================================
// 速度依存の操舵効率
// ===========================================================
float FlightModel::CalculateControlEfficiency(float speed) const
{
	if (speed < 30.0f) { return speed / 30.0f * 0.3f; }
	if (speed < 100.0f) { return 0.3f + (speed - 30.0f) / 70.0f * 0.7f; }
	if (speed < 300.0f) { return 1.0f; }
	if (speed < 500.0f) { return 1.0f - (speed - 300.0f) / 200.0f * 0.5f; }
	return 0.5f;
}


// ===========================================================
// 合力の計算（全面改修版）
// ===========================================================
MyMath::Vector3 FlightModel::CalculateTotalForce(float deltaTime)
{
	(void)deltaTime;

	MyMath::Vector3 totalForce = { 0.0f, 0.0f, 0.0f };
	float speed = MyMath::Length(velocity_);
	float altitude = position_.y;

	// 高度依存の空気密度 (ISA簡易モデル)
	float airDensity = CalculateAirDensity(altitude);

	// 動圧 q = 0.5 * ρ * v²
	float dynamicPressure = 0.5f * airDensity * speed * speed;
	float wingArea = airframe_.GetWingArea();


	// --- 1. 推力（高度 + エンジンダメージ補正） ---
	float engineDamage = airframe_.GetDamageState(DamageZone::Engine);
	float thrustMag = engine_.GetThrustAtAltitude(altitude, engineDamage);
	MyMath::Vector3 forward = GetForwardDirection();
	MyMath::Vector3 thrust = MyMath::Multiply(thrustMag, forward);
	totalForce = MyMath::Add(totalForce, thrust);


	// --- 2. 重力（常に下方向に作用） ---
	float totalMass = GetTotalMass();
	MyMath::Vector3 gravity = { 0.0f, -kGravity * totalMass, 0.0f };
	totalForce = MyMath::Add(totalForce, gravity);


	// --- 3. 揚力（AoA依存 + 翼ダメージ + フラップ補正） ---
	if (speed > 1.0f) {
		float cl = CalculateLiftCoefficient(currentAoA_);

		// フラップによるCL増加
		cl += airframe_.GetFlapPosition() * airframe_.GetFlapLiftBonus();

		// 翼ダメージ補正
		float wingDmgL = airframe_.GetDamageState(DamageZone::LeftWing);
		float wingDmgR = airframe_.GetDamageState(DamageZone::RightWing);
		float wingDamageFactor = 1.0f - (wingDmgL + wingDmgR) * 0.5f * 0.8f;
		cl *= wingDamageFactor;

		float liftMag = dynamicPressure * cl * wingArea;
		MyMath::Vector3 up = GetUpDirection();
		MyMath::Vector3 lift = MyMath::Multiply(liftMag, up);
		totalForce = MyMath::Add(totalForce, lift);


		// --- 4. 誘導抵抗（揚力の副産物） ---
		float ar = airframe_.GetAspectRatio();
		float e = airframe_.GetOswaldEfficiency();
		if (ar > 0.0f && e > 0.0f) {
			float inducedCd = (cl * cl) / (kPi * ar * e);
			float inducedDragMag = dynamicPressure * inducedCd * wingArea;
			MyMath::Vector3 velDir = MyMath::Normalize(velocity_);
			MyMath::Vector3 inducedDrag = MyMath::Multiply(-inducedDragMag, velDir);
			totalForce = MyMath::Add(totalForce, inducedDrag);
		}
	}


	// --- 5. 寄生抵抗（基本 + フラップ + エアブレーキ + ペイロード + ダメージ） ---
	if (speed > 0.001f) {
		float totalCd = airframe_.GetEffectiveBaseDrag();

		// フラップの追加抵抗
		totalCd += airframe_.GetFlapPosition() * airframe_.GetFlapDragBonus();

		// エアブレーキの追加抵抗
		totalCd += airframe_.GetAirBrakePosition() * airframe_.GetAirBrakeDragBonus();

		// ペイロードの追加抵抗
		for (const auto& payload : payloads_) {
			totalCd += payload->GetDragCoeff();
		}

		float parasiteDragMag = dynamicPressure * totalCd * wingArea;
		MyMath::Vector3 velDir = MyMath::Normalize(velocity_);
		MyMath::Vector3 parasiteDrag = MyMath::Multiply(-parasiteDragMag, velDir);
		totalForce = MyMath::Add(totalForce, parasiteDrag);

		// --- 6. 横滑り抵抗（垂直尾翼による抗力 / 風見鶏効果の力の成分） ---
		MyMath::Vector3 right = GetRightDirection();
		float sideVel = MyMath::Dot(velocity_, right);
		// 横滑り速度に対する強力な抵抗をかける（垂直尾翼の面積を翼面積の約0.2倍と仮定し、高い抗力係数を掛ける）
		float sideDragMag = dynamicPressure * wingArea * 0.2f * (sideVel / speed) * 5.0f;
		MyMath::Vector3 sideForce = MyMath::Multiply(-sideDragMag, right);
		totalForce = MyMath::Add(totalForce, sideForce);
	}

	return totalForce;
}


// ===========================================================
// 姿勢（クォータニオン）の更新（速度依存 + ダメージ連動版）
// ===========================================================
void FlightModel::UpdateOrientation(float deltaTime)
{
	float speed = MyMath::Length(velocity_);

	// 速度依存の操舵効率
	float controlEff = CalculateControlEfficiency(speed);

	// ダメージによる操舵低下
	controlEff *= airframe_.GetControlDamageFactor();

	// ブラックアウト/レッドアウト中の操舵低下
	float gPenalty = 1.0f - (std::max)(blackoutFactor_, redoutFactor_) * 0.8f;
	controlEff *= gPenalty;

	// --- 重心(CG)と圧力中心(CP)の概念に基づくピッチモーメント計算 ---
	float airDensity = CalculateAirDensity(position_.y);
	float dynamicPressure = 0.5f * airDensity * speed * speed;
	float cl = CalculateLiftCoefficient(currentAoA_);
	float wingLiftForce = dynamicPressure * airframe_.GetWingArea() * std::fabs(cl);

	const float kNormalCPOffsetZ = -0.2f;
	float currentCPOffsetZ = kNormalCPOffsetZ;

	float stallDepth = 0.0f;
	if (isStalling_) {
		stallDepth = std::fabs(currentAoA_) - airframe_.GetCriticalAoA();
		float cgZ = airframe_.GetCenterOfGravityZ();
		if (cgZ >= 0.0f) {
			currentCPOffsetZ -= stallDepth * 5.0f;
		} else {
			currentCPOffsetZ += stallDepth * 5.0f;
		}
	}

	float wingTorque = wingLiftForce * (-currentCPOffsetZ);
	float tailTrimTorque = -(wingLiftForce * (-kNormalCPOffsetZ));
	float totalPitchTorque = wingTorque + tailTrimTorque;

	const float kInverseInertiaY = 0.00005f;
	float aerodynamicPitchMoment = totalPitchTorque * kInverseInertiaY;

	// オーバーシュートによる振動防止
	if (isStalling_ && deltaTime > 0.0001f) {
		float maxMoment = (stallDepth * 0.5f) / deltaTime;
		if (aerodynamicPitchMoment > maxMoment) {
			aerodynamicPitchMoment = maxMoment;
		}
	}

	if (currentAoA_ < 0.0f) {
		aerodynamicPitchMoment = -aerodynamicPitchMoment;
	}

	// =====================================================
	// 角加速度の計算（操舵入力を角加速度として扱う）
	// =====================================================
	// 操舵入力は「目標角速度」への加速度として作用
	float targetPitchRate = inputPitch_ * kPitchRate * controlEff;
	float targetRollRate  = inputRoll_  * kRollRate  * controlEff;
	float targetYawRate   = inputYaw_   * kYawRate   * controlEff;

	// 空力ダンピング係数（速度が高いほどダンピングが強い）
	// 高い値 = 素早く目標角速度に収束（振動しにくい）
	float dampingBase = 5.0f;
	float speedFactor = 1.0f;
	if (speed > 30.0f) {
		speedFactor = std::clamp(speed / 150.0f, 0.5f, 2.0f);
	}
	float damping = dampingBase * speedFactor;

	// 角速度を目標値に向けて滑らかに追従（指数的スムージング）
	float smoothT = 1.0f - std::exp(-damping * deltaTime);

	angularVelocity_.x += (targetPitchRate - angularVelocity_.x) * smoothT;
	angularVelocity_.z += (targetRollRate  - angularVelocity_.z) * smoothT;
	angularVelocity_.y += (targetYawRate   - angularVelocity_.y) * smoothT;

	// 空力ピッチモーメントは角加速度として直接加算
	angularVelocity_.x += aerodynamicPitchMoment * deltaTime;

	// 左右翼の揚力差による空気力学的ロールモーメント計算
	float asymRoll = airframe_.GetDamageModel().GetAsymmetricRollMoment();
	if (std::fabs(asymRoll) > 0.001f && speed > 5.0f) {
		float dynamicPress = 0.5f * CalculateAirDensity(position_.y) * speed * speed;
		float totalWingLift = dynamicPress * airframe_.GetWingArea() * std::fabs(CalculateLiftCoefficient(currentAoA_));
		float wingArm = 2.5f; // 翼の中心までのモーメントアーム (m)
		
		// 右翼揚力 > 左翼揚力 の時、asymRoll > 0 となり、正の角加速度を加算して左へロール（破損した左翼が落ちる）
		float rollTorque = asymRoll * (totalWingLift * 0.5f) * wingArm;
		const float kInverseInertiaZ = 0.00005f; // 慣性モーメントの逆数 (1 / Iz)
		float rollAlpha = rollTorque * kInverseInertiaZ;
		
		angularVelocity_.z += rollAlpha * deltaTime;
	}

	// =====================================================
	// 角速度から姿勢を更新
	// =====================================================
	float pitchAngle = angularVelocity_.x * deltaTime;
	float rollAngle  = angularVelocity_.z * deltaTime;
	float yawAngle   = angularVelocity_.y * deltaTime;

	// ローカル軸の取得
	MyMath::Vector3 right = GetRightDirection();
	MyMath::Vector3 up = GetUpDirection();
	MyMath::Vector3 forward = GetForwardDirection();

	// 各軸回りの微小回転クォータニオンを生成して合成
	auto makeRotation = [](const MyMath::Vector3& axis, float angle) -> MyMath::Quaternion {
		float halfAngle = angle * 0.5f;
		float s = std::sin(halfAngle);
		float c = std::cos(halfAngle);
		return { axis.x * s, axis.y * s, axis.z * s, c };
	};

	// クォータニオンの乗算
	auto qMul = [](const MyMath::Quaternion& a, const MyMath::Quaternion& b) -> MyMath::Quaternion {
		return {
			a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
			a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
			a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
			a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
		};
	};

	// ピッチ → ヨー → ロール の順に回転を適用
	MyMath::Quaternion qPitch = makeRotation(right, pitchAngle);
	MyMath::Quaternion qYaw = makeRotation(up, yawAngle);
	MyMath::Quaternion qRoll = makeRotation(forward, rollAngle);

	MyMath::Quaternion combined = qMul(qRoll, qMul(qYaw, qPitch));
	orientation_ = qMul(combined, orientation_);

	// クォータニオンの正規化
	float len = std::sqrt(
		orientation_.x * orientation_.x +
		orientation_.y * orientation_.y +
		orientation_.z * orientation_.z +
		orientation_.w * orientation_.w
	);
	if (len > 0.0001f) {
		float invLen = 1.0f / len;
		orientation_.x *= invLen;
		orientation_.y *= invLen;
		orientation_.z *= invLen;
		orientation_.w *= invLen;
	}
}


// ===========================================================
// ===========================================================
void FlightModel::UpdateGEffects(float deltaTime)
{
	// G効果が無効化されている場合、またはオートパイロット時
	if (!gEffectsEnabled_ || isAutopilot_) {
		blackoutFactor_ = 0.0f;
		redoutFactor_ = 0.0f;
		blackoutToleranceTimer_ = 0.0f;
		redoutToleranceTimer_ = 0.0f;
		return;
	}

	// --- Gエフェクト進行の共通処理 ---
	auto UpdateGEffect = [&](float currentG, float onsetG, float buildRate, float& toleranceTimer, float& factor) {
		bool isTriggered = false;
		float excess = 0.0f;
		
		// 閾値判定（OnsetG が正ならブラックアウト、負ならレッドアウト）
		if (onsetG > 0.0f && currentG > onsetG) {
			isTriggered = true;
			excess = currentG - onsetG;
		} else if (onsetG < 0.0f && currentG < onsetG) {
			isTriggered = true;
			excess = onsetG - currentG; // 超過分を正の値にする
		}

		if (isTriggered) {
			// 規定Gを超えたら、まずは猶予タイマーを進める
			toleranceTimer += deltaTime;
			if (toleranceTimer >= 0.3f) { // 0.3秒耐えたら症状が出始める
				factor += excess * buildRate * deltaTime;
			}
		} else {
			toleranceTimer -= deltaTime * 2.0f;
			toleranceTimer = (std::max)(0.0f, toleranceTimer);
			factor -= kGEffectRecoveryRate * deltaTime;
		}
		
		// 0.0 ～ 1.0 の範囲にクランプ
		factor = std::clamp(factor, 0.0f, 1.0f);
	};

	// ブラックアウト（高正G）とレッドアウト（負G）を共通処理で更新
	UpdateGEffect(currentG_, kBlackoutOnsetG, kGEffectBuildRate, blackoutToleranceTimer_, blackoutFactor_);
	UpdateGEffect(currentG_, kRedoutOnsetG, kGEffectBuildRate * 1.5f, redoutToleranceTimer_, redoutFactor_);

	// --- 構造G超過ダメージ（翼に蓄積） ---
	if (isInvulnerable_) {
		return;
	}

	float posLimit = airframe_.GetPositiveGLimit();
	float negLimit = airframe_.GetNegativeGLimit();

	if (currentG_ > posLimit) {
		float overG = currentG_ - posLimit;
		airframe_.ApplyZoneDamage(DamageZone::LeftWing, overG * kGDamageRate * deltaTime);
		airframe_.ApplyZoneDamage(DamageZone::RightWing, overG * kGDamageRate * deltaTime);
	}
	if (currentG_ < negLimit) {
		float overG = negLimit - currentG_;
		airframe_.ApplyZoneDamage(DamageZone::LeftWing, overG * kGDamageRate * deltaTime);
		airframe_.ApplyZoneDamage(DamageZone::RightWing, overG * kGDamageRate * deltaTime);
	}
}


// ===========================================================
// 方向ベクトルの取得（クォータニオンから計算）
// ===========================================================
MyMath::Vector3 FlightModel::GetForwardDirection() const
{
	// クォータニオンで (0,0,1) を回転させた結果
	float x = orientation_.x;
	float y = orientation_.y;
	float z = orientation_.z;
	float w = orientation_.w;

	return {
		2.0f * (x * z + w * y),
		2.0f * (y * z - w * x),
		1.0f - 2.0f * (x * x + y * y)
	};
}

MyMath::Vector3 FlightModel::GetUpDirection() const
{
	// クォータニオンで (0,1,0) を回転させた結果
	float x = orientation_.x;
	float y = orientation_.y;
	float z = orientation_.z;
	float w = orientation_.w;

	return {
		2.0f * (x * y - w * z),
		1.0f - 2.0f * (x * x + z * z),
		2.0f * (y * z + w * x)
	};
}

MyMath::Vector3 FlightModel::GetRightDirection() const
{
	// クォータニオンで (1,0,0) を回転させた結果
	float x = orientation_.x;
	float y = orientation_.y;
	float z = orientation_.z;
	float w = orientation_.w;

	return {
		1.0f - 2.0f * (y * y + z * z),
		2.0f * (x * y + w * z),
		2.0f * (x * z - w * y)
	};
}

MyMath::Matrix4x4 FlightModel::GetWorldMatrix() const
{
	// スケール(1,1,1)、回転(orientation_)、平行移動(position_) からアフィン行列を合成
	MyMath::Matrix4x4 rotMat = MyMath::MakeRotateMatrix(orientation_);
	MyMath::Matrix4x4 transMat = MyMath::MakeTranslateMatrix(position_);
	return MyMath::Multiply(rotMat, transMat);
}
