#pragma once
#include<map>
#include <string>
#include <memory>
#include "Model.h"
class ModelCommon;
class DirectXCommon;
class SrvManager;
/// <summary>
/// 3Dモデルアセット管理クラス
/// </summary>
class ModelManager
{

public:
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static ModelManager* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	ModelManager();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Finalize();

	/// <summary>
	/// モデルファイルの読み込み
	/// </summary>
	/// <param name="filePath"></param>
	void LoadModel(const std::string& filePath);
	Model::Animation LoadAnimation(const std::string& filePath);
	std::vector<std::string> LoadAnimationNames(const std::string& filePath);
	/// <summary>
	/// モデルの検索
	/// </summary>
	/// <param name="filePath"></param>
	/// <returns></returns>
	Model* FindModel(const std::string& filePath);
	~ModelManager();
private:
	static std::unique_ptr<ModelManager> instance_;
	ModelManager(const ModelManager&) = delete;
	ModelManager& operator=(const ModelManager&) = delete;
	std::unique_ptr<ModelCommon> modelCommon_ = nullptr;

	//モデルデータ
	std::map<std::string, std::unique_ptr<Model>> models_;
};

