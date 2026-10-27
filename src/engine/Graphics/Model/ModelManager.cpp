#include "ModelManager.h"
#include "ModelCommon.h"
#include "../System/DirectXCommon.h"
#include "../System/SrvManager.h"
#include <filesystem>
#include <future>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>

std::unique_ptr<ModelManager> ModelManager::instance_ = nullptr;

ModelManager::ModelManager() = default;
ModelManager::~ModelManager() = default;

ModelManager* ModelManager::GetInstance()
{
	if (instance_ == nullptr) {
		instance_ = std::make_unique<ModelManager>();
	}
	return instance_.get();
}

void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	modelCommon_ = std::make_unique<ModelCommon>();
	modelCommon_->Initialize(dxCommon, srvManager);
}

void ModelManager::Finalize()
{
	instance_.reset();
}

void ModelManager::LoadModel(const std::string& filePath)
{
	if (models_.contains(filePath))
	{
		return;
	}
	
	//モデルの生成とファイル読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();

	// パスがそのまま存在すればそのまま使い、なければ assets/ を付加
	std::string fullPath;
	if (std::filesystem::exists(filePath)) {
		fullPath = filePath;
	} else {
		fullPath = "assets/" + filePath;
	}
	model->Initialize(modelCommon_.get(), "", fullPath);

	//モデルをmapコンテナに格納する
	models_.insert(std::make_pair(filePath, std::move(model)));
}

void ModelManager::LoadModelsParallel(const std::vector<std::string>& filePaths, bool useFastObjLoader)
{
	std::vector<std::string> toLoad;
	for (const auto& path : filePaths) {
		if (!models_.contains(path)) {
			toLoad.push_back(path);
		}
	}
	if (toLoad.empty()) {
		return;
	}

	// 1. 固定数ワーカーによる並列パース（Assimp）
	//    ファイルごとに std::async を立てると（例: 100チャンク=100スレッド）
	//    コア数を大幅に超えてコンテキストスイッチ・ヒープ競合が多発し、逆に遅くなるため
	//    (論理コア数 - 1) 本のワーカーがジョブを取り合う形式にする
	auto parseStart = std::chrono::high_resolution_clock::now();

	const size_t jobCount = toLoad.size();
	std::vector<Model::ModelData> results(jobCount);
	std::atomic<size_t> nextJob{ 0 };

	unsigned int hw = std::thread::hardware_concurrency();
	if (hw == 0) { hw = 4; }
	const size_t workerCount = (std::min)(jobCount, static_cast<size_t>((std::max)(1u, hw - 1)));

	auto worker = [&]() {
		for (;;) {
			size_t i = nextJob.fetch_add(1, std::memory_order_relaxed);
			if (i >= jobCount) { break; }
			const std::string& path = toLoad[i];
			std::string fullPath = std::filesystem::exists(path) ? path : ("assets/" + path);
			results[i] = {};
			if (useFastObjLoader && fullPath.ends_with(".obj")) {
				results[i] = Model::LoadObjFileFast(fullPath);
			}
			// 軽量ローダー非対応 or 失敗時は Assimp にフォールバック
			if (results[i].vertices.empty()) {
				results[i] = Model::LoadModelFile("", fullPath);
			}
		}
	};

	std::vector<std::thread> threads;
	threads.reserve(workerCount);
	for (size_t t = 0; t < workerCount; ++t) {
		threads.emplace_back(worker);
	}
	for (auto& th : threads) {
		th.join();
	}

	auto parseEnd = std::chrono::high_resolution_clock::now();

	// 2. GPUリソース生成と登録（メインスレッド）
	for (size_t i = 0; i < jobCount; ++i) {
		auto model = std::make_unique<Model>();
		model->InitializeFromModelData(modelCommon_.get(), std::move(results[i]));
		models_.insert(std::make_pair(toLoad[i], std::move(model)));
	}

	auto gpuEnd = std::chrono::high_resolution_clock::now();
	lastParallelParseMs_ = std::chrono::duration<double, std::milli>(parseEnd - parseStart).count();
	lastParallelGpuInitMs_ = std::chrono::duration<double, std::milli>(gpuEnd - parseEnd).count();
	lastParallelWorkerCount_ = static_cast<uint32_t>(workerCount);
}

Model* ModelManager::FindModel(const std::string& filePath)
{
	if (models_.contains(filePath))
	{
		//読み込みモデルを戻り値としてreturn
		return models_.at(filePath).get();
	}

	//ファイル名一致なし
	return nullptr;
}

Model::Animation ModelManager::LoadAnimation(const std::string& filePath)
{
	std::string fullPath;
	if (std::filesystem::exists(filePath)) {
		fullPath = filePath;
	} else {
		fullPath = "assets/" + filePath;
	}
	return Model::LoadAnimationFile("", fullPath,"");
}

std::vector<std::string> ModelManager::LoadAnimationNames(const std::string& filePath)
{
	std::string fullPath;
	if (std::filesystem::exists(filePath)) {
		fullPath = filePath;
	} else {
		fullPath = "assets/" + filePath;
	}
	return Model::LoadAnimationNames("", fullPath);
}
