#include "WinApp.h"
#include "Windows.h"
#ifdef USE_IMGUI
#include "../../../external/imgui/imgui_impl_win32.h" 
#endif // USE_IMGUI
#pragma comment(lib,"winmm.lib")

#ifdef USE_IMGUI
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif // USE_IMGUI



// ウィンドウプロシージャ
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif // USE_IMGUI

	switch (msg) {
	case WM_ACTIVATE:
		if (LOWORD(wparam) == WA_INACTIVE) {
			// 他のアプリにフォーカスが移った時: カーソル制限を全解除し、OS標準カーソルを確実に復元
			while (::ShowCursor(TRUE) < 0) {}
			::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
			::ClipCursor(nullptr);
		}
		break;
	case WM_SETCURSOR:
		// クライアント領域外（タイトルバーやウィンドウ枠）または非アクティブ時は標準カーソルを表示
		if (LOWORD(lparam) != HTCLIENT || ::GetForegroundWindow() != hwnd) {
			::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void WinApp::Initialize(const std::wstring& title, const std::wstring& version) {
	//システムタイマーの分解能を上げる
	timeBeginPeriod(1);

	CoInitializeEx(0, COINIT_MULTITHREADED);


	wc_.lpfnWndProc = WindowProc;
	wc_.lpszClassName = L"CG2WindowClass";
	wc_.hInstance = GetModuleHandle(nullptr);
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc_);

	RECT wrc = { 0, 0, kClientWidth, kClientHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	std::wstring windowTitle = title + L" by Dawn Engine ver." + version;

	hwnd_ = CreateWindow(
		wc_.lpszClassName,
		windowTitle.c_str(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc_.hInstance,
		nullptr
	);

	ShowWindow(hwnd_, SW_SHOW);
}

bool WinApp::ProcessMessage() {
	MSG msg{};
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (msg.message == WM_QUIT) {
		return true;
	}

	return false;
}

void WinApp::Finalize() {
	UnregisterClass(wc_.lpszClassName, wc_.hInstance);
	CoUninitialize();
}
