#pragma once
#include <memory>
#include <cmath>
#include <algorithm>
#include "../FlightModel/FlightModel.h"
#include "../FlightModel/MouseAimController.h"

/// <summary>
/// 自機を追従するカメラの制御クラス（三人称視点、フリービュー対応）
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

    float cameraDistance_ = 22.0f;
    float cameraHeight_ = 8.0f;
    float cameraPosLag_ = 5.0f;
    float cameraLookLag_ = 10.0f;
    float currentFov_ = 0.45f;
    bool isZoomed_ = false;

    bool freeViewActive_ = false;
    float freeViewYaw_ = 0.0f;
    float freeViewPitch_ = 0.0f;
    float freeViewYawVelocity_ = 0.0f;
    float freeViewPitchVelocity_ = 0.0f;
    float freeViewDistance_ = 25.0f;
    MyMath::Vector3 freeViewLookOffset_ = { 0.0f, 0.0f, 0.0f };

    float cachedCameraYaw_ = 0.0f;
    float cachedCameraPitch_ = 0.0f;

    MyMath::Vector3 LookAtRotation(const MyMath::Vector3& from, const MyMath::Vector3& to) const;
};
