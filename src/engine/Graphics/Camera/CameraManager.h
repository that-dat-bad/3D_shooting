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
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	CameraManager() = default;

	~CameraManager() = default;
	void Initialize();
	void Finalize();

	// カメラの生成 (newしてmapに登録)
	void CreateCamera(const std::string& name);

	// アクティブカメラの切り替え
	void SetActiveCamera(const std::string& name);

	// アクティブカメラの取得 (描画やオブジェクト側で使う)
	Camera* GetActiveCamera() const { return activeCamera_; }

	// 名前指定でカメラを取得 (存在しない場合はnullptr)
	Camera* GetCamera(const std::string& name) const;

	// 指定名のカメラが存在するか
	bool HasCamera(const std::string& name) const;

	// 更新 (アクティブなカメラだけ更新する)
	void Update();

	//指定されたカメラの削除
	void DeleteCamera(const std::string& name);

	void SetNearClip(float nearClip);

	void SetFarClip(float farClip);

private:
	static std::unique_ptr<CameraManager> instance_;

	CameraManager(const CameraManager&) = delete;
	CameraManager& operator=(const CameraManager&) = delete;

	// カメラのマップ (名前, 実体)
	std::map<std::string, std::unique_ptr<Camera>> cameras_;

	// 現在有効なカメラへのポインタ (所有権は持たない)
	Camera* activeCamera_ = nullptr;
};