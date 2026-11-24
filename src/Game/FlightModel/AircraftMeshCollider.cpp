#include "AircraftMeshCollider.h"
#include "DamageModel.h"
#include "../../engine/Graphics/Model/Model.h"
#include "../../engine/Physics/Collision3DManager.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

using namespace MyMath;

namespace {
	struct QuantizedPoint {
		int64_t x;
		int64_t y;
		int64_t z;

		bool operator==(const QuantizedPoint& rhs) const {
			return x == rhs.x && y == rhs.y && z == rhs.z;
		}
		bool operator<(const QuantizedPoint& rhs) const {
			if (x != rhs.x) return x < rhs.x;
			if (y != rhs.y) return y < rhs.y;
			return z < rhs.z;
		}
	};

	struct QuantizedEdgeKey {
		QuantizedPoint a;
		QuantizedPoint b;

		bool operator==(const QuantizedEdgeKey& rhs) const {
			return a == rhs.a && b == rhs.b;
		}
	};

	struct QuantizedEdgeHash {
		size_t operator()(const QuantizedEdgeKey& k) const {
			auto mix = [](int64_t v) -> size_t {
				return std::hash<int64_t>{}(v);
			};
			size_t h = mix(k.a.x);
			h ^= mix(k.a.y) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
			h ^= mix(k.a.z) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
			h ^= mix(k.b.x) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
			h ^= mix(k.b.y) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
			h ^= mix(k.b.z) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
			return h;
		}
	};

	QuantizedPoint QuantizePoint(const Vector3& p) {
		return {
			static_cast<int64_t>(std::llround(p.x * 1000.0f)),
			static_cast<int64_t>(std::llround(p.y * 1000.0f)),
			static_cast<int64_t>(std::llround(p.z * 1000.0f))
		};
	}

	QuantizedEdgeKey MakeEdgeKey(const Vector3& p0, const Vector3& p1) {
		QuantizedPoint q0 = QuantizePoint(p0);
		QuantizedPoint q1 = QuantizePoint(p1);
		if (q1 < q0) {
			return { q1, q0 };
		}
		return { q0, q1 };
	}

	bool MatchPartName(const std::string& nodeName, DamagePart& outPart) {
		static const std::pair<DamagePart, const char*> kPartNameTable[] = {
			{ DamagePart::Fuse,       "Fuse" },
			{ DamagePart::Engine1,    "Engine1" },
			{ DamagePart::Propeller1, "Propeller1" },
			{ DamagePart::Wing_L,     "Wing_L" },
			{ DamagePart::Wing_R,     "Wing_R" },
			{ DamagePart::Wing1_L,    "Wing1_L" },
			{ DamagePart::Wing1_R,    "Wing1_R" },
			{ DamagePart::Wing2_L,    "Wing2_L" },
			{ DamagePart::Wing2_R,    "Wing2_R" },
			{ DamagePart::Tail,       "Tail" },
			{ DamagePart::Rudder,     "Rudder" },
			{ DamagePart::Elevator0,  "Elevator_L" },
			{ DamagePart::Elevator1,  "Elevator_R" }
		};
		for (const auto& [part, baseName] : kPartNameTable) {
			const std::string base(baseName);
			if (nodeName == base || nodeName.starts_with(base + ".")) {
				outPart = part;
				return true;
			}
		}
		return false;
	}

	void CollectTrianglesRecursive(
		const Model::Node& node,
		DamagePart currentPart,
		const Model::ModelData& modelData,
		const Matrix4x4& baseRotMatrix,
		std::unordered_map<DamagePart, std::vector<MeshTriangle>>& outPartTriangles)
	{
		DamagePart nodePart = currentPart;
		DamagePart matched{};
		if (MatchPartName(node.name, matched)) {
			nodePart = matched;
		}

		auto& tris = outPartTriangles[nodePart];
		for (const auto& mesh : node.meshes) {
			for (uint32_t i = 0; i + 2 < mesh.indexCount; i += 3) {
				const size_t i0 = static_cast<size_t>(mesh.indexOffset) + i;
				const size_t i1 = i0 + 1;
				const size_t i2 = i0 + 2;
				if (i2 >= modelData.indices.size()) {
					break;
				}
				const uint32_t idx0 = modelData.indices[i0];
				const uint32_t idx1 = modelData.indices[i1];
				const uint32_t idx2 = modelData.indices[i2];
				if (idx0 >= modelData.vertices.size() ||
				    idx1 >= modelData.vertices.size() ||
				    idx2 >= modelData.vertices.size()) {
					continue;
				}

				const auto& rv0 = modelData.vertices[idx0].position;
				const auto& rv1 = modelData.vertices[idx1].position;
				const auto& rv2 = modelData.vertices[idx2].position;

				const Vector3 v0 = TransformV3({ rv0.x, rv0.y, rv0.z }, baseRotMatrix);
				const Vector3 v1 = TransformV3({ rv1.x, rv1.y, rv1.z }, baseRotMatrix);
				const Vector3 v2 = TransformV3({ rv2.x, rv2.y, rv2.z }, baseRotMatrix);

				const Vector3 crossN = Cross(Subtract(v1, v0), Subtract(v2, v0));
				if (Dot(crossN, crossN) < 1e-10f) {
					continue;
				}

				tris.push_back({ v0, v1, v2 });
			}
		}

		for (const auto& child : node.children) {
			CollectTrianglesRecursive(child, nodePart, modelData, baseRotMatrix, outPartTriangles);
		}
	}

	void ExpandBounds(AABBCollider& bounds, const Vector3& p) {
		bounds.min.x = (std::min)(bounds.min.x, p.x);
		bounds.min.y = (std::min)(bounds.min.y, p.y);
		bounds.min.z = (std::min)(bounds.min.z, p.z);
		bounds.max.x = (std::max)(bounds.max.x, p.x);
		bounds.max.y = (std::max)(bounds.max.y, p.y);
		bounds.max.z = (std::max)(bounds.max.z, p.z);
	}

	bool IsPointInAABB(const Vector3& p, const AABBCollider& box) {
		return p.x >= box.min.x && p.x <= box.max.x &&
		       p.y >= box.min.y && p.y <= box.max.y &&
		       p.z >= box.min.z && p.z <= box.max.z;
	}
}

void AircraftMeshCollider::Clear() {
	parts_.clear();
	totalBounds_ = {};
	boundingRadius_ = 5.0f;
	totalTriangleCount_ = 0;
	totalEdgeCount_ = 0;
}

bool AircraftMeshCollider::BuildFromModel(const Model* model, const Vector3& baseRotation) {
	Clear();
	if (!model) {
		return false;
	}

	const auto& modelData = model->GetModelDataRef();
	if (modelData.vertices.empty() || modelData.indices.empty()) {
		return false;
	}

	const Matrix4x4 baseRotMatrix = MakeAffineMatrix(
		Vector3{ 1.0f, 1.0f, 1.0f },
		Vector3{ baseRotation.x, baseRotation.y, baseRotation.z },
		Vector3{ 0.0f, 0.0f, 0.0f });

	std::unordered_map<DamagePart, std::vector<MeshTriangle>> partTriMap;
	CollectTrianglesRecursive(modelData.rootNode, DamagePart::Fuse, modelData, baseRotMatrix, partTriMap);

	if (partTriMap.empty()) {
		return false;
	}

	totalBounds_.min = { 1e9f, 1e9f, 1e9f };
	totalBounds_.max = { -1e9f, -1e9f, -1e9f };
	float maxRadiusSq = 0.0f;

	for (auto& [part, tris] : partTriMap) {
		if (tris.empty()) {
			continue;
		}

		PartMeshCollider partCol;
		partCol.part = part;
		partCol.localBounds.min = { 1e9f, 1e9f, 1e9f };
		partCol.localBounds.max = { -1e9f, -1e9f, -1e9f };
		partCol.triangles = std::move(tris);

		std::unordered_set<QuantizedEdgeKey, QuantizedEdgeHash> uniqueEdges;
		uniqueEdges.reserve(partCol.triangles.size() * 2);
		partCol.wireEdges.reserve(partCol.triangles.size() * 2);

		for (const auto& tri : partCol.triangles) {
			const Vector3 pts[3] = { tri.v0, tri.v1, tri.v2 };
			for (const auto& pt : pts) {
				ExpandBounds(partCol.localBounds, pt);
				ExpandBounds(totalBounds_, pt);
				maxRadiusSq = (std::max)(maxRadiusSq, Dot(pt, pt));
			}

			const std::pair<Vector3, Vector3> edges[3] = {
				{ tri.v0, tri.v1 },
				{ tri.v1, tri.v2 },
				{ tri.v2, tri.v0 }
			};
			for (const auto& [ea, eb] : edges) {
				const QuantizedEdgeKey key = MakeEdgeKey(ea, eb);
				if (uniqueEdges.insert(key).second) {
					partCol.wireEdges.push_back({ ea, eb });
				}
			}
		}

		totalTriangleCount_ += partCol.triangles.size();
		totalEdgeCount_ += partCol.wireEdges.size();
		parts_.push_back(std::move(partCol));
	}

	if (totalTriangleCount_ == 0) {
		Clear();
		return false;
	}

	boundingRadius_ = (std::max)(2.0f, std::sqrt(maxRadiusSq));
	return true;
}

MeshHitResult AircraftMeshCollider::IntersectRayLocal(
	const Ray& localRay,
	const std::function<bool(DamagePart)>& isPartEnabled) const
{
	MeshHitResult bestResult;
	if (!IsValid()) {
		return bestResult;
	}

	if (!Collision3DManager::CheckRayAABB(localRay, totalBounds_)) {
		return bestResult;
	}

	float closestDist = localRay.maxDistance;

	for (const auto& part : parts_) {
		if (isPartEnabled && !isPartEnabled(part.part)) {
			continue;
		}

		float aabbTMin = 0.0f;
		if (!Collision3DManager::CheckRayAABB(localRay, part.localBounds, &aabbTMin)) {
			continue;
		}
		if (aabbTMin > closestDist) {
			continue;
		}

		for (uint32_t ti = 0; ti < static_cast<uint32_t>(part.triangles.size()); ++ti) {
			const auto& tri = part.triangles[ti];
			RaycastHit tempHit;
			if (Collision3DManager::RayTriangleIntersect(localRay, tri.v0, tri.v1, tri.v2, &tempHit)) {
				if (tempHit.distance < closestDist) {
					closestDist = tempHit.distance;
					bestResult.hit = true;
					bestResult.part = part.part;
					bestResult.distance = tempHit.distance;
					bestResult.localPoint = tempHit.point;
					bestResult.localNormal = tempHit.normal;
					bestResult.hitTriangle = tri;
					bestResult.triangleIndex = ti;
				}
			}
		}
	}

	return bestResult;
}

MeshHitResult AircraftMeshCollider::IntersectSegmentLocal(
	const Vector3& localStart,
	const Vector3& localEnd,
	float paddingMeters,
	const std::function<bool(DamagePart)>& isPartEnabled) const
{
	MeshHitResult miss;
	if (!IsValid()) {
		return miss;
	}

	const Vector3 delta = Subtract(localEnd, localStart);
	const float segLen = Length(delta);

	if (segLen < 1e-4f) {
		// ほぼ静止・同一座標の場合は近接レイ＋パーツAABBフォールバックで判定
		Ray fallbackRay;
		fallbackRay.origin = { localEnd.x, localEnd.y, localEnd.z - paddingMeters };
		fallbackRay.direction = { 0.0f, 0.0f, 1.0f };
		fallbackRay.maxDistance = paddingMeters * 2.0f;
		MeshHitResult rayRes = IntersectRayLocal(fallbackRay, isPartEnabled);
		if (rayRes.hit) {
			return rayRes;
		}
		for (const auto& part : parts_) {
			if (isPartEnabled && !isPartEnabled(part.part)) {
				continue;
			}
			AABBCollider padded = part.localBounds;
			padded.min = Subtract(padded.min, { paddingMeters, paddingMeters, paddingMeters });
			padded.max = Add(padded.max, { paddingMeters, paddingMeters, paddingMeters });
			if (IsPointInAABB(localEnd, padded)) {
				MeshHitResult boxHit;
				boxHit.hit = true;
				boxHit.part = part.part;
				boxHit.localPoint = localEnd;
				boxHit.localNormal = { 0.0f, 1.0f, 0.0f };
				return boxHit;
			}
		}
		return miss;
	}

	const Vector3 dir = Multiply(1.0f / segLen, delta);
	Ray segmentRay;
	segmentRay.origin = Subtract(localStart, Multiply(paddingMeters, dir));
	segmentRay.direction = dir;
	segmentRay.maxDistance = segLen + paddingMeters * 2.0f;

	return IntersectRayLocal(segmentRay, isPartEnabled);
}

std::vector<SubColliderDef> AircraftMeshCollider::GenerateSubColliders() const {
	std::vector<SubColliderDef> cols;
	if (!IsValid()) {
		return cols;
	}

	bool hasEngine = false;
	bool hasPilot = false;
	bool hasTank1 = false;
	bool hasTank2 = false;
	const PartMeshCollider* fusePart = nullptr;
	const PartMeshCollider* wingLPart = nullptr;
	const PartMeshCollider* wingRPart = nullptr;

	for (const auto& part : parts_) {
		if (part.part == DamagePart::Engine1) hasEngine = true;
		if (part.part == DamagePart::Pilot) hasPilot = true;
		if (part.part == DamagePart::Tank1) hasTank1 = true;
		if (part.part == DamagePart::Tank2) hasTank2 = true;
		if (part.part == DamagePart::Fuse) fusePart = &part;
		if (part.part == DamagePart::Wing_L) wingLPart = &part;
		if (part.part == DamagePart::Wing_R) wingRPart = &part;

		const Vector3 center = Multiply(0.5f, Add(part.localBounds.min, part.localBounds.max));
		Vector3 halfSize = Multiply(0.5f, Subtract(part.localBounds.max, part.localBounds.min));
		halfSize.x = (std::max)(halfSize.x, 0.08f);
		halfSize.y = (std::max)(halfSize.y, 0.08f);
		halfSize.z = (std::max)(halfSize.z, 0.08f);
		cols.push_back({ part.part, center, halfSize });
	}

	const AABBCollider refFuse = fusePart ? fusePart->localBounds : totalBounds_;
	const Vector3 fuseCenter = Multiply(0.5f, Add(refFuse.min, refFuse.max));
	const Vector3 fuseHalf = Multiply(0.5f, Subtract(refFuse.max, refFuse.min));

	if (!hasPilot) {
		cols.push_back({
			DamagePart::Pilot,
			{ fuseCenter.x, fuseCenter.y + fuseHalf.y * 0.35f, fuseCenter.z + fuseHalf.z * 0.25f },
			{ (std::max)(0.35f, fuseHalf.x * 0.40f), (std::max)(0.35f, fuseHalf.y * 0.40f), (std::max)(0.55f, fuseHalf.z * 0.14f) }
		});
	}
	if (!hasEngine) {
		cols.push_back({
			DamagePart::Engine1,
			{ fuseCenter.x, fuseCenter.y, fuseCenter.z - fuseHalf.z * 0.45f },
			{ (std::max)(0.50f, fuseHalf.x * 0.65f), (std::max)(0.45f, fuseHalf.y * 0.55f), (std::max)(1.20f, fuseHalf.z * 0.30f) }
		});
	}
	if (!hasTank1) {
		if (wingLPart) {
			const Vector3 wc = Multiply(0.5f, Add(wingLPart->localBounds.min, wingLPart->localBounds.max));
			const Vector3 wh = Multiply(0.5f, Subtract(wingLPart->localBounds.max, wingLPart->localBounds.min));
			cols.push_back({
				DamagePart::Tank1,
				wc,
				{ (std::max)(0.45f, wh.x * 0.65f), (std::max)(0.10f, wh.y * 0.70f), (std::max)(0.50f, wh.z * 0.55f) }
			});
		} else {
			cols.push_back({
				DamagePart::Tank1,
				{ fuseCenter.x - fuseHalf.x * 0.35f, fuseCenter.y - fuseHalf.y * 0.10f, fuseCenter.z },
				{ (std::max)(0.35f, fuseHalf.x * 0.35f), (std::max)(0.25f, fuseHalf.y * 0.35f), (std::max)(0.80f, fuseHalf.z * 0.25f) }
			});
		}
	}
	if (!hasTank2) {
		if (wingRPart) {
			const Vector3 wc = Multiply(0.5f, Add(wingRPart->localBounds.min, wingRPart->localBounds.max));
			const Vector3 wh = Multiply(0.5f, Subtract(wingRPart->localBounds.max, wingRPart->localBounds.min));
			cols.push_back({
				DamagePart::Tank2,
				wc,
				{ (std::max)(0.45f, wh.x * 0.65f), (std::max)(0.10f, wh.y * 0.70f), (std::max)(0.50f, wh.z * 0.55f) }
			});
		} else {
			cols.push_back({
				DamagePart::Tank2,
				{ fuseCenter.x + fuseHalf.x * 0.35f, fuseCenter.y - fuseHalf.y * 0.10f, fuseCenter.z },
				{ (std::max)(0.35f, fuseHalf.x * 0.35f), (std::max)(0.25f, fuseHalf.y * 0.35f), (std::max)(0.80f, fuseHalf.z * 0.25f) }
			});
		}
	}

	return cols;
}
