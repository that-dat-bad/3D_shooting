#include "Camera.h"
#include"../base/WinApp.h"
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

#include <cstdlib>

void Camera::Shake(float duration, float magnitude) {
	shakeTimer_ = duration;
	shakeMagnitude_ = magnitude;
}

void Camera::Update() {
	Vector3 actualTranslate = transform_.translate;
	Vector3 actualRotate = transform_.rotate;

	if (shakeTimer_ > 0.0f) {
		shakeTimer_ -= 1.0f / 60.0f;
		if (shakeTimer_ < 0.0f) {
			shakeTimer_ = 0.0f;
		} else {
			// 乱数で揺れを計算（平行移動ではなく回転を揺らすことで、スカイボックスへの不自然な影響を防ぐ）
			// magnitude が Translate用の値（5.0等）になっているため、Rotate用のスケールに調整
			float angleScale = 0.01f * shakeMagnitude_;
			float offsetX = ((rand() % 100) / 100.0f - 0.5f) * 2.0f * angleScale;
			float offsetY = ((rand() % 100) / 100.0f - 0.5f) * 2.0f * angleScale;
			float offsetZ = ((rand() % 100) / 100.0f - 0.5f) * 2.0f * angleScale * 0.5f; // ロールは控えめに
			
			actualRotate.x += offsetX;
			actualRotate.y += offsetY;
			actualRotate.z += offsetZ;
		}
	}

	worldMatrix_ = MakeAffineMatrix(transform_.scale, actualRotate, actualTranslate);
	viewMatrix_ = Inverse(worldMatrix_);
	projectionMatrix_ = MakePerspectiveMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
}