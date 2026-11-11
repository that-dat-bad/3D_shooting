#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include "../../engine/io/MCPClient.h"
#include "../System/MissionManager.h"

namespace MissionSyncDefaults {
	inline constexpr const char* kDefaultHost = "localhost";
	inline constexpr int kDefaultPort = 8766;
	inline constexpr int kPollIntervalMs = 250;
	inline constexpr const char* kLiveMissionEndpoint = "/mission/live";
	inline constexpr const char* kLiveMissionFileQueryPrefix = "/mission/live?file=";
	inline constexpr const char* kDefaultMissionFilePath = "Resources/missions/mission00.json";
	inline constexpr int64_t kInitialRevision = 0;

	// JSON キー定数
	inline constexpr const char* kKeyRevision = "revision";
	inline constexpr const char* kKeyResetRevision = "resetRevision";
	inline constexpr const char* kKeyMissionFilePath = "missionFilePath";
	inline constexpr const char* kKeyMissionData = "missionData";
}

// ミッション同期管理クラス
class MissionSyncManager {
public:
	static MissionSyncManager* GetInstance();

	// 接続設定
	void Initialize(
		const std::string& host = MissionSyncDefaults::kDefaultHost,
		int port = MissionSyncDefaults::kDefaultPort);

	// 対象ミッションファイルパスの設定
	void SetActiveMissionFilePath(const std::string& filePath);

	// 同期開始
	void StartSync();

	// 同期停止
	void StopSync();

	// 終了処理
	void Finalize();

	// 保留中の更新を取り出す
	// outMissionData: ミッションデータ出力先
	// outFilePath: ファイルパス出力先
	// outRestartRequested: リスタート要求フラグ出力先
	bool ConsumePendingUpdate(
		MissionData& outMissionData,
		std::string& outFilePath,
		bool& outRestartRequested);

	// 即時同期要求
	bool RequestSyncNow();

	bool IsConnected() const { return mcpClient_.IsConnected(); }
	bool IsSyncing() const { return mcpClient_.IsPolling(); }
	bool IsAutoApplyEnabled() const { return autoApplyEnabled_.load(); }
	void SetAutoApplyEnabled(bool enabled) { autoApplyEnabled_.store(enabled); }
	int64_t GetLastAppliedRevision() const { return lastAppliedRevision_.load(); }

private:
	MissionSyncManager() = default;
	~MissionSyncManager();
	MissionSyncManager(const MissionSyncManager&) = delete;
	MissionSyncManager& operator=(const MissionSyncManager&) = delete;

	void HandleIncomingPayload(const nlohmann::json& payload);
	std::string BuildLiveEndpoint() const;

private:
	MCPClient mcpClient_;
	std::string host_ = MissionSyncDefaults::kDefaultHost;
	int port_ = MissionSyncDefaults::kDefaultPort;
	std::string activeMissionFilePath_;

	std::atomic<bool> isInitialized_{ false };
	std::atomic<bool> autoApplyEnabled_{ true };
	std::atomic<int64_t> lastReceivedRevision_{ MissionSyncDefaults::kInitialRevision };
	std::atomic<int64_t> lastReceivedResetRevision_{ MissionSyncDefaults::kInitialRevision };
	std::atomic<int64_t> lastAppliedRevision_{ MissionSyncDefaults::kInitialRevision };

	mutable std::mutex payloadMutex_;
	bool hasPendingUpdate_ = false;
	bool pendingRestartRequested_ = false;
	MissionData pendingMissionData_{};
	std::string pendingMissionFilePath_;
};

