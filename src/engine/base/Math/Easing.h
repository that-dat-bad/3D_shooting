#pragma once
#include <cmath>
#include <algorithm>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

namespace Easing {

/// @brief 0.0f〜1.0f の範囲にクランプ
inline float Clamp01(float t) {
	return (std::max)(0.0f, (std::min)(1.0f, t));
}

/// @brief 線形補間 (float)
inline float Lerp(float a, float b, float t) {
	return a + (b - a) * t;
}

/// @brief 線形補間 (Vector2)
inline Vector2 Lerp(const Vector2& a, const Vector2& b, float t) {
	return { Lerp(a.x, b.x, t), Lerp(a.y, b.y, t) };
}

/// @brief 線形補間 (Vector3)
inline Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
	return { Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t) };
}

/// @brief 線形補間 (Vector4)
inline MyMath::Vector4 Lerp(const MyMath::Vector4& a, const MyMath::Vector4& b, float t) {
	return { Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t), Lerp(a.w, b.w, t) };
}

// ========================================================
// Quadratic (2次曲線)
// ========================================================
inline float EaseInQuad(float t) {
	return t * t;
}

inline float EaseOutQuad(float t) {
	return 1.0f - (1.0f - t) * (1.0f - t);
}

inline float EaseInOutQuad(float t) {
	return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
}

// ========================================================
// Cubic (3次曲線: 自然で滑らかな加減速)
// ========================================================
inline float EaseInCubic(float t) {
	return t * t * t;
}

inline float EaseOutCubic(float t) {
	float inv = 1.0f - t;
	return 1.0f - inv * inv * inv;
}

inline float EaseInOutCubic(float t) {
	return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

// ========================================================
// Exponential (指数: 素早く立ち上がり/定着)
// ========================================================
inline float EaseInExpo(float t) {
	return (t <= 0.0f) ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
}

inline float EaseOutExpo(float t) {
	return (t >= 1.0f) ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
}

// ========================================================
// Sine (正弦波: 穏やかなイージング)
// ========================================================
inline float EaseInSine(float t) {
	return 1.0f - std::cos(t * 1.57079632679f);
}

inline float EaseOutSine(float t) {
	return std::sin(t * 1.57079632679f);
}

inline float EaseInOutSine(float t) {
	return -(std::cos(3.14159265359f * t) - 1.0f) * 0.5f;
}

// ========================================================
// Back (オーバーシュート・バウンス感)
// ========================================================
inline float EaseOutBack(float t, float s = 1.70158f) {
	float inv = t - 1.0f;
	return 1.0f + (s + 1.0f) * inv * inv * inv + s * inv * inv;
}

} // namespace Easing
