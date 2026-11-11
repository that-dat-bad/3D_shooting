#pragma once
#include "Matrix4x4.h"

namespace MyMath {
	/// <summary>
	/// クォータニオン (四元数) 構造体
	/// 回転を表現するために使用する
	/// </summary>
	struct Quaternion {
		float x, y, z, w;
	};

	/// <summary>
	/// クォータニオンから回転行列を生成
	/// </summary>
	/// <param name="q">対象のクォータニオン</param>
	/// <returns>回転行列</returns>
	Matrix4x4 MakeRotateMatrix(const Quaternion& q);

	/// <summary>
	/// アフィン変換行列の生成 (クォータニオン回転を利用)
	/// </summary>
	/// <param name="scale">スケール値</param>
	/// <param name="rotate">回転(クォータニオン)</param>
	/// <param name="translate">平行移動量</param>
	/// <returns>アフィン変換行列</returns>
	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);

	/// <summary>
	/// クォータニオンの球面線形補間 (Slerp)
	/// 回転を滑らかに繋ぐために使用する
	/// </summary>
	/// <param name="q1">開始回転</param>
	/// <param name="q2">終了回転</param>
	/// <param name="t">補間係数 (0.0 ～ 1.0)</param>
	/// <returns>補間されたクォータニオン</returns>
	Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t);
}
