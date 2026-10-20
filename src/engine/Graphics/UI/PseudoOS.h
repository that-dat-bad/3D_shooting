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

/// @brief 疑似OSデスクトップ環境（マルチウィンドウ、タスクバー、カスタムカーソル、サイバーOS描画基盤）
///        TitleScene, GameOverScene, ClearScene などの非戦闘シーンで統一された高密度ミリタリーOS-UIを提供する。
class PseudoOS {
public:
	/// @brief OSのカラーテーマ
	enum class Theme {
		TerminalGreen,   ///< 通常ターミナル（タイトル等: 鮮烈ネオングリーン）
		KernelPanicRed,  ///< カーネルパニック（ゲームオーバー: 深紅アラート＆アンバー）
		TacticalCyan,    ///< 作戦成功・デブリーフィング（クリア: エレクトリックシアン＆エメラルド）
	};

	/// @brief ウィンドウ定義
	struct Window {
		int id = 0;
		std::string title;
		Vector2 pos = { 0.0f, 0.0f };
		Vector2 size = { 0.0f, 0.0f };
		Vector2 normalPos = { 0.0f, 0.0f };
		Vector2 normalSize = { 0.0f, 0.0f };
		float titleBarHeight = 30.0f;
		bool isOpen = true;
		bool isMinimized = false;
		bool isDragging = false;
		Vector2 dragOffset = { 0.0f, 0.0f };
		float animScale = 1.0f; ///< 最小化・復元アニメーション (0.0〜1.0)

		/// @brief ウィンドウ内部の描画コールバック
		std::function<void(const Window&, PseudoOS*)> onDrawContent;
	};

	/// @brief スタートメニューの項目
	struct StartMenuItem {
		std::string label;
		std::function<void()> onClick;
		bool isSeparator = false;
	};

	PseudoOS();
	~PseudoOS();

	/// @brief 初期化
	/// @param spriteCommon SpriteCommonポインタ
	/// @param theme カラーテーマ
	void Initialize(SpriteCommon* spriteCommon, Theme theme = Theme::TerminalGreen);

	/// @brief 毎フレーム更新（内部で Input からスケーリング済みマウス座標とボタン入力を自動取得）
	/// @param dt デルタタイム
	void Update(float dt);

	/// @brief 毎フレーム更新（マウス入力手動指定版）
	/// @param dt デルタタイム
	/// @param mousePos マウス画面座標
	/// @param isLDown 左クリック押下中
	/// @param isLTrig 左クリックトリガー
	void Update(float dt, const Vector2& mousePos, bool isLDown, bool isLTrig);

	/// @brief デスクトップ環境の描画（背景・ウィンドウ・タスクバー・カーソル）
	void Draw();

	/// @brief カラーテーマの動的切り替え
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
		Opening,  ///< ズームイン展開中 (0.0 -> 1.0)
		Closing,  ///< 縮小消滅中 (1.0 -> 0.0)
	};

	/// @brief 全ウィンドウの展開アニメーション開始
	/// @param duration 所要時間（秒、デフォルト0.35s）
	void PlayWindowOpenAnimation(float duration = 0.35f);

	/// @brief 全ウィンドウの縮小消滅アニメーション開始
	/// @param duration 所要時間（秒、デフォルト0.28s）
	void PlayWindowCloseAnimation(float duration = 0.28f);

	/// @brief 全ウィンドウの位置・スケールを初期状態にリセット
	void ResetWindowPositions();

	/// @brief ウィンドウアニメーションが完了しているか
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
	/// @brief CRTモニター電源オフ収束演出を描画（上下からスリット化し、横一線の光になって消滅）
	/// @param progress 演出進行度 (0.0〜1.0)
	/// @param screenWidth 画面幅 (デフォルト1280.0f)
	/// @param screenHeight 画面高さ (デフォルト720.0f)
	void DrawCRTShutdown(float progress, float screenWidth = 1280.0f, float screenHeight = 720.0f);

	/// @brief ボタンクリック時の拡散発光ボーダーを描画
	/// @param pos ボタン位置
	/// @param size ボタンサイズ
	/// @param timer 残りタイマー
	/// @param duration 全体の所要時間 (デフォルト0.25f)
	/// @param color 発光色
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
	Vector4 colPrimary_        = { 0.35f, 1.00f, 0.60f, 1.0f };
	Vector4 colSecondary_      = { 0.15f, 0.55f, 0.30f, 0.85f };
	Vector4 colHighlight_      = { 0.90f, 1.00f, 0.95f, 1.0f };
	Vector4 colPanelBg_        = { 0.01f, 0.04f, 0.02f, 0.98f };
	Vector4 colTaskbarBg_      = { 0.01f, 0.04f, 0.02f, 0.98f };
	Vector4 colBorderActive_   = { 0.45f, 1.00f, 0.70f, 0.98f };
	Vector4 colBorderInactive_ = { 0.12f, 0.35f, 0.20f, 0.50f };
	Vector4 colLedActive_      = { 0.40f, 1.00f, 0.70f, 1.0f };

	// ウィンドウアニメーション状態
	WindowAnimState windowAnimState_ = WindowAnimState::None;
	float windowAnimTimer_ = 0.0f;
	float windowAnimDuration_ = 0.35f;
};
