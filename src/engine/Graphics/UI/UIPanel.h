#pragma once
#include "UIElement.h"
#include <memory>
#include "../Sprite/Sprite.h"

class SpriteCommon;

/// @brief 半透明背景パネルのUI要素
class UIPanel : public UIElement {
public:
	/// @brief 初期化
	/// @param spriteCommon SpriteCommonのポインタ
	void Initialize(SpriteCommon* spriteCommon);

	void Update() override;
	void Draw() override;

	/// @brief パネルの背景色を設定
	void SetBackgroundColor(const Vector4& color) { SetColor(color); }

private:
	std::unique_ptr<Sprite> backgroundSprite_;
	SpriteCommon* spriteCommon_ = nullptr;
};
