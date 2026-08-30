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

private:
	std::string fontName_ = "Roboto";
	std::string text_;
	float fontSize_ = 32.0f;
	Vector2 anchorPoint_ = {0.0f, 0.0f};
};
