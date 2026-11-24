#pragma once
#include "../../engine/base/Math/MyMath.h"

/// @brief パーティクルデモ専用のシネマティック追従カメラ
class ParticleDemoCamera {
public:
	struct CameraContext {
		float dt = 0.0f;
		float elapsed = 0.0f;
		MyMath::Vector3 playerPos{};
		MyMath::Vector3 playerForward{ 0.0f, 0.0f, 1.0f };
		MyMath::Vector3 playerRight{ 1.0f, 0.0f, 0.0f };
		MyMath::Vector3 playerUp{ 0.0f, 1.0f, 0.0f };
		bool hasTarget = false;
		MyMath::Vector3 targetPos{};
		bool isExplosionHold = false;
		bool isFlareFocus = false;
	};

	/// @brief カメラ補間状態をリセット
	void Reset();

	/// @brief 自機と目標が同時に映えるデモカメラ位置・注視点を算出
	void Update(
		const CameraContext& ctx,
		MyMath::Vector3& outCamPos,
		MyMath::Vector3& outLookTarget);

private:
	bool hasCameraState_ = false;
	MyMath::Vector3 smoothRelOffset_{};
	MyMath::Vector3 smoothLookOffset_{};
};
