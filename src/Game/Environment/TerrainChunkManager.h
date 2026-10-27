#pragma once
#include <vector>
#include <string>
#include <memory>
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/base/Math/MyMath.h"

class Object3dCommon;
class Camera;

/// @brief 地形の描画モード（1枚巨大メッシュ vs チャンク分割カリング）
enum class TerrainMode {
	SingleMesh, // 1枚メッシュ版 (Resources/terrains/demo.obj)
	Chunked     // 4000mチャンク分割版 (Resources/terrains/demo/chunks_info.json)
};

/// @brief 個別チャンクのメタデータと3Dオブジェクト
struct TerrainChunk {
	int gridX = 0;
	int gridY = 0;
	int worldCx = 0;
	int worldCy = 0;
	float minX = 0.0f;
	float maxX = 0.0f;
	float minZ = 0.0f;
	float maxZ = 0.0f;
	Vector3 center = { 0.0f, 0.0f, 0.0f };
	std::string fileName;
	std::string fullPath;
	std::unique_ptr<Object3d> object3d;
	bool isLoaded = false;
	bool isVisible = false;
	float distanceToViewer = 0.0f;
};

/// @brief 広域地形(40km x 40km)のロード・管理・描画マネージャー（1枚メッシュ / チャンク分割 両対応）
class TerrainChunkManager {
public:
	static TerrainChunkManager* GetInstance();

	/// @brief 地形の初期化
	/// @param object3dCommon 3Dオブジェクト描画共通基盤
	/// @param camera 描画用カメラ
	/// @param terrainPath demo.obj または chunks_info.json へのパス
	void Initialize(Object3dCommon* object3dCommon, Camera* camera, const std::string& terrainPath = "Resources/terrains/demo/chunks_info.json");

	/// @brief モード切り替え（SingleMesh または Chunked）
	void SetTerrainMode(TerrainMode mode);
	TerrainMode GetTerrainMode() const { return mode_; }

	/// @brief 毎フレームの更新（距離カリング、カメラ追従）
	/// @param viewerPos 視点座標（自機またはカメラ位置）
	void Update(const Vector3& viewerPos);

	/// @brief 地形を描画
	void Draw();

	/// @brief 終了処理
	void Finalize();

	/// @brief 地形がロード完了しているか
	bool IsLoaded() const { return isLoaded_; }

	/// @brief チャンク/メッシュの再読み込み（ホットリロード）
	void Reload();

	/// @brief 表示・非表示切り替え
	void SetVisible(bool visible) { isVisible_ = visible; }
	bool IsVisible() const { return isVisible_; }

	/// @brief チャンク統計
	int GetTotalChunkCount() const { return (mode_ == TerrainMode::SingleMesh) ? 1 : static_cast<int>(chunks_.size()); }
	int GetLoadedChunkCount() const;
	int GetRenderedChunkCount() const { return renderedChunkCount_; }

	/// @brief カリング描画距離の設定（メートル単位）
	void SetDrawDistance(float distance) { drawDistance_ = distance; }
	float GetDrawDistance() const { return drawDistance_; }

	/// @brief ImGuiによる地形監視・モード切り替えUI
	void DrawImGui();

private:
	TerrainChunkManager() = default;
	~TerrainChunkManager() = default;
	TerrainChunkManager(const TerrainChunkManager&) = delete;
	TerrainChunkManager& operator=(const TerrainChunkManager&) = delete;

	void LoadSingleMesh(const std::string& objPath);
	void LoadChunked(const std::string& jsonPath);

	TerrainMode mode_ = TerrainMode::Chunked;
	std::string singleMeshPath_ = "Resources/terrains/demo.obj";
	std::string chunkJsonPath_ = "Resources/terrains/demo/chunks_info.json";

	std::unique_ptr<Object3d> singleMeshObject_;
	std::vector<TerrainChunk> chunks_;

	Object3dCommon* object3dCommon_ = nullptr;
	Camera* camera_ = nullptr;

	bool isLoaded_ = false;
	bool isVisible_ = true;
	float drawDistance_ = 10000.0f; // デフォルト10kmカリング距離
	int renderedChunkCount_ = 0;
	double lastLoadTimeMs_ = 0.0;
};
