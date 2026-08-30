#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <cmath>
#include <random>
#include "../base/Math/MyMath.h"

using namespace MyMath;

/// @brief ループアニメーション種別
enum class UILoopMotion {
	None = 0,	///< なし
	Pulse,		///< 脈動（サイン波による拡大縮小・呼吸）
	Floating,	///< 浮遊（上下にフワフワと漂う）
	Shake,		///< 震え（被弾・ピンチ・警告時のランダム振動）
};

/// @brief 文字列から UILoopMotion への変換ヘルパー
inline UILoopMotion StringToLoopMotion(const std::string& str) {
	if (str == "Pulse" || str == "pulse") return UILoopMotion::Pulse;
	if (str == "Floating" || str == "floating" || str == "Float" || str == "float") return UILoopMotion::Floating;
	if (str == "Shake" || str == "shake") return UILoopMotion::Shake;
	return UILoopMotion::None;
}

/// @brief UILoopMotion から文字列への変換ヘルパー
inline std::string LoopMotionToString(UILoopMotion motion) {
	switch (motion) {
	case UILoopMotion::Pulse:    return "Pulse";
	case UILoopMotion::Floating: return "Floating";
	case UILoopMotion::Shake:    return "Shake";
	default:                     return "None";
	}
}

/// @brief 特定の状態（通常・ホバー・選択・Custom等）におけるスタイル・モーション定義
struct UIStateStyle {
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };	///< 目標カラー＆アルファ
	float scale = 1.0f;								///< 目標スケール倍率 (1.0 = 100%)
	Vector2 offset = { 0.0f, 0.0f };				///< 位置オフセット (X, Y)
	UILoopMotion loopMotion = UILoopMotion::None;	///< ループモーション
	float motionSpeed = 3.0f;						///< モーション速度 (Hz相当)
	float motionIntensity = 1.0f;					///< モーションの強さ (振幅・ピクセル量)
};
