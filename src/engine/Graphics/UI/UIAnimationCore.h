#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <cmath>
#include <random>
#include "../base/Math/MyMath.h"

using namespace MyMath;

// ループアニメーション種別
enum class UILoopMotion {
	None = 0,	// なし
	Pulse,		// 脈動
	Floating,	// 浮遊
	Shake,		// 震え
};

// 文字列からUILoopMotionへの変換
inline UILoopMotion StringToLoopMotion(const std::string& str) {
	if (str == "Pulse" || str == "pulse") return UILoopMotion::Pulse;
	if (str == "Floating" || str == "floating" || str == "Float" || str == "float") return UILoopMotion::Floating;
	if (str == "Shake" || str == "shake") return UILoopMotion::Shake;
	return UILoopMotion::None;
}

// UILoopMotionから文字列への変換
inline std::string LoopMotionToString(UILoopMotion motion) {
	switch (motion) {
	case UILoopMotion::Pulse:    return "Pulse";
	case UILoopMotion::Floating: return "Floating";
	case UILoopMotion::Shake:    return "Shake";
	default:                     return "None";
	}
}

// UIスタイル設定
struct UIStateStyle {
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	float scale = 1.0f;
	Vector2 offset = { 0.0f, 0.0f };
	UILoopMotion loopMotion = UILoopMotion::None;
	float motionSpeed = 3.0f;
	float motionIntensity = 1.0f;
};
