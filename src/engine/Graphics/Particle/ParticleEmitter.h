#pragma once
#include <string>
#include <cstdint>
#include "../base/Math/MyMath.h"
using namespace MyMath;

#include "ParticleManager.h"

/// <summary>
/// パーティクル発生器
/// </summary>
class ParticleEmitter {
public:
	/// <summary>
	/// コンストラクタ
	/// </summary>
	/// <param name="name">パーティクルグループ名</param>
	/// <param name="transform">発生位置などの情報</param>
	/// <param name="count">一度に発生させる数</param>
	/// <param name="frequency">発生頻度(秒間隔)</param>
	ParticleEmitter(const std::string& name, const Transform& transform, uint32_t count, float frequency);

	/// <summary>
	/// 毎フレームの更新処理(時間経過による自動発生)
	/// </summary>
	void Update();

	/// <summary>
	/// 手動でパーティクルを発生させる
	/// </summary>
	void Emit();

	/// <summary>
	/// パラメータの設定
	/// </summary>
	/// <param name="params">パーティクルパラメータ</param>
	void SetParticleParameters(const ParticleParameters& params) { params_ = params; }

private:
	std::string name_;      // 発生させるパーティクルグループ名
	Transform transform_;   // 発生位置などの情報
	uint32_t count_;        // 一度に発生させる数
	float frequency_;       // 発生頻度 (秒単位の間隔)
	float frequencyTime_;   // 経過時間計測用

	ParticleParameters params_; // 発生させるパーティクルのパラメータ
};