#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <map>
#include "../base/Math/MyMath.h"
#include "../../Physics/Collider.h"

using namespace MyMath;
class ModelCommon;
class Camera;

/// <summary>
/// 3Dモデルデータを保持し描画を行うクラス
/// (アセットファイルからのデータ読み込み、アニメーション対応)
/// </summary>
class Model {
public:
	/// <summary>
	/// モデルの初期化
	/// </summary>
	/// <param name="modelCommon">ModelCommonポインタ</param>
	/// <param name="directorypath">ディレクトリパス</param>
	/// <param name="filename">ファイル名</param>
	void Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename);
	
	/// <summary>
	/// モデルの描画コマンド発行
	/// </summary>
	void Draw();

	/// <summary>
	/// 特定のノード（とその子孫）のみを描画するコマンド発行
	/// </summary>
	void DrawNode(const std::string& nodeName);

	/// <summary>
	/// デバッグ用: スケルトンの描画
	/// </summary>
	/// <param name="objectWorldMatrix">オブジェクトのワールド行列</param>
	/// <param name="camera">カメラ</param>
	/// <param name="color">描画色</param>
	void DebugDrawSkeleton(const Matrix4x4& objectWorldMatrix, Camera* camera, const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

	void SetShininess(float shininess) { for (auto* mat : materialDatas_) { if (mat) mat->shininess = shininess; } }
	float GetShininess() const { return !materialDatas_.empty() && materialDatas_[0] ? materialDatas_[0]->shininess : 0.0f; }

	void SetEnvironmentCoefficient(float coefficient) { for (auto* mat : materialDatas_) { if (mat) mat->environmentCoefficient = coefficient; } }
	float GetEnvironmentCoefficient() const { return !materialDatas_.empty() && materialDatas_[0] ? materialDatas_[0]->environmentCoefficient : 0.0f; }

	void SetSpecularIntensity(float intensity) { for (auto* mat : materialDatas_) { if (mat) mat->specularIntensity = intensity; } }
	float GetSpecularIntensity() const { return !materialDatas_.empty() && materialDatas_[0] ? materialDatas_[0]->specularIntensity : 0.0f; }

	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
		float padding[3] = { 0.0f, 0.0f, 0.0f };
		Vector4 weight;
		int32_t indices[4];
	};
	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float shininess;
		float environmentCoefficient;
		float specularIntensity; // 反射強度
		Vector3 emissiveColor = { 0.0f, 0.0f, 0.0f }; // 自発光色
		float emissiveIntensity = 0.0f;               // 自発光強度
		Matrix4x4 uvTransform; // UV変換行列
	};

	struct BoneMatrix {
		Matrix4x4 matrices[100];
	};
	struct SkinningParams {
		uint32_t numVertices;
		float padding[3] = { 0.0f, 0.0f, 0.0f };
	};
	struct MaterialData {
		std::string textureFilePath;
		uint32_t textureIndex = 0;
		Vector4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f }; //デフォルトは白
		float shininess = 50.0f;
		float environmentCoefficient = 0.0f; //環境マップの反射度合い
		float specularIntensity = 1.0f;      // 反射強度
		Vector3 emissiveColor = { 0.0f, 0.0f, 0.0f }; // 自発光色
		float emissiveIntensity = 0.0f;               // 自発光強度
	};
	struct MeshInfo {
		uint32_t indexOffset;
		uint32_t indexCount;
		uint32_t materialIndex = 0;
	};
	struct Node {
		Matrix4x4 localMatrix = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
		std::string name;
		std::vector<Node> children;
		std::vector<MeshInfo> meshes;
	};
	struct ModelData {
		std::vector<VertexData> vertices;
		std::vector<uint32_t> indices;
		std::vector<MaterialData> materials;
		Node rootNode;
		struct BoneData {
			std::string name;
			Matrix4x4 inverseBindPoseMatrix;//初期姿勢の逆行列
		};
		std::vector<BoneData> bones;//ボーンの配列
		std::map < std::string, uint32_t> boneIndexMap;//ボーン名からインデックスへのマップ
	};

	template <typename tValue>
	struct Keyframe {
		float time;
		tValue value;
	};
	using KeyframeVector3 = Keyframe<Vector3>;
	using KeyframeQuaternion = Keyframe<Quaternion>;

	struct NodeAnimation {
		std::vector<KeyframeVector3> translate;
		std::vector<KeyframeQuaternion> rotate;
		std::vector<KeyframeVector3> scale;
	};

	struct Animation {
		float duration;
		std::map<std::string, NodeAnimation> nodeAnimations;
	};

	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);
	static Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename, const std::string& animationName);
	static std::vector<std::string> LoadAnimationNames(const std::string& directoryPath, const std::string& filename, const std::string& animationName = "");
	ModelData GetModelData() const { return modelData_; }

	// アニメーションを再生スタートする関数
	void PlayAnimation(Animation* animation);

	// 毎フレーム呼んでアニメーションを進める関数
	void Update(float deltaTime);

	/// <summary>
	/// モデル全体のメッシュに対するレイキャスト判定
	/// </summary>
	bool IntersectRay(const Ray& ray, const Matrix4x4& worldMatrix, RaycastHit* outHit = nullptr) const;

	/// <summary>
	/// 特定ノードのメッシュに対するレイキャスト判定
	/// </summary>
	bool IntersectRayNode(const std::string& nodeName, const Ray& ray, const Matrix4x4& nodeWorldMatrix, RaycastHit* outHit = nullptr) const;

	/// @brief マテリアルのカラーを設定
	void SetMaterialColor(uint32_t materialIndex, const Vector4& color);
	/// @brief マテリアルのライティング有効/無効を設定
	void SetMaterialEnableLighting(uint32_t materialIndex, bool enableLighting);
	/// @brief マテリアルの反射強度を設定
	void SetMaterialSpecularIntensity(uint32_t materialIndex, float intensity);
	/// @brief マテリアルの自発光（エミッシブ）を設定
	void SetMaterialEmissive(uint32_t materialIndex, const Vector3& color, float intensity);

private:

	ModelCommon* modelCommon_ = nullptr;

	// モデルデータ
	ModelData modelData_;

	//--頂点データ--//
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_ = nullptr;
	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData_ = nullptr;
	// バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	// CSスキニング用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> skinnedVertexBuffer_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW skinnedVertexBufferView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> skinningParamsResource_ = nullptr;

	uint32_t rawVertexSrvIndex_ = 0;
	uint32_t skinnedVertexUavIndex_ = 0;

	//--インデックスデータ--//
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_ = nullptr;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

	//--マテリアル--//
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> materialResources_;
	std::vector<Material*> materialDatas_;


	Microsoft::WRL::ComPtr<ID3D12Resource> boneResource_;
	BoneMatrix* boneData_ = nullptr;

	// 現在再生中のアニメーションデータ
	Animation* playingAnimation_ = nullptr;
	// 現在の再生時間（秒）
	float animationTime_ = 0.0f;
	void UpdateNodeAnimation(const Node& node, const Matrix4x4& parentMatrix);
	void DebugDrawNodeSkeleton(const Node& node, const Matrix4x4& parentMatrix, const Matrix4x4& objectWorldMatrix, Camera* camera, const Vector4& color);
	void DrawNodeRecursive(const Node& node, const std::string& targetName, bool isTargetFound);
};