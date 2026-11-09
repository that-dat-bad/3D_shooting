#define NOMINMAX
#include "PlayerCamera.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/io/Input.h"

using namespace MyMath;

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kBaseFov = 0.45f;
    constexpr float kZoomFov = 0.22f;

    Vector3 CrossVec(const Vector3& a, const Vector3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    Vector3 SafeNormalize(const Vector3& v, const Vector3& fallback) {
        float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (lenSq > 1e-10f) {
            float inv = 1.0f / std::sqrt(lenSq);
            return { v.x * inv, v.y * inv, v.z * inv };
        }
        return fallback;
    }

    Vector3 RotateAroundAxis(const Vector3& v, const Vector3& unitAxis, float angle) {
        float c = std::cos(angle);
        float s = std::sin(angle);
        Vector3 kxv = CrossVec(unitAxis, v);
        float kdv = Dot(unitAxis, v);
        return {
            v.x * c + kxv.x * s + unitAxis.x * kdv * (1.0f - c),
            v.y * c + kxv.y * s + unitAxis.y * kdv * (1.0f - c),
            v.z * c + kxv.z * s + unitAxis.z * kdv * (1.0f - c)
        };
    }

    /// 180度反転特異点でも破綻しない球面線形補間 (Slerp on S^2)
    Vector3 SlerpDirection(const Vector3& from, const Vector3& to, float t, const Vector3& fallbackUp) {
        t = std::clamp(t, 0.0f, 1.0f);
        float d = std::clamp(Dot(from, to), -1.0f, 1.0f);
        float theta = std::acos(d);
        if (theta < 1e-4f) {
            return SafeNormalize(Lerp(from, to, t), from);
        }

        Vector3 axis = CrossVec(from, to);
        float axisLenSq = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
        if (axisLenSq < 1e-8f) {
            axis = CrossVec(from, fallbackUp);
            if (Dot(axis, axis) < 1e-8f) {
                Vector3 alt = (std::fabs(from.y) < 0.9f) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
                axis = CrossVec(from, alt);
            }
        }
        axis = SafeNormalize(axis, { 0.0f, 1.0f, 0.0f });
        return SafeNormalize(RotateAroundAxis(from, axis, theta * t), to);
    }

    /// 球面 S^2 上の無条件安定・過減衰2次バネダンパ積分器
    /// ハードクランプ壁を一切持たず、0°〜180°の全域で均一かつ滑らかな追従角速度を生成する
    Vector3 StepSphericalSpringDamper(
        const Vector3& dir,
        const Vector3& target,
        Vector3& ioAngVel,
        float wn,
        float zeta,
        float dt,
        const Vector3& fallbackUp)
    {
        float d = std::clamp(Dot(dir, target), -1.0f, 1.0f);
        float theta = std::acos(d);

        Vector3 rotErr = { 0.0f, 0.0f, 0.0f };
        if (theta > 1e-5f) {
            Vector3 axis = CrossVec(dir, target);
            if (Dot(axis, axis) < 1e-8f) {
                axis = CrossVec(dir, fallbackUp);
                if (Dot(axis, axis) < 1e-8f) {
                    Vector3 alt = (std::fabs(dir.y) < 0.9f) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
                    axis = CrossVec(dir, alt);
                }
            }
            axis = SafeNormalize(axis, { 0.0f, 1.0f, 0.0f });
            rotErr = Multiply(theta, axis);
        }

        float damp = 2.0f * zeta * wn;
        float decay = std::exp(-damp * dt);
        float forceInt = (damp > 1e-4f) ? ((1.0f - decay) / damp) : dt;
        ioAngVel = Add(Multiply(decay, ioAngVel), Multiply(wn * wn * forceInt, rotErr));

        float velMag = Length(ioAngVel);
        float stepAngle = velMag * dt;
        if (velMag > 1e-6f && stepAngle > 1e-6f) {
            if (theta > 1e-5f && Dot(ioAngVel, rotErr) > 0.0f && stepAngle > theta) {
                stepAngle = theta;
            }
            Vector3 velAxis = Multiply(1.0f / velMag, ioAngVel);
            return SafeNormalize(RotateAroundAxis(dir, velAxis, stepAngle), target);
        }
        return dir;
    }

    float SmoothStep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (std::max)(1e-5f, edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
}

void PlayerCamera::Initialize(const MyMath::Vector3& initialPos, const MyMath::Vector3& initialForward) {
    Vector3 fwd = SafeNormalize(initialForward, { 0.0f, 0.0f, 1.0f });
    camForward_ = fwd;
    camAngVel_ = { 0.0f, 0.0f, 0.0f };
    transitionPosOffset_ = { 0.0f, 0.0f, 0.0f };

    Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
    if (std::fabs(Dot(fwd, worldUp)) > 0.98f) {
        worldUp = { 1.0f, 0.0f, 0.0f };
    }
    Vector3 right = SafeNormalize(CrossVec(worldUp, fwd), { 1.0f, 0.0f, 0.0f });
    camUp_ = SafeNormalize(CrossVec(fwd, right), { 0.0f, 1.0f, 0.0f });

    currentDist_ = cameraDistance_;
    currentHeight_ = cameraHeight_;
    currentFov_ = kBaseFov;
    isZoomed_ = false;
    freeViewActive_ = false;
    needsTransitionSync_ = false;

    cameraCurrentPos_ = Add(initialPos, Add(Multiply(-currentDist_, camForward_), Multiply(currentHeight_, camUp_)));
    cameraLookTarget_ = Add(cameraCurrentPos_, Multiply(90.0f, camForward_));

    Vector3 camRot = BasisToEuler(right, camUp_, camForward_);
    cachedCameraPitch_ = camRot.x;
    cachedCameraYaw_ = camRot.y;

    Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
    if (camera) {
        camera->SetTranslate(cameraCurrentPos_);
        camera->SetRotate(camRot);
        camera->SetFovY(currentFov_);
        camera->UpdateMatrices();
        Object3dCommon::GetInstance()->SetCameraPosition(cameraCurrentPos_);
    }
}

void PlayerCamera::SnapTo(const MyMath::Vector3& pos, const MyMath::Vector3& lookTarget) {
    cameraCurrentPos_ = pos;
    cameraLookTarget_ = lookTarget;
    Vector3 camRot = LookAtRotation(cameraCurrentPos_, cameraLookTarget_);
    cachedCameraPitch_ = camRot.x;
    cachedCameraYaw_ = camRot.y;

    camForward_ = SafeNormalize(Subtract(lookTarget, pos), camForward_);
    Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
    if (std::fabs(Dot(camForward_, worldUp)) > 0.98f) {
        worldUp = camUp_;
    }
    Vector3 right = SafeNormalize(CrossVec(worldUp, camForward_), { 1.0f, 0.0f, 0.0f });
    camUp_ = SafeNormalize(CrossVec(camForward_, right), { 0.0f, 1.0f, 0.0f });
    camAngVel_ = { 0.0f, 0.0f, 0.0f };
    needsTransitionSync_ = true;

    Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
    if (camera) {
        camera->SetTranslate(cameraCurrentPos_);
        camera->SetRotate(camRot);
        camera->UpdateMatrices();
        Object3dCommon::GetInstance()->SetCameraPosition(cameraCurrentPos_);
    }
}

void PlayerCamera::Update(float rawDt, FlightModel* flightModel, MouseAimController* mouseAimController, bool mouseAimEnabled) {
    if (!flightModel) {
        return;
    }

    const float dt = std::clamp(rawDt, 0.0001f, 0.1f);
    const Vector3 aircraftPos = flightModel->GetPosition();
    const Vector3 forward = SafeNormalize(flightModel->GetForwardDirection(), { 0.0f, 0.0f, 1.0f });
    const Vector3 up = SafeNormalize(flightModel->GetUpDirection(), { 0.0f, 1.0f, 0.0f });
    const Vector3 acRight = SafeNormalize(flightModel->GetRightDirection(), { 1.0f, 0.0f, 0.0f });

    Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
    Input* input = Input::GetInstance();

    // =========================================================================
    // フリービュー (Cキー押下中)
    // =========================================================================
    if (input && input->PushKey(DIK_C)) {
        if (!freeViewActive_) {
            freeViewActive_ = true;
            Vector3 camOffset = Subtract(cameraCurrentPos_, aircraftPos);
            freeViewDistance_ = Length(camOffset);
            if (freeViewDistance_ < 5.0f) { freeViewDistance_ = cameraDistance_; }
            Vector3 dir = SafeNormalize(camOffset, Multiply(-1.0f, forward));
            freeViewPitch_ = std::asin(std::clamp(dir.y, -1.0f, 1.0f));
            freeViewYaw_   = std::atan2(dir.x, dir.z);
            freeViewPitchVelocity_ = 0.0f;
            freeViewYawVelocity_   = 0.0f;
            freeViewLookOffset_ = { 0.0f, 0.0f, 0.0f };
        }
    } else {
        if (freeViewActive_) {
            freeViewActive_ = false;
            needsTransitionSync_ = true;
        }
    }

    if (freeViewActive_) {
        Input::MouseMove mouseMove = input->GetMouseMove();
        const float sensitivity = 0.002f;
        const float damping = 0.85f;
        const float accelScale = 1.0f - damping;

        freeViewYawVelocity_   += static_cast<float>(mouseMove.lX) * sensitivity * accelScale;
        freeViewPitchVelocity_ += static_cast<float>(mouseMove.lY) * sensitivity * accelScale;

        freeViewYaw_   += freeViewYawVelocity_;
        freeViewPitch_ += freeViewPitchVelocity_;

        freeViewYawVelocity_   *= damping;
        freeViewPitchVelocity_ *= damping;

        const float pitchLimit = kPi * 0.49f;
        if (freeViewPitch_ > pitchLimit) { freeViewPitch_ = pitchLimit; freeViewPitchVelocity_ = 0.0f; }
        if (freeViewPitch_ < -pitchLimit) { freeViewPitch_ = -pitchLimit; freeViewPitchVelocity_ = 0.0f; }

        Vector3 offset;
        offset.x = freeViewDistance_ * std::cos(freeViewPitch_) * std::sin(freeViewYaw_);
        offset.y = freeViewDistance_ * std::sin(freeViewPitch_);
        offset.z = freeViewDistance_ * std::cos(freeViewPitch_) * std::cos(freeViewYaw_);

        Vector3 freeViewPos = Add(aircraftPos, offset);
        cameraCurrentPos_ = freeViewPos;
        cameraLookTarget_ = Add(aircraftPos, freeViewLookOffset_);

        Vector3 cameraRotation = LookAtRotation(freeViewPos, cameraLookTarget_);
        cachedCameraPitch_ = cameraRotation.x;
        cachedCameraYaw_   = cameraRotation.y;

        camForward_ = SafeNormalize(Subtract(cameraLookTarget_, freeViewPos), forward);
        Vector3 fRight = SafeNormalize(CrossVec({ 0.0f, 1.0f, 0.0f }, camForward_), { 1.0f, 0.0f, 0.0f });
        camUp_ = SafeNormalize(CrossVec(camForward_, fRight), { 0.0f, 1.0f, 0.0f });
        camAngVel_ = { 0.0f, 0.0f, 0.0f };

        if (camera) {
            camera->SetTranslate(freeViewPos);
            camera->SetRotate(cameraRotation);
            camera->UpdateMatrices();
            Object3dCommon::GetInstance()->SetCameraPosition(freeViewPos);
        }
        return;
    }

    // ズーム切替 (右クリックのみFOVが変化し、マウス移動による動的FOV変動は一切行わない)
    if (input && input->TriggerMouse(1)) {
        isZoomed_ = !isZoomed_;
    }

    const float targetFov = isZoomed_ ? kZoomFov : kBaseFov;
    if (std::fabs(targetFov - currentFov_) > 1e-5f) {
        currentFov_ += (targetFov - currentFov_) * (1.0f - std::exp(-14.0f * dt));
    } else {
        currentFov_ = targetFov;
    }

    // 固定ブーム距離D・固定ブーム高さH（ズーム時のみ画面下部の機体比率を保つようHを比例スケール）
    const float fovScale = (std::min)(1.0f, std::tan(currentFov_ * 0.5f) / std::tan(kBaseFov * 0.5f));
    currentDist_ = (std::max)(5.0f, cameraDistance_);
    currentHeight_ = cameraHeight_ * fovScale;

    // SnapTo・フリービュー解除直後の座標段差のみオフセットへ吸収し、通常飛行時は厳密にゼロに保つ
    if (needsTransitionSync_) {
        Vector3 nominalBoomPos = Add(aircraftPos, Add(Multiply(-currentDist_, camForward_), Multiply(currentHeight_, camUp_)));
        transitionPosOffset_ = Subtract(cameraCurrentPos_, nominalBoomPos);
        needsTransitionSync_ = false;
    }
    if (Dot(transitionPosOffset_, transitionPosOffset_) > 1e-6f) {
        transitionPosOffset_ = Multiply(std::exp(-12.0f * dt), transitionPosOffset_);
        if (Dot(transitionPosOffset_, transitionPosOffset_) <= 1e-6f) {
            transitionPosOffset_ = { 0.0f, 0.0f, 0.0f };
        }
    }

    // =========================================================================
    // マニュアル操縦モード (mouseAimEnabled == false)
    // =========================================================================
    if (!mouseAimEnabled || !mouseAimController) {
        const float wnManual = std::clamp(cameraPosLag_, 1.0f, 20.0f) * 1.15f;
        camForward_ = StepSphericalSpringDamper(camForward_, forward, camAngVel_, wnManual, 1.08f, dt, up);

        Vector3 transUp = SafeNormalize(Subtract(camUp_, Multiply(Dot(camUp_, camForward_), camForward_)), up);
        Vector3 targetUp = SafeNormalize(Subtract(up, Multiply(Dot(up, camForward_), camForward_)), transUp);
        camUp_ = SlerpDirection(transUp, targetUp, 1.0f - std::exp(-7.0f * dt), camForward_);

        Vector3 Zc = camForward_;
        Vector3 Xc = SafeNormalize(CrossVec(camUp_, Zc), acRight);
        Vector3 Yc = SafeNormalize(CrossVec(Zc, Xc), camUp_);
        camUp_ = Yc;

        Vector3 rigidBoomPos = Add(aircraftPos, Add(Multiply(-currentDist_, Zc), Multiply(currentHeight_, Yc)));
        cameraCurrentPos_ = Add(rigidBoomPos, transitionPosOffset_);
        cameraLookTarget_ = Add(cameraCurrentPos_, Multiply(90.0f, Zc));

        Vector3 camRot = BasisToEuler(Xc, Yc, Zc);
        cachedCameraPitch_ = camRot.x;
        cachedCameraYaw_ = camRot.y;

        if (camera) {
            camera->SetTranslate(cameraCurrentPos_);
            camera->SetRotate(camRot);
            camera->SetFovY(currentFov_);
            camera->UpdateMatrices();
            Object3dCommon::GetInstance()->SetCameraPosition(cameraCurrentPos_);
        }
        return;
    }

    // =========================================================================
    // War Thunder準拠 マウスエイム追従カメラ
    // 1. 一定FOV・一定距離D・一定高さHの剛体ブーム: P_cam = P_aircraft - D*Zc + H*Yc
    //    → ビュー空間における機体座標が毎フレーム厳密に (0, -H, +D) となり、前後揺れ(酔い)が完全にゼロ。
    // 2. ハードクランプ壁のない均一過減衰2次球面ダンパで Zc → aimDir を滑らかに回転追従
    //    → 微小な上下振りの手ブレを95%以上遮断しつつ、大角度旋回でも壁への吸いつきが一切発生しない。
    // =========================================================================
    const Vector3 aimDir = SafeNormalize(mouseAimController->GetTargetDirection(), forward);

    const float wnAim = std::clamp(cameraLookLag_, 1.5f, 20.0f) * 1.15f;
    camForward_ = StepSphericalSpringDamper(camForward_, aimDir, camAngVel_, wnAim, 1.08f, dt, camUp_);

    // ワールド水平基準Up安定化 ＋ 微量な機体バンク感(8%) ＋ 天頂付近の並行輸送ブレンド
    Vector3 transportedUp = Subtract(camUp_, Multiply(Dot(camUp_, camForward_), camForward_));
    transportedUp = SafeNormalize(transportedUp, { 0.0f, 1.0f, 0.0f });

    const float horizonWeight = 1.0f - SmoothStep(0.75f, 0.97f, std::fabs(camForward_.y));
    if (horizonWeight > 0.001f) {
        const Vector3 worldUpRef = { 0.0f, 1.0f, 0.0f };
        const Vector3 blendedRef = SafeNormalize(Add(worldUpRef, Multiply(0.08f, up)), worldUpRef);
        Vector3 projHorizonUp = Subtract(blendedRef, Multiply(Dot(blendedRef, camForward_), camForward_));
        if (Dot(projHorizonUp, projHorizonUp) > 0.04f) {
            projHorizonUp = Normalize(projHorizonUp);
            const float rollRate = 6.0f * horizonWeight;
            transportedUp = SlerpDirection(transportedUp, projHorizonUp, 1.0f - std::exp(-rollRate * dt), camForward_);
        }
    }

    const Vector3 Zc = camForward_;
    const Vector3 Xc = SafeNormalize(CrossVec(transportedUp, Zc), acRight);
    const Vector3 Yc = SafeNormalize(CrossVec(Zc, Xc), transportedUp);
    camUp_ = Yc;

    const Vector3 rigidBoomPos = Add(aircraftPos, Add(Multiply(-currentDist_, Zc), Multiply(currentHeight_, Yc)));
    cameraCurrentPos_ = Add(rigidBoomPos, transitionPosOffset_);
    cameraLookTarget_ = Add(cameraCurrentPos_, Multiply(90.0f, Zc));

    const Vector3 cameraRotation = BasisToEuler(Xc, Yc, Zc);
    cachedCameraPitch_ = cameraRotation.x;
    cachedCameraYaw_ = cameraRotation.y;

    if (camera) {
        camera->SetTranslate(cameraCurrentPos_);
        camera->SetRotate(cameraRotation);
        camera->SetFovY(currentFov_);
        camera->UpdateMatrices();
        Object3dCommon::GetInstance()->SetCameraPosition(cameraCurrentPos_);
    }
}

MyMath::Vector3 PlayerCamera::BasisToEuler(const MyMath::Vector3& right, const MyMath::Vector3& up, const MyMath::Vector3& forward) {
    float sinY = std::clamp(-right.z, -1.0f, 1.0f);
    float yaw = std::asin(sinY);
    float cosY = std::sqrt((std::max)(0.0f, 1.0f - sinY * sinY));

    float pitch = 0.0f;
    float roll = 0.0f;
    if (cosY > 1e-4f) {
        pitch = std::atan2(up.z, forward.z);
        roll = std::atan2(right.y, right.x);
    } else {
        roll = 0.0f;
        pitch = (sinY > 0.0f) ? std::atan2(up.x, up.y) : std::atan2(-up.x, up.y);
    }
    return { pitch, yaw, roll };
}

MyMath::Vector3 PlayerCamera::QuaternionToEuler(const MyMath::Quaternion& q) {
    Vector3 euler{};

    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    euler.x = std::atan2(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    if (std::fabs(sinp) >= 1.0f) {
        euler.y = std::copysign(3.14159265f / 2.0f, sinp);
    } else {
        euler.y = std::asin(sinp);
    }

    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    euler.z = std::atan2(siny_cosp, cosy_cosp);

    return euler;
}

MyMath::Vector3 PlayerCamera::LookAtRotation(const MyMath::Vector3& from, const MyMath::Vector3& to) const {
    Vector3 dir = Subtract(to, from);
    float len = Length(dir);
    if (len < 0.0001f) {
        return { cachedCameraPitch_, cachedCameraYaw_, 0.0f };
    }

    dir = Normalize(dir);

    float clampedY = std::clamp(dir.y, -0.999f, 0.999f);
    float pitch = -std::asin(clampedY);

    float horizontalLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
    float yaw;
    if (horizontalLen < 0.05f) {
        yaw = cachedCameraYaw_;
    } else {
        yaw = std::atan2(dir.x, dir.z);
    }

    return { pitch, yaw, 0.0f };
}
