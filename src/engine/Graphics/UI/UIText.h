#pragma once
#include "UIElement.h"
#include <string>
#include <memory>
#include "../Sprite/Sprite.h"

class SpriteCommon;

/// @brief テキスト表示用のUI要素
class UIText : public UIElement {
public:
	/// @brief 初期化
	/// @param fontName 使用するフォント名（FontManagerに登録済みのもの）
	/// @param text 表示テキスト
	/// @param fontSize フォントサイズ
	void Initialize(const std::string& fontName, const std::string& text, float fontSize);

	void Update() override;
	void Draw() override;

	void SetText(const std::string& text) { text_ = text; }
	const std::string& GetText() const { return text_; }

	void SetFontSize(float size) { fontSize_ = size; }
	float GetFontSize() const { return fontSize_; }

	void SetFontName(const std::string& name) { fontName_ = name; }
	const std::string& GetFontName() const { return fontName_; }

	void SetAnchorPoint(const Vector2& anchor) { anchorPoint_ = anchor; }
	const Vector2& GetAnchorPoint() const { return anchorPoint_; }

	// --- 装飾（ドロップシャドウ） ---
	void SetDropShadow(bool enable, const Vector2& offset = {2.0f, 2.0f}, const Vector4& color = {0.0f, 0.0f, 0.0f, 1.0f}) {
		hasDropShadow_ = enable;
		shadowOffset_ = offset;
		shadowColor_ = color;
	}
	bool HasDropShadow() const { return hasDropShadow_; }
	Vector2 GetShadowOffset() const { return shadowOffset_; }
	Vector4 GetShadowColor() const { return shadowColor_; }

	// --- 装飾（アウトライン） ---
	void SetOutline(bool enable, float thickness = 1.0f, const Vector4& color = {0.0f, 0.0f, 0.0f, 1.0f}) {
		hasOutline_ = enable;
		outlineThickness_ = thickness;
		outlineColor_ = color;
	}
	bool HasOutline() const { return hasOutline_; }
	float GetOutlineThickness() const { return outlineThickness_; }
	Vector4 GetOutlineColor() const { return outlineColor_; }

private:
	std::string fontName_ = "HackGen";
	std::string text_;
	float fontSize_ = 32.0f;
	Vector2 anchorPoint_ = {0.0f, 0.0f};

	bool hasDropShadow_ = false;
	Vector2 shadowOffset_ = {2.0f, 2.0f};
	Vector4 shadowColor_ = {0.0f, 0.0f, 0.0f, 1.0f};

	bool hasOutline_ = false;
	float outlineThickness_ = 1.0f;
	Vector4 outlineColor_ = {0.0f, 0.0f, 0.0f, 1.0f};
};
