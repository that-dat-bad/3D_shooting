#include "Collision2DManager.h"
#include <cmath>
#include <algorithm>

// ============================================================
// 登録・クリア
// ============================================================

void Collision2DManager::Register(ICollisionBody2D* body) {
	if (body) {
		bodies_.push_back(body);
	}
}

void Collision2DManager::Clear() {
	bodies_.clear();
}

// ============================================================
// 総当たり判定
// ============================================================

void Collision2DManager::CheckAllCollisions() {
	for (auto itA = bodies_.begin(); itA != bodies_.end(); ++itA) {
		auto itB = itA;
		++itB;
		for (; itB != bodies_.end(); ++itB) {
			ICollisionBody2D* bodyA = *itA;
			ICollisionBody2D* bodyB = *itB;

			if (!bodyA->IsCollisionActive() || !bodyB->IsCollisionActive()) {
				continue;
			}

			if (!ShouldCheckCollision(bodyA, bodyB)) {
				continue;
			}

			if (CheckBodyCollision(bodyA, bodyB)) {
				bodyA->OnCollision(bodyB);
				bodyB->OnCollision(bodyA);
			}
		}
	}
}

// ============================================================
// 属性/マスクフィルタリング
// ============================================================

bool Collision2DManager::ShouldCheckCollision(const ICollisionBody2D* a, const ICollisionBody2D* b) {
	return (a->GetCollisionAttribute() & b->GetCollisionMask()) != 0 &&
	       (b->GetCollisionAttribute() & a->GetCollisionMask()) != 0;
}

// ============================================================
// 形状判定のディスパッチ
// ============================================================

bool Collision2DManager::CheckBodyCollision(ICollisionBody2D* a, ICollisionBody2D* b) {
	ColliderType2D typeA = a->GetColliderType2D();
	ColliderType2D typeB = b->GetColliderType2D();

	if (typeA == ColliderType2D::Circle && typeB == ColliderType2D::Circle) {
		return CheckCircleCircle(a->GetCircleCollider(), b->GetCircleCollider());
	}
	if (typeA == ColliderType2D::Rect && typeB == ColliderType2D::Rect) {
		return CheckRectRect(a->GetRectCollider(), b->GetRectCollider());
	}
	// Circle vs Rect
	if (typeA == ColliderType2D::Circle && typeB == ColliderType2D::Rect) {
		return CheckCircleRect(a->GetCircleCollider(), b->GetRectCollider());
	}
	if (typeA == ColliderType2D::Rect && typeB == ColliderType2D::Circle) {
		return CheckCircleRect(b->GetCircleCollider(), a->GetRectCollider());
	}

	return false;
}

// ============================================================
// ジオメトリユーティリティ
// ============================================================

bool Collision2DManager::CheckCircleCircle(const CircleCollider& a, const CircleCollider& b) {
	float dx = a.center.x - b.center.x;
	float dy = a.center.y - b.center.y;
	float distSq = dx * dx + dy * dy;
	float radiusSum = a.radius + b.radius;
	return distSq <= radiusSum * radiusSum;
}

bool Collision2DManager::CheckRectRect(const RectCollider& a, const RectCollider& b) {
	if (a.max.x < b.min.x || a.min.x > b.max.x) { return false; }
	if (a.max.y < b.min.y || a.min.y > b.max.y) { return false; }
	return true;
}

bool Collision2DManager::CheckCircleRect(const CircleCollider& circle, const RectCollider& rect) {
	float closestX = std::clamp(circle.center.x, rect.min.x, rect.max.x);
	float closestY = std::clamp(circle.center.y, rect.min.y, rect.max.y);

	float dx = circle.center.x - closestX;
	float dy = circle.center.y - closestY;

	float distSq = dx * dx + dy * dy;
	return distSq <= circle.radius * circle.radius;
}

bool Collision2DManager::CheckPointRect(const Vector2& point, const RectCollider& rect) {
	return point.x >= rect.min.x && point.x <= rect.max.x &&
	       point.y >= rect.min.y && point.y <= rect.max.y;
}

bool Collision2DManager::CheckPointCircle(const Vector2& point, const CircleCollider& circle) {
	float dx = point.x - circle.center.x;
	float dy = point.y - circle.center.y;
	float distSq = dx * dx + dy * dy;
	return distSq <= circle.radius * circle.radius;
}
