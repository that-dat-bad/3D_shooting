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

	// 1. ドロップシャドウ
	if (hasDropShadow_) {
		Vector4 shadowCol = { shadowColor_.x, shadowColor_.y, shadowColor_.z, shadowColor_.w * renderColor.w };
		TextRenderer::GetInstance()->Print(
			fontName_, text_,
			renderPos.x + shadowOffset_.x,
			renderPos.y + shadowOffset_.y,
			renderSize, shadowCol, anchorPoint_
		);
	}

	// 2. アウトライン (上下左右斜めに8回描画して擬似的に縁取り)
	if (hasOutline_) {
		Vector4 outlineCol = { outlineColor_.x, outlineColor_.y, outlineColor_.z, outlineColor_.w * renderColor.w };
		float offsets[8][2] = {
			{-1, -1}, { 0, -1}, { 1, -1},
			{-1,  0},           { 1,  0},
			{-1,  1}, { 0,  1}, { 1,  1}
		};
		for (int i = 0; i < 8; ++i) {
			TextRenderer::GetInstance()->Print(
				fontName_, text_,
				renderPos.x + offsets[i][0] * outlineThickness_,
				renderPos.y + offsets[i][1] * outlineThickness_,
				renderSize, outlineCol, anchorPoint_
			);
		}
	}

	// 3. メインテキスト
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
