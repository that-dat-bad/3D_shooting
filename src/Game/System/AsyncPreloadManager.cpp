#include "AsyncPreloadManager.h"
#include <fstream>
#include <iostream>

AsyncPreloadManager* AsyncPreloadManager::GetInstance() {
    static AsyncPreloadManager instance;
    return &instance;
}

void AsyncPreloadManager::StartStagePreloadAsync(const std::string& missionPath, const std::string& aircraftCfgPath) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 既にプリロード中なら重複起動しない
    if (isPreloading_) {
        return;
    }

    // 既に同じパスの結果がキャッシュ済みなら再ロード不要
    if (cachedResult_ && cachedResult_->missionPath == missionPath && cachedResult_->aircraftPath == aircraftCfgPath) {
        return;
    }

    isPreloading_ = true;
    preloadFuture_ = std::async(std::launch::async, [missionPath, aircraftCfgPath]() -> PreloadResult {
        PreloadResult res;
        res.missionPath = missionPath;
        res.aircraftPath = aircraftCfgPath;

        // 1. ミッションデータの非同期パース
        try {
            std::ifstream file(missionPath);
            if (file.is_open()) {
                nlohmann::json j;
                file >> j;
                res.missionData = j.get<MissionData>();
                res.missionLoaded = true;
            }
        } catch (...) {
            res.missionLoaded = false;
        }

        // 2. 機体CFGの非同期パース
        try {
            if (res.aircraftConfig.LoadFromCfg(aircraftCfgPath)) {
                res.aircraftLoaded = true;
            }
        } catch (...) {
            res.aircraftLoaded = false;
        }

        return res;
    });
}

bool AsyncPreloadManager::IsStagePreloadReady() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cachedResult_) {
        return true;
    }
    if (isPreloading_ && preloadFuture_.valid()) {
        return (preloadFuture_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
    }
    return false;
}

void AsyncPreloadManager::WaitForStagePreload() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isPreloading_ && preloadFuture_.valid()) {
        cachedResult_ = std::make_unique<PreloadResult>(preloadFuture_.get());
        isPreloading_ = false;
    }
}

bool AsyncPreloadManager::ApplyPreloadedMission(MissionManager& missionManager) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 進行中のタスクを回収
    if (isPreloading_ && preloadFuture_.valid()) {
        if (preloadFuture_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            cachedResult_ = std::make_unique<PreloadResult>(preloadFuture_.get());
            isPreloading_ = false;
        }
    }

    if (cachedResult_ && cachedResult_->missionLoaded) {
        missionManager.GetCurrentMission() = cachedResult_->missionData;
        missionManager.SetCurrentFilePath(cachedResult_->missionPath);
        missionManager.ResetRuntimeState();
        return true;
    }
    return false;
}

bool AsyncPreloadManager::ApplyPreloadedAircraftConfig(AircraftConfig& aircraftConfig) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 進行中のタスクを回収
    if (isPreloading_ && preloadFuture_.valid()) {
        if (preloadFuture_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            cachedResult_ = std::make_unique<PreloadResult>(preloadFuture_.get());
            isPreloading_ = false;
        }
    }

    if (cachedResult_ && cachedResult_->aircraftLoaded) {
        aircraftConfig = cachedResult_->aircraftConfig;
        return true;
    }
    return false;
}

void AsyncPreloadManager::ClearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isPreloading_ && preloadFuture_.valid()) {
        preloadFuture_.wait();
        isPreloading_ = false;
    }
    cachedResult_.reset();
}
