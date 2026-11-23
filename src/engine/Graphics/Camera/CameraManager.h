#pragma once
#include "Camera.h"
#include <map>
#include <string>
#include <memory>

/// <summary>
/// カメラ管理クラス
/// </summary>
class CameraManager {
public: 
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static CameraManager* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ
	/// </summary>
	CameraManager() = default;

	~CameraManager() = default;
	void Initialize();
	void Finalize();

	// カメラの生成
	void CreateCamera(const std::string& name);

	// アクティブカメラの切り替え
	void SetActiveCamera(const std::string& name);

	// アクティブカメラの取得
	Camera* GetActiveCamera() const { return activeCamera_; }

	// 名前指定でカメラを取得
	Camera* GetCamera(const std::string& name) const;

	// 指定名のカメラが存在するか
	bool HasCamera(const std::string& name) const;

	// 更新
	void Update();

	//指定されたカメラの削除
	void DeleteCamera(const std::string& name);

	void SetNearClip(float nearClip);

	void SetFarClip(float farClip);

private:
	static std::unique_ptr<CameraManager> instance_;

	CameraManager(const CameraManager&) = delete;
	CameraManager& operator=(const CameraManager&) = delete;

	// カメラのマップ
	std::map<std::string, std::unique_ptr<Camera>> cameras_;

	// 現在有効なカメラへのポインタ
	Camera* activeCamera_ = nullptr;
};