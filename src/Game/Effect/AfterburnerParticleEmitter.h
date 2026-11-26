#pragma once
#include "../../engine/base/Math/MyMath.h"

class Afterburner;

/// <summary>
/// アフターバーナー点火時のスパークバースト＆衝撃波リング、および連続超音速排気粒子エミッタ
/// </summary>
class AfterburnerParticleEmitter {
public:
	void Reset();

	void Update(
		float dt,
		const Afterburner& afterburner,
		const MyMath::Matrix4x4& aircraftWorldMatrix,
		const MyMath::Vector3& aircraftVelocity,
		float intensity);

private:
	static void ExtractNozzlePose(
		const MyMath::Vector3& localOffset,
		const MyMath::Vector3& nozzleRot,
		const MyMath::Matrix4x4& visualWorld,
		MyMath::Vector3& outPos,
		MyMath::Vector3& outExhaustDir);

	void EmitIgnitionBurst(
		const MyMath::Vector3& nozzlePos,
		const MyMath::Vector3& exhaustDir,
		const MyMath::Vector3& aircraftVelocity) const;

	void EmitSupersonicStreamStep(
		const MyMath::Vector3& nozzlePos,
		const MyMath::Vector3& exhaustDir,
		const MyMath::Vector3& aircraftVelocity,
		float flameLength,
		float intensity) const;

	bool wasActive_ = false;
	float ignitionCooldown_ = 0.0f;
	float emitAccum_ = 0.0f;
};
