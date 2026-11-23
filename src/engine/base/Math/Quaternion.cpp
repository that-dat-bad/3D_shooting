#include "Quaternion.h"
#include <cmath>

using namespace MyMath;

Matrix4x4 MyMath::MakeRotateMatrix(const Quaternion& q) {
	Matrix4x4 result = Identity4x4();
	result.m[0][0] = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	result.m[0][1] = 2.0f * (q.x * q.y + q.w * q.z);
	result.m[0][2] = 2.0f * (q.x * q.z - q.w * q.y);
	result.m[0][3] = 0.0f;

	result.m[1][0] = 2.0f * (q.x * q.y - q.w * q.z);
	result.m[1][1] = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
	result.m[1][2] = 2.0f * (q.y * q.z + q.w * q.x);
	result.m[1][3] = 0.0f;

	result.m[2][0] = 2.0f * (q.x * q.z + q.w * q.y);
	result.m[2][1] = 2.0f * (q.y * q.z - q.w * q.x);
	result.m[2][2] = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	result.m[2][3] = 0.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;
	return result;
}

Quaternion MyMath::Slerp(const MyMath::Quaternion& q1, const MyMath::Quaternion& q2, float t) {
	float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
	Quaternion q3 = { q2.x, q2.y, q2.z, q2.w };

	// 逆回転のほうが近い場合は反転させる
	if (dot < 0.0f) {
		q3 = { -q2.x, -q2.y, -q2.z, -q2.w };
		dot = -dot;
	}

	// 角度が近すぎる場合はゼロ除算を防ぐため普通の線形補間で済ませる
	if (dot > 0.9995f) {
		Quaternion res = {
			q1.x + t * (q3.x - q1.x), q1.y + t * (q3.y - q1.y),
			q1.z + t * (q3.z - q1.z), q1.w + t * (q3.w - q1.w)
		};
		float len = std::sqrt(res.x * res.x + res.y * res.y + res.z * res.z + res.w * res.w);
		return { res.x / len, res.y / len, res.z / len, res.w / len };
	}

	float theta0 = std::acos(dot);
	float theta = theta0 * t;
	float sinTheta = std::sin(theta);
	float sinTheta0 = std::sin(theta0);
	float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
	float s1 = sinTheta / sinTheta0;

	return {
		q1.x * s0 + q3.x * s1, q1.y * s0 + q3.y * s1,
		q1.z * s0 + q3.z * s1, q1.w * s0 + q3.w * s1
	};
}

Matrix4x4 MyMath::MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate) {
	Matrix4x4 matScale = MakeScaleMatrix(scale);      // 既存の関数
	Matrix4x4 matRotate = MakeRotateMatrix(rotate);   // さっき作った関数
	Matrix4x4 matTranslate = MakeTranslateMatrix(translate); // 既存の関数

	return matScale * matRotate * matTranslate;
}