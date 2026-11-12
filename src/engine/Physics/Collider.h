#pragma once
#include <string>
#include <cstdint>
#include "../base/Math/MyMath.h"

// ============================================================
// 3D コライダー
// ============================================================

/// <summary>
/// コライダーの種類（3D）
/// </summary>
enum class ColliderType3D {
	Sphere,
	AABB,
};

/// <summary>
/// 球コライダー
/// </summary>
struct SphereCollider {
	MyMath::Vector3 center;
	float radius = 1.0f;
};

/// <summary>
/// AABBコライダー
/// </summary>
struct AABBCollider {
	MyMath::Vector3 min;
	MyMath::Vector3 max;
};

/// <summary>
/// レイ（光線）構造体
/// </summary>
struct Ray {
	MyMath::Vector3 origin = { 0.0f, 0.0f, 0.0f };
	MyMath::Vector3 direction = { 0.0f, 0.0f, 1.0f }; // 正規化推奨
	float maxDistance = 10000.0f;
};

/// <summary>
/// レイキャストヒット結果情報
/// </summary>
struct RaycastHit {
	bool hit = false;
	float distance = 0.0f;
	MyMath::Vector3 point = { 0.0f, 0.0f, 0.0f };
	MyMath::Vector3 normal = { 0.0f, 1.0f, 0.0f };
	std::string nodeName;
	uint32_t triangleIndex = 0;
};

// ============================================================
// 2D コライダー
// ============================================================

/// <summary>
/// コライダーの種類（2D）
/// </summary>
enum class ColliderType2D {
	Circle,
	Rect,
};

/// <summary>
/// 円コライダー（2D）
/// </summary>
struct CircleCollider {
	Vector2 center;
	float radius = 1.0f;
};

/// <summary>
/// 矩形コライダー（2D / AABB）
/// </summary>
struct RectCollider {
	Vector2 min;
	Vector2 max;
};
