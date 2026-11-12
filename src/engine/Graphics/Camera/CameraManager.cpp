#include "CameraManager.h"
#include <cassert>

std::unique_ptr<CameraManager> CameraManager::instance_ = nullptr;

CameraManager* CameraManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique<CameraManager>();
	}
	return instance_.get();
}

void CameraManager::Initialize() {
	// デフォルトカメラを作っておく（"default" という名前で）
	CreateCamera("default");
	SetActiveCamera("default");
}

void CameraManager::Finalize() {
	cameras_.clear();
}

void CameraManager::CreateCamera(const std::string& name) {
	// 既に同じ名前があったら何もしない（あるいは上書き）
	if (cameras_.contains(name)) {
		return;
	}

	// 新しいカメラを作成してMapに登録
	std::unique_ptr<Camera> newCamera = std::make_unique<Camera>();
	cameras_.insert(std::make_pair(name, std::move(newCamera)));
}

void CameraManager::SetActiveCamera(const std::string& name) {
	// 指定した名前のカメラが存在するかチェック
	if (cameras_.contains(name)) {
		activeCamera_ = cameras_[name].get(); // 生ポインタを取得
	}
}

Camera* CameraManager::GetCamera(const std::string& name) const {
	auto it = cameras_.find(name);
	if (it != cameras_.end()) {
		return it->second.get();
	}
	return nullptr;
}

bool CameraManager::HasCamera(const std::string& name) const {
	return cameras_.contains(name);
}

void CameraManager::Update() {
	if (activeCamera_) {
		activeCamera_->Update();
	}
}

void CameraManager::DeleteCamera(const std::string& name) {
	if (cameras_.contains(name)) {
		// アクティブカメラが削除対象ならアクティブカメラをクリア
		if (activeCamera_ == cameras_[name].get()) {
			activeCamera_ = nullptr;
		}
		// カメラを削除
		cameras_.erase(name);
	}
}

void CameraManager::SetNearClip(float nearClip) {
	if (activeCamera_) {
		activeCamera_->SetNearClip(nearClip);
	}
}

void CameraManager::SetFarClip(float farClip) {
	if (activeCamera_) {
		activeCamera_->SetFarClip(farClip);
	}
}