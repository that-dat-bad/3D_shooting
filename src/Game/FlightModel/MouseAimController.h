#pragma once
#include "math/MyMath.h"

class FlightModel;

/// <summary>汎用PIDコントローラー（1軸分）</summary>
struct PIDController {
	float kP = 0.0f;    // 比例ゲイン
	float kI = 0.0f;    // 積分ゲイン
	float kD = 0.0f;    // 微分ゲイン

	float integral = 0.0f;      // 積分値
	float prevError = 0.0f;     // 前フレームの誤差
	float maxIntegral = 1.0f;   // 積分アンチワインドアップ上限

	/// <summary>PIDのパラメータを設定</summary>
	void SetGains(float p, float i, float d, float maxI = 1.0f) {
		kP = p; kI = i; kD = d; maxIntegral = maxI;
	}

	/// <summary>誤差と経過時間からPID出力を計算</summary>
	float Update(float error, float dt) {
		// 比例項
		float pTerm = kP * error;

		// 積分項（台形法 + アンチワインドアップ）
		integral += error * dt;
		if (integral > maxIntegral) { integral = maxIntegral; }
		if (integral < -maxIntegral) { integral = -maxIntegral; }
		float iTerm = kI * integral;

		// 微分項（誤差の変化率）
		float derivative = 0.0f;
		if (dt > 0.0001f) {
			derivative = (error - prevError) / dt;
		}
		float dTerm = kD * derivative;

		prevError = error;

		return pTerm + iTerm + dTerm;
	}

	/// <summary>状態をリセット</summary>
	void Reset() {
		integral = 0.0f;
		prevError = 0.0f;
	}
};


/// <summary>
/// War Thunder準拠 マウスエイム・コントローラー
/// ワールド水平基準の球面座標(方位角・仰角±89°)で目標方向を更新し、PID制御で操舵入力を生成。
/// </summary>
class MouseAimController {
public:
	MouseAimController() = default;
	~MouseAimController() = default;

	/// <summary>初期化</summary>
	void Initialize();

	/// マウスの相対移動量でワールド空間の目標方向を回転
	void UpdateTargetDirection(long deltaX, long deltaY, const MyMath::Vector3& camRight, const MyMath::Vector3& camUp);

	/// 目標方向を指定の前方ベクトルにリセット
	void ResetToDirection(const MyMath::Vector3& forward);

	/// PID制御で操舵入力を生成
	/// @param orientation 機体の現在姿勢（クォータニオン）
	/// @param deltaTime フレーム間の経過時間（秒）
	/// @param outPitch 生成されたピッチ入力 (-1 ～ 1)
	/// @param outRoll  生成されたロール入力 (-1 ～ 1)
	/// @param outYaw   生成されたヨー入力 (-1 ～ 1)
	void CalculateSteeringInput(
		const MyMath::Quaternion& orientation,
		float deltaTime,
		float& outPitch, float& outRoll, float& outYaw
	);

	/// 有効/無効の切り替え
	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }
	void ToggleEnabled() { enabled_ = !enabled_; }

	/// マウス感度の設定
	void SetSensitivity(float sensitivity) { sensitivity_ = sensitivity; }
	float GetSensitivity() const { return sensitivity_; }

	/// ワールド空間の目標方向ベクトル（レティクル投影用）
	MyMath::Vector3 GetTargetDirection() const { return targetDirection_; }

	/// PIDコントローラーへのアクセス（ImGuiチューニング用）
	PIDController& GetPitchPID() { return pitchPID_; }
	PIDController& GetRollPID() { return rollPID_; }
	PIDController& GetYawPID() { return yawPID_; }
	PIDController& GetBankPID() { return bankPID_; }

private:
	bool enabled_ = true;

	// マウス感度
	float sensitivity_ = 0.05f; // reduced mouse sensitivity

	// ワールド水平基準の方位角・仰角（War Thunder準拠：360°方位角＋±89°仰角）
	float targetYaw_ = 0.0f;
	float targetPitch_ = 0.0f;

	// ワールド空間の目標方向（単位ベクトル）
	MyMath::Vector3 targetDirection_ = { 0.0f, 0.0f, 1.0f };

	// コーディネートターンの閾値（ラジアン）
	float coordinatedTurnThreshold_ = 0.03f;  // ≈ 1.7度

	// === PIDコントローラー（各軸） ===
	PIDController pitchPID_;   // ピッチ誤差 → ピッチ入力
	PIDController rollPID_;    // バンク誤差 → ロール入力（コーディネートターン時）
	PIDController yawPID_;     // ヨー誤差 → ヨー入力（小角度時）
	PIDController bankPID_;    // 水平復帰用ロール

	// ヘルパー関数
	MyMath::Vector3 GetForwardFromOrientation(const MyMath::Quaternion& q) const;
	MyMath::Vector3 GetUpFromOrientation(const MyMath::Quaternion& q) const;
	MyMath::Vector3 GetRightFromOrientation(const MyMath::Quaternion& q) const;
};
