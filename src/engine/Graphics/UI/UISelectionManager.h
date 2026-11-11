#pragma once
#include "UIButton.h"
#include <vector>

enum class UIInputMode {
	KeyboardGamepad,
	MousePointer,
	Both
};

// UI選択管理
class UISelectionManager {
public:
	UISelectionManager() = default;
	~UISelectionManager() = default;

	// ボタンの登録
	// button: 登録するボタン
	void AddButton(UIButton* button);

	// ボタンのクリア
	void Clear();

	void SetInputMode(UIInputMode mode) { inputMode_ = mode; }
	UIInputMode GetInputMode() const { return inputMode_; }

	int GetSelectedIndex() const { return selectedIndex_; }
	void SetSelectedIndex(int index) {
		if (!buttons_.empty() && index >= 0 && index < static_cast<int>(buttons_.size())) {
			selectedIndex_ = index;
		}
	}

	// 更新
	void Update();

private:
	std::vector<UIButton*> buttons_;
	UIInputMode inputMode_ = UIInputMode::Both;
	int selectedIndex_ = 0;
};
