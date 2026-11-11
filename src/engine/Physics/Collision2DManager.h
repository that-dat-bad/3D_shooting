#pragma once
#include <list>
#include "Collider.h"
#include "ICollisionBody.h"

/// <summary>
/// 2D衝突判定マネージャ
/// UI・マップ要素の衝突判定を管理する
/// </summary>
class Collision2DManager {
public:
	/// <summary>
	/// コライダーを登録する（毎フレーム呼ぶ）
	/// </summary>
	/// <param name="body">判定に参加するオブジェクト</param>
	void Register(ICollisionBody2D* body);

	/// <summary>登録済みの全コライダーの総当たり判定を実行</summary>
	void CheckAllCollisions();

	/// <summary>登録リストをクリア（フレーム開始時に呼ぶ）</summary>
	void Clear();

	/// <summary>現在の登録数を取得</summary>
	size_t GetBodyCount() const { return bodies_.size(); }

	// ============================================================
	// ジオメトリユーティリティ (static)
	// ============================================================

	/// <summary>円と円の衝突判定</summary>
	static bool CheckCircleCircle(const CircleCollider& a, const CircleCollider& b);

	/// <summary>矩形と矩形の衝突判定（AABB 2D）</summary>
	static bool CheckRectRect(const RectCollider& a, const RectCollider& b);

	/// <summary>円と矩形の衝突判定</summary>
	static bool CheckCircleRect(const CircleCollider& circle, const RectCollider& rect);

	/// <summary>点が矩形内にあるか（UI クリック判定用）</summary>
	static bool CheckPointRect(const Vector2& point, const RectCollider& rect);

	/// <summary>点が円内にあるか</summary>
	static bool CheckPointCircle(const Vector2& point, const CircleCollider& circle);

private:
	/// <summary>属性/マスクの組み合わせで判定すべきかチェック</summary>
	static bool ShouldCheckCollision(const ICollisionBody2D* a, const ICollisionBody2D* b);

	/// <summary>2つのボディの形状に応じた衝突判定</summary>
	static bool CheckBodyCollision(ICollisionBody2D* a, ICollisionBody2D* b);

	std::list<ICollisionBody2D*> bodies_;
};
