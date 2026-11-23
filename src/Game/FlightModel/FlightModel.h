#pragma once

#include <vector>
#include <memory>
#include "math/MyMath.h"

#include "AirFrame.h"
#include "DamageModel.h"
#include "Engine.h"
#include "Payload/IPayload.h"


// 飛行機の物理挙動を計算・管理するクラス
class FlightModel {
public:
	FlightModel();
	~FlightModel() = default;

	// 初期化
	// airframeData: 機体パラメータ
	// engineData: エンジンパラメータ
	void Initialize(const AirframeData& airframeData, const EngineData& engineData);

	// 更新
	// deltaTime: デルタタイム
	void Update(float deltaTime);


	// -- ペイロードの管理 --
	void AddPayload(std::unique_ptr<IPayload> payload);
	void ClearPayloads() { payloads_.clear(); }
	void FireWeapons(); // トリガーを引いた時用


	// ---setter --
	void SetThrottleInput(float throttle) { targetThrottle_ = throttle; }                      // 0.0f ~ 1.0f
	void SetThrottle(float throttle) { engine_.SetThrottle(throttle); targetThrottle_ = throttle; }
	void SetControlInput(float pitch, float roll, float yaw) { inputPitch_ = pitch; inputRoll_ = roll; inputYaw_ = yaw; }   // 各軸 -1.0f ~ 1.0f
	void SetFlapInput(bool deploy) { airframe_.SetFlapDesired(deploy); }
	void SetAirBrakeInput(bool deploy) { airframe_.SetAirBrakeDesired(deploy); }


	// -- getter --
	MyMath::Vector3 GetPosition() const { return position_; }
	MyMath::Vector3 GetVelocity() const { return velocity_; }
	MyMath::Quaternion GetOrientation() const { return orientation_; }
	float GetSpeed() const { return MyMath::Length(velocity_); }
	float GetTotalMass() const;
	float GetCurrentThrottle() const { return engine_.GetCurrentThrottle(); }
	float GetCurrentFuel() const { return airframe_.GetCurrentInternalFuel(); }
	float GetCurrentAoA() const { return currentAoA_; }
	float GetCurrentG() const { return currentG_; }
	float GetAltitude() const { return position_.y; }
	bool  IsStalling() const { return isStalling_; }
	float GetFlapPosition() const { return airframe_.GetFlapPosition(); }
	float GetAirBrakePosition() const { return airframe_.GetAirBrakePosition(); }
	float GetBlackoutFactor() const { return blackoutFactor_; }   // 0.0～1.0
	float GetRedoutFactor() const { return redoutFactor_; }       // 0.0～1.0

	// ダメージ連動
	Airframe& GetAirframe() { return airframe_; }
	const Airframe& GetAirframe() const { return airframe_; }
	DamageModel& GetDamageModel() { return airframe_.GetDamageModel(); }
	const DamageModel& GetDamageModel() const { return airframe_.GetDamageModel(); }

	// 無敵モード制御
	void SetInvulnerable(bool inv) { isInvulnerable_ = inv; airframe_.GetDamageModel().SetInvulnerable(inv); }
	bool IsInvulnerable() const { return isInvulnerable_; }

	// G効果の制御
	void SetGEffectsEnabled(bool enabled) { gEffectsEnabled_ = enabled; if (!enabled) { ResetGEffects(); } }
	bool IsGEffectsEnabled() const { return gEffectsEnabled_; }
	void ResetGEffects() { blackoutFactor_ = 0.0f; redoutFactor_ = 0.0f; blackoutToleranceTimer_ = 0.0f; redoutToleranceTimer_ = 0.0f; }

	// オートパイロットモード
	void SetAutopilot(bool autopilot) { isAutopilot_ = autopilot; }
	bool IsAutopilot() const { return isAutopilot_; }

	// 弾丸の被弾処理
	HitResult ProcessBulletHit(const MyMath::Vector3& bulletWorldPos, float rawDamage) {
		return airframe_.GetDamageModel().ProcessBulletHit(bulletWorldPos, position_, orientation_, rawDamage);
	}

	// 外部から位置・姿勢・速度をセットする
	void SetPosition(const MyMath::Vector3& pos) { position_ = pos; }
	void SetOrientation(const MyMath::Quaternion& ori) { orientation_ = ori; }
	static constexpr float kDefaultGroundElevation = 0.0f;
	void SetVelocity(const MyMath::Vector3& vel) { velocity_ = vel; }
	void SetGroundElevation(float groundY) { groundElevation_ = groundY; }
	float GetGroundElevation() const { return groundElevation_; }
	void SetWaterSurface(bool hasWater, float waterY, float waterDepth) {
		hasWaterSurface_ = hasWater;
		waterElevation_ = waterY;
		waterDepth_ = waterDepth;
	}
	bool IsInWaterContact() const { return inWaterContact_; }

	// 方向ベクトルの取得
	MyMath::Vector3 GetForwardDirection() const;
	MyMath::Vector3 GetUpDirection() const;
	MyMath::Vector3 GetRightDirection() const;

	// 機体のワールド変換行列を取得
	MyMath::Matrix4x4 GetWorldMatrix() const;


private:
	// コンポーネント
	Airframe airframe_;
	Engine   engine_;

	// 動的に着脱されるペイロード群
	std::vector<std::unique_ptr<IPayload>> payloads_;


	// 物理ステータス
	MyMath::Vector3 position_;      // 座標
	MyMath::Vector3 velocity_;      // 速度
	MyMath::Vector3 acceleration_;  // 加速度
	MyMath::Quaternion orientation_; // 姿勢

	// 角速度(rad/s)
	MyMath::Vector3 angularVelocity_ = { 0.0f, 0.0f, 0.0f };

	// フライトモデル拡張状態
	float currentAoA_;        // 現在の迎え角 (rad)
	float currentG_;          // 現在のG
	bool  isStalling_;        // 失速中フラグ
	float blackoutFactor_;    // ブラックアウト度合い
	float redoutFactor_;      // レッドアウト度合い
	float blackoutToleranceTimer_ = 0.0f; // ブラックアウトまでの猶予タイマー
	float redoutToleranceTimer_ = 0.0f;   // レッドアウトまでの猶予タイマー
	bool  isInvulnerable_ = false;       // 無敵フラグ
	bool  gEffectsEnabled_ = true;       // G効果有効フラグ
	bool  isAutopilot_ = false;          // オートパイロット演出フラグ
	float groundElevation_ = kDefaultGroundElevation; // 現在XZ座標直下の地形標高Y
	bool  hasWaterSurface_ = false;
	float waterElevation_ = 0.0f;
	float waterDepth_ = 0.0f;
	bool  inWaterContact_ = false;

	// プレイヤーからの入力状態
	float targetThrottle_;  // 目標スロットル
	float inputPitch_;      // ピッチ入力
	float inputRoll_;       // ロール入力
	float inputYaw_;        // ヨー入力


	// ヘルパー関数
	MyMath::Vector3 CalculateTotalForce(float deltaTime);
	void UpdateOrientation(float deltaTime);
	float CalculateAirDensity(float altitude) const;
	float CalculateAoA() const;
	float CalculateLiftCoefficient(float aoa) const;
	float CalculateControlEfficiency(float speed) const;
	void  UpdateGEffects(float deltaTime);

};