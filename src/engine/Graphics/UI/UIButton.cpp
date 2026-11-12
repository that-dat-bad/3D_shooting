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
	promptBlinkTimer_ += 1.0f / 60.0f;

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
	backgroundPanel_.SetAnchorPoint(anchorPoint_);
	backgroundPanel_.Update();

	// ラベルテキストの更新（アライメントに応じた配置 + オフセット微調整）
	float btnLeft = position_.x - size_.x * anchorPoint_.x;
	float textX = 0.0f;
	float textY = position_.y + size_.y * (0.5f - anchorPoint_.y) + textOffset_.y;

	if (textAlignment_ == TextAlignment::Left) {
		textX = btnLeft + leftPadding_ + textOffset_.x;
		labelText_.SetAnchorPoint({ 0.0f, 0.5f });
	} else {
		textX = position_.x + size_.x * (0.5f - anchorPoint_.x) + textOffset_.x;
		labelText_.SetAnchorPoint({ 0.5f, 0.5f });
	}

	labelText_.SetPosition({ textX, textY });
	labelText_.SetVisible(isVisible_);
	labelText_.Update();
}

void UIButton::Draw() {
	if (!isVisible_) return;

	// 背景描画
	backgroundPanel_.Draw();

	// テキスト描画（本体テキストは固定ライン配置・ガタつきなし）
	labelText_.Draw();

	// 選択時のプロンプト装飾（左側余白の >> と末尾の点滅カーソル _）
	if (isSelected_ && enablePromptDecorators_) {
		TextRenderer* tr = TextRenderer::GetInstance();
		if (tr) {
			const std::string& fontName = labelText_.GetFontName();
			Vector2 textSize = tr->CalculateTextSize(fontName, label_, fontSize_);

			float btnLeft = position_.x - size_.x * anchorPoint_.x;
			float textCenterY = position_.y + size_.y * (0.5f - anchorPoint_.y) + textOffset_.y;

			// 1. 左側余白の ">>"（テキストの左手前に端正に配置し、重なりを完全防止）
			float promptX = 0.0f;
			if (textAlignment_ == TextAlignment::Left) {
				promptX = btnLeft + (std::max)(8.0f, leftPadding_ - 24.0f);
			} else {
				float textCenterX = position_.x + size_.x * (0.5f - anchorPoint_.x) + textOffset_.x;
				promptX = textCenterX - textSize.x * 0.5f - 24.0f;
			}
			tr->Print(fontName, ">>", promptX, textCenterY, fontSize_, selectedTextColor_, { 0.0f, 0.5f });

			// 2. テキスト末尾の点滅カーソル "_"（0.36秒周期で点滅）
			bool blink = (std::fmod(promptBlinkTimer_, 0.36f) < 0.18f);
			if (blink) {
				float cursorX = 0.0f;
				if (textAlignment_ == TextAlignment::Left) {
					cursorX = btnLeft + leftPadding_ + textOffset_.x + textSize.x + 6.0f;
				} else {
					float textCenterX = position_.x + size_.x * (0.5f - anchorPoint_.x) + textOffset_.x;
					cursorX = textCenterX + textSize.x * 0.5f + 6.0f;
				}
				tr->Print(fontName, "_", cursorX, textCenterY, fontSize_, selectedTextColor_, { 0.0f, 0.5f });
			}
		}
	}
}

bool UIButton::IsMouseInside() const {
	Input* input = Input::GetInstance();
	HWND hwnd = input->GetHwnd();
	Input::MousePosition mousePos = input->GetMouseScreenPosition();

	float mx = static_cast<float>(mousePos.x);
	float my = static_cast<float>(mousePos.y);

	if (hwnd) {
		RECT rc;
		if (::GetClientRect(hwnd, &rc) && (rc.right - rc.left > 0) && (rc.bottom - rc.top > 0)) {
			float actualW = static_cast<float>(rc.right - rc.left);
			float actualH = static_cast<float>(rc.bottom - rc.top);
			mx *= (static_cast<float>(WinApp::kClientWidth) / actualW);
			my *= (static_cast<float>(WinApp::kClientHeight) / actualH);
		}
	}

	float left = position_.x - size_.x * anchorPoint_.x;
	float right = left + size_.x;
	float top = position_.y - size_.y * anchorPoint_.y;
	float bottom = top + size_.y;

	return (mx >= left && mx <= right && my >= top && my <= bottom);
}
