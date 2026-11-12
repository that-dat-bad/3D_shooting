#pragma once
#include "Collision3DManager.h"
#include "Collision2DManager.h"

/// <summary>
/// 衝突判定の統合マネージャ（Facade）
/// 3D・2D の判定マネージャをまとめて管理する
/// Scene 側はこのクラスのみに依存すればよい
/// </summary>
class CollisionSystem {
public:
	// ============================================================
	// マネージャへのアクセス
	// ============================================================

	/// <summary>3D判定マネージャを取得</summary>
	Collision3DManager& Get3D() { return manager3d_; }
	const Collision3DManager& Get3D() const { return manager3d_; }

	/// <summary>2D判定マネージャを取得</summary>
	Collision2DManager& Get2D() { return manager2d_; }
	const Collision2DManager& Get2D() const { return manager2d_; }

	// ============================================================
	// ショートカット（直接登録）
	// ============================================================

	/// <summary>3Dコライダーを登録</summary>
	void Register(ICollisionBody3D* body) { manager3d_.Register(body); }

	/// <summary>2Dコライダーを登録</summary>
	void Register(ICollisionBody2D* body) { manager2d_.Register(body); }

	// ============================================================
	// フレーム制御
	// ============================================================

	/// <summary>全判定を実行（3D → 2D の順）</summary>
	void UpdateAll();

	/// <summary>全登録をクリア（フレーム開始時）</summary>
	void ClearAll();

private:
	Collision3DManager manager3d_;
	Collision2DManager manager2d_;
};
