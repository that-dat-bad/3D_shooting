#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "UISelectionManager.h"
#include "../../io/Input.h"

void UISelectionManager::AddButton(UIButton* button) {
	if (button) {
		buttons_.push_back(button);
	}
}

void UISelectionManager::Clear() {
	buttons_.clear();
	selectedIndex_ = 0;
}

void UISelectionManager::Update() {
	if (buttons_.empty()) return;

	Input* input = Input::GetInstance();
	int count = static_cast<int>(buttons_.size());

	// キーボード・ゲームパッド入力
	if (inputMode_ == UIInputMode::KeyboardGamepad || inputMode_ == UIInputMode::Both) {
		if (input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_RIGHT) || input->TriggerKey(DIK_S) || input->TriggerKey(DIK_D)) {
			selectedIndex_ = (selectedIndex_ + 1) % count;
		}
		if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_LEFT) || input->TriggerKey(DIK_W) || input->TriggerKey(DIK_A)) {
			selectedIndex_ = (selectedIndex_ - 1 + count) % count;
		}
	}

	// マウス入力
	if (inputMode_ == UIInputMode::MousePointer || inputMode_ == UIInputMode::Both) {
		Input::MouseMove move = input->GetMouseMove();
		// カーソル下のボタンを選択
		if (move.lX != 0 || move.lY != 0) {
			for (int i = 0; i < count; ++i) {
				if (buttons_[i]->IsMouseInside()) {
					selectedIndex_ = i;
					break;
				}
			}
		}
	}

	// ボタンの更新
	for (int i = 0; i < count; ++i) {
		buttons_[i]->SetSelected(i == selectedIndex_);
		buttons_[i]->Update();
	}

	// 決定操作
	bool isTriggered = false;
	
	// キーボードでの決定
	if (inputMode_ == UIInputMode::KeyboardGamepad || inputMode_ == UIInputMode::Both) {
		if (input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE)) {
			isTriggered = true;
		}
	}
	
	// マウスでの決定
	if (inputMode_ == UIInputMode::MousePointer || inputMode_ == UIInputMode::Both) {
		if (input->TriggerMouse(0)) {
			if (buttons_[selectedIndex_]->IsMouseInside()) {
				isTriggered = true;
			}
		}
	}

	if (isTriggered && selectedIndex_ >= 0 && selectedIndex_ < count) {
		buttons_[selectedIndex_]->Click();
	}
}
