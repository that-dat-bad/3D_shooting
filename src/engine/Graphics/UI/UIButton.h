#pragma once
#include "UIElement.h"
#include "UIText.h"
#include "UIPanel.h"
#include <string>
#include <functional>
#include <memory>
#include "../Sprite/Sprite.h"

class SpriteCommon;

/// @brief ボタンUI要素。マウスホバー・クリック判定付き。
class UIButton : public UIElement {
public:
	/// @brief ボタンの状態
	enum class State {
		Normal,		///< 通常
		Hovered,	///< マウスホバー中
		Pressed,	///< 押下中
		Selected,	///< キーボード選択中
	};

	/// @brief 初期化
	/// @param spriteCommon SpriteCommonのポインタ
	/// @param label ボタンに表示するテキスト
	/// @param fontSize フォントサイズ
	void Initialize(SpriteCommon* spriteCommon, const std::string& label, float fontSize = 28.0f);

	void Update() override;
	void Draw() override;

	/// @brief クリック時のコールバック設定
	void SetOnClick(std::function<void()> callback) { onClick_ = callback; }
	
	/// @brief 強制的にクリックイベントを発火する
	void Click() { if (onClick_) onClick_(); }

	/// @brief ボタンラベルのUIText要素を取得（エディタ連携用）
	UIText* GetLabelText() { return &labelText_; }
	const UIText* GetLabelText() const { return &labelText_; }

	/// @brief ボタンラベルの設定
	void SetLabel(const std::string& label);
	const std::string& GetLabel() const;

	/// @brief キーボード選択状態の設定（タイトルメニュー等で使用）
	void SetSelected(bool selected) { isSelected_ = selected; }
	bool IsSelected() const { return isSelected_; }

	/// @brief ボタンが押されたかチェック（トリガー判定）
	bool IsClicked() const { return isClicked_; }

	/// @brief 現在の状態を取得
	State GetState() const { return state_; }

	// 色設定
	void SetNormalColor(const Vector4& color) { normalColor_ = color; }
	void SetHoverColor(const Vector4& color) { hoverColor_ = color; }
	void SetSelectedColor(const Vector4& color) { selectedColor_ = color; }
	void SetTextColor(const Vector4& color);

	/// @brief マウスが矩形の範囲内にいるかチェック
	bool IsMouseInside() const;

private:
	std::string label_;
	float fontSize_ = 28.0f;

	// ラベルテキスト要素
	UIText labelText_;

	// 背景パネル
	UIPanel backgroundPanel_;

	// 状態管理
	State state_ = State::Normal;
	bool isSelected_ = false;
	bool isClicked_ = false;
	std::function<void()> onClick_;

	// 色設定
	Vector4 normalColor_ = { 0.15f, 0.15f, 0.20f, 0.85f };
	Vector4 hoverColor_ = { 0.25f, 0.30f, 0.45f, 0.95f };
	Vector4 selectedColor_ = { 0.20f, 0.35f, 0.55f, 0.95f };
	Vector4 textColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };

	// アニメーション
	float hoverAlpha_ = 0.0f; // 0.0〜1.0のホバーブレンド値
};
