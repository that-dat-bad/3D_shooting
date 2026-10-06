
#include "Input.h"
#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#include <cassert>


std::unique_ptr<Input> Input::instance_ = nullptr;

Input* Input::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique<Input>();
	}
	return instance_.get();
}

void Input::Initialize(HINSTANCE hInstance,HWND hwnd)
{
	HRESULT result;



	//インスタンスの生成
	result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput_, nullptr);
	assert(SUCCEEDED(result));

	//キーボードデバイスの生成
	result = directInput_->CreateDevice(GUID_SysKeyboard, &keyboard_, NULL);
	assert(SUCCEEDED(result));

	//入力データのセット
	result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result));

	//排他制御レベルのセット
	result = keyboard_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));



	//マウスデバイスの生成
	result = directInput_->CreateDevice(GUID_SysMouse, &mouse_, NULL);
	assert(SUCCEEDED(result));

	//入力データのセット
	result = mouse_->SetDataFormat(&c_dfDIMouse2);
	assert(SUCCEEDED(result));

	//排他制御レベルのセット
	mouse_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(result));

	hwnd_ = hwnd;
}

void Input::Update()
{
	//キー入力を保存
	memcpy(preKeys_, keys_, sizeof(keys_));
	memcpy(&preMouseState_, &mouseState_, sizeof(mouseState_));

	// 入力取得
	keyboard_->Acquire();
	keyboard_->GetDeviceState(sizeof(keys_), keys_);
	
	HRESULT hr = mouse_->Acquire();
	hr = mouse_->GetDeviceState(sizeof(mouseState_), &mouseState_);
	if (FAILED(hr)) {
		mouse_->Acquire();
		mouse_->GetDeviceState(sizeof(mouseState_), &mouseState_);
	}

	// Win32 API のマウスボタン状態も統合（フォーカス状態やDirectInput一時ロスト時にも確実に検出）
	if ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) {
		mouseState_.rgbButtons[0] = 0x80;
	}
	if ((::GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0) {
		mouseState_.rgbButtons[1] = 0x80;
	}
	if ((::GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0) {
		mouseState_.rgbButtons[2] = 0x80;
	}

	// カーソルロック中は毎フレーム、OSカーソルをウィンドウ中央に戻す
	if (cursorLocked_ && hwnd_) {
		RECT rect;
		::GetClientRect(hwnd_, &rect);
		POINT center;
		center.x = (rect.right - rect.left) / 2;
		center.y = (rect.bottom - rect.top) / 2;
		::ClientToScreen(hwnd_, &center);
		::SetCursorPos(center.x, center.y);
	}

}

Input::MouseMove Input::GetMouseMove() {
	MouseMove tmp;
	tmp.lX = mouseState_.lX;
	tmp.lY = mouseState_.lY;
	tmp.lZ = mouseState_.lZ;
	return tmp;
}

bool Input::PushMouse(int buttonNumber)
{
	if (mouseState_.rgbButtons[buttonNumber])
	{
		return true;
	}
	return false;
}

bool Input::TriggerMouse(int buttonNumber)
{
	if (!preMouseState_.rgbButtons[buttonNumber] && mouseState_.rgbButtons[buttonNumber])
	{
		return true;
	}
	return false;
}

Input::MousePosition Input::GetMouseScreenPosition(HWND hwnd)
{
	POINT pt;
	::GetCursorPos(&pt);
	HWND targetHwnd = hwnd ? hwnd : hwnd_;
	if (targetHwnd)
	{
		::ScreenToClient(targetHwnd, &pt);
	}
	return { pt.x, pt.y };
}

bool Input::PushKey(BYTE keyNumber)
{
	if (keys_[keyNumber])
	{
		return true;
	}
	return false;
}

bool Input::TriggerKey(BYTE keyNumber)
{
	if (!preKeys_[keyNumber]&&keys_[keyNumber])
	{
		return true;
	}

	return false;
}

void Input::LockCursor()
{
	if (!cursorLocked_) {
		cursorLocked_ = true;
		while (::ShowCursor(FALSE) >= 0) {}
	}
}

void Input::UnlockCursor()
{
	if (cursorLocked_) {
		cursorLocked_ = false;
		while (::ShowCursor(TRUE) < 0) {}
	}
}
