#include "TerrainChunkManager.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/io/json.hpp"
#include "../System/BenchmarkConfig.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cmath>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

using json = nlohmann::json;

TerrainChunkManager* TerrainChunkManager::GetInstance() {
	static TerrainChunkManager instance;
	return &instance;
}

void TerrainChunkManager::Initialize(Object3dCommon* object3dCommon, Camera* camera, const std::string& terrainPath) {
	object3dCommon_ = object3dCommon;
	camera_ = camera;
	isLoaded_ = false;

	if (terrainPath.ends_with(".obj")) {
		mode_ = TerrainMode::SingleMesh;
		singleMeshPath_ = terrainPath;
	} else if (terrainPath.ends_with(".json")) {
		mode_ = TerrainMode::Chunked;
		chunkJsonPath_ = terrainPath;
	}

	if (mode_ == TerrainMode::SingleMesh) {
		LoadSingleMesh(singleMeshPath_);
	} else {
		LoadChunked(chunkJsonPath_);
	}
}

void TerrainChunkManager::SetTerrainMode(TerrainMode mode) {
	if (mode_ == mode && isLoaded_) return;
	mode_ = mode;
	Reload();
}

void TerrainChunkManager::Reload() {
	if (!object3dCommon_ || !camera_) return;
	Finalize();
	if (mode_ == TerrainMode::SingleMesh) {
		LoadSingleMesh(singleMeshPath_);
	} else {
		LoadChunked(chunkJsonPath_);
	}
}

void TerrainChunkManager::LoadSingleMesh(const std::string& objPath) {
	chunks_.clear();
	singleMeshObject_.reset();
	isLoaded_ = false;

	if (!std::filesystem::exists(objPath)) {
		char errBuf[256];
		sprintf_s(errBuf, "[TerrainChunkManager] Error: 1-Mesh file not found: %s", objPath.c_str());
		Benchmark::Log(errBuf);
		return;
	}

	char startBuf[256];
	sprintf_s(startBuf, "[TerrainChunkManager] === Loading 1-Mesh Monolithic Terrain: %s ===", objPath.c_str());
	Benchmark::Log(startBuf);

	auto startTime = std::chrono::high_resolution_clock::now();

	// 1枚メッシュのAssimp読み込み＆GPU頂点/インデックスバッファ構築
	ModelManager::GetInstance()->LoadModel(objPath);

	Model* model = ModelManager::GetInstance()->FindModel(objPath);
	if (model) {
		singleMeshObject_ = std::make_unique<Object3d>();
		singleMeshObject_->Initialize(object3dCommon_);
		singleMeshObject_->SetCamera(camera_);
		singleMeshObject_->SetModel(objPath);
		singleMeshObject_->SetScale({ 1.0f, 1.0f, 1.0f });
		singleMeshObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
		singleMeshObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
		singleMeshObject_->Update();
	}

	auto endTime = std::chrono::high_resolution_clock::now();
	lastLoadTimeMs_ = std::chrono::duration<double, std::milli>(endTime - startTime).count();

	char logBuf[256];
	sprintf_s(logBuf, "[TerrainChunkManager] 1-Mesh Monolithic Terrain loaded: %.2f ms (Main-Thread Hitch Spike: %.2f ms)",
		lastLoadTimeMs_, lastLoadTimeMs_);
	Benchmark::Log(logBuf);

	isLoaded_ = (singleMeshObject_ != nullptr);
}

// ============================================================================
// 統合地形パッケージ（.terrain）フォーマット定義（LOD拡張性対応）
// 全100チャンク＋各チャンクの複数LODメッシュを1ファイルに集約
// ============================================================================
#pragma pack(push, 1)
struct TerrainPackageHeader {
	char magic[4] = { 'T', 'R', 'N', 'P' }; // "TRNP" (Terrain Package)
	uint32_t version = 1;                   // フォーマットバージョン
	uint32_t chunkCount = 0;                // チャンク総数 (例: 100)
	uint32_t maxLODCount = 1;               // サポート最大LOD段階数 (現在は1: LOD0のみ、将来複数)
	float worldSizeX = 40000.0f;            // 全体ワールドXサイズ
	float worldSizeZ = 40000.0f;            // 全体ワールドZサイズ
	float chunkSize = 4000.0f;              // 1チャンクの一辺のサイズ
	uint8_t reserved[32] = { 0 };           // 将来拡張用パディング（バイオーム・衝突・ハイトマップなど）
};

struct ChunkMetaHeader {
	int32_t gridX = 0;
	int32_t gridY = 0;
	int32_t worldCx = 0;
	int32_t worldCy = 0;
	float minX = 0.0f;
	float maxX = 0.0f;
	float minZ = 0.0f;
	float maxZ = 0.0f;
	float centerX = 0.0f;
	float centerY = 0.0f;
	float centerZ = 0.0f;
	uint32_t lodCount = 1;                  // 格納されているLODメッシュ数
	uint8_t reserved[32] = { 0 };           // 将来拡張用パディング（標高Min/Max、バイオームIDなど）
};

struct LodMetaHeader {
	uint32_t lodLevel = 0;                  // 0 = LOD0(最高精細), 1 = LOD1, ...
	float switchDistance = 0.0f;            // LOD切り替え推奨距離（将来用、0なら自動）
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	uint32_t materialCount = 0;
	uint8_t reserved[16] = { 0 };
};
#pragma pack(pop)

bool TerrainChunkManager::SavePackage(const std::string& packagePath) {
	std::ofstream ofs(packagePath, std::ios::binary);
	if (!ofs) return false;

	TerrainPackageHeader pHeader;
	pHeader.chunkCount = static_cast<uint32_t>(chunks_.size());
	pHeader.maxLODCount = 1; // 現在はLOD0のみ、将来LOD1〜LOD3に対応
	ofs.write(reinterpret_cast<const char*>(&pHeader), sizeof(pHeader));

	for (const auto& chunk : chunks_) {
		ChunkMetaHeader cHeader;
		cHeader.gridX = chunk.gridX;
		cHeader.gridY = chunk.gridY;
		cHeader.worldCx = chunk.worldCx;
		cHeader.worldCy = chunk.worldCy;
		cHeader.minX = chunk.minX;
		cHeader.maxX = chunk.maxX;
		cHeader.minZ = chunk.minZ;
		cHeader.maxZ = chunk.maxZ;
		cHeader.centerX = chunk.center.x;
		cHeader.centerY = chunk.center.y;
		cHeader.centerZ = chunk.center.z;
		cHeader.lodCount = 1; // 現在はLOD0のみ
		ofs.write(reinterpret_cast<const char*>(&cHeader), sizeof(cHeader));

		// 識別子
		uint32_t fnLen = static_cast<uint32_t>(chunk.fileName.size());
		ofs.write(reinterpret_cast<const char*>(&fnLen), sizeof(fnLen));
		if (fnLen > 0) ofs.write(chunk.fileName.data(), fnLen);

		uint32_t fpLen = static_cast<uint32_t>(chunk.fullPath.size());
		ofs.write(reinterpret_cast<const char*>(&fpLen), sizeof(fpLen));
		if (fpLen > 0) ofs.write(chunk.fullPath.data(), fpLen);

		// LOD0 メッシュデータ
		Model* model = ModelManager::GetInstance()->FindModel(chunk.fullPath);
		Model::ModelData data = model ? model->GetModelData() : Model::ModelData();

		LodMetaHeader lHeader;
		lHeader.lodLevel = 0;
		lHeader.switchDistance = 0.0f;
		lHeader.vertexCount = static_cast<uint32_t>(data.vertices.size());
		lHeader.indexCount = static_cast<uint32_t>(data.indices.size());
		lHeader.materialCount = static_cast<uint32_t>(data.materials.size());
		ofs.write(reinterpret_cast<const char*>(&lHeader), sizeof(lHeader));

		// 頂点配列
		if (lHeader.vertexCount > 0) {
			ofs.write(reinterpret_cast<const char*>(data.vertices.data()), sizeof(Model::VertexData) * lHeader.vertexCount);
		}
		// インデックス配列
		if (lHeader.indexCount > 0) {
			ofs.write(reinterpret_cast<const char*>(data.indices.data()), sizeof(uint32_t) * lHeader.indexCount);
		}
		// マテリアル
		for (const auto& mat : data.materials) {
			uint32_t texLen = static_cast<uint32_t>(mat.textureFilePath.size());
			ofs.write(reinterpret_cast<const char*>(&texLen), sizeof(texLen));
			if (texLen > 0) ofs.write(mat.textureFilePath.data(), texLen);
			ofs.write(reinterpret_cast<const char*>(&mat.baseColor), sizeof(mat.baseColor));
			ofs.write(reinterpret_cast<const char*>(&mat.shininess), sizeof(mat.shininess));
			ofs.write(reinterpret_cast<const char*>(&mat.environmentCoefficient), sizeof(mat.environmentCoefficient));
			ofs.write(reinterpret_cast<const char*>(&mat.specularIntensity), sizeof(mat.specularIntensity));
			ofs.write(reinterpret_cast<const char*>(&mat.emissiveColor), sizeof(mat.emissiveColor));
			ofs.write(reinterpret_cast<const char*>(&mat.emissiveIntensity), sizeof(mat.emissiveIntensity));
		}
		// ノード構造
		uint32_t rootNameLen = static_cast<uint32_t>(data.rootNode.name.size());
		ofs.write(reinterpret_cast<const char*>(&rootNameLen), sizeof(rootNameLen));
		if (rootNameLen > 0) ofs.write(data.rootNode.name.data(), rootNameLen);

		uint32_t childCount = static_cast<uint32_t>(data.rootNode.children.size());
		ofs.write(reinterpret_cast<const char*>(&childCount), sizeof(childCount));
		for (const auto& child : data.rootNode.children) {
			uint32_t childNameLen = static_cast<uint32_t>(child.name.size());
			ofs.write(reinterpret_cast<const char*>(&childNameLen), sizeof(childNameLen));
			if (childNameLen > 0) ofs.write(child.name.data(), childNameLen);

			uint32_t meshCount = static_cast<uint32_t>(child.meshes.size());
			ofs.write(reinterpret_cast<const char*>(&meshCount), sizeof(meshCount));
			if (meshCount > 0) {
				ofs.write(reinterpret_cast<const char*>(child.meshes.data()), sizeof(Model::MeshInfo) * meshCount);
			}
		}
	}
	return true;
}

bool TerrainChunkManager::LoadPackage(const std::string& packagePath) {
	std::ifstream ifs(packagePath, std::ios::binary);
	if (!ifs) return false;

	TerrainPackageHeader pHeader;
	ifs.read(reinterpret_cast<char*>(&pHeader), sizeof(pHeader));
	if (std::memcmp(pHeader.magic, "TRNP", 4) != 0 || pHeader.version != 1) {
		return false;
	}

	chunks_.clear();
	chunks_.reserve(pHeader.chunkCount);

	ModelCommon* modelCommon = ModelManager::GetInstance()->GetModelCommon();

	for (uint32_t i = 0; i < pHeader.chunkCount; ++i) {
		ChunkMetaHeader cHeader;
		ifs.read(reinterpret_cast<char*>(&cHeader), sizeof(cHeader));

		TerrainChunk chunk;
		chunk.gridX = cHeader.gridX;
		chunk.gridY = cHeader.gridY;
		chunk.worldCx = cHeader.worldCx;
		chunk.worldCy = cHeader.worldCy;
		chunk.minX = cHeader.minX;
		chunk.maxX = cHeader.maxX;
		chunk.minZ = cHeader.minZ;
		chunk.maxZ = cHeader.maxZ;
		chunk.center = { cHeader.centerX, cHeader.centerY, cHeader.centerZ };

		uint32_t fnLen = 0;
		ifs.read(reinterpret_cast<char*>(&fnLen), sizeof(fnLen));
		if (fnLen > 0) {
			chunk.fileName.resize(fnLen);
			ifs.read(chunk.fileName.data(), fnLen);
		}
		uint32_t fpLen = 0;
		ifs.read(reinterpret_cast<char*>(&fpLen), sizeof(fpLen));
		if (fpLen > 0) {
			chunk.fullPath.resize(fpLen);
			ifs.read(chunk.fullPath.data(), fpLen);
		}

		for (uint32_t lodIdx = 0; lodIdx < cHeader.lodCount; ++lodIdx) {
			LodMetaHeader lHeader;
			ifs.read(reinterpret_cast<char*>(&lHeader), sizeof(lHeader));

			Model::ModelData data;
			data.vertices.resize(lHeader.vertexCount);
			if (lHeader.vertexCount > 0) {
				ifs.read(reinterpret_cast<char*>(data.vertices.data()), sizeof(Model::VertexData) * lHeader.vertexCount);
			}
			data.indices.resize(lHeader.indexCount);
			if (lHeader.indexCount > 0) {
				ifs.read(reinterpret_cast<char*>(data.indices.data()), sizeof(uint32_t) * lHeader.indexCount);
			}

			data.materials.resize(lHeader.materialCount);
			for (uint32_t m = 0; m < lHeader.materialCount; ++m) {
				auto& mat = data.materials[m];
				uint32_t texLen = 0;
				ifs.read(reinterpret_cast<char*>(&texLen), sizeof(texLen));
				if (texLen > 0) {
					mat.textureFilePath.resize(texLen);
					ifs.read(mat.textureFilePath.data(), texLen);
				}
				ifs.read(reinterpret_cast<char*>(&mat.baseColor), sizeof(mat.baseColor));
				ifs.read(reinterpret_cast<char*>(&mat.shininess), sizeof(mat.shininess));
				ifs.read(reinterpret_cast<char*>(&mat.environmentCoefficient), sizeof(mat.environmentCoefficient));
				ifs.read(reinterpret_cast<char*>(&mat.specularIntensity), sizeof(mat.specularIntensity));
				ifs.read(reinterpret_cast<char*>(&mat.emissiveColor), sizeof(mat.emissiveColor));
				ifs.read(reinterpret_cast<char*>(&mat.emissiveIntensity), sizeof(mat.emissiveIntensity));
			}

			uint32_t rootNameLen = 0;
			ifs.read(reinterpret_cast<char*>(&rootNameLen), sizeof(rootNameLen));
			if (rootNameLen > 0) {
				data.rootNode.name.resize(rootNameLen);
				ifs.read(data.rootNode.name.data(), rootNameLen);
			}

			uint32_t childCount = 0;
			ifs.read(reinterpret_cast<char*>(&childCount), sizeof(childCount));
			data.rootNode.children.resize(childCount);
			for (uint32_t c = 0; c < childCount; ++c) {
				auto& child = data.rootNode.children[c];
				uint32_t childNameLen = 0;
				ifs.read(reinterpret_cast<char*>(&childNameLen), sizeof(childNameLen));
				if (childNameLen > 0) {
					child.name.resize(childNameLen);
					ifs.read(child.name.data(), childNameLen);
				}
				uint32_t meshCount = 0;
				ifs.read(reinterpret_cast<char*>(&meshCount), sizeof(meshCount));
				child.meshes.resize(meshCount);
				if (meshCount > 0) {
					ifs.read(reinterpret_cast<char*>(child.meshes.data()), sizeof(Model::MeshInfo) * meshCount);
				}
			}

			// LODキーの設定
			std::string modelKey = chunk.fullPath;
			if (lHeader.lodLevel > 0) {
				modelKey += "_LOD" + std::to_string(lHeader.lodLevel);
			}

			if (!ModelManager::GetInstance()->FindModel(modelKey) && modelCommon) {
				auto model = std::make_unique<Model>();
				model->InitializeFromModelData(modelCommon, std::move(data));
				ModelManager::GetInstance()->RegisterModel(modelKey, std::move(model));
			}

			TerrainLodMesh lodMesh;
			lodMesh.lodLevel = lHeader.lodLevel;
			lodMesh.switchDistance = lHeader.switchDistance;
			lodMesh.fullPath = modelKey;
			lodMesh.object3d = std::make_unique<Object3d>();
			lodMesh.object3d->Initialize(object3dCommon_);
			lodMesh.object3d->SetCamera(camera_);
			lodMesh.object3d->SetModel(modelKey);
			lodMesh.object3d->SetScale({ 1.0f, 1.0f, 1.0f });
			lodMesh.object3d->SetRotate({ 0.0f, 0.0f, 0.0f });
			lodMesh.object3d->SetTranslate({ 0.0f, 0.0f, 0.0f });
			lodMesh.object3d->Update();

			chunk.lodMeshes.push_back(std::move(lodMesh));
		}

		// LOD0 のオブジェクトを直接 object3d にも初期化（既存描画互換用）
		if (!chunk.lodMeshes.empty()) {
			chunk.object3d = std::make_unique<Object3d>();
			chunk.object3d->Initialize(object3dCommon_);
			chunk.object3d->SetCamera(camera_);
			chunk.object3d->SetModel(chunk.fullPath);
			chunk.object3d->SetScale({ 1.0f, 1.0f, 1.0f });
			chunk.object3d->SetRotate({ 0.0f, 0.0f, 0.0f });
			chunk.object3d->SetTranslate({ 0.0f, 0.0f, 0.0f });
			chunk.object3d->Update();
			chunk.isLoaded = true;
		}

		chunks_.push_back(std::move(chunk));
	}

	isLoaded_ = true;
	return true;
}

void TerrainChunkManager::LoadChunked(const std::string& jsonPath) {
	chunks_.clear();
	singleMeshObject_.reset();
	isLoaded_ = false;

	// 1. 統合パッケージ（chunks.terrain）の確認
	//    chunks_info.json と同じディレクトリの chunks.terrain を探索
	std::filesystem::path jPath(jsonPath);
	std::filesystem::path packagePath = jPath.parent_path() / "chunks.terrain";

	std::error_code ec;
	bool packageValid = false;
	if (std::filesystem::exists(packagePath, ec) && std::filesystem::exists(jPath, ec)) {
		auto pkgTime = std::filesystem::last_write_time(packagePath, ec);
		auto jsonTime = std::filesystem::last_write_time(jPath, ec);
		if (!ec && pkgTime >= jsonTime) {
			packageValid = true;
		}
	}

	if (packageValid) {
		auto startTime = std::chrono::high_resolution_clock::now();
		if (LoadPackage(packagePath.string())) {
			auto endTime = std::chrono::high_resolution_clock::now();
			lastLoadTimeMs_ = std::chrono::duration<double, std::milli>(endTime - startTime).count();
			char logBuf[256];
			sprintf_s(logBuf, "[TerrainChunkManager] Successfully loaded %zu chunks from unified package %s (%.2f ms)",
				chunks_.size(), packagePath.filename().string().c_str(), lastLoadTimeMs_);
			Benchmark::Log(logBuf);
			return;
		}
	}

	if (!std::filesystem::exists(jsonPath)) {
		char errBuf[256];
		sprintf_s(errBuf, "[TerrainChunkManager] Error: File not found: %s", jsonPath.c_str());
		Benchmark::Log(errBuf);
		return;
	}

	auto startTime = std::chrono::high_resolution_clock::now();

	// 2. chunks_info.json のパース
	std::ifstream file(jsonPath);
	if (!file.is_open()) {
		Benchmark::Log("[TerrainChunkManager] Error: Failed to open chunks_info.json");
		return;
	}

	json j;
	try {
		file >> j;
	} catch (const std::exception& e) {
		char errBuf[256];
		sprintf_s(errBuf, "[TerrainChunkManager] JSON parse error: %s", e.what());
		Benchmark::Log(errBuf);
		return;
	}

	if (!j.contains("chunks") || !j["chunks"].is_array()) {
		Benchmark::Log("[TerrainChunkManager] JSON format error: 'chunks' array not found");
		return;
	}

	std::string baseDir = std::filesystem::path(jsonPath).parent_path().string();
	std::vector<std::string> modelPathsToLoad;

	for (const auto& item : j["chunks"]) {
		TerrainChunk chunk;
		chunk.gridX = item.value("grid_x", 0);
		chunk.gridY = item.value("grid_y", 0);
		chunk.worldCx = item.value("world_cx", 0);
		chunk.worldCy = item.value("world_cy", 0);

		float origMinX = item.value("min_x", 0.0f);
		float origMaxX = item.value("max_x", 0.0f);
		float origMinY = item.value("min_y", 0.0f);
		float origMaxY = item.value("max_y", 0.0f);

		// DirectX座標系変換（X反転・Z=-Y）
		chunk.minX = -origMaxX;
		chunk.maxX = -origMinX;
		chunk.minZ = -origMaxY;
		chunk.maxZ = -origMinY;

		chunk.center.x = (chunk.minX + chunk.maxX) * 0.5f;
		chunk.center.y = 0.0f;
		chunk.center.z = (chunk.minZ + chunk.maxZ) * 0.5f;

		chunk.fileName = item.value("file", "");
		if (chunk.fileName.empty()) {
			if (item.contains("lod_files") && !item["lod_files"].empty()) {
				chunk.fileName = item["lod_files"][0].get<std::string>();
			}
		}

		if (!baseDir.empty()) {
			chunk.fullPath = baseDir + "/" + chunk.fileName;
		} else {
			chunk.fullPath = chunk.fileName;
		}

		std::replace(chunk.fullPath.begin(), chunk.fullPath.end(), '\\', '/');

		modelPathsToLoad.push_back(chunk.fullPath);
		chunks_.push_back(std::move(chunk));
	}

	char logBuf[256];
	sprintf_s(logBuf, "[TerrainChunkManager] Found %zu chunks in %s. Loading models in parallel...",
		chunks_.size(), jsonPath.c_str());
	Benchmark::Log(logBuf);

	// 3. ModelManager による全チャンクモデルの並列ロード（マルチスレッドAssimp/FastOBJパース）
	if (!modelPathsToLoad.empty()) {
		ModelManager::GetInstance()->LoadModelsParallel(modelPathsToLoad, /*useFastObjLoader=*/true);

		auto* mm = ModelManager::GetInstance();
		sprintf_s(logBuf, "[TerrainChunkManager] Breakdown -> Parse(FastOBJ, %u workers): %.2f ms / GPU Init(main thread): %.2f ms",
			mm->GetLastParallelWorkerCount(), mm->GetLastParallelParseMs(), mm->GetLastParallelGpuInitMs());
		Benchmark::Log(logBuf);
	}

	// 4. 各チャンクの Object3d 生成と初期化
	for (auto& chunk : chunks_) {
		Model* model = ModelManager::GetInstance()->FindModel(chunk.fullPath);
		if (model) {
			chunk.object3d = std::make_unique<Object3d>();
			chunk.object3d->Initialize(object3dCommon_);
			chunk.object3d->SetCamera(camera_);
			chunk.object3d->SetModel(chunk.fullPath);
			chunk.object3d->SetScale({ 1.0f, 1.0f, 1.0f });
			chunk.object3d->SetRotate({ 0.0f, 0.0f, 0.0f });
			chunk.object3d->SetTranslate({ 0.0f, 0.0f, 0.0f });
			chunk.object3d->Update();
			chunk.isLoaded = true;

			// LOD0メッシュとしても登録
			TerrainLodMesh lodMesh;
			lodMesh.lodLevel = 0;
			lodMesh.switchDistance = 0.0f;
			lodMesh.fullPath = chunk.fullPath;
			lodMesh.object3d = std::make_unique<Object3d>();
			lodMesh.object3d->Initialize(object3dCommon_);
			lodMesh.object3d->SetCamera(camera_);
			lodMesh.object3d->SetModel(chunk.fullPath);
			lodMesh.object3d->SetScale({ 1.0f, 1.0f, 1.0f });
			lodMesh.object3d->SetRotate({ 0.0f, 0.0f, 0.0f });
			lodMesh.object3d->SetTranslate({ 0.0f, 0.0f, 0.0f });
			lodMesh.object3d->Update();
			chunk.lodMeshes.push_back(std::move(lodMesh));
		}
	}

	// 5. 次回起動のために 1 つの統合パッケージ（chunks.terrain）へ保存
	SavePackage(packagePath.string());

	auto endTime = std::chrono::high_resolution_clock::now();
	lastLoadTimeMs_ = std::chrono::duration<double, std::milli>(endTime - startTime).count();

	sprintf_s(logBuf, "[TerrainChunkManager] Successfully loaded and created unified package %s (%d/%zu chunks, %.2f ms)",
		packagePath.filename().string().c_str(), GetLoadedChunkCount(), chunks_.size(), lastLoadTimeMs_);
	Benchmark::Log(logBuf);

	isLoaded_ = true;
}

int TerrainChunkManager::GetLoadedChunkCount() const {
	if (mode_ == TerrainMode::SingleMesh) {
		return (singleMeshObject_ != nullptr) ? 1 : 0;
	}
	int count = 0;
	for (const auto& chunk : chunks_) {
		if (chunk.isLoaded) count++;
	}
	return count;
}

void TerrainChunkManager::Update(const Vector3& viewerPos) {
	if (!isLoaded_ || !isVisible_) return;

	Camera* activeCam = CameraManager::GetInstance()->GetActiveCamera();
	if (activeCam) {
		camera_ = activeCam;
	}

	if (mode_ == TerrainMode::SingleMesh) {
		if (singleMeshObject_) {
			singleMeshObject_->SetCamera(camera_);
			singleMeshObject_->Update();
		}
		renderedChunkCount_ = 1;
		return;
	}

	renderedChunkCount_ = 0;
	for (auto& chunk : chunks_) {
		if (!chunk.isLoaded || !chunk.object3d) {
			chunk.isVisible = false;
			continue;
		}

		// チャンクAABB（4000m四方の矩形境界）への最近接最短距離を計算
		float clampedX = std::clamp(viewerPos.x, chunk.minX, chunk.maxX);
		float clampedZ = std::clamp(viewerPos.z, chunk.minZ, chunk.maxZ);
		float dx = clampedX - viewerPos.x;
		float dz = clampedZ - viewerPos.z;
		float distToAABB = std::sqrt(dx * dx + dz * dz);
		chunk.distanceToViewer = distToAABB;

		// 10kmカリング判定
		if (distToAABB <= drawDistance_) {
			chunk.isVisible = true;
			chunk.object3d->SetCamera(camera_);
			chunk.object3d->Update();
		} else {
			chunk.isVisible = false;
		}
	}
}

void TerrainChunkManager::Draw() {
	if (!isLoaded_ || !isVisible_) return;

	Object3dCommon::GetInstance()->SetupCommonState();

	if (mode_ == TerrainMode::SingleMesh) {
		if (singleMeshObject_) {
			singleMeshObject_->Draw();
			renderedChunkCount_ = 1;
		}
		return;
	}

	renderedChunkCount_ = 0;
	for (auto& chunk : chunks_) {
		if (chunk.isVisible && chunk.object3d) {
			chunk.object3d->Draw();
			renderedChunkCount_++;
		}
	}
}

void TerrainChunkManager::Finalize() {
	chunks_.clear();
	singleMeshObject_.reset();
	isLoaded_ = false;
}

void TerrainChunkManager::DrawImGui() {
#ifdef USE_IMGUI
	if (!ImGui::Begin("Terrain System & Mode Switcher")) {
		ImGui::End();
		return;
	}

	ImGui::Text("Active Mode:");
	int modeIndex = (mode_ == TerrainMode::SingleMesh) ? 0 : 1;
	if (ImGui::RadioButton("1-Mesh (demo.obj)", &modeIndex, 0)) {
		SetTerrainMode(TerrainMode::SingleMesh);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Chunked (10km Culling)", &modeIndex, 1)) {
		SetTerrainMode(TerrainMode::Chunked);
	}

	ImGui::Separator();
	ImGui::Text("Load Status: %s (Last Load Time: %.2f ms)", isLoaded_ ? "Loaded" : "Not Loaded", lastLoadTimeMs_);
	ImGui::Checkbox("Visible", &isVisible_);

	if (mode_ == TerrainMode::SingleMesh) {
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Mode: 1-Mesh Monolithic Terrain (40km x 40km)");
		ImGui::Text("Rendered Meshes: 1 / 1 (All 730,000+ polygons always drawn)");
		ImGui::TextDisabled("Note: Distance culling cannot be applied to a single mesh.");
	} else {
		ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Mode: 4000m Chunked Terrain with Distance Culling");
		ImGui::Text("Loaded Chunks: %d / %zu", GetLoadedChunkCount(), chunks_.size());
		ImGui::Text("Rendered (In Culling Range): %d / %zu", renderedChunkCount_, chunks_.size());

		ImGui::SliderFloat("Culling Distance (m)", &drawDistance_, 2000.0f, 40000.0f, "%.0f m");
		if (ImGui::Button("Reset to 10km (10000m)")) {
			drawDistance_ = 10000.0f;
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Reload Terrain")) {
		Reload();
	}

	ImGui::End();
#endif
}
