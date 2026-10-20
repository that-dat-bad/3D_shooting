#pragma once
#include "OSSceneBase.h"
#include <vector>
#include <memory>
#include "../../engine/Graphics/UI/UIElement.h"
#include "../../engine/Graphics/UI/UIText.h"
#include "../../engine/Graphics/UI/UIButton.h"

/// @brief クリアシーン。疑似OS環境によるミッション完了リザルト表示と次のアクション選択。
class ClearScene : public OSSceneBase {
public:
    ClearScene();
    ~ClearScene() override;
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawUI() override;
    void PrewarmAssets() override;
    void Finalize() override;

private:
    // --- UI要素 ---
    UIText missionCompleteText_;       ///< 「MISSION COMPLETE」テキスト
    UIText enemiesDestroyedLabel_;     ///< 「ENEMIES DESTROYED」ラベル
    UIText enemiesDestroyedValue_;     ///< 撃破数の値
    UIText clearTimeLabel_;            ///< 「CLEAR TIME」ラベル
    UIText clearTimeValue_;            ///< クリアタイムの値
    UIText hpRemainingLabel_;          ///< 「HP REMAINING」ラベル
    UIText hpRemainingValue_;          ///< 残りHP値
    UIButton replayButton_;            ///< リプレイボタン
    UIButton titleButton_;             ///< タイトルへ戻るボタン

    // 項目15: VHSテープのリワインド（巻き戻し）演出
    bool isRewinding_ = false;
    float rewindTimer_ = 0.0f;
    static constexpr float kRewindDuration = 0.90f;

    // 項目16, 17, 18: データアップロードとウィンドウ閉鎖シーケンス
    bool isUploading_ = false;
    float uploadTimer_ = 0.0f;
    std::unique_ptr<Sprite> uploadSprite_;

    // 各ウィンドウ描画ヘルパー
    void DrawDebriefWindow(const PseudoOS::Window& win);
    void DrawFlightLogWindow(const PseudoOS::Window& win);
    void DrawIntelAssessmentWindow(const PseudoOS::Window& win);
    void DrawTheaterReconWindow(const PseudoOS::Window& win);

    // 演出UI描画ヘルパー
    void DrawRewindEffectUI();
    void DrawUploadSequenceUI();
};
