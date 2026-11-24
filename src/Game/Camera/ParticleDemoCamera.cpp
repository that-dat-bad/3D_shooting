#include "ParticleDemoCamera.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

namespace {
	constexpr float kSafeMinCamAltitude = 12.0f;
	constexpr float kCamFollowSharpness = 6.0f;
	constexpr float kMaxPlayerViewAngleRad = 0.17f; // 横FOV半角(~0.38rad)・縦FOV半角(0.225rad)内に自機を収める上限
}

void ParticleDemoCamera::Reset() {
	hasCameraState_ = false;
	smoothRelOffset_ = {};
	smoothLookOffset_ = {};
}

void ParticleDemoCamera::Update(
	const CameraContext& ctx,
	Vector3& outCamPos,
	Vector3& outLookTarget)
{
	const Vector3 worldUp{ 0.0f, 1.0f, 0.0f };
	const Vector3 pos = ctx.playerPos;
	const Vector3 forward = (Length(ctx.playerForward) > 0.001f)
		? Normalize(ctx.playerForward)
		: Vector3{ 0.0f, 0.0f, 1.0f };

	// 敵ターゲット（または撃破直後の爆煙ホールド座標）がある交戦区間では
	// 自機と敵／爆散を同時にフレームインさせる（撃破ホールド中は時間・フレアに関わらず注視を継続）
	Vector3 toTarget = Subtract(ctx.targetPos, pos);
	const float targetDist = Length(toTarget);
	const bool frameTarget = ctx.hasTarget && targetDist > 6.0f && targetDist < 2600.0f &&
		(ctx.isExplosionHold || (ctx.elapsed < 14.0f && !ctx.isFlareFocus));
	const bool frameFlare = !frameTarget && ctx.isFlareFocus && (ctx.elapsed >= 11.0f);

	Vector3 desiredRelOffset{};
	Vector3 desiredLookOffset{};

	if (frameTarget) {
		const Vector3 targetDir = Multiply(1.0f / targetDist, toTarget);

		// 撃破ホールド中は常に敵(爆散地点)方向の反対側にカメラを構え、通り過ぎ後も爆散と自機を同時に映す
		const float fwdDot = Dot(forward, targetDir);
		const Vector3 aimAxis = (ctx.isExplosionHold || fwdDot > -0.25f)
			? targetDir
			: Normalize(Lerp(forward, targetDir, 0.35f));
		Vector3 side = Cross(worldUp, aimAxis);
		if (Length(side) < 0.001f) {
			side = ctx.playerRight;
		} else {
			side = Normalize(side);
		}

		float backDist = 29.0f;
		float sideOffset = -4.2f;
		float upOffset = 2.2f;
		if (ctx.elapsed < 6.2f) {
			const float p = std::clamp(ctx.elapsed / 6.2f, 0.0f, 1.0f);
			backDist = 28.0f + 2.0f * p;
			sideOffset = -4.2f + 1.0f * std::sin(p * 3.14159265f);
			upOffset = 2.1f + 0.4f * p;
		} else {
			const float p = std::clamp((ctx.elapsed - 6.2f) / 7.8f, 0.0f, 1.0f);
			backDist = 30.0f + 2.5f * std::sin(p * 3.14159265f);
			sideOffset = 4.6f * std::cos(p * 1.8f);
			upOffset = 2.4f;
		}

		desiredRelOffset = Add(
			Add(Multiply(-backDist, aimAxis), Multiply(sideOffset, side)),
			Multiply(upOffset, worldUp));

		// 注視点を敵そのもの（targetPos）に合わせ、自機を手前に収めつつ敵をしっかり注視する
		desiredLookOffset = toTarget;
	} else if (frameFlare) {
		// フレア放出時：横斜め前から自機後方・下方へ広がるオレンジフレア群と煙トレイルを注視
		Vector3 horizFwd = { forward.x, 0.0f, forward.z };
		horizFwd = (Length(horizFwd) > 0.001f) ? Normalize(horizFwd) : Vector3{ 0.0f, 0.0f, 1.0f };
		const Vector3 horizRight = Normalize(Cross(worldUp, horizFwd));

		desiredRelOffset = Add(
			Add(Multiply(8.0f, horizFwd), Multiply(-20.0f, horizRight)),
			Multiply(4.0f, worldUp));
		desiredLookOffset = Add(Multiply(-9.0f, forward), Multiply(-2.5f, worldUp));
	} else {
		// 高速通過（ベイパーコーン／ソニックブーム）や高AoAブレイク時は機体中心のシネマティック構図
		Vector3 horizFwd = { forward.x, 0.0f, forward.z };
		horizFwd = (Length(horizFwd) > 0.001f) ? Normalize(horizFwd) : Vector3{ 0.0f, 0.0f, 1.0f };
		const Vector3 horizRight = Normalize(Cross(worldUp, horizFwd));
		const Vector3 camFwdBasis = Normalize(Lerp(horizFwd, forward, 0.35f));
		const Vector3 camRightBasis = Normalize(Lerp(horizRight, ctx.playerRight, 0.20f));
		const Vector3 camUpBasis = Normalize(Lerp(worldUp, ctx.playerUp, 0.15f));

		float backDist = 27.0f;
		float sideOffset = -7.5f;
		float upOffset = 4.8f;
		float lookAhead = 6.0f;

		if (ctx.elapsed >= 14.5f && ctx.elapsed < 15.5f) {
			const float p = ctx.elapsed - 14.5f;
			const float angle = 1.15f * std::sin(p * 3.14159265f);
			backDist = 28.0f * std::cos(angle);
			sideOffset = -28.0f * std::sin(angle);
			upOffset = 3.0f;
			lookAhead = 4.0f;
		} else if (ctx.elapsed >= 21.5f) {
			backDist = 25.0f;
			sideOffset = -9.5f;
			upOffset = 8.5f;
			lookAhead = 3.5f;
		}

		desiredRelOffset = Add(
			Add(Multiply(-backDist, camFwdBasis), Multiply(sideOffset, camRightBasis)),
			Multiply(upOffset, camUpBasis));
		desiredLookOffset = Add(Multiply(lookAhead, forward), Multiply(0.5f, ctx.playerUp));
	}

	// ワールド座標ではなく自機からの相対オフセットを補間し、高速飛行時のカメラ取り残しを防止
	if (!hasCameraState_) {
		smoothRelOffset_ = desiredRelOffset;
		smoothLookOffset_ = desiredLookOffset;
		hasCameraState_ = true;
	} else {
		const float sharpness = ctx.isExplosionHold ? 10.0f : kCamFollowSharpness;
		const float alpha = 1.0f - std::exp(-(std::max)(ctx.dt, 0.0f) * sharpness);
		smoothRelOffset_ = Lerp(smoothRelOffset_, desiredRelOffset, alpha);
		smoothLookOffset_ = Lerp(smoothLookOffset_, desiredLookOffset, alpha);
	}

	outCamPos = Add(pos, smoothRelOffset_);
	outCamPos.y = (std::max)(outCamPos.y, kSafeMinCamAltitude);
	outLookTarget = Add(pos, smoothLookOffset_);

	// どんな角度でも自機が画角外へ外れないよう注視方向をクランプ
	const Vector3 camToPlayerVec = Subtract(pos, outCamPos);
	const Vector3 camToLookVec = Subtract(outLookTarget, outCamPos);
	const float playerDist = Length(camToPlayerVec);
	const float lookDist = Length(camToLookVec);
	if (playerDist > 0.01f && lookDist > 0.01f) {
		const Vector3 dirPlayer = Multiply(1.0f / playerDist, camToPlayerVec);
		const Vector3 dirLook = Multiply(1.0f / lookDist, camToLookVec);
		const float dotVal = std::clamp(Dot(dirPlayer, dirLook), -1.0f, 1.0f);
		const float angle = std::acos(dotVal);
		if (angle > kMaxPlayerViewAngleRad) {
			const float keepRatio = kMaxPlayerViewAngleRad / angle;
			const Vector3 clampedDir = Normalize(Lerp(dirPlayer, dirLook, keepRatio));
			outLookTarget = Add(outCamPos, Multiply(lookDist, clampedDir));
		}
	}
}
