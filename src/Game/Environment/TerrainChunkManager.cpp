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

void TerrainChunkManager::LoadChunked(const std::string& jsonPath) {
	chunks_.clear();
	singleMeshObject_.reset();
	isLoaded_ = false;

	if (!std::filesystem::exists(jsonPath)) {
		char errBuf[256];
		sprintf_s(errBuf, "[TerrainChunkManager] Error: File not found: %s", jsonPath.c_str());
		Benchmark::Log(errBuf);
		return;
	}

	auto startTime = std::chrono::high_resolution_clock::now();

	// 1. chunks_info.json のパース
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

	// 2. ModelManager による全チャンクモデルの並列ロード（マルチスレッドAssimpパース）
	if (!modelPathsToLoad.empty()) {
		ModelManager::GetInstance()->LoadModelsParallel(modelPathsToLoad, /*useFastObjLoader=*/true);

		auto* mm = ModelManager::GetInstance();
		sprintf_s(logBuf, "[TerrainChunkManager] Breakdown -> Parse(FastOBJ, %u workers): %.2f ms / GPU Init(main thread): %.2f ms",
			mm->GetLastParallelWorkerCount(), mm->GetLastParallelParseMs(), mm->GetLastParallelGpuInitMs());
		Benchmark::Log(logBuf);
	}

	// 3. 各チャンクの Object3d 生成と初期化
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
		}
	}

	auto endTime = std::chrono::high_resolution_clock::now();
	lastLoadTimeMs_ = std::chrono::duration<double, std::milli>(endTime - startTime).count();

	sprintf_s(logBuf, "[TerrainChunkManager] Successfully loaded %d/%zu chunks (%.2f ms)",
		GetLoadedChunkCount(), chunks_.size(), lastLoadTimeMs_);
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
