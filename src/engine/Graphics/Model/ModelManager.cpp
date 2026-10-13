#include "ModelManager.h"
#include "ModelCommon.h"
#include "../System/DirectXCommon.h"
#include "../System/SrvManager.h"
#include <filesystem>
#include <future>
#include <vector>

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

void ModelManager::LoadModelsParallel(const std::vector<std::string>& filePaths)
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

	// 1. 全コアを活用した複数スレッド並列ファイルパース（Assimp）
	std::vector<std::future<std::pair<std::string, Model::ModelData>>> futures;
	futures.reserve(toLoad.size());

	for (const auto& path : toLoad) {
		futures.push_back(std::async(std::launch::async, [path]() {
			std::string fullPath;
			if (std::filesystem::exists(path)) {
				fullPath = path;
			} else {
				fullPath = "assets/" + path;
			}
			Model::ModelData data = Model::LoadModelFile("", fullPath);
			return std::make_pair(path, std::move(data));
		}));
	}

	// 2. パース完了順にGPUリソース生成と登録
	for (auto& fut : futures) {
		auto [path, modelData] = fut.get();
		auto model = std::make_unique<Model>();
		model->InitializeFromModelData(modelCommon_.get(), std::move(modelData));
		models_.insert(std::make_pair(path, std::move(model)));
	}
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
