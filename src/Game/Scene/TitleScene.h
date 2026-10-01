#pragma once
#include "IScene.h"
#include <vector>
#include <memory>
#include "../../engine/Graphics/UI/UIPanel.h"
#include "../../engine/Graphics/UI/UIText.h"
#include "../../engine/Graphics/UI/UIButton.h"
#include "../../engine/Graphics/UI/UISelectionManager.h"

/// @brief タイトルシーン。ゲームタイトルとメニューボタンを表示する。
class TitleScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawUI() override;
	void Finalize() override;

private:
	// --- UI要素 ---
	UIPanel backgroundPanel_;		///< 背景オーバーレイ
	UIText titleText_;				///< ゲームタイトル「戦雷」
	UIText subtitleText_;			///< サブタイトル
	UIButton startButton_;			///< スタートボタン
	UIButton editorButton_;         ///< エディタボタン
	UIButton settingsButton_;		///< 設定ボタン
	UIButton exitButton_;			///< 終了ボタン

	UISelectionManager selectionManager_;

	// アニメーション
	float animTimer_ = 0.0f;		///< テキストアニメーション用タイマー
};