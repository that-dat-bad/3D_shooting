#pragma once
#ifdef USE_IMGUI

#include "IScene.h"
#include <memory>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Model/Skybox.h"
#include "../../engine/Graphics/Particle/EffectManager.h"

// エフェクトエディタシーン
class EffectEditorScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawUI() override;
	void Finalize() override;

private:
	void DrawReferenceGridAndHeightLimit(Camera* camera);

	std::unique_ptr<Skybox> skybox_ = nullptr;
	EffectHandle* previewHandle_ = nullptr;

	MyMath::Vector3 emitOrigin_ = { 0.0f, 0.0f, 0.0f };
	bool isSkyboxVisible_ = false;
	bool showGrid_ = true;
	bool showHeightLimitGuide_ = true;
	bool simulateConditionTrue_ = false;
	uint32_t whiteTexIndex_ = 0;
};

#endif // USE_IMGUI

