#pragma once
#include <list>
#include "Collider.h"
#include "ICollisionBody.h"

/// <summary>
/// 3D衝突判定マネージャ
/// 登録されたコライダーを毎フレーム総当たり判定し、コールバックを呼ぶ
/// </summary>
class Collision3DManager {
public:
	/// <summary>
	/// コライダーを登録する（毎フレーム呼ぶ）
	/// </summary>
	/// <param name="body">判定に参加するオブジェクト</param>
	void Register(ICollisionBody3D* body);

	/// <summary>
	/// 登録済みの全コライダーの総当たり判定を実行
	/// 衝突があれば双方の OnCollision() を呼ぶ
	/// </summary>
	void CheckAllCollisions();

	/// <summary>登録リストをクリア（フレーム開始時に呼ぶ）</summary>
	void Clear();

	/// <summary>現在の登録数を取得</summary>
	size_t GetBodyCount() const { return bodies_.size(); }

	// ============================================================
	// ジオメトリユーティリティ (static)
	// ============================================================

	/// <summary>球と球の衝突判定</summary>
	static bool CheckSphereSphere(const SphereCollider& a, const SphereCollider& b);

	/// <summary>AABBとAABBの衝突判定</summary>
	static bool CheckAABBAABB(const AABBCollider& a, const AABBCollider& b);

	/// <summary>球とAABBの衝突判定</summary>
	static bool CheckSphereAABB(const SphereCollider& sphere, const AABBCollider& aabb);

	/// <summary>レイとAABBの衝突判定（高速アーリーアウト用）</summary>
	static bool CheckRayAABB(const Ray& ray, const AABBCollider& aabb, float* outTMin = nullptr);

	/// <summary>レイと三角形ポリゴンの Möller–Trumbore 交差判定</summary>
	static bool RayTriangleIntersect(
		const Ray& ray,
		const MyMath::Vector3& v0,
		const MyMath::Vector3& v1,
		const MyMath::Vector3& v2,
		RaycastHit* outHit = nullptr);

	/// <summary>
	/// 座標が地面より下かどうか
	/// </summary>
	/// <param name="position">判定座標</param>
	/// <param name="groundY">地面の高さ（デフォルト 0）</param>
	static bool CheckGroundCollision(const MyMath::Vector3& position, float groundY = 0.0f);

private:
	/// <summary>属性/マスクの組み合わせで判定すべきかチェック</summary>
	static bool ShouldCheckCollision(const ICollisionBody3D* a, const ICollisionBody3D* b);

	/// <summary>2つのボディの形状に応じた衝突判定</summary>
	static bool CheckBodyCollision(ICollisionBody3D* a, ICollisionBody3D* b);

	std::list<ICollisionBody3D*> bodies_;
};
