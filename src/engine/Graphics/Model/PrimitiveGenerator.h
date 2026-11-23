#pragma once
#include <vector>
#include "PrimitiveModel.h"

class PrimitiveGenerator {
public:
	// リングの頂点データを生成
	static std::vector<PrimitiveModel::VertexData> CreateRing(int segments, float innerR, float outerR);

	// 円柱の頂点データを生成
	static std::vector<PrimitiveModel::VertexData> CreateCylinder(int segments, float height, float radius);

	// プレーンの頂点データを生成
	static std::vector<PrimitiveModel::VertexData> CreatePlane(float size);

	// 円錐の頂点データを生成
	static std::vector<PrimitiveModel::VertexData> CreateCone(int segments, float height, float radius);
};
