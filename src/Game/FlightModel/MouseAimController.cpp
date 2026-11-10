#include "MouseAimController.h"
#include <cmath>
#include <algorithm>

namespace {
	constexpr float kPi = 3.14159265358979f;
	constexpr float kTwoPi = 6.283185307179586f;

	// War Thunder準拠：天頂・天底のジンバル反転を防止する最大仰角（±88.5度）
	constexpr float kMaxSinElevation = 0.9996f;

	// デッドゾーン（この角度以下の微小差分は入力ゼロ）
	constexpr float kDeadZone = 0.003f; // ≈ 0.17度

	// マウス移動量 → 角度変換のスケール (rad per mouse_delta per sensitivity)
	constexpr float kAngleScale = 0.02f;

	// 角度を [-pi, pi] に正規化
	inline float WrapAngle(float a) {
		while (a > kPi) a -= kTwoPi;
		while (a < -kPi) a += kTwoPi;
		return a;
	}

	// 軸・角度回転（ロドリゲスの公式）
	inline MyMath::Vector3 RotateVectorAxisAngle(const MyMath::Vector3& v, const MyMath::Vector3& axis, float angle) {
		float cosA = std::cos(angle);
		float sinA = std::sin(angle);
		float dot = v.x * axis.x + v.y * axis.y + v.z * axis.z;
		MyMath::Vector3 cross = {
			axis.y * v.z - axis.z * v.y,
			axis.z * v.x - axis.x * v.z,
			axis.x * v.y - axis.y * v.x
		};
		return {
			v.x * cosA + cross.x * sinA + axis.x * dot * (1.0f - cosA),
			v.y * cosA + cross.y * sinA + axis.y * dot * (1.0f - cosA),
			v.z * cosA + cross.z * sinA + axis.z * dot * (1.0f - cosA)
		};
	}

	inline MyMath::Vector3 SafeNormalize(const MyMath::Vector3& v, const MyMath::Vector3& fallback = { 0.0f, 0.0f, 1.0f }) {
		float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
		if (lenSq > 0.000001f) {
			float invLen = 1.0f / std::sqrt(lenSq);
			return { v.x * invLen, v.y * invLen, v.z * invLen };
		}
		return fallback;
	}
}


// ===========================================================
// 初期化
// ===========================================================
void MouseAimController::Initialize()
{
	targetYaw_ = 0.0f;
	targetPitch_ = 0.0f;
	targetDirection_ = { 0.0f, 0.0f, 1.0f };

	// --- PIDゲインの設定 (War Thunder インストラクター応答性) ---
	// ピッチ: 素早く目標を捉え、かつオーバーシュートを抑える
	pitchPID_.SetGains(4.0f, 0.2f, 1.2f, 0.5f);
	// ロール: Bank-and-Pull のための超高速ロール応答
	rollPID_.SetGains(3.5f, 0.1f, 0.8f, 0.3f);
	// ヨー: 微小ズレの精密補正
	yawPID_.SetGains(3.0f, 0.2f, 0.6f, 0.3f);
	// 水平復帰ロール
	bankPID_.SetGains(2.5f, 0.1f, 0.5f, 0.3f);
}


// ===========================================================
// マウス相対移動量でカメラ視点基準の目標方向を回転 (War Thunder準拠)
// ===========================================================
void MouseAimController::UpdateTargetDirection(long deltaX, long deltaY, const MyMath::Vector3& camRight, const MyMath::Vector3& camUp)
{
	(void)camUp;
	if (!enabled_) { return; }
	if (deltaX == 0 && deltaY == 0) { return; }

	float yawDelta = static_cast<float>(deltaX) * sensitivity_ * kAngleScale;
	float pitchDelta = static_cast<float>(deltaY) * sensitivity_ * kAngleScale;

	// 1フレーム内の極端なOSスパイク入力に対する安全ガード
	constexpr float kMaxStepAngle = 0.85f;
	float stepAngle = std::sqrt(yawDelta * yawDelta + pitchDelta * pitchDelta);
	if (stepAngle > kMaxStepAngle) {
		float scale = kMaxStepAngle / stepAngle;
		yawDelta *= scale;
		pitchDelta *= scale;
	}

	// 1. ピッチ回転: 画面左右軸（カメラ右軸 camRight）回りに回転
	// マウス上移動 (deltaY < 0, pitchDelta < 0) で機首上げ（+Y方向）、下移動で機首下げ（-Y方向）
	MyMath::Vector3 rAxis = SafeNormalize(camRight, { 1.0f, 0.0f, 0.0f });
	MyMath::Vector3 dirAfterPitch = RotateVectorAxisAngle(targetDirection_, rAxis, pitchDelta);

	// 2. ヨー回転: ワールド上軸 (0, 1, 0) 回りに回転（地平線に沿って水平旋回）
	// マウス右移動 (deltaX > 0) で右旋回
	const MyMath::Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
	MyMath::Vector3 dirAfterYaw = RotateVectorAxisAngle(dirAfterPitch, worldUp, yawDelta);

	// 3. ジンバルロックガード（天頂・天底の反転防止）
	if (dirAfterYaw.y > kMaxSinElevation) {
		dirAfterYaw.y = kMaxSinElevation;
	} else if (dirAfterYaw.y < -kMaxSinElevation) {
		dirAfterYaw.y = -kMaxSinElevation;
	}

	targetDirection_ = SafeNormalize(dirAfterYaw, targetDirection_);
}


// ===========================================================
// 目標方向を指定の前方ベクトルにリセット
// ===========================================================
void MouseAimController::ResetToDirection(const MyMath::Vector3& forward)
{
	targetDirection_ = SafeNormalize(forward, { 0.0f, 0.0f, 1.0f });

	pitchPID_.Reset();
	rollPID_.Reset();
	yawPID_.Reset();
	bankPID_.Reset();
}


// ===========================================================
// War Thunder準拠 インストラクター操舵入力生成 (Bank and Pull)
// ===========================================================
void MouseAimController::CalculateSteeringInput(
	const MyMath::Quaternion& orientation,
	float deltaTime,
	float& outPitch, float& outRoll, float& outYaw
)
{
	if (!enabled_ || deltaTime < 0.0001f) {
		outPitch = 0.0f;
		outRoll = 0.0f;
		outYaw = 0.0f;
		return;
	}

	// 機体の現在方向ベクトルを取得
	MyMath::Vector3 forward = GetForwardFromOrientation(orientation);
	MyMath::Vector3 up = GetUpFromOrientation(orientation);
	MyMath::Vector3 right = GetRightFromOrientation(orientation);

	// 目標方向を機体のローカル座標系に射影
	float localYaw = MyMath::Dot(targetDirection_, right);     // 右への成分 (+: 右, -: 左)
	float localPitch = MyMath::Dot(targetDirection_, up);       // 上への成分 (+: 上, -: 下)
	float forwardDot = MyMath::Dot(targetDirection_, forward); // 前方成分 (+: 前, -: 後)

	float transverseDist = std::sqrt(localYaw * localYaw + localPitch * localPitch);
	float angleError = std::atan2(transverseDist, forwardDot); // 目標との総角度誤差 [0, pi]

	// デッドゾーン判定: すでに目標を捉えている場合は微小入力をカット
	if (angleError < kDeadZone) {
		outPitch = 0.0f;
		outRoll = 0.0f;
		outYaw = 0.0f;
		pitchPID_.integral *= 0.95f;
		yawPID_.integral *= 0.95f;
		rollPID_.integral *= 0.95f;
		bankPID_.integral *= 0.95f;
		return;
	}

	// =========================================================================
	// 1. ロール目標の決定 (War Thunder Instructor: 旋回バンク と 水平復帰)
	// =========================================================================

	// A. 旋回用バンク角 (Bank to Turn):
	// 目標方向へ機体上面 (up) を向けるためのロール角度誤差 [-pi, pi]
	// localYaw > 0 (右) => phiTurn > 0 (右ロール)
	// localYaw < 0 (左) => phiTurn < 0 (左ロール)
	// localPitch < 0 (下・斜め下) => |phiTurn| > pi/2 (反転・背面急降下 / スプリットS)
	float phiTurn = std::atan2(localYaw, localPitch);

	// B. 水平復帰用バンク角 (Leveling / Horizon Alignment):
	// 機首 forward に垂直な平面へワールド上方向 (0, 1, 0) を射影
	const MyMath::Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
	float fDotUp = MyMath::Dot(worldUp, forward);
	MyMath::Vector3 projUp = {
		worldUp.x - fDotUp * forward.x,
		worldUp.y - fDotUp * forward.y,
		worldUp.z - fDotUp * forward.z
	};
	MyMath::Vector3 horizonUp = SafeNormalize(projUp, up);

	// 機体の現在の上 (up) と horizonUp との間のロール角度誤差
	MyMath::Vector3 upCross = {
		up.y * horizonUp.z - up.z * horizonUp.y,
		up.z * horizonUp.x - up.x * horizonUp.z,
		up.x * horizonUp.y - up.y * horizonUp.x
	};
	float sinPhiLevel = MyMath::Dot(upCross, forward);
	float cosPhiLevel = MyMath::Dot(up, horizonUp);
	float phiLevel = std::atan2(sinPhiLevel, cosPhiLevel);

	// C. 旋回重み (Turn Weight) の計算:
	// 目標誤差 angleError が小さい時は水平維持・精密エイム、
	// angleError が大きい時は Bank-and-Pull (最大ロール＆ピッチ) を発動！
	// 0.04 rad (≈ 2.3°) から 0.20 rad (≈ 11.5°) の間で滑らかにブレンド
	constexpr float kFineThreshold = 0.040f;
	constexpr float kTurnThreshold = 0.200f;
	float tTurn = std::clamp((angleError - kFineThreshold) / (kTurnThreshold - kFineThreshold), 0.0f, 1.0f);
	float wTurn = tTurn * tTurn * (3.0f - 2.0f * tTurn); // SmoothStep

	// D. 望ましいロール角度誤差 (phiDesired) の最短回転経路ブレンド:
	// 目標が右 (phiTurn > 0) の時は右バンク (outRoll < 0) したいので目標角は -phiTurn
	// 右バンク中 (phiLevel > 0) から水平に戻す時は左ロール (outRoll > 0) したいので目標角は +phiLevel
	float deltaPhi = WrapAngle((-phiTurn) - phiLevel);
	float phiDesired = WrapAngle(phiLevel + wTurn * deltaPhi);

	// ロール入力の計算 (PD制御 + アグレッシブなロール操舵)
	float rollOut = rollPID_.Update(phiDesired, deltaTime);
	float rollMultiplier = 1.0f + wTurn * 1.5f; // 旋回初期は最速でバンク姿勢へ移行
	outRoll = std::clamp(rollOut * rollMultiplier, -1.0f, 1.0f);

	// =========================================================================
	// 2. ピッチ入力の決定 (Elevator: Bank-and-Pull)
	// =========================================================================
	// 機体上面が旋回方向を向いている度合い (Alignment)
	// cos(phiTurn) > 0 のとき上面が目標側、1.0 で完全一致
	float rollAlignment = std::clamp(std::cos(phiTurn), 0.0f, 1.0f);

	// 大角度旋回時: ロールが合っていれば全力でエレベーターを引き (-1.0f)、過負荷G旋回を行う
	float turnPitchDemand = -std::clamp(angleError * 3.5f, 0.0f, 1.0f) * rollAlignment;

	// 小角度精密エイム時: localPitch 誤差に対する高精度 PID
	float finePitchDemand = -pitchPID_.Update(localPitch, deltaTime);

	float combinedPitch = (1.0f - wTurn) * finePitchDemand + wTurn * turnPitchDemand;
	outPitch = std::clamp(combinedPitch, -1.0f, 1.0f);

	// =========================================================================
	// 3. ヨー入力の決定 (Rudder)
	// =========================================================================
	// 小角度時: ラダーを積極的に使って左右の微小ズレを精密にゼロへ詰める
	float fineYaw = yawPID_.Update(localYaw, deltaTime);
	// 大角度旋回時: 横滑りによる失速を防ぎつつ、協調旋回をアシスト
	float turnYaw = std::clamp(localYaw * 0.15f, -0.15f, 0.15f);

	float combinedYaw = (1.0f - wTurn) * fineYaw + wTurn * turnYaw;
	outYaw = std::clamp(combinedYaw, -1.0f, 1.0f);
}


// ===========================================================
// ヘルパー: クォータニオンから方向ベクトルを取得
// ===========================================================
MyMath::Vector3 MouseAimController::GetForwardFromOrientation(const MyMath::Quaternion& q) const
{
	float x = q.x, y = q.y, z = q.z, w = q.w;
	return {
		2.0f * (x * z + w * y),
		2.0f * (y * z - w * x),
		1.0f - 2.0f * (x * x + y * y)
	};
}

MyMath::Vector3 MouseAimController::GetUpFromOrientation(const MyMath::Quaternion& q) const
{
	float x = q.x, y = q.y, z = q.z, w = q.w;
	return {
		2.0f * (x * y - w * z),
		1.0f - 2.0f * (x * x + z * z),
		2.0f * (y * z + w * x)
	};
}

MyMath::Vector3 MouseAimController::GetRightFromOrientation(const MyMath::Quaternion& q) const
{
	float x = q.x, y = q.y, z = q.z, w = q.w;
	return {
		1.0f - 2.0f * (y * y + z * z),
		2.0f * (x * y + w * z),
		2.0f * (x * z - w * y)
	};
}
