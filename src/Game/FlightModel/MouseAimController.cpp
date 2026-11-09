#include "MouseAimController.h"
#include <cmath>
#include <algorithm>

namespace {
	constexpr float kPi = 3.14159265358979f;
	constexpr float kTwoPi = 6.283185307179586f;

	// War Thunder準拠：天頂・天底のジンバル反転を防止する最大仰角（±89度）
	constexpr float kMaxElevation = 89.0f * (kPi / 180.0f);

	// デッドゾーン（この角度以下の差分は無視）
	constexpr float kDeadZone = 0.005f;      // ≈ 0.3度

	// バンク角の最大値（コーディネートターン時）
	constexpr float kMaxBankAngle = 1.2f;    // ≈ 70度

	// マウス移動量 → 角度変換のスケール (rad per mouse_delta per sensitivity)
	constexpr float kAngleScale = 0.02f;
}


// ===========================================================
// 初期化
// ===========================================================
void MouseAimController::Initialize()
{
	targetYaw_ = 0.0f;
	targetPitch_ = 0.0f;
	targetDirection_ = { 0.0f, 0.0f, 1.0f };

	// --- PIDゲインの初期設定 ---
	pitchPID_.SetGains(2.5f, 0.3f, 0.8f, 0.5f);
	rollPID_.SetGains(2.0f, 0.1f, 0.6f, 0.3f);
	yawPID_.SetGains(2.0f, 0.2f, 0.5f, 0.3f);
	bankPID_.SetGains(1.0f, 0.1f, 0.4f, 0.3f);
}


// ===========================================================
// マウス相対移動量でワールド空間の目標方向を回転（War Thunder準拠・球面方位角/仰角更新）
// ===========================================================
void MouseAimController::UpdateTargetDirection(long deltaX, long deltaY, const MyMath::Vector3& camRight, const MyMath::Vector3& camUp)
{
	(void)camRight;
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

	targetYaw_ += yawDelta;
	if (targetYaw_ > kPi) {
		targetYaw_ = std::fmod(targetYaw_ + kPi, kTwoPi) - kPi;
	} else if (targetYaw_ < -kPi) {
		targetYaw_ = std::fmod(targetYaw_ - kPi, kTwoPi) + kPi;
	}

	targetPitch_ = std::clamp(targetPitch_ - pitchDelta, -kMaxElevation, kMaxElevation);

	const float cosPitch = std::cos(targetPitch_);
	targetDirection_ = {
		cosPitch * std::sin(targetYaw_),
		std::sin(targetPitch_),
		cosPitch * std::cos(targetYaw_)
	};
}


// ===========================================================
// 目標方向を指定の前方ベクトルにリセット
// ===========================================================
void MouseAimController::ResetToDirection(const MyMath::Vector3& forward)
{
	float len = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	MyMath::Vector3 dir = { 0.0f, 0.0f, 1.0f };
	if (len > 0.0001f) {
		float invLen = 1.0f / len;
		dir = { forward.x * invLen, forward.y * invLen, forward.z * invLen };
	}

	targetPitch_ = std::clamp(std::asin(std::clamp(dir.y, -1.0f, 1.0f)), -kMaxElevation, kMaxElevation);
	float horizLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
	if (horizLen > 0.0001f) {
		targetYaw_ = std::atan2(dir.x, dir.z);
	}

	const float cosPitch = std::cos(targetPitch_);
	targetDirection_ = {
		cosPitch * std::sin(targetYaw_),
		std::sin(targetPitch_),
		cosPitch * std::cos(targetYaw_)
	};

	pitchPID_.Reset();
	rollPID_.Reset();
	yawPID_.Reset();
	bankPID_.Reset();
}


// ===========================================================
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


	// =====================================================
	// 機体前方方向と目標方向の差分を計算
	// =====================================================
	MyMath::Vector3 forward = GetForwardFromOrientation(orientation);
	MyMath::Vector3 up = GetUpFromOrientation(orientation);
	MyMath::Vector3 right = GetRightFromOrientation(orientation);

	// 目標方向を機体のローカル座標系に射影
	float localPitchError = MyMath::Dot(targetDirection_, up);
	float localYawError = MyMath::Dot(targetDirection_, right);
	float forwardDot = MyMath::Dot(targetDirection_, forward);

	// 角度差分の推定
	float pitchAngleError = std::asin(std::clamp(localPitchError, -1.0f, 1.0f));
	float yawAngleError = std::asin(std::clamp(localYawError, -1.0f, 1.0f));

	// 目標が後方にある場合の補正
	if (forwardDot < 0.0f) {
		if (yawAngleError > 0.0f) {
			yawAngleError = kPi - yawAngleError;
		} else {
			yawAngleError = -kPi - yawAngleError;
		}
	}

	// =====================================================
	// =====================================================
	float absYawError = std::fabs(yawAngleError);

	if (absYawError < kDeadZone && std::fabs(pitchAngleError) < kDeadZone) {
		outPitch = 0.0f;
		outRoll = 0.0f;
		outYaw = 0.0f;
		// 積分値を徐々に減衰（デッドゾーン内で急にリセットしない）
		pitchPID_.integral *= 0.95f;
		yawPID_.integral *= 0.95f;
		rollPID_.integral *= 0.95f;
		return;
	}

	// 現在のバンク角の推定
	MyMath::Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
	float currentBankIndicator = MyMath::Dot(right, worldUp);

	if (absYawError > coordinatedTurnThreshold_) {
		// =============================================
		// コーディネートターン: バンク＆プル (ロールしてピッチで引く)
		// =============================================

		// ヨーの誤差に応じて目標バンク角を決定。横移動に素早く反応させるため係数を大きめにする。
		float targetBank = std::clamp(-yawAngleError * 3.5f, -kMaxBankAngle, kMaxBankAngle);

		float bankError = targetBank - currentBankIndicator;
		float rollOutput = rollPID_.Update(bankError, deltaTime);
		outRoll = std::clamp(rollOutput, -1.0f, 1.0f);

		// ピッチは純粋に目標方向（レティクル）へ向けるための誤差から直接計算
		float pitchOutput = pitchPID_.Update(pitchAngleError, deltaTime);
		outPitch = std::clamp(-pitchOutput, -1.0f, 1.0f);

		// ヨー（ラダー）は機首の横滑りを抑えつつ追従を補助する程度に
		float yawOutput = yawPID_.Update(yawAngleError, deltaTime);
		outYaw = std::clamp(yawOutput * 0.4f, -1.0f, 1.0f);

	} else {
		// =============================================
		// 小角度修正: 各軸PIDで直接制御
		// =============================================

		float pitchOutput = pitchPID_.Update(pitchAngleError, deltaTime);
		outPitch = std::clamp(-pitchOutput, -1.0f, 1.0f);

		float yawOutput = yawPID_.Update(yawAngleError, deltaTime);
		outYaw = std::clamp(yawOutput, -1.0f, 1.0f);

		float levelError = -currentBankIndicator;
		float bankOutput = bankPID_.Update(levelError, deltaTime);
		outRoll = std::clamp(bankOutput, -1.0f, 1.0f);
	}
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
