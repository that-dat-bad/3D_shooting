#pragma once
#include<Windows.h>
#include <wrl/client.h>
#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#include <memory>
#include "../base/Math/Vector2.h"

/// <summary>
/// 入力管理クラス (キーボード、マウス)
/// </summary>
class Input
{
public:
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
	static Input* GetInstance();

	/// <summary>
	/// デフォルトコンストラクタ (std::make_unique対応のためpublic)
	/// </summary>
	Input() = default;

	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="hInstance">インスタンスハンドル</param>
	/// <param name="hwnd">ウィンドウハンドル</param>
	void Initialize(HINSTANCE hInstance, HWND hwnd);

	/// <summary>
	/// マウスカーソルをウィンドウ中央にロック＆非表示にする
	/// </summary>
	void LockCursor();

	/// <summary>
	/// マウスカーソルのロックを解除＆表示する
	/// </summary>
	void UnlockCursor();

	/// <summary>
	/// カーソルがロックされているか取得
	/// </summary>
	/// <returns>ロックされていればtrue</returns>
	bool IsCursorLocked() const { return cursorLocked_; }

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update();

	/// <summary>
	/// キーの押下状態取得
	/// </summary>
	/// <param name="keyNumber">キーコード (DIK_*)</param>
	/// <returns>押されていればtrue</returns>
	bool PushKey(BYTE keyNumber);

	/// <summary>
	/// キーのトリガー状態取得（押した瞬間）
	/// </summary>
	/// <param name="keyNumber">キーコード (DIK_*)</param>
	/// <returns>押した瞬間であればtrue</returns>
	bool TriggerKey(BYTE keyNumber);

	struct MouseMove {
		long lX;
		long lY;
		long lZ;
	};

	/// <summary>
	/// マウスの移動量取得
	/// </summary>
	/// <returns>移動量構造体</returns>
	MouseMove GetMouseMove();

	/// <summary>
	/// マウスボタンの押下状態取得
	/// </summary>
	/// <param name="buttonNumber">ボタン番号 (0:左, 1:右, 2:中)</param>
	/// <returns>押されていればtrue</returns>
	bool PushMouse(int buttonNumber);

	/// <summary>
	/// マウスボタンのトリガー状態取得
	/// </summary>
	/// <param name="buttonNumber">ボタン番号 (0:左, 1:右, 2:中)</param>
	/// <returns>押した瞬間であればtrue</returns>
	bool TriggerMouse(int buttonNumber);

	// スクリーン座標でのマウス位置取得（クライアント領域基準）
	struct MousePosition {
		long x;
		long y;
	};

	/// <summary>
	/// クライアント領域基準でのマウス位置取得
	/// </summary>
	/// <param name="hwnd">ウィンドウハンドル（省略時は初期化時のHWNDを使用）</param>
	/// <returns>座標構造体</returns>
	MousePosition GetMouseScreenPosition(HWND hwnd = nullptr);

	/// <summary>
	/// 仮想画面解像度にスケーリングしたマウス座標取得（全シーン共通）
	/// </summary>
	/// <param name="virtualWidth">仮想画面幅（例: 1280.0f）</param>
	/// <param name="virtualHeight">仮想画面高（例: 720.0f）</param>
	/// <param name="hwnd">ウィンドウハンドル（省略時は初期化時のHWNDを使用）</param>
	/// <returns>スケーリング済みマウス座標</returns>
	Vector2 GetVirtualMousePosition(float virtualWidth, float virtualHeight, HWND hwnd = nullptr);

	/// <summary>
	/// ウィンドウハンドル取得
	/// </summary>
	HWND GetHwnd() const { return hwnd_; }

	~Input() = default;

private:
	Input(const Input&) = delete;
	Input& operator=(const Input&) = delete;
	static std::unique_ptr<Input> instance_;

	ComPtr<IDirectInput8> directInput_;
	ComPtr<IDirectInputDevice8> keyboard_;
	ComPtr<IDirectInputDevice8> mouse_;
	BYTE keys_[256] = {};
	BYTE preKeys_[256] = {};
	DIMOUSESTATE2 mouseState_ = {};
	DIMOUSESTATE2 preMouseState_ = {};

	HWND hwnd_ = nullptr;           // ウィンドウハンドル（カーソル制御用）
	bool cursorLocked_ = false;     // カーソルロック状態
};

