#pragma once
#include "IScene.h"
#include "../../engine/Graphics/UI/UIPanel.h"
#include "../../engine/Graphics/UI/UIText.h"
#include "../../engine/Graphics/UI/UIButton.h"
#include "../../engine/Graphics/UI/UISelectionManager.h"

class ResultScene : public IScene {
public:
	ResultScene();
	~ResultScene();
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawUI() override;
	void Finalize() override;

private:
	UIPanel backgroundPanel_;
	UIText gameOverText_;
	
	UIButton retryButton_;
	UIButton titleButton_;

	UISelectionManager selectionManager_;

	float animTimer_ = 0.0f;
	float fadeInAlpha_ = 0.0f;
};
