#include "Camera.h"
#include"../base/WinApp.h"
#include <cstdlib>

Camera::Camera()

	: transform_({ 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f })
	, fovY_(0.45f)
	, aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
	, nearClip_(0.1f)
	, farClip_(100000.0f)
	, worldMatrix_(MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate))
	, viewMatrix_(Inverse(worldMatrix_))
	, projectionMatrix_(MakePerspectiveMatrix(fovY_, aspectRatio_, nearClip_, farClip_))
	, viewProjectionMatrix_(Multiply(viewMatrix_, projectionMatrix_))
{
}

void Camera::Shake(float duration, float magnitude) {
	shakeTimer_ = duration;
	shakeMagnitude_ = magnitude;
}

void Camera::StepShakeOncePerFrame() {
	if (shakeSteppedThisFrame_) {
		return;
	}
	shakeSteppedThisFrame_ = true;

	if (shakeTimer_ > 0.0f) {
		shakeTimer_ -= 1.0f / 60.0f;
		if (shakeTimer_ <= 0.0f) {
			shakeTimer_ = 0.0f;
			shakeOffset_ = { 0.0f, 0.0f, 0.0f };
		} else {
			// 乱数で揺れを計算（平行移動ではなく回転を揺らすことで、スカイボックスへの不自然な影響を防ぐ）
			// magnitude が Translate用の値（5.0等）になっているため、Rotate用のスケールに調整
			float angleScale = 0.01f * shakeMagnitude_;
			shakeOffset_.x = ((rand() % 100) / 100.0f - 0.5f) * 2.0f * angleScale;
			shakeOffset_.y = ((rand() % 100) / 100.0f - 0.5f) * 2.0f * angleScale;
			shakeOffset_.z = ((rand() % 100) / 100.0f - 0.5f) * 2.0f * angleScale * 0.5f; // ロールは控えめに
		}
	} else {
		shakeOffset_ = { 0.0f, 0.0f, 0.0f };
	}
}

void Camera::Update() {
	StepShakeOncePerFrame();
	UpdateMatrices();
	shakeSteppedThisFrame_ = false;
}

void Camera::UpdateMatrices() {
	StepShakeOncePerFrame();

	Vector3 actualTranslate = transform_.translate;
	Vector3 actualRotate = Add(transform_.rotate, shakeOffset_);

	worldMatrix_ = MakeAffineMatrix(transform_.scale, actualRotate, actualTranslate);
	viewMatrix_ = Inverse(worldMatrix_);
	projectionMatrix_ = MakePerspectiveMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
}
