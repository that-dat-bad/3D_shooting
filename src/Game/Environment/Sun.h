#pragma once
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Camera/Camera.h"

class Sun {
public:
	void Initialize();

	// 太陽光パラメータを DirectX 側の Constant Buffer に適用
	void UpdateLight();

	// カメラ方向に応じた太陽ブルーム（グレア）の動的適用
	void UpdateBloom(Camera* camera);

	// 太陽のビジュアル（ビルボード）の描画
	void Draw(Camera* camera);

	// ゲッター/セッター
	void SetDirection(const MyMath::Vector3& direction) { direction_ = direction; }
	const MyMath::Vector3& GetDirection() const { return direction_; }

	void SetColor(const MyMath::Vector4& color) { color_ = color; }
	const MyMath::Vector4& GetColor() const { return color_; }

	void SetIntensity(float intensity) { intensity_ = intensity; }
	float GetIntensity() const { return intensity_; }

private:
	MyMath::Vector3 direction_ = { 0.5f, -0.8f, 0.3f };
	MyMath::Vector4 color_ = { 1.0f, 0.95f, 0.85f, 1.0f };
	float intensity_ = 0.5f;
};
