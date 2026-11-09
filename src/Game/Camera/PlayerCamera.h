#pragma once
#include <memory>
#include <cmath>
#include <algorithm>
#include "../FlightModel/FlightModel.h"
#include "../FlightModel/MouseAimController.h"

/// <summary>
/// 自機を追従するカメラの制御クラス（War Thunder準拠 固定ブーム球面追従仕様）
/// カメラ光軸(Zc)とブーム軸を単一の正規直交基底(Xc, Yc, Zc)に統合し、
/// 一定距離D・一定高さH・一定FOVで自機を常にビュー空間(0, -H, +D)へ完全固定することで、
/// 激しい上下左右のマウス操作でも前後揺れ(カメラ酔い)や境界壁への張り付きを一切起こさない。
/// </summary>
class PlayerCamera {
public:
    /// <summary>初期化処理</summary>
    void Initialize(const MyMath::Vector3& initialPos, const MyMath::Vector3& initialForward);

    /// <summary>カメラの位置・姿勢の更新処理</summary>
    void Update(float dt, FlightModel* flightModel, MouseAimController* mouseAimController, bool mouseAimEnabled);

    /// <summary>カメラの現在位置と注視点を直接指定して同期（イントロからのシームレス接続用）</summary>
    void SnapTo(const MyMath::Vector3& pos, const MyMath::Vector3& lookTarget);

    bool& GetFreeViewActive() { return freeViewActive_; }
    float* GetCameraDistancePtr() { return &cameraDistance_; }
    float* GetCameraHeightPtr() { return &cameraHeight_; }
    float* GetCameraPosLagPtr() { return &cameraPosLag_; }
    float* GetCameraLookLagPtr() { return &cameraLookLag_; }

    float GetCachedCameraYaw() const { return cachedCameraYaw_; }
    float GetCachedCameraPitch() const { return cachedCameraPitch_; }

    static MyMath::Vector3 QuaternionToEuler(const MyMath::Quaternion& q);

private:
    MyMath::Vector3 cameraCurrentPos_{};
    MyMath::Vector3 cameraLookTarget_{};

    // War Thunder準拠：単一カメラ基底 (camForward_ = Zc, camUp_ = Yc) と角速度状態
    MyMath::Vector3 camForward_ = { 0.0f, 0.0f, 1.0f };
    MyMath::Vector3 camUp_ = { 0.0f, 1.0f, 0.0f };
    MyMath::Vector3 camAngVel_ = { 0.0f, 0.0f, 0.0f };
    MyMath::Vector3 transitionPosOffset_ = { 0.0f, 0.0f, 0.0f };

    float cameraDistance_ = 30.0f;
    float cameraHeight_ = 3.2f;
    float cameraPosLag_ = 3.5f;
    float cameraLookLag_ = 5.5f;

    float currentDist_ = 30.0f;
    float currentHeight_ = 3.2f;
    float currentFov_ = 0.45f;
    bool isZoomed_ = false;
    bool needsTransitionSync_ = false;

    bool freeViewActive_ = false;
    float freeViewYaw_ = 0.0f;
    float freeViewPitch_ = 0.0f;
    float freeViewYawVelocity_ = 0.0f;
    float freeViewPitchVelocity_ = 0.0f;
    float freeViewDistance_ = 30.0f;
    MyMath::Vector3 freeViewLookOffset_ = { 0.0f, 0.0f, 0.0f };

    float cachedCameraYaw_ = 0.0f;
    float cachedCameraPitch_ = 0.0f;

    MyMath::Vector3 LookAtRotation(const MyMath::Vector3& from, const MyMath::Vector3& to) const;
    static MyMath::Vector3 BasisToEuler(const MyMath::Vector3& right, const MyMath::Vector3& up, const MyMath::Vector3& forward);
};
