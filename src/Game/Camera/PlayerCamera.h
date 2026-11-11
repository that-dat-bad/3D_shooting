#pragma once
#include <memory>
#include <cmath>
#include <algorithm>
#include "../FlightModel/FlightModel.h"
#include "../FlightModel/MouseAimController.h"

// プレイヤー追従カメラ
class PlayerCamera {
public:
    // 初期化
    void Initialize(const MyMath::Vector3& initialPos, const MyMath::Vector3& initialForward);

    // 更新
    void Update(float dt, FlightModel* flightModel, MouseAimController* mouseAimController, bool mouseAimEnabled);

    // 位置と注視点の同期
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
