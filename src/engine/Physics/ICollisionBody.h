#pragma once
#include <cstdint>
#include "Collider.h"

// ============================================================
// 3D 衝突ボディ インターフェース
// ============================================================

/// <summary>
/// 3D衝突判定に参加するオブジェクトが実装するインターフェース
/// 
/// 使い方:
/// 1. ゲームオブジェクトがこのクラスを継承する
/// 2. 属性で「自分が何か」を、マスクで「何と当たるか」を指定
/// 3. マネージャに Register() して UpdateAll() で自動判定
/// </summary>
class ICollisionBody3D {
public:
	virtual ~ICollisionBody3D() = default;

	/// <summary>自分のコライダー形状の種類を返す</summary>
	virtual ColliderType3D GetColliderType3D() const { return ColliderType3D::Sphere; }

	/// <summary>球コライダーを返す</summary>
	virtual SphereCollider GetSphereCollider() const { return {}; }

	/// <summary>AABBコライダーを返す</summary>
	virtual AABBCollider GetAABBCollider() const { return {}; }

	/// <summary>自分の衝突属性ビットを返す</summary>
	virtual uint32_t GetCollisionAttribute() const = 0;

	/// <summary>衝突対象のマスクビットを返す</summary>
	virtual uint32_t GetCollisionMask() const = 0;

	/// <summary>
	/// 衝突時に呼ばれるコールバック
	/// </summary>
	/// <param name="other">衝突した相手</param>
	virtual void OnCollision([[maybe_unused]] ICollisionBody3D* other) {}

	/// <summary>この判定ボディが有効かどうか</summary>
	virtual bool IsCollisionActive() const { return true; }
};

// ============================================================
// 2D 衝突ボディ インターフェース
// ============================================================

/// <summary>
/// 2D衝突判定に参加するオブジェクトが実装するインターフェース
/// UI・マップ判定用
/// </summary>
class ICollisionBody2D {
public:
	virtual ~ICollisionBody2D() = default;

	/// <summary>自分のコライダー形状の種類を返す</summary>
	virtual ColliderType2D GetColliderType2D() const { return ColliderType2D::Rect; }

	/// <summary>円コライダーを返す</summary>
	virtual CircleCollider GetCircleCollider() const { return {}; }

	/// <summary>矩形コライダーを返す</summary>
	virtual RectCollider GetRectCollider() const { return {}; }

	/// <summary>自分の衝突属性ビットを返す</summary>
	virtual uint32_t GetCollisionAttribute() const = 0;

	/// <summary>衝突対象のマスクビットを返す</summary>
	virtual uint32_t GetCollisionMask() const = 0;

	/// <summary>
	/// 衝突時に呼ばれるコールバック
	/// </summary>
	/// <param name="other">衝突した相手</param>
	virtual void OnCollision([[maybe_unused]] ICollisionBody2D* other) {}

	/// <summary>この判定ボディが有効かどうか</summary>
	virtual bool IsCollisionActive() const { return true; }
};
