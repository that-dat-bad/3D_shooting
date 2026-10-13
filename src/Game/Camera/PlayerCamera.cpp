#include "PlayerCamera.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/io/Input.h"

using namespace MyMath;

void PlayerCamera::Initialize(const MyMath::Vector3& initialPos, const MyMath::Vector3& initialForward) {
    cameraCurrentPos_ = Add(initialPos, Add(Multiply(-cameraDistance_, initialForward), { 0.0f, cameraHeight_, 0.0f }));
    cameraLookTarget_ = initialPos;

    Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
    if (camera) {
        camera->SetTranslate(cameraCurrentPos_);
        camera->SetRotate(LookAtRotation(cameraCurrentPos_, cameraLookTarget_));
    }

    Vector3 camRot = LookAtRotation(cameraCurrentPos_, cameraLookTarget_);
    cachedCameraPitch_ = camRot.x;
    cachedCameraYaw_ = camRot.y;
}

void PlayerCamera::SnapTo(const MyMath::Vector3& pos, const MyMath::Vector3& lookTarget) {
    cameraCurrentPos_ = pos;
    cameraLookTarget_ = lookTarget;
    Vector3 camRot = LookAtRotation(cameraCurrentPos_, cameraLookTarget_);
    cachedCameraPitch_ = camRot.x;
    cachedCameraYaw_ = camRot.y;

    Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
    if (camera) {
        camera->SetTranslate(cameraCurrentPos_);
        camera->SetRotate(camRot);
        Object3dCommon::GetInstance()->SetCameraPosition(cameraCurrentPos_);
    }
}

void PlayerCamera::Update(float dt, FlightModel* flightModel, MouseAimController* mouseAimController, bool mouseAimEnabled) {
    Vector3 aircraftPos = flightModel->GetPosition();
    Vector3 forward = flightModel->GetForwardDirection();
    Vector3 up = flightModel->GetUpDirection();

    Input* input = Input::GetInstance();
    if (input->PushKey(DIK_C)) {
        if (!freeViewActive_) {
            freeViewActive_ = true;
            Vector3 camOffset = Subtract(cameraCurrentPos_, aircraftPos);
            freeViewDistance_ = Length(camOffset);
            if (freeViewDistance_ < 1.0f) { freeViewDistance_ = cameraDistance_; }
            Vector3 dir = Normalize(camOffset);
            freeViewPitch_ = std::asin(std::clamp(dir.y, -1.0f, 1.0f));
            freeViewYaw_   = std::atan2(dir.x, dir.z);
            freeViewPitchVelocity_ = 0.0f;
            freeViewYawVelocity_   = 0.0f;
            freeViewLookOffset_ = Subtract(cameraLookTarget_, aircraftPos);
        }
    } else {
        freeViewActive_ = false;
    }

    if (freeViewActive_) {
        Input::MouseMove mouseMove = Input::GetInstance()->GetMouseMove();
        const float sensitivity = 0.002f;
        const float damping = 0.85f;
        const float accelScale = 1.0f - damping;

        freeViewYawVelocity_   += static_cast<float>(mouseMove.lX) * sensitivity * accelScale;
        freeViewPitchVelocity_ += static_cast<float>(mouseMove.lY) * sensitivity * accelScale;

        freeViewYaw_   += freeViewYawVelocity_;
        freeViewPitch_ += freeViewPitchVelocity_;

        freeViewYawVelocity_   *= damping;
        freeViewPitchVelocity_ *= damping;

        const float pitchLimit = 3.141592f * 0.49f;
        if (freeViewPitch_ > pitchLimit) { freeViewPitch_ = pitchLimit; freeViewPitchVelocity_ = 0.0f; }
        if (freeViewPitch_ < -pitchLimit) { freeViewPitch_ = -pitchLimit; freeViewPitchVelocity_ = 0.0f; }

        Vector3 offset;
        offset.x = freeViewDistance_ * std::cos(freeViewPitch_) * std::sin(freeViewYaw_);
        offset.y = freeViewDistance_ * std::sin(freeViewPitch_);
        offset.z = freeViewDistance_ * std::cos(freeViewPitch_) * std::cos(freeViewYaw_);

        Vector3 freeViewPos = Add(aircraftPos, offset);
        cameraCurrentPos_ = freeViewPos;

        bool hasMouseInput = (std::fabs(freeViewYawVelocity_) > 0.0001f || std::fabs(freeViewPitchVelocity_) > 0.0001f);
        if (hasMouseInput) {
            float blendT = 1.0f - std::exp(-15.0f * dt);
            freeViewLookOffset_.x *= (1.0f - blendT);
            freeViewLookOffset_.y *= (1.0f - blendT);
            freeViewLookOffset_.z *= (1.0f - blendT);
        }

        cameraLookTarget_ = Add(aircraftPos, freeViewLookOffset_);

        Vector3 cameraRotation = LookAtRotation(freeViewPos, cameraLookTarget_);
        cachedCameraPitch_ = cameraRotation.x;
        cachedCameraYaw_   = cameraRotation.y;

        Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
        if (camera) {
            camera->SetTranslate(freeViewPos);
            camera->SetRotate(cameraRotation);
            Object3dCommon::GetInstance()->SetCameraPosition(freeViewPos);
        }
        return;
    }

    Vector3 viewDir = forward;
    Vector3 cameraUp = up;

    if (mouseAimEnabled) {
        Vector3 aimDir = mouseAimController->GetTargetDirection();
        const float forwardBlend = 0.35f;
        viewDir = Normalize(Lerp(aimDir, forward, forwardBlend));
        
        Vector3 worldUp = {0.0f, 1.0f, 0.0f};
        if (std::fabs(viewDir.y) > 0.95f) {
            worldUp = up;
        }
        cameraUp = Normalize(Lerp(worldUp, up, 0.3f));
    }

    Vector3 backOffset = Multiply(-cameraDistance_, viewDir);
    Vector3 upOffset = Multiply(cameraHeight_, cameraUp);
    Vector3 desiredPos = Add(aircraftPos, Add(backOffset, upOffset));

    Vector3 lookForward = Multiply(40.0f, viewDir);
    Vector3 lookUp = Multiply(cameraHeight_ * 0.6f, cameraUp);
    Vector3 desiredLookTarget = Add(aircraftPos, Add(lookForward, lookUp));

    float posT = 1.0f - std::exp(-cameraPosLag_ * dt);
    float lookT = 1.0f - std::exp(-cameraLookLag_ * dt);

    cameraCurrentPos_ = Lerp(cameraCurrentPos_, desiredPos, posT);
    cameraLookTarget_ = Lerp(cameraLookTarget_, desiredLookTarget, lookT);

    Vector3 cameraRotation = LookAtRotation(cameraCurrentPos_, cameraLookTarget_);

    cachedCameraPitch_ = cameraRotation.x;
    cachedCameraYaw_ = cameraRotation.y;

    Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
    if (camera) {
        camera->SetTranslate(cameraCurrentPos_);
        camera->SetRotate(cameraRotation);

        if (Input::GetInstance()->TriggerMouse(1)) {
            isZoomed_ = !isZoomed_;
        }
        
        float targetFov = isZoomed_ ? 0.20f : 0.45f;
        float fovT = 1.0f - std::exp(-15.0f * dt);
        currentFov_ += (targetFov - currentFov_) * fovT;
        camera->SetFovY(currentFov_);

        Object3dCommon::GetInstance()->SetCameraPosition(cameraCurrentPos_);
    }
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
