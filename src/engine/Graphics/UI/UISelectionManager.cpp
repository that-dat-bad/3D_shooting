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

	// 1. キーボード・ゲームパッド入力の処理
	if (inputMode_ == UIInputMode::KeyboardGamepad || inputMode_ == UIInputMode::Both) {
		// 次の項目へ
		if (input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_RIGHT) || input->TriggerKey(DIK_S) || input->TriggerKey(DIK_D)) {
			selectedIndex_ = (selectedIndex_ + 1) % count;
		}
		// 前の項目へ
		if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_LEFT) || input->TriggerKey(DIK_W) || input->TriggerKey(DIK_A)) {
			selectedIndex_ = (selectedIndex_ - 1 + count) % count;
		}
	}

	// 2. マウス入力の処理
	if (inputMode_ == UIInputMode::MousePointer || inputMode_ == UIInputMode::Both) {
		Input::MouseMove move = input->GetMouseMove();
		// マウスが動いた場合、カーソル下にあるボタンにフォーカスを移動する
		if (move.lX != 0 || move.lY != 0) {
			for (int i = 0; i < count; ++i) {
				if (buttons_[i]->IsMouseInside()) {
					selectedIndex_ = i;
					break;
				}
			}
		}
	}

	// 3. 各ボタンへ状態を反映して更新
	for (int i = 0; i < count; ++i) {
		buttons_[i]->SetSelected(i == selectedIndex_);
		buttons_[i]->Update();
	}

	// 4. 決定操作（クリック・Enter/Space）の処理
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
			// マウスクリックの場合は、現在ホバーしているボタンをクリックした時のみ発火
			if (buttons_[selectedIndex_]->IsMouseInside()) {
				isTriggered = true;
			}
		}
	}

	// 発火
	if (isTriggered && selectedIndex_ >= 0 && selectedIndex_ < count) {
		buttons_[selectedIndex_]->Click();
	}
}
