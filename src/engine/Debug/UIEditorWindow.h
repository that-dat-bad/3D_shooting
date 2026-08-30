#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "../base/Math/MyMath.h"

using namespace MyMath;

class UIText;

/// @brief Win32ネイティブ別ウィンドウで動作するUIエディタ。
///        リアルタイムな文字装飾・レイアウト調整、状態（Normal/Hover/Selected/Custom）
///        およびモーション（Tween, Pulse, Float, Shake）の編集・保存を提供する。
class UIEditorWindow {
public:
	static UIEditorWindow* GetInstance();

	/// @brief エディタウィンドウの初期化・生成（非表示で生成）
	bool Initialize(HWND parentHwnd = nullptr);
	bool Initialize(HINSTANCE hInstance, HWND parentHwnd);

	/// @brief 毎フレーム更新（登録テキスト一覧の変更検知など）
	void Update();

	/// @brief 終了処理
	void Finalize();

	/// @brief ウィンドウの表示/非表示切り替え
	void Show(bool show = true);
	void Toggle() { ToggleVisible(); }
	void ToggleVisible();
	bool IsVisible() const;

	/// @brief 現在選択されているUIText要素を取得
	UIText* GetSelectedText() const;

private:
	UIEditorWindow() = default;
	~UIEditorWindow();
	UIEditorWindow(const UIEditorWindow&) = delete;
	UIEditorWindow& operator=(const UIEditorWindow&) = delete;

	// ウィンドウプロシージャ
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// コントロール生成
	void CreateControls();

	// UI更新・同期
	void RefreshTextList();
	void OnSelectionChanged();
	void OnStateSelectionChanged();
	void PopulateFieldsFromText(UIText* text);
	void ApplyFieldsToText();

	// カラーピッカー呼び出し
	void OpenColorPicker();

	// スナップショット管理
	struct Snapshot {
		std::string text;
		std::string fontName;
		float fontSize = 32.0f;
		Vector2 position = { 0.0f, 0.0f };
		Vector2 anchorPoint = { 0.0f, 0.0f };
		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool visible = true;
		bool isValid = false;
	};
	void TakeSnapshot(UIText* text);
	void RestoreSnapshot(UIText* text);

	// INI操作
	void SaveToIni(const std::string& filePath = "assets/ui/text_styles.ini");
	void LoadFromIni(const std::string& filePath = "assets/ui/text_styles.ini");

private:
	HWND hwnd_ = nullptr;
	HWND parentHwnd_ = nullptr;
	bool isUpdatingUI_ = false; // プログラムによるUI変更時にイベント再帰呼び出しを防ぐフラグ
	size_t registeredCountCache_ = 0;

	std::string selectedName_;
	std::string selectedState_ = "Normal";
	bool isPreviewing_ = false;
	Snapshot snapshot_;

	// --- コントロールのHWND ---
	HWND hGroupSelect_ = nullptr;
	HWND hComboSelect_ = nullptr;
	HWND hBtnRefresh_ = nullptr;

	HWND hGroupText_ = nullptr;
	HWND hEditText_ = nullptr;

	HWND hGroupFont_ = nullptr;
	HWND hComboFont_ = nullptr;
	HWND hTrackFontSize_ = nullptr;
	HWND hEditFontSize_ = nullptr;

	HWND hGroupTransform_ = nullptr;
	HWND hTrackPosX_ = nullptr;
	HWND hEditPosX_ = nullptr;
	HWND hTrackPosY_ = nullptr;
	HWND hEditPosY_ = nullptr;
	HWND hTrackAnchorX_ = nullptr;
	HWND hEditAnchorX_ = nullptr;
	HWND hTrackAnchorY_ = nullptr;
	HWND hEditAnchorY_ = nullptr;

	HWND hGroupAppearance_ = nullptr;
	HWND hColorSwatch_ = nullptr;
	HWND hBtnPickColor_ = nullptr;
	HWND hTrackAlpha_ = nullptr;
	HWND hEditAlpha_ = nullptr;
	HWND hCheckVisible_ = nullptr;

	// --- モーション・状態グループ ---
	HWND hGroupMotion_ = nullptr;
	HWND hComboState_ = nullptr;
	HWND hTrackStateScale_ = nullptr;
	HWND hEditStateScale_ = nullptr;
	HWND hTrackStateOffsetY_ = nullptr;
	HWND hEditStateOffsetY_ = nullptr;
	HWND hComboMotion_ = nullptr;
	HWND hTrackMotionSpeed_ = nullptr;
	HWND hEditMotionSpeed_ = nullptr;
	HWND hTrackMotionIntensity_ = nullptr;
	HWND hEditMotionIntensity_ = nullptr;
	HWND hBtnPreviewState_ = nullptr;

	HWND hGroupActions_ = nullptr;
	HWND hBtnSaveJson_ = nullptr;
	HWND hBtnLoadJson_ = nullptr;
	HWND hBtnUndo_ = nullptr;

	COLORREF currentColorRef_ = RGB(255, 255, 255);
	COLORREF customColors_[16] = {};
	HBRUSH hColorBrush_ = nullptr;
	HBRUSH hBgBrush_ = nullptr;
	HBRUSH hEditBgBrush_ = nullptr;
};
