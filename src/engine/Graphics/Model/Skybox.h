#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <cstdint>
#include "../base/Math/MyMath.h"

using namespace MyMath;

class SkyboxCommon;
class Camera;
class SrvManager;

class Skybox
{
public:
	struct SkyboxVertex {
		Vector4 position;
	};

	// マテリアル (PS b0に対応)
	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float shininess;
		Vector2 padding;
		Matrix4x4 uvTransform;
	};

	// 変換行列 (VS b1に対応)
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};

	void Initialize(SkyboxCommon* skyboxCommon);
	void Update();
	void Draw();

	// セッター
	void SetTextureIndex(uint32_t textureIndex) { textureIndex_ = textureIndex; }
	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetColor(const Vector4& color);

private:
	// 箱の頂点・インデックスを作成
	void CreateBox();

	SkyboxCommon* skyboxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Camera* camera_ = nullptr;

	// 頂点・インデックスバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

	// マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;

	// 変換行列
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;

	// テクスチャインデックス (cubemap)
	uint32_t textureIndex_ = 0;

	// トランスフォーム
	Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	static const uint32_t kVertexCount = 24;  // 6面 × 4頂点
	static const uint32_t kIndexCount = 36;   // 6面 × 2三角形 × 3頂点
};
