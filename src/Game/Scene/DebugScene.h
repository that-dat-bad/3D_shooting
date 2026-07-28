#pragma once
#include "IScene.h"
#include <vector>
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/Graphics/Sprite/Sprite.h"
#include "../../engine/Graphics/Model/Skybox.h"
#include <memory>

#include "../../engine/Physics/Collider.h"

/// エンジン機能テスト用のデバッグシーン
class DebugScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void Finalize() override;
private:
	std::unique_ptr<Object3d> sphereObject = nullptr;
	std::unique_ptr<Object3d> terrainObject = nullptr;
	std::unique_ptr<Skybox> skybox_ = nullptr;
	bool isSkyboxVisible_ = true;

	Ray debugRay_;
	RaycastHit debugHit_;
};
