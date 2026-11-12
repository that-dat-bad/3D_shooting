#pragma once

/// <summary>
/// 3次元ベクトル
/// </summary>

namespace MyMath {
	struct Vector3 {
		float x;
		float y;
		float z;
	};
	/// <summary>
	/// ベクトルの加算
	/// </summary>
	/// <param name="v1">ベクトル1</param>
	/// <param name="v2">ベクトル2</param>
	/// <returns>加算結果ベクトル</returns>
	Vector3 Add(const Vector3& v1, const Vector3& v2);

	/// <summary>
	/// ベクトルの減算
	/// </summary>
	/// <param name="v1">引かれるベクトル</param>
	/// <param name="v2">引くベクトル</param>
	/// <returns>減算結果ベクトル</returns>
	Vector3 Subtract(const Vector3& v1, const Vector3& v2);

	/// <summary>
	/// ベクトルのスカラー倍
	/// </summary>
	/// <param name="scaler">スケール値</param>
	/// <param name="v">対象ベクトル</param>
	/// <returns>スカラー倍されたベクトル</returns>
	Vector3 Multiply(float scaler, const Vector3& v);

	/// <summary>
	/// ベクトルの内積
	/// </summary>
	/// <param name="v1">ベクトル1</param>
	/// <param name="v2">ベクトル2</param>
	/// <returns>内積値</returns>
	float Dot(const Vector3& v1, const Vector3& v2);

	/// <summary>
	/// ベクトルの長さ (ノルム)
	/// </summary>
	/// <param name="v">対象ベクトル</param>
	/// <returns>ベクトルの長さ</returns>
	float Length(const Vector3& v);

	/// <summary>
	/// ベクトルの正規化 (長さを1にする)
	/// </summary>
	/// <param name="v">対象ベクトル</param>
	/// <returns>正規化されたベクトル</returns>
	Vector3 Normalize(const Vector3& v);

	/// <summary>
	/// ベクトルの外積 (クロス積)
	/// </summary>
	/// <param name="v1">ベクトル1</param>
	/// <param name="v2">ベクトル2</param>
	/// <returns>外積結果ベクトル</returns>
	Vector3 Cross(const Vector3& v1, const Vector3& v2);

	/// <summary>
	/// ベクトルの線形補間
	/// </summary>
	/// <param name="v1">開始値</param>
	/// <param name="v2">終了値</param>
	/// <param name="t">補間係数 (0.0 ～ 1.0)</param>
	/// <returns>補間されたベクトル</returns>
	Vector3 Lerp(const Vector3& v1, const Vector3& v2, float t);
}