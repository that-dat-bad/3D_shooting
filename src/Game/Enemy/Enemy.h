#pragma once
#include <memory>
#include <string>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/Physics/ICollisionBody.h"
#include "../../engine/Physics/CollisionConfig.h"
#include "../FlightModel/FlightModel.h"
#include "../FlightModel/Payload/Gunpod.h"
#include "../Bullet/BulletManager.h"
#include "../FlightModel/MouseAimController.h"

class Object3dCommon;
class Camera;

enum class AIType {
	ChaseAttack,
	CruiseEvade,
	FollowWaypoint
};

enum class TypeBState {
	Cruising,
	Evading
};

/// <summary>
/// 空中ターゲット（敵AI）
/// </summary>
class Enemy : public ICollisionBody3D {
public:
	Enemy() = default;
	~Enemy() = default;

	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="position">配置座標</param>
	/// <param name="modelPath">モデルファイルパス</param>
	/// <param name="maxHealth">最大HP</param>
	/// <param name="airframeData">機体パラメータ</param>
	/// <param name="engineData">エンジンパラメータ</param>
	/// <param name="gunpodData">ガンポッドパラメータ</param>
	/// <param name="playerFlightModel">自機のフライトモデルへのポインタ</param>
	/// <param name="bulletManager">弾丸マネージャーへのポインタ</param>
	/// <param name="aiType">AI挙動タイプ</param>
	void Initialize(
		const MyMath::Vector3& position, 
		const std::string& modelPath, 
		float maxHealth,
		const AirframeData& airframeData,
		const EngineData& engineData,
		const GunPodData& gunpodData,
		FlightModel* playerFlightModel,
		BulletManager* bulletManager,
		AIType aiType = AIType::ChaseAttack
	);

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update(float deltaTime);

	/// <summary>
	/// 描画処理
	/// </summary>
	void Draw();

	/// <summary>
	/// ダメージを受ける
	/// </summary>
	/// <param name="damage">ダメージ量</param>
	void TakeDamage(float damage);

	// === アクセッサ ===
	bool IsAlive() const { return isAlive_; }
	MyMath::Vector3 GetPosition() const { return flightModel_.GetPosition(); }
	float GetCollisionRadius() const { return collisionRadius_; }
	float GetHealth() const { return health_; }
	float GetMaxHealth() const { return maxHealth_; }
	FlightModel& GetFlightModel() { return flightModel_; }
	const FlightModel& GetFlightModel() const { return flightModel_; }
	AIType GetAIType() const { return aiType_; }
	TypeBState GetTypeBState() const { return typeBState_; }
	const char* GetTypeBStateString() const {
		return typeBState_ == TypeBState::Cruising ? "Cruising" : "Evading";
	}

	// === ICollisionBody3D 実装 ===
	SphereCollider GetSphereCollider() const override {
		return { flightModel_.GetPosition(), collisionRadius_ };
	}
	uint32_t GetCollisionAttribute() const override { return CollisionAttribute::kEnemy; }
	uint32_t GetCollisionMask() const override { return CollisionMask::kEnemy; }
	void OnCollision(ICollisionBody3D* other) override;
	bool IsCollisionActive() const override { return isAlive_; }

private:
	/// @brief AI思考・操縦入力の更新
	void UpdateAI(float deltaTime);

	// 物理モデルとAIコントローラー
	FlightModel flightModel_;
	MouseAimController aimController_;
	GunPod gunpod_;

	// 参照ポインタ
	FlightModel* playerFlightModel_ = nullptr;
	BulletManager* bulletManager_ = nullptr;

	float health_ = 0.0f;
	float maxHealth_ = 0.0f;
	bool isAlive_ = true;

	// 衝突判定用の半径
	float collisionRadius_ = 5.0f;

	// エフェクト関連
	float muzzleFlashTimer_ = 0.0f;
	int muzzleFlashCount_ = 0;

	// 描画用3Dオブジェクト
	std::unique_ptr<Object3d> object3d_ = nullptr;

	// AI設定・ステート
	AIType aiType_ = AIType::ChaseAttack;
	TypeBState typeBState_ = TypeBState::Cruising;
	MyMath::Vector3 cruiseDirection_ = { 0.0f, 0.0f, 1.0f };
};
