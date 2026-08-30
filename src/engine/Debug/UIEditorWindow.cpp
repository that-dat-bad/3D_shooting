#include "UIEditorWindow.h"
#include "../Graphics/UI/UITextRegistry.h"
#include "../Graphics/UI/UIText.h"
#include "../Graphics/Text/FontManager.h"
#include <sstream>
#include <iomanip>
#include <cassert>

#pragma comment(lib, "comctl32.lib")

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// コントロールID定義
enum ControlIDs {
	IDC_COMBO_SELECT = 1001,
	IDC_BTN_REFRESH,
	IDC_EDIT_TEXT,
	IDC_COMBO_FONT,
	IDC_TRACK_FONTSIZE,
	IDC_EDIT_FONTSIZE,
	IDC_TRACK_POSX,
	IDC_EDIT_POSX,
	IDC_TRACK_POSY,
	IDC_EDIT_POSY,
	IDC_TRACK_ANCHORX,
	IDC_EDIT_ANCHORX,
	IDC_TRACK_ANCHORY,
	IDC_EDIT_ANCHORY,
	IDC_BTN_PICKCOLOR,
	IDC_TRACK_ALPHA,
	IDC_EDIT_ALPHA,
	IDC_CHECK_VISIBLE,
	IDC_COMBO_STATE,
	IDC_TRACK_STATESCALE,
	IDC_EDIT_STATESCALE,
	IDC_TRACK_STATEOFFSETY,
	IDC_EDIT_STATEOFFSETY,
	IDC_COMBO_MOTION,
	IDC_TRACK_MOTIONSPEED,
	IDC_EDIT_MOTIONSPEED,
	IDC_TRACK_MOTIONINTENSITY,
	IDC_EDIT_MOTIONINTENSITY,
	IDC_BTN_PREVIEWSTATE,
	IDC_BTN_SAVEINI,
	IDC_BTN_LOADINI,
	IDC_BTN_UNDO,
};

static const wchar_t* kClassName = L"NativeUIEditorWindowClass";

// ヘルパー: std::string -> std::wstring
static std::wstring Utf8ToWide(const std::string& str) {
	if (str.empty()) return L"";
	int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
	std::wstring wstr(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], sizeNeeded);
	return wstr;
}

// ヘルパー: std::wstring -> std::string
static std::string WideToUtf8(const std::wstring& wstr) {
	if (wstr.empty()) return "";
	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
	std::string str(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], sizeNeeded, nullptr, nullptr);
	return str;
}

UIEditorWindow* UIEditorWindow::GetInstance() {
	static UIEditorWindow instance;
	return &instance;
}

UIEditorWindow::~UIEditorWindow() {
	Finalize();
}

bool UIEditorWindow::Initialize(HWND parentHwnd) {
	HINSTANCE hInst = GetModuleHandle(nullptr);
	return Initialize(hInst, parentHwnd);
}

bool UIEditorWindow::Initialize(HINSTANCE hInstance, HWND parentHwnd) {
	parentHwnd_ = parentHwnd;

	hBgBrush_ = CreateSolidBrush(RGB(30, 32, 38));
	hEditBgBrush_ = CreateSolidBrush(RGB(42, 45, 52));

	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
	InitCommonControlsEx(&icex);

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = UIEditorWindow::WindowProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = hBgBrush_;
	wc.lpszClassName = kClassName;

	RegisterClassExW(&wc);

	int winW = 460;
	int winH = 780;

	RECT rcParent = {};
	if (parentHwnd_) {
		GetWindowRect(parentHwnd_, &rcParent);
	}

	int winX = rcParent.right > 0 ? (rcParent.right + 10) : 100;
	int winY = rcParent.top > 0 ? rcParent.top : 100;

	hwnd_ = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
		kClassName,
		L"Native UI Text & Motion Editor",
		WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		winX, winY, winW, winH,
		parentHwnd_,
		nullptr,
		hInstance,
		this
	);

	if (!hwnd_) {
		return false;
	}

	// Windows ダークモードタイトルバー適用
	BOOL darkMode = TRUE;
	DwmSetWindowAttribute(hwnd_, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));

	CreateControls();
	return true;
}

void UIEditorWindow::Update() {
	if (!hwnd_ || !IsWindow(hwnd_)) return;

	size_t curCount = UITextRegistry::GetInstance()->GetAll().size();
	if (curCount != registeredCountCache_) {
		registeredCountCache_ = curCount;
		RefreshTextList();
	}
}

void UIEditorWindow::Finalize() {
	if (hColorBrush_) {
		DeleteObject(hColorBrush_);
		hColorBrush_ = nullptr;
	}
	if (hBgBrush_) {
		DeleteObject(hBgBrush_);
		hBgBrush_ = nullptr;
	}
	if (hEditBgBrush_) {
		DeleteObject(hEditBgBrush_);
		hEditBgBrush_ = nullptr;
	}
	if (hwnd_ && IsWindow(hwnd_)) {
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}
}

void UIEditorWindow::Show(bool show) {
	if (!hwnd_) return;
	if (show) {
		ShowWindow(hwnd_, SW_SHOW);
		UpdateWindow(hwnd_);
		RefreshTextList();
	} else {
		// プレビュー解除
		UIText* curText = GetSelectedText();
		if (curText) {
			curText->ClearPreviewStateOverride();
		}
		isPreviewing_ = false;
		ShowWindow(hwnd_, SW_HIDE);
	}
}

void UIEditorWindow::ToggleVisible() {
	Show(!IsVisible());
}

bool UIEditorWindow::IsVisible() const {
	return hwnd_ && IsWindowVisible(hwnd_);
}

UIText* UIEditorWindow::GetSelectedText() const {
	if (selectedName_.empty()) return nullptr;
	const auto& map = UITextRegistry::GetInstance()->GetAll();
	auto it = map.find(selectedName_);
	if (it != map.end()) {
		return it->second;
	}
	return nullptr;
}

LRESULT CALLBACK UIEditorWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	UIEditorWindow* self = nullptr;
	if (msg == WM_NCCREATE) {
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		self = reinterpret_cast<UIEditorWindow*>(cs->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
	} else {
		self = reinterpret_cast<UIEditorWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

	if (self) {
		return self->HandleMessage(hwnd, msg, wParam, lParam);
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT UIEditorWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CLOSE:
		Show(false);
		return 0;

	case WM_ERASEBKGND: {
		HDC hdc = (HDC)wParam;
		RECT rc;
		GetClientRect(hwnd, &rc);
		FillRect(hdc, &rc, hBgBrush_);
		return 1;
	}

	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORBTN:
	case WM_CTLCOLORDLG: {
		HDC hdcStatic = (HDC)wParam;
		HWND hStatic = (HWND)lParam;
		if (hStatic == hColorSwatch_) {
			return (LRESULT)hColorBrush_;
		}
		SetTextColor(hdcStatic, RGB(225, 230, 240));
		SetBkColor(hdcStatic, RGB(30, 32, 38));
		SetBkMode(hdcStatic, TRANSPARENT);
		return (LRESULT)hBgBrush_;
	}

	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX: {
		HDC hdcEdit = (HDC)wParam;
		SetTextColor(hdcEdit, RGB(245, 245, 250));
		SetBkColor(hdcEdit, RGB(42, 45, 52));
		return (LRESULT)hEditBgBrush_;
	}

	case WM_COMMAND: {
		WORD wmId = LOWORD(wParam);
		WORD wmEvent = HIWORD(wParam);

		if (isUpdatingUI_) break;

		switch (wmId) {
		case IDC_COMBO_SELECT:
			if (wmEvent == CBN_SELCHANGE) {
				OnSelectionChanged();
			}
			break;

		case IDC_BTN_REFRESH:
			RefreshTextList();
			break;

		case IDC_EDIT_TEXT:
			if (wmEvent == EN_CHANGE) {
				ApplyFieldsToText();
			}
			break;

		case IDC_COMBO_FONT:
			if (wmEvent == CBN_SELCHANGE) {
				ApplyFieldsToText();
			}
			break;

		case IDC_COMBO_STATE:
			if (wmEvent == CBN_SELCHANGE) {
				OnStateSelectionChanged();
			}
			break;

		case IDC_COMBO_MOTION:
			if (wmEvent == CBN_SELCHANGE) {
				ApplyFieldsToText();
			}
			break;

		case IDC_EDIT_FONTSIZE:
		case IDC_EDIT_POSX:
		case IDC_EDIT_POSY:
		case IDC_EDIT_ANCHORX:
		case IDC_EDIT_ANCHORY:
		case IDC_EDIT_ALPHA:
		case IDC_EDIT_STATESCALE:
		case IDC_EDIT_STATEOFFSETY:
		case IDC_EDIT_MOTIONSPEED:
		case IDC_EDIT_MOTIONINTENSITY:
			if (wmEvent == EN_CHANGE) {
				ApplyFieldsToText();
			}
			break;

		case IDC_BTN_PICKCOLOR:
			OpenColorPicker();
			break;

		case IDC_CHECK_VISIBLE:
			ApplyFieldsToText();
			break;

		case IDC_BTN_PREVIEWSTATE: {
			UIText* text = GetSelectedText();
			if (text) {
				isPreviewing_ = !isPreviewing_;
				if (isPreviewing_) {
					text->SetPreviewStateOverride(selectedState_);
					SetWindowTextW(hBtnPreviewState_, L"Stop Preview");
				} else {
					text->ClearPreviewStateOverride();
					SetWindowTextW(hBtnPreviewState_, L"Preview State");
				}
			}
			break;
		}

		case IDC_BTN_SAVEINI:
			SaveToIni("assets/ui/text_styles.ini");
			break;

		case IDC_BTN_LOADINI:
			LoadFromIni("assets/ui/text_styles.ini");
			break;

		case IDC_BTN_UNDO: {
			UIText* text = GetSelectedText();
			if (text && snapshot_.isValid) {
				RestoreSnapshot(text);
				PopulateFieldsFromText(text);
			}
			break;
		}
		}
		return 0;
	}

	case WM_HSCROLL: {
		if (isUpdatingUI_) break;
		HWND hTrack = (HWND)lParam;
		if (!hTrack) break;

		isUpdatingUI_ = true;

		if (hTrack == hTrackFontSize_) {
			int val = (int)SendMessageW(hTrackFontSize_, TBM_GETPOS, 0, 0);
			SetWindowTextW(hEditFontSize_, std::to_wstring(val).c_str());
		} else if (hTrack == hTrackPosX_) {
			int val = (int)SendMessageW(hTrackPosX_, TBM_GETPOS, 0, 0);
			SetWindowTextW(hEditPosX_, std::to_wstring(val).c_str());
		} else if (hTrack == hTrackPosY_) {
			int val = (int)SendMessageW(hTrackPosY_, TBM_GETPOS, 0, 0);
			SetWindowTextW(hEditPosY_, std::to_wstring(val).c_str());
		} else if (hTrack == hTrackAnchorX_) {
			int val = (int)SendMessageW(hTrackAnchorX_, TBM_GETPOS, 0, 0);
			std::wstringstream ss;
			ss << std::fixed << std::setprecision(2) << (val / 100.0f);
			SetWindowTextW(hEditAnchorX_, ss.str().c_str());
		} else if (hTrack == hTrackAnchorY_) {
			int val = (int)SendMessageW(hTrackAnchorY_, TBM_GETPOS, 0, 0);
			std::wstringstream ss;
			ss << std::fixed << std::setprecision(2) << (val / 100.0f);
			SetWindowTextW(hEditAnchorY_, ss.str().c_str());
		} else if (hTrack == hTrackAlpha_) {
			int val = (int)SendMessageW(hTrackAlpha_, TBM_GETPOS, 0, 0);
			std::wstringstream ss;
			ss << std::fixed << std::setprecision(2) << (val / 100.0f);
			SetWindowTextW(hEditAlpha_, ss.str().c_str());
		} else if (hTrack == hTrackStateScale_) {
			int val = (int)SendMessageW(hTrackStateScale_, TBM_GETPOS, 0, 0);
			std::wstringstream ss;
			ss << std::fixed << std::setprecision(2) << (val / 100.0f);
			SetWindowTextW(hEditStateScale_, ss.str().c_str());
		} else if (hTrack == hTrackStateOffsetY_) {
			int val = (int)SendMessageW(hTrackStateOffsetY_, TBM_GETPOS, 0, 0);
			SetWindowTextW(hEditStateOffsetY_, std::to_wstring(val - 50).c_str());
		} else if (hTrack == hTrackMotionSpeed_) {
			int val = (int)SendMessageW(hTrackMotionSpeed_, TBM_GETPOS, 0, 0);
			std::wstringstream ss;
			ss << std::fixed << std::setprecision(1) << (val / 10.0f);
			SetWindowTextW(hEditMotionSpeed_, ss.str().c_str());
		} else if (hTrack == hTrackMotionIntensity_) {
			int val = (int)SendMessageW(hTrackMotionIntensity_, TBM_GETPOS, 0, 0);
			std::wstringstream ss;
			ss << std::fixed << std::setprecision(2) << (val / 100.0f);
			SetWindowTextW(hEditMotionIntensity_, ss.str().c_str());
		}

		isUpdatingUI_ = false;
		ApplyFieldsToText();
		return 0;
	}
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void UIEditorWindow::CreateControls() {
	HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd_, GWLP_HINSTANCE);
	HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

	int curY = 10;
	int padX = 15;
	int width = 415;

	auto CreateLabel = [&](const wchar_t* text, int x, int y, int w, int h) {
		HWND lbl = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT, x, y, w, h, hwnd_, nullptr, hInst, nullptr);
		SendMessageW(lbl, WM_SETFONT, (WPARAM)hFont, TRUE);
		return lbl;
	};

	// --- 1. ターゲット選択グループ ---
	hGroupSelect_ = CreateWindowW(L"BUTTON", L" 1. Target UI Element ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, padX, curY, width, 55, hwnd_, nullptr, hInst, nullptr);
	SendMessageW(hGroupSelect_, WM_SETFONT, (WPARAM)hFont, TRUE);

	hComboSelect_ = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, padX + 15, curY + 20, 290, 200, hwnd_, (HMENU)IDC_COMBO_SELECT, hInst, nullptr);
	SendMessageW(hComboSelect_, WM_SETFONT, (WPARAM)hFont, TRUE);

	hBtnRefresh_ = CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, padX + 315, curY + 19, 85, 25, hwnd_, (HMENU)IDC_BTN_REFRESH, hInst, nullptr);
	SendMessageW(hBtnRefresh_, WM_SETFONT, (WPARAM)hFont, TRUE);

	curY += 62;

	// --- 2. テキスト内容グループ ---
	hGroupText_ = CreateWindowW(L"BUTTON", L" 2. Content & Font ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, padX, curY, width, 75, hwnd_, nullptr, hInst, nullptr);
	SendMessageW(hGroupText_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Text:", padX + 15, curY + 20, 40, 20);
	hEditText_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, padX + 60, curY + 18, 335, 22, hwnd_, (HMENU)IDC_EDIT_TEXT, hInst, nullptr);
	SendMessageW(hEditText_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Font:", padX + 15, curY + 46, 40, 20);
	hComboFont_ = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, padX + 60, curY + 44, 150, 150, hwnd_, (HMENU)IDC_COMBO_FONT, hInst, nullptr);
	SendMessageW(hComboFont_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Size:", padX + 220, curY + 46, 35, 20);
	hTrackFontSize_ = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_NOTICKS, padX + 255, curY + 44, 95, 22, hwnd_, (HMENU)IDC_TRACK_FONTSIZE, hInst, nullptr);
	SendMessageW(hTrackFontSize_, TBM_SETRANGE, TRUE, MAKELPARAM(8, 200));
	SendMessageW(hTrackFontSize_, TBM_SETPOS, TRUE, 32);
	hEditFontSize_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"32", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_NUMBER, padX + 355, curY + 44, 40, 22, hwnd_, (HMENU)IDC_EDIT_FONTSIZE, hInst, nullptr);
	SendMessageW(hEditFontSize_, WM_SETFONT, (WPARAM)hFont, TRUE);

	curY += 82;

	// --- 3. 配置・アンカーグループ ---
	hGroupTransform_ = CreateWindowW(L"BUTTON", L" 3. Position & Anchor ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, padX, curY, width, 75, hwnd_, nullptr, hInst, nullptr);
	SendMessageW(hGroupTransform_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Pos X:", padX + 15, curY + 20, 42, 20);
	hTrackPosX_ = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_NOTICKS, padX + 60, curY + 18, 95, 22, hwnd_, (HMENU)IDC_TRACK_POSX, hInst, nullptr);
	SendMessageW(hTrackPosX_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1920));
	hEditPosX_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0", WS_CHILD | WS_VISIBLE | ES_CENTER, padX + 160, curY + 18, 45, 22, hwnd_, (HMENU)IDC_EDIT_POSX, hInst, nullptr);
	SendMessageW(hEditPosX_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Pos Y:", padX + 215, curY + 20, 42, 20);
	hTrackPosY_ = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_NOTICKS, padX + 260, curY + 18, 90, 22, hwnd_, (HMENU)IDC_TRACK_POSY, hInst, nullptr);
	SendMessageW(hTrackPosY_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1080));
	hEditPosY_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0", WS_CHILD | WS_VISIBLE | ES_CENTER, padX + 355, curY + 18, 40, 22, hwnd_, (HMENU)IDC_EDIT_POSY, hInst, nullptr);
	SendMessageW(hEditPosY_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Anc X:", padX + 15, curY + 46, 42, 20);
	hTrackAnchorX_ = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_NOTICKS, padX + 60, curY + 44, 95, 22, hwnd_, (HMENU)IDC_TRACK_ANCHORX, hInst, nullptr);
	SendMessageW(hTrackAnchorX_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
	hEditAnchorX_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0.00", WS_CHILD | WS_VISIBLE | ES_CENTER, padX + 160, curY + 44, 45, 22, hwnd_, (HMENU)IDC_EDIT_ANCHORX, hInst, nullptr);
	SendMessageW(hEditAnchorX_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Anc Y:", padX + 215, curY + 46, 42, 20);
	hTrackAnchorY_ = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_NOTICKS, padX + 260, curY + 44, 90, 22, hwnd_, (HMENU)IDC_TRACK_ANCHORY, hInst, nullptr);
	SendMessageW(hTrackAnchorY_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
	hEditAnchorY_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0.00", WS_CHILD | WS_VISIBLE | ES_CENTER, padX + 355, curY + 44, 40, 22, hwnd_, (HMENU)IDC_EDIT_ANCHORY, hInst, nullptr);
	SendMessageW(hEditAnchorY_, WM_SETFONT, (WPARAM)hFont, TRUE);

	curY += 82;

	// --- 4. 外観・カラー・可視性 ---
	hGroupAppearance_ = CreateWindowW(L"BUTTON", L" 4. Base Color & Visibility ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, padX, curY, width, 55, hwnd_, nullptr, hInst, nullptr);
	SendMessageW(hGroupAppearance_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Color:", padX + 15, curY + 22, 40, 20);
	hColorSwatch_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOTIFY, padX + 60, curY + 20, 35, 24, hwnd_, nullptr, hInst, nullptr);
	hBtnPickColor_ = CreateWindowW(L"BUTTON", L"Pick...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, padX + 100, curY + 20, 60, 24, hwnd_, (HMENU)IDC_BTN_PICKCOLOR, hInst, nullptr);
	SendMessageW(hBtnPickColor_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Alpha:", padX + 175, curY + 22, 40, 20);
	hTrackAlpha_ = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_NOTICKS, padX + 215, curY + 20, 75, 22, hwnd_, (HMENU)IDC_TRACK_ALPHA, hInst, nullptr);
	SendMessageW(hTrackAlpha_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
	SendMessageW(hTrackAlpha_, TBM_SETPOS, TRUE, 100);
	hEditAlpha_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"1.00", WS_CHILD | WS_VISIBLE | ES_CENTER, padX + 295, curY + 20, 40, 22, hwnd_, (HMENU)IDC_EDIT_ALPHA, hInst, nullptr);
	SendMessageW(hEditAlpha_, WM_SETFONT, (WPARAM)hFont, TRUE);

	hCheckVisible_ = CreateWindowW(L"BUTTON", L"Visible", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, padX + 345, curY + 22, 60, 20, hwnd_, (HMENU)IDC_CHECK_VISIBLE, hInst, nullptr);
	SendMessageW(hCheckVisible_, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessageW(hCheckVisible_, BM_SETCHECK, BST_CHECKED, 0);

	curY += 62;

	// --- 5. ステート＆モーション設定（Tween, Pulse, Floating, Shake）---
	hGroupMotion_ = CreateWindowW(L"BUTTON", L" 5. State & Motion (Hover, Condition, Anim) ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, padX, curY, width, 140, hwnd_, nullptr, hInst, nullptr);
	SendMessageW(hGroupMotion_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"State:", padX + 15, curY + 22, 40, 20);
	hComboState_ = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, padX + 60, curY + 20, 120, 150, hwnd_, (HMENU)IDC_COMBO_STATE, hInst, nullptr);
	SendMessageW(hComboState_, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessageW(hComboState_, CB_ADDSTRING, 0, (LPARAM)L"Normal");
	SendMessageW(hComboState_, CB_ADDSTRING, 0, (LPARAM)L"Hovered");
	SendMessageW(hComboState_, CB_ADDSTRING, 0, (LPARAM)L"Selected");
	SendMessageW(hComboState_, CB_ADDSTRING, 0, (LPARAM)L"Pressed");
	SendMessageW(hComboState_, CB_ADDSTRING, 0, (LPARAM)L"LowHP");
	SendMessageW(hComboState_, CB_ADDSTRING, 0, (LPARAM)L"Warning");
	SendMessageW(hComboState_, CB_ADDSTRING, 0, (LPARAM)L"Complete");
	SendMessageW(hComboState_, CB_SETCURSEL, 0, 0);

	hBtnPreviewState_ = CreateWindowW(L"BUTTON", L"Preview State", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, padX + 190, curY + 19, 105, 25, hwnd_, (HMENU)IDC_BTN_PREVIEWSTATE, hInst, nullptr);
	SendMessageW(hBtnPreviewState_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Scale:", padX + 15, curY + 50, 42, 20);
	hTrackStateScale_ = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_NOTICKS, padX + 60, curY + 48, 95, 22, hwnd_, (HMENU)IDC_TRACK_STATESCALE, hInst, nullptr);
	SendMessageW(hTrackStateScale_, TBM_SETRANGE, TRUE, MAKELPARAM(50, 200));
	SendMessageW(hTrackStateScale_, TBM_SETPOS, TRUE, 100);
	hEditStateScale_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"1.00", WS_CHILD | WS_VISIBLE | ES_CENTER, padX + 160, curY + 48, 45, 22, hwnd_, (HMENU)IDC_EDIT_STATESCALE, hInst, nullptr);
	SendMessageW(hEditStateScale_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Off Y:", padX + 215, curY + 50, 42, 20);
	hTrackStateOffsetY_ = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_NOTICKS, padX + 260, curY + 48, 90, 22, hwnd_, (HMENU)IDC_TRACK_STATEOFFSETY, hInst, nullptr);
	SendMessageW(hTrackStateOffsetY_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
	SendMessageW(hTrackStateOffsetY_, TBM_SETPOS, TRUE, 50); // 50 = 0px offset
	hEditStateOffsetY_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0", WS_CHILD | WS_VISIBLE | ES_CENTER, padX + 355, curY + 48, 40, 22, hwnd_, (HMENU)IDC_EDIT_STATEOFFSETY, hInst, nullptr);
	SendMessageW(hEditStateOffsetY_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Motion:", padX + 15, curY + 78, 45, 20);
	hComboMotion_ = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, padX + 60, curY + 76, 120, 150, hwnd_, (HMENU)IDC_COMBO_MOTION, hInst, nullptr);
	SendMessageW(hComboMotion_, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessageW(hComboMotion_, CB_ADDSTRING, 0, (LPARAM)L"None");
	SendMessageW(hComboMotion_, CB_ADDSTRING, 0, (LPARAM)L"Pulse");
	SendMessageW(hComboMotion_, CB_ADDSTRING, 0, (LPARAM)L"Floating");
	SendMessageW(hComboMotion_, CB_ADDSTRING, 0, (LPARAM)L"Shake");
	SendMessageW(hComboMotion_, CB_SETCURSEL, 0, 0);

	CreateLabel(L"Speed:", padX + 195, curY + 78, 42, 20);
	hTrackMotionSpeed_ = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_NOTICKS, padX + 240, curY + 76, 110, 22, hwnd_, (HMENU)IDC_TRACK_MOTIONSPEED, hInst, nullptr);
	SendMessageW(hTrackMotionSpeed_, TBM_SETRANGE, TRUE, MAKELPARAM(5, 100)); // 0.5 ~ 10.0
	SendMessageW(hTrackMotionSpeed_, TBM_SETPOS, TRUE, 30);
	hEditMotionSpeed_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"3.0", WS_CHILD | WS_VISIBLE | ES_CENTER, padX + 355, curY + 76, 40, 22, hwnd_, (HMENU)IDC_EDIT_MOTIONSPEED, hInst, nullptr);
	SendMessageW(hEditMotionSpeed_, WM_SETFONT, (WPARAM)hFont, TRUE);

	CreateLabel(L"Power:", padX + 15, curY + 106, 45, 20);
	hTrackMotionIntensity_ = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_NOTICKS, padX + 60, curY + 104, 290, 22, hwnd_, (HMENU)IDC_TRACK_MOTIONINTENSITY, hInst, nullptr);
	SendMessageW(hTrackMotionIntensity_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 500)); // 0.0 ~ 5.0
	SendMessageW(hTrackMotionIntensity_, TBM_SETPOS, TRUE, 100);
	hEditMotionIntensity_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"1.00", WS_CHILD | WS_VISIBLE | ES_CENTER, padX + 355, curY + 104, 40, 22, hwnd_, (HMENU)IDC_EDIT_MOTIONINTENSITY, hInst, nullptr);
	SendMessageW(hEditMotionIntensity_, WM_SETFONT, (WPARAM)hFont, TRUE);

	curY += 148;

	// --- 6. アクションボタン ---
	hGroupActions_ = CreateWindowW(L"BUTTON", L" 6. Actions & File (INI) ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, padX, curY, width, 68, hwnd_, nullptr, hInst, nullptr);
	SendMessageW(hGroupActions_, WM_SETFONT, (WPARAM)hFont, TRUE);

	int btnW = 120;
	int btnH = 30;
	int btnY = curY + 24;

	hBtnSaveJson_ = CreateWindowW(L"BUTTON", L"Save INI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, padX + 15, btnY, btnW, btnH, hwnd_, (HMENU)IDC_BTN_SAVEINI, hInst, nullptr);
	SendMessageW(hBtnSaveJson_, WM_SETFONT, (WPARAM)hFont, TRUE);

	hBtnLoadJson_ = CreateWindowW(L"BUTTON", L"Load INI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, padX + 148, btnY, btnW, btnH, hwnd_, (HMENU)IDC_BTN_LOADINI, hInst, nullptr);
	SendMessageW(hBtnLoadJson_, WM_SETFONT, (WPARAM)hFont, TRUE);

	hBtnUndo_ = CreateWindowW(L"BUTTON", L"Reset / Undo", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, padX + 280, btnY, btnW, btnH, hwnd_, (HMENU)IDC_BTN_UNDO, hInst, nullptr);
	SendMessageW(hBtnUndo_, WM_SETFONT, (WPARAM)hFont, TRUE);

	hColorBrush_ = CreateSolidBrush(currentColorRef_);
}

void UIEditorWindow::RefreshTextList() {
	if (!hComboSelect_) return;

	isUpdatingUI_ = true;
	SendMessageW(hComboSelect_, CB_RESETCONTENT, 0, 0);

	auto names = UITextRegistry::GetInstance()->GetSortedNames();
	int selIndex = 0;
	int idx = 0;

	for (const auto& name : names) {
		std::wstring wName = Utf8ToWide(name);
		SendMessageW(hComboSelect_, CB_ADDSTRING, 0, (LPARAM)wName.c_str());
		if (name == selectedName_) {
			selIndex = idx;
		}
		idx++;
	}

	if (!names.empty()) {
		SendMessageW(hComboSelect_, CB_SETCURSEL, selIndex, 0);
		selectedName_ = names[selIndex];
	} else {
		selectedName_.clear();
	}

	// フォント一覧の更新
	SendMessageW(hComboFont_, CB_RESETCONTENT, 0, 0);
	auto fontNames = FontManager::GetInstance()->GetFontNames();
	int fontIdx = 0;
	for (const auto& fname : fontNames) {
		SendMessageW(hComboFont_, CB_ADDSTRING, 0, (LPARAM)Utf8ToWide(fname).c_str());
	}

	isUpdatingUI_ = false;

	UIText* curText = GetSelectedText();
	if (curText) {
		TakeSnapshot(curText);
		PopulateFieldsFromText(curText);
	}
}

void UIEditorWindow::OnSelectionChanged() {
	int sel = (int)SendMessageW(hComboSelect_, CB_GETCURSEL, 0, 0);
	if (sel == CB_ERR) return;

	wchar_t buf[256] = {};
	SendMessageW(hComboSelect_, CB_GETLBTEXT, sel, (LPARAM)buf);
	selectedName_ = WideToUtf8(buf);

	UIText* text = GetSelectedText();
	if (text) {
		TakeSnapshot(text);
		PopulateFieldsFromText(text);
	}
}

void UIEditorWindow::OnStateSelectionChanged() {
	int sel = (int)SendMessageW(hComboState_, CB_GETCURSEL, 0, 0);
	if (sel == CB_ERR) return;

	wchar_t buf[64] = {};
	SendMessageW(hComboState_, CB_GETLBTEXT, sel, (LPARAM)buf);
	selectedState_ = WideToUtf8(buf);

	UIText* text = GetSelectedText();
	if (text) {
		UIStateStyle st = text->GetStyle(selectedState_);
		isUpdatingUI_ = true;

		// Scale
		std::wstringstream ssSc;
		ssSc << std::fixed << std::setprecision(2) << st.scale;
		SetWindowTextW(hEditStateScale_, ssSc.str().c_str());
		SendMessageW(hTrackStateScale_, TBM_SETPOS, TRUE, (int)(st.scale * 100.0f));

		// Offset Y
		SetWindowTextW(hEditStateOffsetY_, std::to_wstring((int)st.offset.y).c_str());
		SendMessageW(hTrackStateOffsetY_, TBM_SETPOS, TRUE, (int)(st.offset.y + 50.0f));

		// Motion
		int mSel = 0;
		switch (st.loopMotion) {
		case UILoopMotion::Pulse:    mSel = 1; break;
		case UILoopMotion::Floating: mSel = 2; break;
		case UILoopMotion::Shake:    mSel = 3; break;
		default:                     mSel = 0; break;
		}
		SendMessageW(hComboMotion_, CB_SETCURSEL, mSel, 0);

		// Speed
		std::wstringstream ssSp;
		ssSp << std::fixed << std::setprecision(1) << st.motionSpeed;
		SetWindowTextW(hEditMotionSpeed_, ssSp.str().c_str());
		SendMessageW(hTrackMotionSpeed_, TBM_SETPOS, TRUE, (int)(st.motionSpeed * 10.0f));

		// Intensity
		std::wstringstream ssIn;
		ssIn << std::fixed << std::setprecision(2) << st.motionIntensity;
		SetWindowTextW(hEditMotionIntensity_, ssIn.str().c_str());
		SendMessageW(hTrackMotionIntensity_, TBM_SETPOS, TRUE, (int)(st.motionIntensity * 100.0f));

		if (isPreviewing_) {
			text->SetPreviewStateOverride(selectedState_);
		}

		isUpdatingUI_ = false;
	}
}

void UIEditorWindow::PopulateFieldsFromText(UIText* text) {
	if (!text) return;
	isUpdatingUI_ = true;

	// テキスト内容
	SetWindowTextW(hEditText_, Utf8ToWide(text->GetText()).c_str());

	// フォント名
	int fontIdx = (int)SendMessageW(hComboFont_, CB_FINDSTRINGEXACT, -1, (LPARAM)Utf8ToWide(text->GetFontName()).c_str());
	if (fontIdx != CB_ERR) {
		SendMessageW(hComboFont_, CB_SETCURSEL, fontIdx, 0);
	}

	// フォントサイズ
	int sizeVal = (int)text->GetFontSize();
	SetWindowTextW(hEditFontSize_, std::to_wstring(sizeVal).c_str());
	SendMessageW(hTrackFontSize_, TBM_SETPOS, TRUE, sizeVal);

	// 位置 X / Y
	Vector2 pos = text->GetPosition();
	SetWindowTextW(hEditPosX_, std::to_wstring((int)pos.x).c_str());
	SendMessageW(hTrackPosX_, TBM_SETPOS, TRUE, (int)pos.x);
	SetWindowTextW(hEditPosY_, std::to_wstring((int)pos.y).c_str());
	SendMessageW(hTrackPosY_, TBM_SETPOS, TRUE, (int)pos.y);

	// アンカー X / Y
	Vector2 anchor = text->GetAnchorPoint();
	std::wstringstream ssAncX, ssAncY;
	ssAncX << std::fixed << std::setprecision(2) << anchor.x;
	ssAncY << std::fixed << std::setprecision(2) << anchor.y;
	SetWindowTextW(hEditAnchorX_, ssAncX.str().c_str());
	SendMessageW(hTrackAnchorX_, TBM_SETPOS, TRUE, (int)(anchor.x * 100.0f));
	SetWindowTextW(hEditAnchorY_, ssAncY.str().c_str());
	SendMessageW(hTrackAnchorY_, TBM_SETPOS, TRUE, (int)(anchor.y * 100.0f));

	// カラー・アルファ
	Vector4 col = text->GetColor();
	currentColorRef_ = RGB((BYTE)(col.x * 255.0f), (BYTE)(col.y * 255.0f), (BYTE)(col.z * 255.0f));
	if (hColorBrush_) DeleteObject(hColorBrush_);
	hColorBrush_ = CreateSolidBrush(currentColorRef_);
	InvalidateRect(hColorSwatch_, nullptr, TRUE);

	std::wstringstream ssAlpha;
	ssAlpha << std::fixed << std::setprecision(2) << col.w;
	SetWindowTextW(hEditAlpha_, ssAlpha.str().c_str());
	SendMessageW(hTrackAlpha_, TBM_SETPOS, TRUE, (int)(col.w * 100.0f));

	// 可視性
	SendMessageW(hCheckVisible_, BM_SETCHECK, text->IsVisible() ? BST_CHECKED : BST_UNCHECKED, 0);

	isUpdatingUI_ = false;

	// 現在選択中のステート情報を同期
	OnStateSelectionChanged();
}

void UIEditorWindow::ApplyFieldsToText() {
	if (isUpdatingUI_) return;
	UIText* text = GetSelectedText();
	if (!text) return;

	// テキスト内容
	wchar_t textBuf[1024] = {};
	GetWindowTextW(hEditText_, textBuf, 1024);
	text->SetText(WideToUtf8(textBuf));

	// フォント名
	int fontIdx = (int)SendMessageW(hComboFont_, CB_GETCURSEL, 0, 0);
	if (fontIdx != CB_ERR) {
		wchar_t fontBuf[256] = {};
		SendMessageW(hComboFont_, CB_GETLBTEXT, fontIdx, (LPARAM)fontBuf);
		text->SetFontName(WideToUtf8(fontBuf));
	}

	// フォントサイズ
	wchar_t sizeBuf[32] = {};
	GetWindowTextW(hEditFontSize_, sizeBuf, 32);
	try {
		float fSize = std::stof(sizeBuf);
		if (fSize > 0.0f) text->SetFontSize(fSize);
	} catch (...) {}

	// 位置
	wchar_t posXBuf[32] = {}, posYBuf[32] = {};
	GetWindowTextW(hEditPosX_, posXBuf, 32);
	GetWindowTextW(hEditPosY_, posYBuf, 32);
	try {
		text->SetPosition({ std::stof(posXBuf), std::stof(posYBuf) });
	} catch (...) {}

	// アンカー
	wchar_t ancXBuf[32] = {}, ancYBuf[32] = {};
	GetWindowTextW(hEditAnchorX_, ancXBuf, 32);
	GetWindowTextW(hEditAnchorY_, ancYBuf, 32);
	try {
		text->SetAnchorPoint({ std::stof(ancXBuf), std::stof(ancYBuf) });
	} catch (...) {}

	// カラー・アルファ
	wchar_t alphaBuf[32] = {};
	GetWindowTextW(hEditAlpha_, alphaBuf, 32);
	float alphaVal = 1.0f;
	try {
		alphaVal = (std::clamp)(std::stof(alphaBuf), 0.0f, 1.0f);
	} catch (...) {}

	float r = GetRValue(currentColorRef_) / 255.0f;
	float g = GetGValue(currentColorRef_) / 255.0f;
	float b = GetBValue(currentColorRef_) / 255.0f;
	text->SetColor({ r, g, b, alphaVal });

	// 可視性
	LRESULT isChecked = SendMessageW(hCheckVisible_, BM_GETCHECK, 0, 0);
	text->SetVisible(isChecked == BST_CHECKED);

	// ステート＆モーション情報の反映
	UIStateStyle st = text->GetStyle(selectedState_);
	wchar_t scaleBuf[32] = {}, offYBuf[32] = {}, speedBuf[32] = {}, intBuf[32] = {};
	GetWindowTextW(hEditStateScale_, scaleBuf, 32);
	GetWindowTextW(hEditStateOffsetY_, offYBuf, 32);
	GetWindowTextW(hEditMotionSpeed_, speedBuf, 32);
	GetWindowTextW(hEditMotionIntensity_, intBuf, 32);

	try { st.scale = std::stof(scaleBuf); } catch (...) {}
	try { st.offset.y = std::stof(offYBuf); } catch (...) {}
	try { st.motionSpeed = std::stof(speedBuf); } catch (...) {}
	try { st.motionIntensity = std::stof(intBuf); } catch (...) {}

	int mSel = (int)SendMessageW(hComboMotion_, CB_GETCURSEL, 0, 0);
	switch (mSel) {
	case 1:  st.loopMotion = UILoopMotion::Pulse; break;
	case 2:  st.loopMotion = UILoopMotion::Floating; break;
	case 3:  st.loopMotion = UILoopMotion::Shake; break;
	default: st.loopMotion = UILoopMotion::None; break;
	}

	text->SetStyle(selectedState_, st);
}

void UIEditorWindow::OpenColorPicker() {
	CHOOSECOLORW cc = {};
	cc.lStructSize = sizeof(CHOOSECOLORW);
	cc.hwndOwner = hwnd_;
	cc.rgbResult = currentColorRef_;
	cc.lpCustColors = customColors_;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;

	if (ChooseColorW(&cc)) {
		currentColorRef_ = cc.rgbResult;
		if (hColorBrush_) DeleteObject(hColorBrush_);
		hColorBrush_ = CreateSolidBrush(currentColorRef_);
		InvalidateRect(hColorSwatch_, nullptr, TRUE);
		ApplyFieldsToText();
	}
}

void UIEditorWindow::TakeSnapshot(UIText* text) {
	if (!text) return;
	snapshot_.text = text->GetText();
	snapshot_.fontName = text->GetFontName();
	snapshot_.fontSize = text->GetFontSize();
	snapshot_.position = text->GetPosition();
	snapshot_.anchorPoint = text->GetAnchorPoint();
	snapshot_.color = text->GetColor();
	snapshot_.visible = text->IsVisible();
	snapshot_.isValid = true;
}

void UIEditorWindow::RestoreSnapshot(UIText* text) {
	if (!text || !snapshot_.isValid) return;
	text->SetText(snapshot_.text);
	text->SetFontName(snapshot_.fontName);
	text->SetFontSize(snapshot_.fontSize);
	text->SetPosition(snapshot_.position);
	text->SetAnchorPoint(snapshot_.anchorPoint);
	text->SetColor(snapshot_.color);
	text->SetVisible(snapshot_.visible);
}

void UIEditorWindow::SaveToIni(const std::string& filePath) {
	UITextRegistry::GetInstance()->SaveConfig(filePath);
	MessageBoxW(hwnd_, L"UI Text & Motion settings saved to INI successfully!\n(Settings will automatically load when scenes start)", L"Save Complete", MB_OK | MB_ICONINFORMATION);
}

void UIEditorWindow::LoadFromIni(const std::string& filePath) {
	UITextRegistry::GetInstance()->LoadConfig(filePath);

	UIText* curText = GetSelectedText();
	if (curText) {
		PopulateFieldsFromText(curText);
		TakeSnapshot(curText);
	}

	MessageBoxW(hwnd_, L"UI Text & Motion settings loaded from INI and applied!", L"Load Complete", MB_OK | MB_ICONINFORMATION);
}
