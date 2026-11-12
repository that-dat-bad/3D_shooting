#pragma once
#include <vector>
#include "PrimitiveModel.h"

class PrimitiveGenerator {
public:
	// リング（穴あきの円）の頂点データを生成
	static std::vector<PrimitiveModel::VertexData> CreateRing(int segments, float innerR, float outerR);

	// 円柱（シリンダー）の頂点データを生成
	static std::vector<PrimitiveModel::VertexData> CreateCylinder(int segments, float height, float radius);

	// プレーン（四角形）の頂点データを生成
	static std::vector<PrimitiveModel::VertexData> CreatePlane(float size);

	// 円錐（コーン）の頂点データを生成
	static std::vector<PrimitiveModel::VertexData> CreateCone(int segments, float height, float radius);
};
