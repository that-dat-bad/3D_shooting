#pragma once
#include "math/MyMath.h"

class FlightModel;

// フライトインストラクター
// 操作がないときに機体を自動で水平に戻す補助システム。
class FlightInstructor {
public:
	FlightInstructor() = default;
	~FlightInstructor() = default;

	// インストラクターを有効/無効に切り替える
	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }
	void ToggleEnabled() { enabled_ = !enabled_; }

	// 補正の強さを設定（0.0 ～ 1.0, デフォルト 0.5）
	void SetCorrectionStrength(float strength) { correctionStrength_ = strength; }
	float GetCorrectionStrength() const { return correctionStrength_; }

	// プレイヤーの入力と機体の現在姿勢を受け取り、補正済みの操舵入力を返す。
	// rawPitch: プレイヤーのピッチ入力 (-1 ～ 1)
	// rawRoll: プレイヤーのロール入力 (-1 ～ 1)
	// rawYaw: プレイヤーのヨー入力 (-1 ～ 1)
	// orientation: 機体の現在の姿勢クォータニオン
	// outPitch: 補正後のピッチ出力
	// outRoll: 補正後のロール出力
	// outYaw: 補正後のヨー出力
	void ApplyCorrection(
		float rawPitch, float rawRoll, float rawYaw,
		const MyMath::Quaternion& orientation,
		float& outPitch, float& outRoll, float& outYaw
	) const;

private:
	bool  enabled_ = true;
	float correctionStrength_ = 0.5f; // 補正の強さ (0～1)

	// クォータニオンからUp方向を取得するヘルパー
	MyMath::Vector3 GetUpFromOrientation(const MyMath::Quaternion& q) const;
	MyMath::Vector3 GetForwardFromOrientation(const MyMath::Quaternion& q) const;
};
