#pragma once
#include "../../engine/base/Math/MyMath.h"

/// <summary>
/// 機銃発射時の銃口硝煙ストリーム＆着弾時のリッチ跳弾スパークエミッタ
/// </summary>
class GunCombatEffectEmitter {
public:
	static void EmitMuzzleGas(
		const MyMath::Vector3& muzzlePos,
		const MyMath::Vector3& fireDir,
		const MyMath::Vector3& aircraftVelocity,
		const MyMath::Vector3& aircraftUp);

	static void EmitBulletRicochet(
		const MyMath::Vector3& hitPos,
		float floorY);
};
