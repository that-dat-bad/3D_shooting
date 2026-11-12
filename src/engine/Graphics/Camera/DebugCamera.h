#pragma once
#include "../base/Math/MyMath.h"
using namespace MyMath;
#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>    


class DebugCamera {
public:
	/// <summary>
	/// コンストラクタ
	/// </summary>
	DebugCamera();

	/// <summary>
	/// 更新処理 (毎フレーム呼び出す)
	/// </summary>
	/// <param name="key">キーボードの状態</param>
	/// <param name="mouseState">マウスの状態</param>
	void Update(const BYTE* key, const DIMOUSESTATE2& mouseState);

	/// <summary>
	/// ビュー行列の取得
	/// </summary>
	/// <returns>ビュー行列</returns>
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }

private:
	// ビュー行列
	Matrix4x4 viewMatrix_ = Identity4x4();

	// 注視点（カメラが回る中心）
	Vector3 pivot_ = { 0.0f, 0.0f, 0.0f };

	// 注視点からの距離
	float distance_ = 10.0f;

	// 水平・垂直の回転角度
	Vector2 rotation_ = { -0.3f, 1.57f };

	// 前フレームのマウス位置
	Vector2 prevMousePos_ = { 0.0f, 0.0f };
};
