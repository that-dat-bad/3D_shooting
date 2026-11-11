#pragma once
#include "UIElement.h"
#include <memory>
#include "../Sprite/Sprite.h"

class SpriteCommon;

// 背景パネルUI要素
class UIPanel : public UIElement {
public:
	// 初期化
	// spriteCommon: SpriteCommonのポインタ
	void Initialize(SpriteCommon* spriteCommon);

	void Update() override;
	void Draw() override;

	// 背景色の設定
	void SetBackgroundColor(const Vector4& color) { SetColor(color); }

private:
	std::unique_ptr<Sprite> backgroundSprite_;
	SpriteCommon* spriteCommon_ = nullptr;
};
