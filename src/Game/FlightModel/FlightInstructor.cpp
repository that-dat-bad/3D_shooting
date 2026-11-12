#include "FlightInstructor.h"
#include <cmath>
#include <algorithm>

void FlightInstructor::ApplyCorrection(
	float rawPitch, float rawRoll, float rawYaw,
	const MyMath::Quaternion& orientation,
	float& outPitch, float& outRoll, float& outYaw
) const
{
	// 無効時やプレイヤー入力がある場合はそのまま返す
	outPitch = rawPitch;
	outRoll = rawRoll;
	outYaw = rawYaw;

	if (!enabled_) { return; }

	// --- ロール補正（翼を水平に戻す） ---
	// プレイヤーがロール入力をしていないときだけ補正
	if (std::fabs(rawRoll) < 0.01f) {
		// 機体のUp方向とワールドUpの関係からロール角を推定
		MyMath::Vector3 up = GetUpFromOrientation(orientation);

		// ワールドUp (0,1,0) との差分からロール角を求める
		// 機体のRight方向に対するワールドUpのドット = sin(rollAngle) の近似
		// 簡易的に: up.x が正ならば右に傾いている（左ロールで戻す）
		float rollError = up.x; // -1 ～ +1 の範囲

		// 補正入力を生成（エラーに比例、強さで調整）
		outRoll = rollError * correctionStrength_;
		outRoll = std::clamp(outRoll, -1.0f, 1.0f);
	}

	// --- ピッチ補正（機首を水平に戻す） ---
	// プレイヤーがピッチ入力をしていないときだけ補正
	if (std::fabs(rawPitch) < 0.01f) {
		MyMath::Vector3 forward = GetForwardFromOrientation(orientation);

		float pitchError = forward.y;

		// 補正: 機首が上がっているなら下げる方向に入力
		outPitch = pitchError * correctionStrength_;
		outPitch = std::clamp(outPitch, -1.0f, 1.0f);
	}

	// ヨーは補正しない（水平維持には不要、コンパスヘディングは変えない）
}


MyMath::Vector3 FlightInstructor::GetUpFromOrientation(const MyMath::Quaternion& q) const
{
	float x = q.x, y = q.y, z = q.z, w = q.w;
	return {
		2.0f * (x * y - w * z),
		1.0f - 2.0f * (x * x + z * z),
		2.0f * (y * z + w * x)
	};
}

MyMath::Vector3 FlightInstructor::GetForwardFromOrientation(const MyMath::Quaternion& q) const
{
	float x = q.x, y = q.y, z = q.z, w = q.w;
	return {
		2.0f * (x * z + w * y),
		2.0f * (y * z - w * x),
		1.0f - 2.0f * (x * x + y * y)
	};
}
