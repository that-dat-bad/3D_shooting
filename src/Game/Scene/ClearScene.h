#pragma once
#include "IScene.h"
#include <vector>
#include <memory>
#include "../../engine/Graphics/UI/UIElement.h"
#include "../../engine/Graphics/UI/UIText.h"
#include "../../engine/Graphics/UI/UIButton.h"
#include "../../engine/Graphics/UI/UIPanel.h"
#include "../../engine/Graphics/UI/UISelectionManager.h"

/// @brief クリアシーン。ミッション完了時のリザルト表示と次のアクション選択。
class ClearScene : public IScene {
public:
    ClearScene();
    ~ClearScene();
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawUI() override;
    void Finalize() override;

private:
    // --- UI要素 ---
    UIPanel backgroundPanel_;          ///< 背景オーバーレイ
    UIPanel resultPanel_;              ///< リザルト情報パネル
    UIText missionCompleteText_;       ///< 「MISSION COMPLETE」テキスト
    UIText enemiesDestroyedLabel_;     ///< 「ENEMIES DESTROYED」ラベル
    UIText enemiesDestroyedValue_;     ///< 撃破数の値
    UIText clearTimeLabel_;            ///< 「CLEAR TIME」ラベル
    UIText clearTimeValue_;            ///< クリアタイムの値
    UIText hpRemainingLabel_;          ///< 「HP REMAINING」ラベル
    UIText hpRemainingValue_;          ///< 残りHP値
    UIButton replayButton_;            ///< リプレイボタン
    UIButton titleButton_;             ///< タイトルへ戻るボタン

    // メニュー操作
    UISelectionManager selectionManager_;

    // アニメーション
    float animTimer_ = 0.0f;
    float fadeInAlpha_ = 0.0f;         ///< フェードインの進行度
};
