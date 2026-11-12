#pragma once
#include "IScene.h"
#include "../../engine/Graphics/Model/Skybox.h"
#include <memory>
#include <string>
#include <vector>

class DebugScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void Finalize() override;

	~DebugScene() override = default;

private:
	std::unique_ptr<Skybox> skybox_ = nullptr;
	std::vector<std::string> skyboxTextures_;
	int currentSkyboxIndex_ = 0;
	Vector4 skyboxColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
};
