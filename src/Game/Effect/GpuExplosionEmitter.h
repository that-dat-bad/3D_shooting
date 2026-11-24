#pragma once
#include <cstdint>
#include "../../engine/base/Math/MyMath.h"

/// <summary>
/// GPU 65,536粒子対応の多段階ウルトラリッチ爆発エミッタ
/// （黒体放射火球・3Dカールノイズ爆煙・マッハ衝撃波リング・地面バウンド火花・舞い上がる残り火）
/// </summary>
class GpuExplosionEmitter {
public:
	enum class ScalePreset {
		Tactical,        // 実戦用（約4,800粒子／複数同時爆発対応）
		MassiveShowcase  // コンテスト披露用（約18,000粒子／超高密度）
	};

	/// <summary>
	/// 6〜7層の多段階GPU爆発を一括発生させる
	/// </summary>
	/// <param name="position">爆心ワールド座標</param>
	/// <param name="floorY">地面・地形標高（火花バウンド・地上ダストリング基準高さ）</param>
	/// <param name="scale">空間スケール倍率</param>
	/// <param name="preset">粒子密度プリセット</param>
	static void EmitMultiStage(
		const MyMath::Vector3& position,
		float floorY,
		float scale = 1.0f,
		ScalePreset preset = ScalePreset::Tactical);
};
