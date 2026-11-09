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

/// @brief Blender Mission Editor アドオンと HTTP/JSON ライブ同期を行う管理クラス
class MissionSyncManager {
public:
	static MissionSyncManager* GetInstance();

	/// @brief 接続先ホスト・ポートの初期化
	void Initialize(
		const std::string& host = MissionSyncDefaults::kDefaultHost,
		int port = MissionSyncDefaults::kDefaultPort);

	/// @brief 現在ゲーム側で開いているミッションJSON相対パスを設定
	void SetActiveMissionFilePath(const std::string& filePath);

	/// @brief 定期ポーリング開始
	void StartSync();

	/// @brief 定期ポーリング停止
	void StopSync();

	/// @brief 終了処理
	void Finalize();

	/// @brief メインスレッド側で未適用のミッション更新を取り出す
	/// @param outMissionData 最新のミッションデータ出力先
	/// @param outFilePath 対象ミッションファイルパス出力先
	/// @param outRestartRequested Blender側からフルリスタート要求があった場合 true
	/// @return 新しいリビジョンを受信していた場合 true
	bool ConsumePendingUpdate(
		MissionData& outMissionData,
		std::string& outFilePath,
		bool& outRestartRequested);

	/// @brief 単発で即時取得を試行する
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

