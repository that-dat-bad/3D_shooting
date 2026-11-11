#pragma once
#include <string>
#include <memory>
#include <future>
#include <mutex>
#include "../FlightModel/AircraftConfig.h"
#include "MissionManager.h"

// バックグラウンドスレッドで重いステージデータ（ミッションJSON、機体CFG等）を非同期プリロードするマネージャー
class AsyncPreloadManager {
public:
    static AsyncPreloadManager* GetInstance();

    // バックグラウンドスレッドでステージリソースの事前非同期ロードを開始（ゲームオーバー演出中やリトライ待機中に発火）
    void StartStagePreloadAsync(const std::string& missionPath = "Resources/missions/mission00.json",
                                const std::string& aircraftCfgPath = "Resources/aircraft/mig21.cfg");

    // プリロードが完了しているか
    bool IsStagePreloadReady() const;

    // プリロード完了を同期待機
    void WaitForStagePreload();

    // キャッシュされたミッションデータを適用
    bool ApplyPreloadedMission(MissionManager& missionManager);

    // キャッシュされた機体CFGを適用
    bool ApplyPreloadedAircraftConfig(AircraftConfig& aircraftConfig);

    // キャッシュのクリア
    void ClearCache();

private:
    AsyncPreloadManager() = default;
    ~AsyncPreloadManager() = default;

    struct PreloadResult {
        bool missionLoaded = false;
        MissionData missionData;
        std::string missionPath;

        bool aircraftLoaded = false;
        AircraftConfig aircraftConfig;
        std::string aircraftPath;
    };

    mutable std::mutex mutex_;
    std::future<PreloadResult> preloadFuture_;
    std::unique_ptr<PreloadResult> cachedResult_;
    bool isPreloading_ = false;
};
