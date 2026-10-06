#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "UIButton.h"
#include "../Sprite/SpriteCommon.h"
#include "../Text/TextRenderer.h"
#include "../../io/Input.h"
#include "../../base/WinApp.h"
#include <algorithm>

void UIButton::Initialize(SpriteCommon* spriteCommon, const std::string& label, float fontSize) {
	label_ = label;
	fontSize_ = fontSize;

	// 背景パネルの初期化
	backgroundPanel_.Initialize(spriteCommon);
	backgroundPanel_.SetColor(normalColor_);
	backgroundPanel_.SetStyle("Normal",   UIStateStyle{ normalColor_,   1.00f, { 0.0f,  0.0f }, UILoopMotion::None, 1.0f, 1.0f });
	backgroundPanel_.SetStyle("Hovered",  UIStateStyle{ hoverColor_,    1.00f, { 0.0f,  0.0f }, UILoopMotion::None, 1.0f, 1.0f });
	backgroundPanel_.SetStyle("Selected", UIStateStyle{ selectedColor_, 1.00f, { 0.0f,  0.0f }, UILoopMotion::None, 1.0f, 1.0f });

	// ラベルテキストの初期化
	labelText_.Initialize("HackGen", label, fontSize);
	labelText_.SetAnchorPoint({ 0.5f, 0.5f });
	labelText_.SetColor(textColor_);
	labelText_.SetStyle("Normal",   UIStateStyle{ textColor_,         1.00f, { 0.0f, 0.0f }, UILoopMotion::None, 1.0f, 1.0f });
	labelText_.SetStyle("Hovered",  UIStateStyle{ selectedTextColor_, 1.00f, { 0.0f, 0.0f }, UILoopMotion::None, 1.0f, 1.0f });
	labelText_.SetStyle("Selected", UIStateStyle{ selectedTextColor_, 1.00f, { 0.0f, 0.0f }, UILoopMotion::None, 1.0f, 1.0f });
}

void UIButton::SetLabel(const std::string& label) {
	label_ = label;
	labelText_.SetText(label);
}

const std::string& UIButton::GetLabel() const {
	return labelText_.GetText();
}

void UIButton::SetNormalColor(const Vector4& color) {
	normalColor_ = color;
	backgroundPanel_.SetStyle("Normal", UIStateStyle{ normalColor_, 1.00f, { 0.0f, 0.0f }, UILoopMotion::None, 1.0f, 1.0f });
}

void UIButton::SetHoverColor(const Vector4& color) {
	hoverColor_ = color;
	backgroundPanel_.SetStyle("Hovered", UIStateStyle{ hoverColor_, 1.00f, { 0.0f, 0.0f }, UILoopMotion::None, 1.0f, 1.0f });
}

void UIButton::SetSelectedColor(const Vector4& color) {
	selectedColor_ = color;
	backgroundPanel_.SetStyle("Selected", UIStateStyle{ selectedColor_, 1.00f, { 0.0f, 0.0f }, UILoopMotion::None, 1.0f, 1.0f });
}

void UIButton::SetTextColor(const Vector4& color) {
	textColor_ = color;
	labelText_.SetColor(color);
	labelText_.SetStyle("Normal", UIStateStyle{ textColor_, 1.00f, { 0.0f, 0.0f }, UILoopMotion::None, 1.0f, 1.0f });
}

void UIButton::SetSelectedTextColor(const Vector4& color) {
	selectedTextColor_ = color;
	labelText_.SetStyle("Hovered", UIStateStyle{ selectedTextColor_, 1.00f, { 0.0f, 0.0f }, UILoopMotion::None, 1.0f, 1.0f });
	labelText_.SetStyle("Selected", UIStateStyle{ selectedTextColor_, 1.00f, { 0.0f, 0.0f }, UILoopMotion::None, 1.0f, 1.0f });
}

void UIButton::Update() {
	if (!isVisible_ || !isActive_) return;

	isClicked_ = false;

	// マウスホバー判定
	bool mouseInside = IsMouseInside();

	// 状態遷移
	if (isSelected_) {
		state_ = State::Selected;
		labelText_.SetState("Selected");
		backgroundPanel_.SetState("Selected");
	} else if (mouseInside) {
		state_ = State::Hovered;
		labelText_.SetState("Hovered");
		backgroundPanel_.SetState("Hovered");
	} else {
		state_ = State::Normal;
		labelText_.SetState("Normal");
		backgroundPanel_.SetState("Normal");
	}

	// 背景パネルの更新
	backgroundPanel_.SetPosition(position_);
	backgroundPanel_.SetSize(size_);
	backgroundPanel_.Update();

	// ラベルテキストの更新（ボタン中央に配置）
	float textX = position_.x + size_.x * 0.5f;
	float textY = position_.y + size_.y * 0.5f;
	labelText_.SetPosition({ textX, textY });
	labelText_.SetVisible(isVisible_);
	labelText_.Update();
}

void UIButton::Draw() {
	if (!isVisible_) return;

	// 背景描画
	backgroundPanel_.Draw();

	// テキスト描画
	labelText_.Draw();
}

bool UIButton::IsMouseInside() const {
	HWND hwnd = FindWindowW(L"WindowClass", nullptr);
	if (!hwnd) return false;

	Input::MousePosition mousePos = Input::GetInstance()->GetMouseScreenPosition(hwnd);

	float mx = static_cast<float>(mousePos.x);
	float my = static_cast<float>(mousePos.y);

	return (mx >= position_.x && mx <= position_.x + size_.x &&
			my >= position_.y && my <= position_.y + size_.y);
}
