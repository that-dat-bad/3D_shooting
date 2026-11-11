#define _USE_MATH_DEFINES
#include <cmath>
#include "PrimitiveGenerator.h"

std::vector<PrimitiveModel::VertexData> PrimitiveGenerator::CreateRing(int segments, float innerR, float outerR) {
	std::vector<PrimitiveModel::VertexData> vertices;
	float angleStep = (2.0f * float(M_PI)) / segments;

	for (int i = 0; i < segments; ++i) {
		float a1 = i * angleStep;
		float a2 = (i + 1) * angleStep;

		Vector4 in1 = { cosf(a1) * innerR, 0.0f, sinf(a1) * innerR, 1.0f };
		Vector4 out1 = { cosf(a1) * outerR, 0.0f, sinf(a1) * outerR, 1.0f };
		Vector4 in2 = { cosf(a2) * innerR, 0.0f, sinf(a2) * innerR, 1.0f };
		Vector4 out2 = { cosf(a2) * outerR, 0.0f, sinf(a2) * outerR, 1.0f };

		Vector3 n = { 0.0f, 1.0f, 0.0f };

		// 三角形1
		vertices.push_back({ in1, {0.0f, 0.0f}, n });
		vertices.push_back({ out1, {1.0f, 0.0f}, n });
		vertices.push_back({ in2, {0.0f, 1.0f}, n });
		// 三角形2
		vertices.push_back({ in2, {0.0f, 1.0f}, n });
		vertices.push_back({ out1, {1.0f, 0.0f}, n });
		vertices.push_back({ out2, {1.0f, 1.0f}, n });
	}

	return vertices;
}

std::vector<PrimitiveModel::VertexData> PrimitiveGenerator::CreateCylinder(int segments, float height, float radius) {
	std::vector<PrimitiveModel::VertexData> vertices;
	float angleStep = (2.0f * float(M_PI)) / segments;
	float h = height; // 上下幅
	float r = radius;

	for (int i = 0; i < segments; ++i) {
		float a1 = i * angleStep;
		float a2 = (i + 1) * angleStep;

		Vector4 t1 = { cosf(a1) * r, h, sinf(a1) * r, 1.0f };
		Vector4 b1 = { cosf(a1) * r, -h, sinf(a1) * r, 1.0f };
		Vector4 t2 = { cosf(a2) * r, h, sinf(a2) * r, 1.0f };
		Vector4 b2 = { cosf(a2) * r, -h, sinf(a2) * r, 1.0f };

		Vector3 n1 = { cosf(a1), 0.0f, sinf(a1) };
		Vector3 n2 = { cosf(a2), 0.0f, sinf(a2) };

		// 三角形1
		vertices.push_back({ t1, {0.0f, 0.0f}, n1 });
		vertices.push_back({ t2, {1.0f, 0.0f}, n2 });
		vertices.push_back({ b1, {0.0f, 1.0f}, n1 });
		// 三角形2
		vertices.push_back({ b1, {0.0f, 1.0f}, n1 });
		vertices.push_back({ t2, {1.0f, 0.0f}, n2 });
		vertices.push_back({ b2, {1.0f, 1.0f}, n2 });
	}

	return vertices;
}

std::vector<PrimitiveModel::VertexData> PrimitiveGenerator::CreatePlane(float size) {
	std::vector<PrimitiveModel::VertexData> vertices;
	float half = size * 0.5f;

	Vector4 p1 = { -half, 0.0f, -half, 1.0f };
	Vector4 p2 = { -half, 0.0f,  half, 1.0f };
	Vector4 p3 = {  half, 0.0f, -half, 1.0f };
	Vector4 p4 = {  half, 0.0f,  half, 1.0f };

	Vector3 n = { 0.0f, 1.0f, 0.0f };

	vertices.push_back({ p1, {0.0f, 1.0f}, n });
	vertices.push_back({ p2, {0.0f, 0.0f}, n });
	vertices.push_back({ p3, {1.0f, 1.0f}, n });

	vertices.push_back({ p3, {1.0f, 1.0f}, n });
	vertices.push_back({ p2, {0.0f, 0.0f}, n });
	vertices.push_back({ p4, {1.0f, 0.0f}, n });

	return vertices;
}

std::vector<PrimitiveModel::VertexData> PrimitiveGenerator::CreateCone(int segments, float height, float radius) {
	std::vector<PrimitiveModel::VertexData> vertices;
	float angleStep = (2.0f * float(M_PI)) / segments;
	float h = height; // 原点から先端（+h）、底面（-h）
	float r = radius;

	for (int i = 0; i < segments; ++i) {
		float a1 = i * angleStep;
		float a2 = (i + 1) * angleStep;

		// 頂点（コーンの先端。半径0）
		MyMath::Vector4 top = { 0.0f, h, 0.0f, 1.0f };
		// 底面点1
		MyMath::Vector4 b1 = { cosf(a1) * r, -h, sinf(a1) * r, 1.0f };
		// 底面点2
		MyMath::Vector4 b2 = { cosf(a2) * r, -h, sinf(a2) * r, 1.0f };

		// 法線（側面の法線。円周上の方向から少し上向きに）
		float midAngle = (a1 + a2) * 0.5f;
		MyMath::Vector3 n = { cosf(midAngle), 0.5f, sinf(midAngle) };
		n = MyMath::Normalize(n);

		Vector2 uvTop = { 0.5f, 0.5f };
		Vector2 uvB1 = { 0.5f + 0.5f * cosf(a1), 0.5f + 0.5f * sinf(a1) };
		Vector2 uvB2 = { 0.5f + 0.5f * cosf(a2), 0.5f + 0.5f * sinf(a2) };

		// 三角形1（側面）※カリング対応のため順序を設定
		vertices.push_back({ top, uvTop, n });
		vertices.push_back({ b2, uvB2, n });
		vertices.push_back({ b1, uvB1, n });
	}

	return vertices;
}
