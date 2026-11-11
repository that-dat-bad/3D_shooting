#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <array>
#include "../base/Math/MyMath.h"
#include "../System/BlendMode.h"
#include "../Camera/Camera.h"
#include "../System/DirectXCommon.h"

using namespace MyMath;

/// <summary>
/// プリミティブ形状 (リング、シリンダー、コーン、平面) 描画管理クラス
/// </summary>
class PrimitiveModel {
public:
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static PrimitiveModel* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	PrimitiveModel() = default;

	/// <summary>初期化処理</summary>
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>終了処理</summary>
	void Finalize();
	
	/// <summary>描画カウントをリセットする（毎フレームの最初に呼ぶ、またはUpdateなどで呼ぶ）</summary>
	void Reset();

	/// <summary>エフェクト描画関数 (Ring)</summary>
	void DrawRing(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode = BlendMode::kNormal);
	void DrawRing(const Matrix4x4& worldMatrix, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode = BlendMode::kNormal);

	/// <summary>エフェクト描画関数 (Cylinder)</summary>
	void DrawCylinder(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode = BlendMode::kNormal);
	void DrawCylinder(const Matrix4x4& worldMatrix, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode = BlendMode::kNormal);

	/// <summary>エフェクト描画関数 (Plane)</summary>
	void DrawPlane(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode = BlendMode::kNormal);
	void DrawPlane(const Matrix4x4& worldMatrix, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode = BlendMode::kNormal);

	/// <summary>エフェクト描画関数 (Cone)</summary>
	void DrawCone(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode = BlendMode::kNormal);
	void DrawCone(const Matrix4x4& worldMatrix, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode = BlendMode::kNormal);

	/// <summary>エフェクト描画関数 (Cone - クォータニオン版)</summary>
	void DrawCone(const Vector3& scale, const Quaternion& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode = BlendMode::kNormal);

	// 3D直線描画関数 (デバッグ用などに使用)
	void DrawLine3D(const Vector3& p1, const Vector3& p2, const Vector4& color, Camera* camera);

	// 頂点データ構造体（無駄なウェイトやインデックスを省いた軽量版）
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	~PrimitiveModel() = default;

private:
	PrimitiveModel(const PrimitiveModel&) = delete;
	PrimitiveModel& operator=(const PrimitiveModel&) = delete;

	static std::unique_ptr<PrimitiveModel> instance_;

	DirectXCommon* dxCommon_ = nullptr;

	// パイプライン関連
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, static_cast<size_t>(BlendMode::kCountOf)> pipelineStates_;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, static_cast<size_t>(BlendMode::kCountOf)> pipelineStatesLine_;

	// リングのバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> ringVertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW ringVertexBufferView_{};
	uint32_t ringVertexCount_ = 0;

	// シリンダーのバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> cylinderVertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW cylinderVertexBufferView_{};
	uint32_t cylinderVertexCount_ = 0;

	// プレーンのバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> planeVertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW planeVertexBufferView_{};
	uint32_t planeVertexCount_ = 0;

	// コーンのバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> coneVertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW coneVertexBufferView_{};
	uint32_t coneVertexCount_ = 0;

	// ラインのバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> lineVertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW lineVertexBufferView_{};
	uint32_t lineVertexCount_ = 0;

	// トランスフォームとマテリアルを定数バッファとしてGPUに送る構造体
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};
	struct MaterialData {
		Vector4 color;
		int32_t enableLighting;
		float shininess;
		float padding[2];
		Matrix4x4 uvTransform;
	};

	// 定数バッファのアライメント制約（256バイトの倍数）に合わせたサイズ
	static const uint32_t kCbAlignment = 256;
	static const uint32_t kMaxDrawCount = 10000; // 1フレームあたりの最大描画数

	Microsoft::WRL::ComPtr<ID3D12Resource> transformBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialBuffer_;

	uint8_t* transformMappedData_ = nullptr;
	uint8_t* materialMappedData_ = nullptr;

	uint32_t currentDrawCount_ = 0;

private:
	void CreateRootSignature();
	void CreateGraphicsPipeline();
	void CreatePrimitiveBuffers();

	// 描画実行の共通部分
	void CallDrawCommand(D3D12_VERTEX_BUFFER_VIEW& vbView, uint32_t vertexCount, const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode);
	void CallDrawCommand(D3D12_VERTEX_BUFFER_VIEW& vbView, uint32_t vertexCount, const Vector3& scale, const Quaternion& rotate, const Vector3& translate, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode);
	void CallDrawCommand(D3D12_VERTEX_BUFFER_VIEW& vbView, uint32_t vertexCount, const Matrix4x4& wMatrix, const Vector4& color, uint32_t textureIndex, Camera* camera, BlendMode blendMode);
};
