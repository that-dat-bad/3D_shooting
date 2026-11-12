#include "Collision3DManager.h"
#include <cmath>
#include <algorithm>

// ============================================================
// 登録・クリア
// ============================================================

void Collision3DManager::Register(ICollisionBody3D* body) {
	if (body) {
		bodies_.push_back(body);
	}
}

void Collision3DManager::Clear() {
	bodies_.clear();
}

// ============================================================
// 総当たり判定
// ============================================================

void Collision3DManager::CheckAllCollisions() {
	// 全ペアを総当たりで判定
	for (auto itA = bodies_.begin(); itA != bodies_.end(); ++itA) {
		auto itB = itA;
		++itB;
		for (; itB != bodies_.end(); ++itB) {
			ICollisionBody3D* bodyA = *itA;
			ICollisionBody3D* bodyB = *itB;

			// 非アクティブならスキップ
			if (!bodyA->IsCollisionActive() || !bodyB->IsCollisionActive()) {
				continue;
			}

			// 属性/マスクフィルタリング
			if (!ShouldCheckCollision(bodyA, bodyB)) {
				continue;
			}

			// 形状に応じた衝突判定
			if (CheckBodyCollision(bodyA, bodyB)) {
				// 双方のコールバックを呼ぶ
				bodyA->OnCollision(bodyB);
				bodyB->OnCollision(bodyA);
			}
		}
	}
}

// ============================================================
// 属性/マスクフィルタリング
// ============================================================

bool Collision3DManager::ShouldCheckCollision(const ICollisionBody3D* a, const ICollisionBody3D* b) {
	// 双方が相手の属性を衝突対象として認めている場合のみ判定する
	return (a->GetCollisionAttribute() & b->GetCollisionMask()) != 0 &&
	       (b->GetCollisionAttribute() & a->GetCollisionMask()) != 0;
}

// ============================================================
// 形状判定のディスパッチ
// ============================================================

bool Collision3DManager::CheckBodyCollision(ICollisionBody3D* a, ICollisionBody3D* b) {
	ColliderType3D typeA = a->GetColliderType3D();
	ColliderType3D typeB = b->GetColliderType3D();

	if (typeA == ColliderType3D::Sphere && typeB == ColliderType3D::Sphere) {
		return CheckSphereSphere(a->GetSphereCollider(), b->GetSphereCollider());
	}
	if (typeA == ColliderType3D::AABB && typeB == ColliderType3D::AABB) {
		return CheckAABBAABB(a->GetAABBCollider(), b->GetAABBCollider());
	}
	// Sphere vs AABB（順序を正規化）
	if (typeA == ColliderType3D::Sphere && typeB == ColliderType3D::AABB) {
		return CheckSphereAABB(a->GetSphereCollider(), b->GetAABBCollider());
	}
	if (typeA == ColliderType3D::AABB && typeB == ColliderType3D::Sphere) {
		return CheckSphereAABB(b->GetSphereCollider(), a->GetAABBCollider());
	}

	return false;
}

// ============================================================
// ジオメトリユーティリティ
// ============================================================

bool Collision3DManager::CheckSphereSphere(const SphereCollider& a, const SphereCollider& b) {
	MyMath::Vector3 diff = MyMath::Subtract(a.center, b.center);
	float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
	float radiusSum = a.radius + b.radius;
	return distSq <= radiusSum * radiusSum;
}

bool Collision3DManager::CheckAABBAABB(const AABBCollider& a, const AABBCollider& b) {
	// 各軸で重なりがなければ衝突しない
	if (a.max.x < b.min.x || a.min.x > b.max.x) { return false; }
	if (a.max.y < b.min.y || a.min.y > b.max.y) { return false; }
	if (a.max.z < b.min.z || a.min.z > b.max.z) { return false; }
	return true;
}

bool Collision3DManager::CheckSphereAABB(const SphereCollider& sphere, const AABBCollider& aabb) {
	// 球の中心からAABBへの最近接点を求める
	float closestX = std::clamp(sphere.center.x, aabb.min.x, aabb.max.x);
	float closestY = std::clamp(sphere.center.y, aabb.min.y, aabb.max.y);
	float closestZ = std::clamp(sphere.center.z, aabb.min.z, aabb.max.z);

	float dx = sphere.center.x - closestX;
	float dy = sphere.center.y - closestY;
	float dz = sphere.center.z - closestZ;

	float distSq = dx * dx + dy * dy + dz * dz;
	return distSq <= sphere.radius * sphere.radius;
}

bool Collision3DManager::CheckRayAABB(const Ray& ray, const AABBCollider& aabb, float* outTMin) {
	float tMin = 0.0f;
	float tMax = ray.maxDistance;

	// X軸
	if (std::abs(ray.direction.x) < 1e-6f) {
		if (ray.origin.x < aabb.min.x || ray.origin.x > aabb.max.x) return false;
	} else {
		float invD = 1.0f / ray.direction.x;
		float t1 = (aabb.min.x - ray.origin.x) * invD;
		float t2 = (aabb.max.x - ray.origin.x) * invD;
		if (t1 > t2) std::swap(t1, t2);
		tMin = std::max(tMin, t1);
		tMax = std::min(tMax, t2);
		if (tMin > tMax) return false;
	}

	// Y軸
	if (std::abs(ray.direction.y) < 1e-6f) {
		if (ray.origin.y < aabb.min.y || ray.origin.y > aabb.max.y) return false;
	} else {
		float invD = 1.0f / ray.direction.y;
		float t1 = (aabb.min.y - ray.origin.y) * invD;
		float t2 = (aabb.max.y - ray.origin.y) * invD;
		if (t1 > t2) std::swap(t1, t2);
		tMin = std::max(tMin, t1);
		tMax = std::min(tMax, t2);
		if (tMin > tMax) return false;
	}

	// Z軸
	if (std::abs(ray.direction.z) < 1e-6f) {
		if (ray.origin.z < aabb.min.z || ray.origin.z > aabb.max.z) return false;
	} else {
		float invD = 1.0f / ray.direction.z;
		float t1 = (aabb.min.z - ray.origin.z) * invD;
		float t2 = (aabb.max.z - ray.origin.z) * invD;
		if (t1 > t2) std::swap(t1, t2);
		tMin = std::max(tMin, t1);
		tMax = std::min(tMax, t2);
		if (tMin > tMax) return false;
	}

	if (outTMin) *outTMin = tMin;
	return true;
}

bool Collision3DManager::RayTriangleIntersect(
	const Ray& ray,
	const MyMath::Vector3& v0,
	const MyMath::Vector3& v1,
	const MyMath::Vector3& v2,
	RaycastHit* outHit)
{
	const float kEpsilon = 1e-6f;

	MyMath::Vector3 edge1 = MyMath::Subtract(v1, v0);
	MyMath::Vector3 edge2 = MyMath::Subtract(v2, v0);

	MyMath::Vector3 h = MyMath::Cross(ray.direction, edge2);
	float a = MyMath::Dot(edge1, h);

	// 平行（レイとポリゴン平面が交差しない）の場合は即座にスキップ
	if (a > -kEpsilon && a < kEpsilon) {
		return false;
	}

	float f = 1.0f / a;
	MyMath::Vector3 s = MyMath::Subtract(ray.origin, v0);
	float u = f * MyMath::Dot(s, h);

	// 重心座標 u が外側の場合、即座に早期脱出（アーリーアウト）
	if (u < 0.0f || u > 1.0f) {
		return false;
	}

	MyMath::Vector3 q = MyMath::Cross(s, edge1);
	float v = f * MyMath::Dot(ray.direction, q);

	// 重心座標 v および (u + v) が外側の場合、即座に早期脱出（アーリーアウト）
	if (v < 0.0f || u + v > 1.0f) {
		return false;
	}

	// 距離 t の算出
	float t = f * MyMath::Dot(edge2, q);

	if (t > kEpsilon && t <= ray.maxDistance) {
		if (outHit) {
			outHit->hit = true;
			outHit->distance = t;
			outHit->point = MyMath::Add(ray.origin, MyMath::Multiply(t, ray.direction));

			// 法線ベクトルの計算 (edge1 x edge2)
			MyMath::Vector3 n = MyMath::Cross(edge1, edge2);
			outHit->normal = MyMath::Normalize(n);
		}
		return true;
	}

	return false;
}

bool Collision3DManager::CheckGroundCollision(const MyMath::Vector3& position, float groundY) {
	return position.y <= groundY;
}
