#pragma once
#include "../../engine/base/Math/MyMath.h"

class Camera;

/// <summary>
/// GPUパーティクル＋専用3D多層シェルのハイブリッド空力ベイパー＆ソニックブーム演出システム
/// （高G翼面凝縮シート・失速剥離乱流ベイパー・連続補間翼端ボルテックス・遷音速コーン・ソニックブーム）
/// </summary>
class AeroVaporSystem {
public:
	struct AircraftAeroState {
		MyMath::Vector3 position = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 velocity = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 forward  = { 0.0f, 0.0f, 1.0f };
		MyMath::Vector3 right    = { 1.0f, 0.0f, 0.0f };
		MyMath::Vector3 up       = { 0.0f, 1.0f, 0.0f };
		float speed = 0.0f;           // m/s
		float currentG = 1.0f;        // G負荷
		float aoaRad = 0.0f;          // 迎角 (rad)
		float criticalAoARad = 0.50f; // 臨界迎角 (rad)
		float halfWingspan = 5.8f;
		bool isStalling = false;      // 失速状態
	};

	void Reset();

	/// <summary>
	/// 毎フレーム更新（高G翼面雲・失速剥離ベイパー・遷音速ベイパー・マッハ1突破判定）
	/// </summary>
	void Update(float dt, const AircraftAeroState& state);

	/// <summary>
	/// 外部ドライバ（ショーケース仮想機等）向けの3Dベイパーシェル状態更新
	/// </summary>
	void UpdateVisualStateExternal(
		float dt,
		const AircraftAeroState& state,
		float wingIntensity,
		float transonicIntensity);

	/// <summary>
	/// 3D多層ベイパーシェル（主翼凝縮シート＆遷音速コーン）の描画
	/// </summary>
	void Draw(Camera* camera) const;

	/// <summary>
	/// ソニックブーム衝撃波＆プラントル＝グロワート凝縮雲を手動発火（ショーケース／実戦共通）
	/// </summary>
	void TriggerSonicBoom(
		const MyMath::Vector3& position,
		const MyMath::Vector3& forward,
		const MyMath::Vector3& velocity,
		float scale = 1.0f);

	/// <summary>
	/// 高G翼面凝縮シート＋連続翼端ボルテックスを1ステップ分発生
	/// </summary>
	void EmitHighGWingVaporStep(const AircraftAeroState& state, float intensity);

	/// <summary>
	/// 失速・大迎角時の翼上面剥離乱流ベイパーを1ステップ分発生
	/// </summary>
	void EmitStallSeparationStep(const AircraftAeroState& state, float stallIntensity);

	/// <summary>
	/// 遷音速ベイパーコーン雲を1ステップ分手動発生（ショーケース用）
	/// </summary>
	void EmitTransonicConeStep(const AircraftAeroState& state, float intensity);

	/// <summary>
	/// 直近フレームで音速突破ソニックブームが発火したかを消費取得
	/// </summary>
	bool ConsumeSonicBoomTriggered();

	float GetCurrentMach() const { return currentMach_; }
	float GetCurrentWingVaporIntensity() const { return wingVaporIntensity_; }
	float GetCurrentStallVaporIntensity() const { return stallVaporIntensity_; }
	float GetCurrentTransonicIntensity() const { return transonicIntensity_; }

private:
	void UpdateAeroFlowMetrics(float dt, const AircraftAeroState& state);

	static constexpr float kSpeedOfSound = 340.29f; // m/s (海面標準大気)

	float currentMach_ = 0.0f;
	float wingVaporIntensity_ = 0.0f;
	float stallVaporIntensity_ = 0.0f;
	float transonicIntensity_ = 0.0f;
	float wingEmitAccum_ = 0.0f;
	float stallEmitAccum_ = 0.0f;
	float coneEmitAccum_ = 0.0f;
	float sonicBoomCooldown_ = 0.0f;
	bool wasSupersonic_ = false;
	bool sonicBoomTriggered_ = false;

	MyMath::Vector3 prevLeftTip_{};
	MyMath::Vector3 prevRightTip_{};
	bool hasPrevTips_ = false;

	MyMath::Vector3 prevUp_{ 0.0f, 1.0f, 0.0f };
	bool hasPrevUp_ = false;
	float smoothedAoARad_ = 0.0f;
	float smoothedSideslipRad_ = 0.0f;
	float smoothedRollAsymmetry_ = 0.0f;
	float airspeedScale_ = 1.0f;

	AircraftAeroState lastState_{};
	bool hasValidState_ = false;
	float elapsedTime_ = 0.0f;
	float smoothedWingIntensity_ = 0.0f;
	float smoothedTransonicIntensity_ = 0.0f;
};
