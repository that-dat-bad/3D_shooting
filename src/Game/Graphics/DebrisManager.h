#pragma once
#include <vector>
#include <memory>
#include "../../engine/Graphics/Model/Object3d.h"
#include "math/MyMath.h"

class Object3dCommon;
class Camera;

/// @brief 切断・破壊されたパーツの破片物理オブジェクト
struct DebrisObject {
	std::unique_ptr<Object3d> object;
	Matrix4x4 initialWorldMatrix = Identity4x4();
	Vector3 position;
	Vector3 rotation;
	Vector3 velocity;
	Vector3 angularVelocity;
	Vector3 scale = { 1.0f, 1.0f, 1.0f };

	float lifetime = 6.0f;
	float maxLifetime = 6.0f;
	float drag = 0.35f;         // 空気抵抗
	float gravity = 9.8f;        // 重力加速度
	float emitSmokeTimer = 0.0f;

	void Update(float deltaTime);
	void Draw();
	bool IsDead() const { return lifetime <= 0.0f; }
};

/// @brief 破片オブジェクトの一括管理・物理シミュレーションクラス
class DebrisManager {
public:
	static DebrisManager* GetInstance();

	DebrisManager() = default;
	~DebrisManager() = default;

	void Initialize(Object3dCommon* object3dCommon, Camera* camera);
	void Update(float deltaTime);
	void Draw();
	void Clear();

	/// @brief 破片を生成してワールドへ放出
	/// @param model 破片用モデル
	/// @param initialWorldMatrix 切り離された瞬間のワールド行列
	/// @param baseVelocity 機体の現在速度
	/// @param ejectionForce 散乱方向の追加力
	/// @param targetNodeName 描画対象ノード名（空の場合はモデル全体）
	void SpawnDebris(Model* model, const Matrix4x4& initialWorldMatrix, const Vector3& baseVelocity, const Vector3& ejectionForce, const std::string& targetNodeName = "");

private:
	DebrisManager(const DebrisManager&) = delete;
	DebrisManager& operator=(const DebrisManager&) = delete;

	static std::unique_ptr<DebrisManager> instance_;

	Object3dCommon* object3dCommon_ = nullptr;
	Camera* camera_ = nullptr;
	std::vector<std::unique_ptr<DebrisObject>> debrisList_;
};
