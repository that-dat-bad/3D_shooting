#pragma once
#include <cstdint>
#include "../../engine/base/Math/MyMath.h"

/// <summary>
/// 極地吹雪・氷晶ダスト・凍結霧および航空機ウェイク相互作用システム
/// （カメラ追従ラップアラウンド空間 ＋ ランブ＝オゼーン翼端渦＆ジェット排気場カップリング）
/// </summary>
class AmbientWeatherSystem {
public:
	enum class WeatherPreset : int {
		Off = 0,
		IceDust = 1,       // 澄んだ極地のダイヤモンドダスト（軽め・約2,800粒子）
		PolarBlizzard = 2, // 本格極地ブリザード＆凍結霧（標準・約6,500粒子）
		ArcticStorm = 3    // ショーケース用・濃密吹雪＆風洞可視化（約11,000粒子）
	};

	struct WakeSource {
		MyMath::Vector3 position = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 velocity = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 forward  = { 0.0f, 0.0f, 1.0f };
		MyMath::Vector3 right    = { 1.0f, 0.0f, 0.0f };
		float halfWingspan = 6.0f;
		float currentG = 1.0f;
		float throttle = 0.6f;
		float afterburnerIntensity = 0.0f;
		bool active = true;
	};

	void Reset(WeatherPreset preset = WeatherPreset::IceDust);

	void SetPreset(WeatherPreset preset);
	WeatherPreset GetPreset() const { return preset_; }
	static const char* GetPresetName(WeatherPreset preset);

	void SetHalfExtent(float halfExtent) { halfExtent_ = halfExtent; }
	float GetHalfExtent() const { return halfExtent_; }

	void SetWindVelocity(const MyMath::Vector3& wind) { customWind_ = wind; useCustomWind_ = true; }
	void ClearCustomWind() { useCustomWind_ = false; }

	/// <summary>
	/// パーティクル全消去後に初回再充填を予約する
	/// </summary>
	void RequestReseed() { needsInitialSeed_ = true; }

	/// <summary>
	/// 航空機ウェイクベクトル場をGPUパーティクルマネージャへ送信する
	/// </summary>
	void SubmitWakeSource(uint32_t slot, const WakeSource& source) const;

	/// <summary>
	/// カメラ追従環境ボリュームの更新および雪・氷晶・霧パーティクルの定常供給
	/// </summary>
	void Update(float dt, const MyMath::Vector3& cameraPosition);

private:
	struct PresetSpec {
		uint32_t targetCrystalCount;
		uint32_t targetFogCount;
		MyMath::Vector3 windVelocity;
		float halfExtent;
		float crystalAlpha;
		float fogAlpha;
	};

	static PresetSpec GetSpec(WeatherPreset preset);
	void EmitBatch(const MyMath::Vector3& center, const PresetSpec& spec, uint32_t crystalCount, uint32_t fogCount, bool staggerInitialLife = false);

	WeatherPreset preset_ = WeatherPreset::IceDust;
	float halfExtent_ = 0.0f; // 0ならプリセット既定値を使用
	MyMath::Vector3 customWind_ = { -8.0f, -9.0f, -4.0f };
	bool useCustomWind_ = false;
	bool needsInitialSeed_ = true;
	float crystalEmitCarry_ = 0.0f;
	float fogEmitCarry_ = 0.0f;
};
