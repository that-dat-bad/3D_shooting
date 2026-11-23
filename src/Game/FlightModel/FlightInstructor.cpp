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

	// ロール補正
	if (std::fabs(rawRoll) < 0.01f) {
		// 機体のUp方向とワールドUpの関係からロール角を推定
		MyMath::Vector3 up = GetUpFromOrientation(orientation);

		// ワールド上方向との差分からロール角を計算
		float rollError = up.x; // -1 ～ +1 の範囲

		// 補正入力を生成
		outRoll = rollError * correctionStrength_;
		outRoll = std::clamp(outRoll, -1.0f, 1.0f);
	}

	// ピッチ補正
	if (std::fabs(rawPitch) < 0.01f) {
		MyMath::Vector3 forward = GetForwardFromOrientation(orientation);

		float pitchError = forward.y;

		// 補正: 機首が上がっているなら下げる方向に入力
		outPitch = pitchError * correctionStrength_;
		outPitch = std::clamp(outPitch, -1.0f, 1.0f);
	}

	// ヨーは補正しない
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
