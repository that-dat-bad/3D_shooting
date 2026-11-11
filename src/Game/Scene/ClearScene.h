#pragma once
#include "OSSceneBase.h"
#include <vector>
#include <memory>
#include "../../engine/Graphics/UI/UIElement.h"
#include "../../engine/Graphics/UI/UIText.h"
#include "../../engine/Graphics/UI/UIButton.h"

namespace HalcyonClearDefaults {
	inline constexpr int kDefaultMainWinId = 0;
	inline constexpr int kReplayButtonIndex = 0;
	inline constexpr int kReturnButtonIndex = 1;
	inline constexpr int kSecondsPerMinute = 60;
	inline constexpr size_t kRecordBufferSize = 64;

	inline constexpr const char* kFontName = "HackGen";
	inline constexpr const char* kDefaultMainWinTitle = "DEBRIEF : OPERATION DAWN [SUCCESS]";
	inline constexpr const char* kTutorialMainWinTitle = "DEBRIEF : OPERATION HALCYON DAYS [COMPLETE]";
	inline constexpr const char* kDefaultReturnBtnLabel = "[02] RETURN TO BASE";
	inline constexpr const char* kTutorialReturnBtnLabel = "[02] RETURN TO HANGAR";
	inline constexpr const char* kDefaultMissionHeader = "MISSION COMPLETE // DEBRIEF";
	inline constexpr const char* kTutorialMissionHeader = "OPERATION: HALCYON DAYS";

	inline constexpr const char* kHeaderSysLine = "[SYS] OPERATION: HALCYON DAYS // MISSION STATUS: COMPLETE";
	inline constexpr const char* kDividerLine = "--------------------------------------------------";
	inline constexpr const char* kFlightRecordFormat = "FLIGHT RECORD    : %02d MIN %02d SEC";
	inline constexpr const char* kHostileContactsLine = "HOSTILE CONTACTS : 01 (CONFIRMED DESTROYED)";
	inline constexpr const char* kTargetAnalysisLine = "TARGET ANALYSIS  : WESTERN BLOC JET FIGHTER [UNMARKED]";
	inline constexpr const char* kIntelLine1 = "[INTEL] \"第4監視所の生存者なし。侵入機に残されたフライトレコーダの暗号は、";
	inline constexpr const char* kIntelLine2 = "         本国（アヴリスク）中央指令部へ極秘転送された。\"";
	inline constexpr const char* kPressReturnLine = "[SYS] PRESS [ENTER / SPACE] TO RETURN TO HANGAR";

	inline constexpr const char* kTutorialTheaterLine = "SORTIE THEATER : VERNA OUTER CANYON [POLAR DUSK]";
	inline constexpr const char* kTutorialAirframeLine = "FLIGHT AIRFRAME: M-21A [STAR-BORDER-04 // ROOKIE]";
	inline constexpr const char* kTutorialBogeyLine1 = "[X] DRONE-01 [ORANGE] (EXPENDED)";
	inline constexpr const char* kTutorialBogeyLine2 = "[X] WESTERN JET [UNMARKED] (DOWNED)";
	inline constexpr const char* kTutorialIntel1 = "OUTPOST 04     : [NO SURVIVORS // CARRIER LOST]";
	inline constexpr const char* kTutorialIntel2 = "HOSTILE SPEC   : [WESTERN BLOC JET // UNMARKED]";
	inline constexpr const char* kTutorialIntel3 = "WING BREAK BDA : [PORT WING STRUCTURAL BREAK]";
	inline constexpr const char* kTutorialIntel4 = "FLIGHT RECORDER: [ENCRYPTED -> AVRISK CENTRAL HQ]";

	inline constexpr float kCenterRatio = 0.5f;
	inline constexpr float kContentTopOffset = 14.0f;
	inline constexpr float kHeaderAdvanceY = 40.0f;
	inline constexpr float kPanelMarginX = 18.0f;
	inline constexpr float kPanelDoubleMarginX = 36.0f;
	inline constexpr float kPanelHeight = 296.0f;
	inline constexpr float kPanelBorderThickness = 1.0f;
	inline constexpr float kTerminalPadX = 28.0f;
	inline constexpr float kTerminalStartOffsetY = 16.0f;
	inline constexpr float kSysHeaderFontSize = 13.5f;
	inline constexpr float kDividerFontSize = 13.0f;
	inline constexpr float kRecordFontSize = 14.0f;
	inline constexpr float kIntelFontSize = 12.2f;
	inline constexpr float kPromptFontSize = 13.5f;
	inline constexpr float kLineStepTight = 20.0f;
	inline constexpr float kLineStepRecord = 24.0f;
	inline constexpr float kSectionGap = 14.0f;
	inline constexpr float kIntelLineStep = 21.0f;
	inline constexpr float kAfterPanelGap = 16.0f;
	inline constexpr float kGaugeHeight = 10.0f;
	inline constexpr float kGaugePadX = 24.0f;
	inline constexpr float kGaugeMarginW = 48.0f;
	inline constexpr float kGaugeLabelSize = 11.5f;
	inline constexpr float kGaugeLabelStep = 16.0f;
	inline constexpr float kGaugeBottomStep = 22.0f;
	inline constexpr float kPromptBlinkPeriod = 1.10f;
	inline constexpr float kPromptBlinkOnWindow = 0.72f;
	inline constexpr float kPulseFreq = 4.0f;
	inline constexpr float kPulseBase = 0.96f;
	inline constexpr float kPulseAmp = 0.04f;
	inline constexpr float kFullGaugeRatio = 1.0f;

	inline constexpr MyMath::Vector4 kPanelBgColor = { 0.01f, 0.03f, 0.05f, 0.94f };
	inline constexpr MyMath::Vector4 kPanelBorderColor = { 0.22f, 0.72f, 0.88f, 0.80f };
	inline constexpr MyMath::Vector4 kSysHeaderColor = { 0.38f, 1.00f, 0.78f, 1.00f };
	inline constexpr MyMath::Vector4 kDividerColor = { 0.25f, 0.65f, 0.80f, 0.85f };
	inline constexpr MyMath::Vector4 kRecordTextColor = { 0.92f, 0.98f, 1.00f, 1.00f };
	inline constexpr MyMath::Vector4 kIntelTextColor = { 0.68f, 0.92f, 1.00f, 0.98f };
	inline constexpr MyMath::Vector4 kPromptOnColor = { 0.42f, 1.00f, 0.76f, 1.00f };
	inline constexpr MyMath::Vector4 kPromptDimColor = { 0.22f, 0.62f, 0.48f, 0.72f };
	inline constexpr MyMath::Vector4 kGaugeLabelColor = { 0.35f, 0.90f, 1.00f, 0.98f };
	inline constexpr MyMath::Vector4 kGaugeBorderColor = { 0.20f, 0.65f, 0.85f, 0.75f };
	inline constexpr Vector2 kTopCenterAnchor = { 0.5f, 0.0f };
}

// クリアシーン
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
    UIText missionCompleteText_;
    UIText enemiesDestroyedLabel_;
    UIText enemiesDestroyedValue_;
    UIText clearTimeLabel_;
    UIText clearTimeValue_;
    UIText hpRemainingLabel_;
    UIText hpRemainingValue_;
    UIButton replayButton_;
    UIButton titleButton_;

    // VHSテープの巻き戻し演出
    bool isRewinding_ = false;
    float rewindTimer_ = 0.0f;
    static constexpr float kRewindDuration = 0.90f;

    // データアップロードとウィンドウ閉鎖シーケンス
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
