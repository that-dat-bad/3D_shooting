#pragma once
#include "OSSceneBase.h"
#include "../../engine/Graphics/UI/UIText.h"
#include "../../engine/Graphics/UI/UIButton.h"
#include "../../engine/Graphics/UI/TypewriterEffect.h"
#include "../../engine/Graphics/Sprite/Sprite.h"
#include <vector>
#include <string>
#include <memory>

// ゲームオーバーシーン
class GameOverScene : public OSSceneBase {
public:
	GameOverScene();
	~GameOverScene() override;
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawUI() override;
	void PrewarmAssets() override;
	void Finalize() override;

private:
	UIText gameOverText_;
	
	UIButton retryButton_;
	UIButton titleButton_;

	// コンソール風ログ展開アニメーション & 高速スクロール16進数メモリダンプ
	std::vector<std::string> memoryDumpPool_;
	int dumpScrollIndex_ = 0;
	float dumpScrollTimer_ = 0.0f;
	float logDeployTimer_ = 0.0f;
	int visibleLogCount_ = 0;

	// システム再起動演出
	bool isRebooting_ = false;
	float rebootTimer_ = 0.0f;
	std::unique_ptr<Sprite> rebootBlackoutSprite_;

	// リトライ時のホワイトノイズ＆グリッチトランジション
	bool isRetrying_ = false;
	float retryGlitchTimer_ = 0.0f;

	// ボタンクリックフィードバック & ヘッダータイピング
	float btnClickFlashTimer_ = 0.0f;
	int clickedButtonIdx_ = -1;
	TypewriterEffect headerTypewriter_;

	// 各ウィンドウ内部の描画ヘルパー
	void DrawCrashRecoveryWindow(const PseudoOS::Window& win);
	void DrawMemoryExceptionWindow(const PseudoOS::Window& win);
	void DrawHardwareFailureWindow(const PseudoOS::Window& win);
};
