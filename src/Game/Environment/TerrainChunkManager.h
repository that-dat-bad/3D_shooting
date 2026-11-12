#pragma once
#include <vector>
#include <string>
#include <memory>
#include <d3d12.h>
#include <wrl.h>
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/base/Math/MyMath.h"

class Object3dCommon;
class Camera;

// 地形の描画モード
enum class TerrainMode {
	SingleMesh, // 1枚メッシュ版
	Chunked     // チャンク分割版
};

// LODメッシュ
struct TerrainLodMesh {
	uint32_t lodLevel = 0;       // LODレベル
	float switchDistance = 0.0f; // 切り替え距離
	std::string fullPath;        // パス
	std::unique_ptr<Object3d> object3d;
};

namespace TerrainHeightDefaults {
	inline constexpr int kGridResolution = 51;
	inline constexpr int kOriginCellIndex = 0;
	inline constexpr int kNextCellStep = 1;
	inline constexpr int kMaxCellIndex = kGridResolution - kNextCellStep;
	inline constexpr size_t kTotalGridSamples = static_cast<size_t>(kGridResolution * kGridResolution);
	inline constexpr float kZero = 0.0f;
	inline constexpr float kHalf = 0.5f;
	inline constexpr float kUnit = 1.0f;
	inline constexpr float kMinSpanEpsilon = 0.001f;
	inline constexpr float kUnsetHeightSentinel = -1.0e8f;
	inline constexpr float kDefaultFallbackHeight = 0.0f;
	inline constexpr int kNeighborFillPasses = 2;
	inline constexpr int kMinNeighborOffset = -1;
	inline constexpr int kMaxNeighborOffset = 1;
	inline constexpr int kEmptyNeighborCount = 0;
}

// 地形チャンクデータ
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

	// ハイトマップサンプル (51x51)
	std::vector<float> heightSamples;

	// LODメッシュ配列
	std::vector<TerrainLodMesh> lodMeshes;
	int currentLodIndex = 0;

	std::unique_ptr<Object3d> object3d;

	bool isLoaded = false;
	bool isVisible = false;
	float distanceToViewer = 0.0f;
};

// 地形管理クラス
class TerrainChunkManager {
public:
	static TerrainChunkManager* GetInstance();

	// 初期化
	// object3dCommon: 描画共通基盤
	// camera: 描画カメラ
	// terrainPath: 設定ファイルパス
	void Initialize(Object3dCommon* object3dCommon, Camera* camera, const std::string& terrainPath = "Resources/terrains/demo/chunks_info.json");

	// モード切り替え
	void SetTerrainMode(TerrainMode mode);
	TerrainMode GetTerrainMode() const { return mode_; }

	// 更新
	// viewerPos: 視点座標
	void Update(const Vector3& viewerPos);

	// 描画
	void Draw();

	// 終了処理
	void Finalize();

	bool IsLoaded() const { return isLoaded_; }

	// 再読み込み
	void Reload();

	// 表示設定
	void SetVisible(bool visible) { isVisible_ = visible; }
	bool IsVisible() const { return isVisible_; }

	// 統計情報
	int GetTotalChunkCount() const { return (mode_ == TerrainMode::SingleMesh) ? 1 : static_cast<int>(chunks_.size()); }
	int GetLoadedChunkCount() const;
	int GetRenderedChunkCount() const { return renderedChunkCount_; }

	// 描画距離設定
	void SetDrawDistance(float distance) { drawDistance_ = distance; }
	float GetDrawDistance() const { return drawDistance_; }

	// 地形標高取得
	// worldX: X座標
	// worldZ: Z座標
	float GetTerrainHeightAt(
		float worldX,
		float worldZ,
		float fallbackY = TerrainHeightDefaults::kDefaultFallbackHeight) const;

	// デバッグ用UI
	void DrawImGui();

private:
	TerrainChunkManager() = default;
	~TerrainChunkManager() = default;
	TerrainChunkManager(const TerrainChunkManager&) = delete;
	TerrainChunkManager& operator=(const TerrainChunkManager&) = delete;

	void LoadSingleMesh(const std::string& objPath);
	void LoadChunked(const std::string& jsonPath);
	bool LoadPackage(const std::string& packagePath);
	bool SavePackage(const std::string& packagePath);
	static void BuildChunkHeightGrid(TerrainChunk& chunk, const Model* model);

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

	void CreateTerrainPipeline();
	Microsoft::WRL::ComPtr<ID3D12PipelineState> terrainPipelineState_;
};
