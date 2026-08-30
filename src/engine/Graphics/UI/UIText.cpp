#include "UIText.h"
#include "../Text/TextRenderer.h"

void UIText::Initialize(const std::string& fontName, const std::string& text, float fontSize) {
	fontName_ = fontName;
	text_ = text;
	fontSize_ = fontSize;
}

void UIText::Update() {
	if (!isVisible_ || !isActive_) return;
	UpdateAnimation(1.0f / 60.0f);
}

void UIText::Draw() {
	if (!isVisible_) return;

	Vector2 renderPos = GetRenderPosition();
	float renderSize = fontSize_ * GetRenderScale();
	Vector4 renderColor = GetRenderColor();

	TextRenderer::GetInstance()->Print(
		fontName_,
		text_,
		renderPos.x,
		renderPos.y,
		renderSize,
		renderColor,
		anchorPoint_
	);
}
