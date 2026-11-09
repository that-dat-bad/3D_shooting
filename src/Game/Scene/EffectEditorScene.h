#pragma once
#ifdef _DEBUG

#include "IScene.h"
#include <memory>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/Graphics/Model/Skybox.h"
#include "../../engine/Graphics/Particle/EffectManager.h"

/// <summary>
/// Debugビルド専用の独立エフェクトエディタシーン
/// .effect 外部設定ファイルのリアルタイム編集および emit() / stop() / Delete() の動作検証を行う
/// </summary>
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
	std::unique_ptr<Object3d> groundObject_ = nullptr;
	EffectHandle* previewHandle_ = nullptr;

	MyMath::Vector3 emitOrigin_ = { 0.0f, 0.0f, 0.0f };
	bool isSkyboxVisible_ = true;
	bool isGroundVisible_ = true;
	bool showHeightLimitGuide_ = true;
	bool simulateConditionTrue_ = false;
	uint32_t whiteTexIndex_ = 0;
};

#endif // _DEBUG

