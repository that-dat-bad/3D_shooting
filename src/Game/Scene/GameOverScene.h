#pragma once
#include "IScene.h"
#include "../../engine/Graphics/UI/UIPanel.h"
#include "../../engine/Graphics/UI/UIText.h"
#include "../../engine/Graphics/UI/UIButton.h"
#include "../../engine/Graphics/UI/UISelectionManager.h"
#include "../../engine/Graphics/Sprite/Sprite.h"
#include <vector>
#include <string>
#include <memory>

/// @brief ゲームオーバー専用シーン（ミリタリーOSカーネルパニック、16進数メモリダンプ、OS再起動）
class GameOverScene : public IScene {
public:
	GameOverScene();
	~GameOverScene();
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawUI() override;
	void Finalize() override;

private:
	UIPanel backgroundPanel_;
	UIText gameOverText_;
	
	UIButton retryButton_;
	UIButton titleButton_;

	UISelectionManager selectionManager_;

	float animTimer_ = 0.0f;
	float fadeInAlpha_ = 0.0f;

	// 項目3: コンソール風ログ展開アニメーション & 高速スクロール16進数メモリダンプ
	std::vector<std::string> memoryDumpPool_;
	int dumpScrollIndex_ = 0;
	float dumpScrollTimer_ = 0.0f;
	float logDeployTimer_ = 0.0f;     ///< コンソールのように1行ずつカタカタ打ち出されるタイマー
	int visibleLogCount_ = 0;         ///< 現在表示されているログ行数

	// 項目8: システム再起動（リブート）遷移 & ブラウン管電源遮断トランジション
	bool isRebooting_ = false;
	float rebootTimer_ = 0.0f;
	std::unique_ptr<Sprite> rebootBlackoutSprite_;

	// リトライ時のホワイトノイズ＆グリッチトランジション
	bool isRetrying_ = false;
	float retryGlitchTimer_ = 0.0f;
};
