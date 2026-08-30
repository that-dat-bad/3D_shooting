#pragma once
#include "UIButton.h"
#include <vector>

enum class UIInputMode {
	KeyboardGamepad, // 矢印/WASD等でのカーソル移動専用
	MousePointer,    // マウスカーソルでのホバー・クリック専用
	Both             // 両方有効
};

class UISelectionManager {
public:
	UISelectionManager() = default;
	~UISelectionManager() = default;

	/// @brief ボタンの登録
	void AddButton(UIButton* button);

	/// @brief 管理対象ボタンのクリア
	void Clear();

	/// @brief 入力モードの設定
	void SetInputMode(UIInputMode mode) { inputMode_ = mode; }

	/// @brief 現在の入力モードの取得
	UIInputMode GetInputMode() const { return inputMode_; }

	/// @brief 毎フレームの更新処理（入力とフォーカスの反映）
	void Update();

private:
	std::vector<UIButton*> buttons_;
	UIInputMode inputMode_ = UIInputMode::Both;
	int selectedIndex_ = 0;
};
