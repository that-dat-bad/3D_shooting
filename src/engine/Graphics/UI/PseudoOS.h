#pragma once
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "../../base/Math/Vector2.h"
#include "../../base/Math/Vector4.h"
#include "../Sprite/Sprite.h"

class SpriteCommon;
using Vector4 = MyMath::Vector4;

// 疑似OSデスクトップ環境
class PseudoOS {
public:
	// カラーテーマ
	enum class Theme {
		TerminalGreen,   // 通常ターミナル
		KernelPanicRed,  // カーネルパニック
		TacticalCyan,    // 作戦成功・デブリーフィング
	};

	// ウィンドウ定義
	struct Window {
		int id = 0;
		std::string title;
		Vector2 pos = { 0.0f, 0.0f };
		Vector2 size = { 0.0f, 0.0f };
		Vector2 normalPos = { 0.0f, 0.0f };
		Vector2 normalSize = { 0.0f, 0.0f };
		float titleBarHeight = 34.0f;
		bool isOpen = true;
		bool isMinimized = false;
		bool isDragging = false;
		Vector2 dragOffset = { 0.0f, 0.0f };
		float animScale = 1.0f; // 最小化・復元アニメーション

		// 描画コールバック
		std::function<void(const Window&, PseudoOS*)> onDrawContent;
	};

	// スタートメニュー項目
	struct StartMenuItem {
		std::string label;
		std::function<void()> onClick;
		bool isSeparator = false;
	};

	PseudoOS();
	~PseudoOS();

	// 初期化
	// spriteCommon: SpriteCommonポインタ
	// theme: カラーテーマ
	void Initialize(SpriteCommon* spriteCommon, Theme theme = Theme::TerminalGreen);

	// 更新
	// dt: デルタタイム
	void Update(float dt);

	// 更新（マウス入力指定版）
	// dt: デルタタイム
	void Update(float dt, const Vector2& mousePos, bool isLDown, bool isLTrig);

	// 描画
	void Draw();

	// テーマ設定
	void SetTheme(Theme theme);
	Theme GetTheme() const { return currentTheme_; }

	// ========================================================
	// ウィンドウ管理
	// ========================================================
	void AddWindow(const Window& window);
	void ClearWindows();
	Window* GetWindow(int id);
	const Window* GetWindow(int id) const;
	void BringWindowToFront(int id);
	int GetActiveWindowId() const { return activeWindowId_; }
	void SetActiveWindowId(int id) { BringWindowToFront(id); }
	const std::vector<Window>& GetWindows() const { return windows_; }
	std::vector<Window>& GetWindows() { return windows_; }
	const std::vector<int>& GetWindowZOrder() const { return windowZOrder_; }

	// ========================================================
	// ウィンドウ開閉アニメーション・位置リセット
	// ========================================================
	enum class WindowAnimState {
		None,
		Opening,  // 展開中
		Closing,  // 縮小中
	};

	// ウィンドウ展開アニメーション開始
	// duration: 所要時間
	void PlayWindowOpenAnimation(float duration = 0.35f);

	// ウィンドウ縮小アニメーション開始
	// duration: 所要時間
	void PlayWindowCloseAnimation(float duration = 0.28f);

	// ウィンドウ位置リセット
	void ResetWindowPositions();

	// アニメーション完了判定
	bool IsWindowAnimComplete() const { return windowAnimState_ == WindowAnimState::None; }
	WindowAnimState GetWindowAnimState() const { return windowAnimState_; }

	// ========================================================
	// スタートメニュー設定
	// ========================================================
	void SetStartMenuItems(const std::vector<StartMenuItem>& items) { startMenuItems_ = items; }
	void SetStartMenuTitle(const std::string& title) { startMenuTitle_ = title; }
	bool IsStartMenuOpen() const { return isStartMenuOpen_; }
	void SetStartMenuOpen(bool open) { isStartMenuOpen_ = open; }

	// ========================================================
	// OSデスクトップ情報・可視性
	// ========================================================
	void SetOSName(const std::string& name) { osName_ = name; }
	void SetSecurityClearance(const std::string& clearance) { securityClearance_ = clearance; }
	void SetTaskbarVisible(bool visible) { isTaskbarVisible_ = visible; }
	bool IsTaskbarVisible() const { return isTaskbarVisible_; }
	void SetCustomCursorVisible(bool visible) { isCursorVisible_ = visible; }
	bool IsCustomCursorVisible() const { return isCursorVisible_; }

	// ========================================================
	// OS描画プリミティブ（スプライトプール自動管理）
	// ========================================================
	void DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color);
	void DrawBorder(const Vector2& pos, const Vector2& size, const Vector4& color, float thick = 1.5f);
	void DrawPanel(const Vector2& pos, const Vector2& size, const Vector4& bgCol, const Vector4& borderCol, float thick = 1.5f);
	void DrawSpectrumBars(const Vector2& pos, float width, float height, int barCount, float animSpeed, const Vector4& color);
	void DrawGauge(const Vector2& pos, const Vector2& size, float ratio, const Vector4& fillColor, const Vector4& borderCol);
	void DrawWindowCloseButton(const Vector2& pos, float size, bool isHovered);

	// ========================================================
	// 共通画面演出（CRT電源オフ収束・ボタン発光フィードバック）
	// ========================================================
	// CRT電源オフ演出の描画
	// progress: 進行度
	void DrawCRTShutdown(float progress, float screenWidth = 1280.0f, float screenHeight = 720.0f);

	// ボタン発光演出の描画
	// pos: 描画位置
	// size: サイズ
	// timer: タイマー
	void DrawButtonFlash(const Vector2& pos, const Vector2& size, float timer, float duration = 0.25f, const Vector4& color = { 1.0f, 0.95f, 0.85f, 1.0f });

	// ========================================================
	// テーマカラー取得
	// ========================================================
	Vector4 GetPrimaryColor() const { return colPrimary_; }
	Vector4 GetSecondaryColor() const { return colSecondary_; }
	Vector4 GetHighlightColor() const { return colHighlight_; }
	Vector4 GetBorderActiveColor() const { return colBorderActive_; }
	Vector4 GetBorderInactiveColor() const { return colBorderInactive_; }
	Vector4 GetPanelBgColor() const { return colPanelBg_; }

	// ========================================================
	// マウス情報
	// ========================================================
	Vector2 GetCursorPos() const { return cursorPos_; }
	bool IsCursorInsideWindow(int windowId) const;

	// スプライトプールの直接管理（シーン終了時のキャッシュ退避等）
	std::vector<std::unique_ptr<Sprite>>& GetSpritePool() { return spritePool_; }
	std::vector<std::unique_ptr<Sprite>>& GetCursorSprites() { return cursorSprites_; }
	void SetSpritePool(std::vector<std::unique_ptr<Sprite>> pool) { spritePool_ = std::move(pool); }
	void SetCursorSprites(std::vector<std::unique_ptr<Sprite>> sprites) { cursorSprites_ = std::move(sprites); }
	void ResetSpritePool() { spritePoolIdx_ = 0; }

private:
	void ApplyThemePalette();
	void DrawWindowFrame(Window& win);
	void DrawTaskbar();
	void DrawCustomCursor();

	// スプライトプール（512枚）
	SpriteCommon* spriteCommon_ = nullptr;
	std::vector<std::unique_ptr<Sprite>> spritePool_;
	size_t spritePoolIdx_ = 0;

	// カーソル専用スプライト（24枚）
	std::vector<std::unique_ptr<Sprite>> cursorSprites_;

	Theme currentTheme_ = Theme::TerminalGreen;
	std::vector<Window> windows_;
	std::vector<int> windowZOrder_;
	int activeWindowId_ = 0;

	// タスクバー & スタートメニュー
	bool isTaskbarVisible_ = true;
	bool isStartMenuOpen_ = false;
	std::string osName_ = "DAWN_OS TACTICAL DEFENSE MAINFRAME [v4.12.0]";
	std::string securityClearance_ = "CLEARANCE: LEVEL 5 // SECURE DESKTOP ENVIRONMENT";
	std::string startMenuTitle_ = "DAWN_OS : ROOT SESSION";
	std::vector<StartMenuItem> startMenuItems_;

	// カーソル & 波紋
	bool isCursorVisible_ = true;
	Vector2 cursorPos_ = { 640.0f, 360.0f };
	float cursorRippleTimer_ = 0.0f;
	Vector2 cursorRipplePos_ = { 0.0f, 0.0f };

	float animTimer_ = 0.0f;

	// テーマカラーパレット
	Vector4 colPrimary_        = { 0.48f, 0.84f, 0.50f, 1.0f };
	Vector4 colSecondary_      = { 0.30f, 0.58f, 0.33f, 0.85f };
	Vector4 colHighlight_      = { 0.78f, 0.95f, 0.80f, 1.0f };
	Vector4 colPanelBg_        = { 0.01f, 0.04f, 0.015f, 0.98f };
	Vector4 colTaskbarBg_      = { 0.01f, 0.04f, 0.015f, 0.98f };
	Vector4 colBorderActive_   = { 0.53f, 0.88f, 0.55f, 0.98f };
	Vector4 colBorderInactive_ = { 0.18f, 0.36f, 0.20f, 0.50f };
	Vector4 colLedActive_      = { 0.50f, 0.86f, 0.53f, 1.0f };

	// ウィンドウアニメーション状態
	WindowAnimState windowAnimState_ = WindowAnimState::None;
	float windowAnimTimer_ = 0.0f;
	float windowAnimDuration_ = 0.35f;
};
