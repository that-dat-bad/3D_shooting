#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

/// <summary>
/// Windowsアプリケーション管理クラス
/// ウィンドウの生成やメッセージループの処理を行う
/// </summary>
class WinApp {
public:
	/// <summary>クライアント領域の幅</summary>
	static const int32_t kClientWidth = 1280;
	/// <summary>クライアント領域の高さ</summary>
	static const int32_t kClientHeight = 720;

public:
	/// <summary>
	/// ウィンドウの初期化
	/// </summary>
	void Initialize(const std::wstring& title = L"DirectXGame", const std::wstring& version = L"1.0");

	/// <summary>
	/// Windowsメッセージの処理 (毎フレーム呼ぶ)
	/// </summary>
	/// <returns>終了メッセージを受け取った場合は true</returns>
	bool ProcessMessage();

	/// <summary>
	/// ウィンドウの終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// ウィンドウハンドルの取得
	/// </summary>
	/// <returns>HWND</returns>
	HWND GetHwnd() const { return hwnd_; }

private:
	/// <summary>
	/// ウィンドウプロシージャ
	/// </summary>
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	HWND hwnd_ = nullptr;
	WNDCLASS wc_{};
};