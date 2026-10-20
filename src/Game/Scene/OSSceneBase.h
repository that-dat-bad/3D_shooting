#pragma once
#include "IScene.h"
#include "../../engine/Graphics/UI/PseudoOS.h"
#include "../../engine/Graphics/UI/UIPanel.h"
#include "../../engine/Graphics/UI/UISelectionManager.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/base/WinApp.h"
#include <string>
#include <vector>

/// @brief 疑似OS環境（PseudoOS）上で動作する非戦闘シーン（Clear, GameOver等）の共通基底クラス
class OSSceneBase : public IScene {
public:
	static constexpr float kScreenWidth = static_cast<float>(WinApp::kClientWidth);
	static constexpr float kScreenHeight = static_cast<float>(WinApp::kClientHeight);

	OSSceneBase() = default;
	virtual ~OSSceneBase() override = default;

	/// @brief OS環境の基本初期化（背景パネル、テーマ、OS情報文字列、背景色設定）
	void InitializeOS(SpriteCommon* spriteCommon, PseudoOS::Theme theme,
		const std::string& osName, const std::string& clearance,
		const std::string& startMenuTitle,
		const Vector4& bgPanelColor = { 0.01f, 0.02f, 0.05f, 0.95f });

	/// @brief OS環境の毎フレーム基本更新（animTimer, fadeInAlpha, btnClickFlash, pseudoOS, selectionManager更新）
	void UpdateOS(float dt, float fadeInSpeed = 1.5f);

	/// @brief 背景オーバーレイパネルの描画
	void DrawOSBackground();

	/// @brief OSデスクトップ環境（ウィンドウ群、タスクバー、カーソル）の描画
	void DrawOSDesktop();

	/// @brief OS標準カーソルの表示/非表示（ウィンドウフォーカス連動）
	void SetOSCursorVisible(bool visible);

protected:
	bool isUIInitialized_ = false;

	// 疑似OSデスクトップ環境
	PseudoOS pseudoOS_;

	// 背景パネル
	UIPanel backgroundPanel_;

	// キーボード/ゲームパッド メニュー操作マネージャー
	UISelectionManager selectionManager_;

	// アニメーションタイマー＆フェード
	float animTimer_ = 0.0f;
	float fadeInAlpha_ = 0.0f;

	// ボタンクリックフィードバック発光用
	float btnClickFlashTimer_ = 0.0f;
	int clickedButtonIdx_ = -1;

	bool isCursorShown_ = false;
};
