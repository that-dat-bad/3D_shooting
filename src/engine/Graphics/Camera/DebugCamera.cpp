#include "DebugCamera.h"
#include "../base/Math/MyMath.h"
using namespace MyMath;
#include<cmath>


Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up) {
    Vector3 zaxis = Normalize(Subtract(target, eye));
    Vector3 xaxis = Normalize(Cross(up, zaxis));
    Vector3 yaxis = Cross(zaxis, xaxis);
    Matrix4x4 viewMatrix = {
        xaxis.x, yaxis.x, zaxis.x, 0,
        xaxis.y, yaxis.y, zaxis.y, 0,
        xaxis.z, yaxis.z, zaxis.z, 0,
        -Dot(xaxis, eye), -Dot(yaxis, eye), -Dot(zaxis, eye), 1.0f
    };
    return viewMatrix;
}
DebugCamera::DebugCamera() {
    // 最初のビュー行列を計算しておく
    Update(nullptr, {});
}

void DebugCamera::Update(const BYTE* key, const DIMOUSESTATE2& mouseState) {

    // --- 回転処理 (右ボタンドラッグ) ---
    if (mouseState.rgbButtons[1] & 0x80) {
        float dx = static_cast<float>(mouseState.lX) * 0.005f;
        float dy = static_cast<float>(mouseState.lY) * 0.005f;
        rotation_.y += dx; // ヨー回転
        rotation_.x += dy; // ピッチ回転

        // ピッチ角を制限（真上や真下に行きすぎないように）
        float pitchLimit = 3.141592f * 0.49f; // 約89.9度
        if (rotation_.x > pitchLimit) { rotation_.x = pitchLimit; }
        if (rotation_.x < -pitchLimit) { rotation_.x = -pitchLimit; }
    }

    // --- ズーム処理 (マウスホイール) ---
    float wheel = static_cast<float>(mouseState.lZ) * 0.01f;
    distance_ -= wheel;
    // 距離に下限を設定
    if (distance_ < 1.0f) {
        distance_ = 1.0f;
    }

    // --- パン処理 (中ボタンドラッグ) ---
    if (mouseState.rgbButtons[2] & 0x80) {
        float dx = static_cast<float>(mouseState.lX) * -0.01f;
        float dy = static_cast<float>(mouseState.lY) * 0.01f;

        // カメラのワールド行列（ビュー行列の逆行列）からカメラのX軸とY軸を取得
        Matrix4x4 cameraWorldMatrix = Inverse(viewMatrix_);
        Vector3 cameraRight = { cameraWorldMatrix.m[0][0], cameraWorldMatrix.m[0][1], cameraWorldMatrix.m[0][2] };
        Vector3 cameraUp = { cameraWorldMatrix.m[1][0], cameraWorldMatrix.m[1][1], cameraWorldMatrix.m[1][2] };

        // 移動量を計算してピボットを更新
        Vector3 move = Add(Multiply(dx, cameraRight), Multiply(dy, cameraUp));
        pivot_ = Add(pivot_, move);
    }


    // --- ビュー行列の計算 ---
    // 1. 注視点からのオフセットを計算（球面座標）
    Vector3 offset;
    offset.x = distance_ * std::cos(rotation_.x) * std::sin(rotation_.y);
    offset.y = distance_ * std::sin(rotation_.x);
    offset.z = distance_ * std::cos(rotation_.x) * std::cos(rotation_.y);

    // 2. カメラの座標を決定
    Vector3 eyePosition = Add(pivot_, offset);

    // 3. LookAtビュー行列を生成
    viewMatrix_ = MakeLookAtMatrix(eyePosition, pivot_, { 0.0f, 1.0f, 0.0f });
}
