#pragma once
#include "Vector3.h"

/// <summary>
/// 独自数学ライブラリ名前空間
/// </summary>
namespace MyMath {
	/// <summary>
	/// 4x4 行列構造体
	/// </summary>
	struct Matrix4x4 {
		float m[4][4];
	};

	/// <summary>
	/// 行列の加算
	/// </summary>
	/// <param name="m1">行列1</param>
	/// <param name="m2">行列2</param>
	/// <returns>加算結果の行列</returns>
	Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);

	/// <summary>
	/// 行列の減算
	/// </summary>
	/// <param name="m1">引かれる行列</param>
	/// <param name="m2">引く行列</param>
	/// <returns>減算結果の行列</returns>
	Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);

	/// <summary>
	/// 行列の積 (乗算)
	/// </summary>
	/// <param name="m1">左側の行列</param>
	/// <param name="m2">右側の行列</param>
	/// <returns>乗算結果の行列</returns>
	Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

	/// <summary>
	/// 逆行列の計算
	/// </summary>
	/// <param name="m">対象の行列</param>
	/// <returns>逆行列</returns>
	Matrix4x4 Inverse(const Matrix4x4& m);

	/// <summary>
	/// 転置行列の計算
	/// </summary>
	/// <param name="m">対象の行列</param>
	/// <returns>転置行列</returns>
	Matrix4x4 Transpose(const Matrix4x4& m);

	/// <summary>
	/// 単位行列の生成
	/// </summary>
	/// <returns>単位行列</returns>
	Matrix4x4 Identity4x4();

	/// <summary>
	/// 平行移動行列の生成
	/// </summary>
	/// <param name="translate">平行移動量</param>
	/// <returns>平行移動行列</returns>
	Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

	/// <summary>
	/// 拡大縮小行列の生成
	/// </summary>
	/// <param name="scale">スケール値</param>
	/// <returns>拡大縮小行列</returns>
	Matrix4x4 MakeScaleMatrix(const Vector3& scale);

	/// <summary>
	/// X軸周りの回転行列の生成
	/// </summary>
	/// <param name="angle">回転角度(ラジアン)</param>
	/// <returns>回転行列</returns>
	Matrix4x4 MakeRotateXMatrix(float angle);

	/// <summary>
	/// Y軸周りの回転行列の生成
	/// </summary>
	/// <param name="angle">回転角度(ラジアン)</param>
	/// <returns>回転行列</returns>
	Matrix4x4 MakeRotateYMatrix(float angle);

	/// <summary>
	/// Z軸周りの回転行列の生成
	/// </summary>
	/// <param name="angle">回転角度(ラジアン)</param>
	/// <returns>回転行列</returns>
	Matrix4x4 MakeRotateZMatrix(float angle);

	/// <summary>
	/// アフィン変換行列(スケール、回転、平行移動の合成行列)の生成
	/// </summary>
	/// <param name="scale">スケール値</param>
	/// <param name="rotate">オイラー角回転(ラジアン)</param>
	/// <param name="translate">平行移動量</param>
	/// <returns>アフィン変換行列</returns>
	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	/// <summary>
	/// ベクトルの座標変換 (w=1として行列と乗算)
	/// </summary>
	/// <param name="vector">対象のベクトル</param>
	/// <param name="matrix">変換行列</param>
	/// <returns>変換後のベクトル</returns>
	Vector3 TransformV3(const Vector3& vector, const Matrix4x4& matrix);

	/// <summary>
	/// 透視投影行列の生成
	/// </summary>
	/// <param name="fovY">縦の視野角(ラジアン)</param>
	/// <param name="aspectRatio">アスペクト比(幅/高さ)</param>
	/// <param name="nearClip">ニアクリップ距離</param>
	/// <param name="farClip">ファークリップ距離</param>
	/// <returns>透視投影行列</returns>
	Matrix4x4 MakePerspectiveMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	/// <summary>
	/// 正射影(平行投影)行列の生成
	/// </summary>
	/// <param name="left">左端</param>
	/// <param name="top">上端</param>
	/// <param name="right">右端</param>
	/// <param name="bottom">下端</param>
	/// <param name="nearClip">ニアクリップ距離</param>
	/// <param name="farClip">ファークリップ距離</param>
	/// <returns>正射影行列</returns>
	Matrix4x4 makeOrthographicmMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

	/// <summary>
	/// ビューポート行列の生成
	/// </summary>
	/// <param name="left">左端</param>
	/// <param name="top">上端</param>
	/// <param name="width">幅</param>
	/// <param name="height">高さ</param>
	/// <param name="minDepth">最小深度</param>
	/// <param name="maxDepth">最大深度</param>
	/// <returns>ビューポート行列</returns>
	Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

	/// <summary>
	/// 法線ベクトルの変換 (w=0として行列と乗算し、正規化はしない)
	/// </summary>
	/// <param name="v">法線ベクトル</param>
	/// <param name="m">変換行列</param>
	/// <returns>変換後の法線ベクトル</returns>
	Vector3 TransformNormal(const Vector3& v, const Matrix4x4& m);

	/// <summary>
	/// 行列の掛け算演算子オーバーロード
	/// </summary>
	/// <param name="lhs">左側の行列</param>
	/// <param name="rhs">右側の行列</param>
	/// <returns>乗算結果の行列</returns>
	inline Matrix4x4 operator*(const Matrix4x4& lhs, const Matrix4x4& rhs) {
		Matrix4x4 result;
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				result.m[i][j] = 0.0f;
				for (int k = 0; k < 4; ++k) {
					result.m[i][j] += lhs.m[i][k] * rhs.m[k][j];
				}
			}
		}
		return result;
	}
}
