#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Physics/Collider.h"

class Model;
enum class DamagePart : uint8_t;
struct SubColliderDef;

struct MeshTriangle {
	MyMath::Vector3 v0{};
	MyMath::Vector3 v1{};
	MyMath::Vector3 v2{};
};

struct MeshWireEdge {
	MyMath::Vector3 a{};
	MyMath::Vector3 b{};
};

struct PartMeshCollider {
	DamagePart part{};
	AABBCollider localBounds{};
	std::vector<MeshTriangle> triangles;
	std::vector<MeshWireEdge> wireEdges;
};

struct MeshHitResult {
	bool hit = false;
	DamagePart part{};
	float distance = 0.0f;
	MyMath::Vector3 localPoint{};
	MyMath::Vector3 localNormal{};
	MeshTriangle hitTriangle{};
	uint32_t triangleIndex = 0;
};

// モデルメッシュから構築する機体部位別コリジョン（FlightModelローカル空間基準）
class AircraftMeshCollider {
public:
	bool BuildFromModel(
		const Model* model,
		const MyMath::Vector3& baseRotation = { 0.0f, 3.14159265358979323846f, 0.0f }
	);

	void Clear();

	bool IsValid() const { return !parts_.empty() && totalTriangleCount_ > 0; }
	const std::vector<PartMeshCollider>& GetParts() const { return parts_; }
	const AABBCollider& GetTotalBounds() const { return totalBounds_; }
	float GetBoundingRadius() const { return boundingRadius_; }
	size_t GetTotalTriangleCount() const { return totalTriangleCount_; }
	size_t GetTotalEdgeCount() const { return totalEdgeCount_; }

	// ローカル空間でのレイ交差判定（パーツAABBで枝刈り後、三角形と厳密判定）
	MeshHitResult IntersectRayLocal(
		const Ray& localRay,
		const std::function<bool(DamagePart)>& isPartEnabled = nullptr
	) const;

	// ローカル空間での線分（前フレーム→現フレーム弾丸軌跡）交差判定
	MeshHitResult IntersectSegmentLocal(
		const MyMath::Vector3& localStart,
		const MyMath::Vector3& localEnd,
		float paddingMeters = 0.35f,
		const std::function<bool(DamagePart)>& isPartEnabled = nullptr
	) const;

	// メッシュの各部位AABBと内部モジュール配置から地上/内部衝突用SubCollider群を自動生成
	std::vector<SubColliderDef> GenerateSubColliders() const;

private:
	std::vector<PartMeshCollider> parts_;
	AABBCollider totalBounds_{};
	float boundingRadius_ = 5.0f;
	size_t totalTriangleCount_ = 0;
	size_t totalEdgeCount_ = 0;
};
