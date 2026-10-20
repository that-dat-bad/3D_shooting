#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "OSSceneBase.h"
#include "../../engine/io/Input.h"

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

void OSSceneBase::InitializeOS(SpriteCommon* spriteCommon, PseudoOS::Theme theme,
	const std::string& osName, const std::string& clearance,
	const std::string& startMenuTitle,
	const Vector4& bgPanelColor)
{
	backgroundPanel_.Initialize(spriteCommon);
	backgroundPanel_.SetPosition({ 0.0f, 0.0f });
	backgroundPanel_.SetSize({ kScreenWidth, kScreenHeight });
	backgroundPanel_.SetBackgroundColor(bgPanelColor);

	pseudoOS_.Initialize(spriteCommon, theme);
	pseudoOS_.SetOSName(osName);
	pseudoOS_.SetSecurityClearance(clearance);
	pseudoOS_.SetStartMenuTitle(startMenuTitle);

	isUIInitialized_ = true;
}

void OSSceneBase::UpdateOS(float dt, float fadeInSpeed) {
	animTimer_ += dt;

	if (fadeInAlpha_ < 1.0f) {
		fadeInAlpha_ += fadeInSpeed * dt;
		if (fadeInAlpha_ > 1.0f) fadeInAlpha_ = 1.0f;
	}

	if (btnClickFlashTimer_ > 0.0f) {
		btnClickFlashTimer_ -= dt;
		if (btnClickFlashTimer_ < 0.0f) btnClickFlashTimer_ = 0.0f;
	}

	// 疑似OSデスクトップ環境更新（入力自動取得＆アニメーション更新）
	pseudoOS_.Update(dt);

	// キーボード/ゲームパッドの選択操作時のフォーカス維持
	Input* input = Input::GetInstance();
	if (input) {
		int prevIndex = selectionManager_.GetSelectedIndex();
		selectionManager_.Update();
		int currIndex = selectionManager_.GetSelectedIndex();

		if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_DOWN) ||
			input->TriggerKey(DIK_LEFT) || input->TriggerKey(DIK_RIGHT) ||
			input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) ||
			currIndex != prevIndex)
		{
			pseudoOS_.BringWindowToFront(0);
		}
	}
}

void OSSceneBase::DrawOSBackground() {
	backgroundPanel_.Draw();
}

void OSSceneBase::DrawOSDesktop() {
	pseudoOS_.Draw();
}

void OSSceneBase::SetOSCursorVisible(bool visible) {
	isCursorShown_ = visible;
#ifdef USE_IMGUI
	ImGui::SetMouseCursor(visible ? ImGuiMouseCursor_Arrow : ImGuiMouseCursor_None);
#endif
	HWND hwnd = Input::GetInstance() ? Input::GetInstance()->GetHwnd() : nullptr;
	bool isForeground = (hwnd && ::GetForegroundWindow() == hwnd);
	if (visible || !isForeground) {
		while (ShowCursor(TRUE) < 0) {}
		::SetCursor(LoadCursor(nullptr, IDC_ARROW));
	} else {
		while (ShowCursor(FALSE) >= 0) {}
		::SetCursor(nullptr);
	}
}
