#include "MissionSyncManager.h"

MissionSyncManager* MissionSyncManager::GetInstance() {
	static MissionSyncManager instance;
	return &instance;
}

MissionSyncManager::~MissionSyncManager() {
	Finalize();
}

void MissionSyncManager::Initialize(const std::string& host, int port) {
	host_ = host;
	port_ = port;
	mcpClient_.Configure(host_, port_);
	isInitialized_.store(true);
}

void MissionSyncManager::SetActiveMissionFilePath(const std::string& filePath) {
	if (activeMissionFilePath_ == filePath) {
		return;
	}
	activeMissionFilePath_ = filePath;
	if (mcpClient_.IsPolling()) {
		StopSync();
		StartSync();
	}
}

std::string MissionSyncManager::BuildLiveEndpoint() const {
	if (activeMissionFilePath_.empty()) {
		return MissionSyncDefaults::kLiveMissionEndpoint;
	}
	return std::string(MissionSyncDefaults::kLiveMissionFileQueryPrefix) + activeMissionFilePath_;
}

void MissionSyncManager::StartSync() {
	if (!isInitialized_.load()) {
		Initialize();
	}
	if (mcpClient_.IsPolling()) {
		return;
	}

	mcpClient_.StartPolling(
		MissionSyncDefaults::kPollIntervalMs,
		[this](const nlohmann::json& payload) {
			HandleIncomingPayload(payload);
		},
		BuildLiveEndpoint());
}

void MissionSyncManager::StopSync() {
	mcpClient_.StopPolling();
}

void MissionSyncManager::Finalize() {
	StopSync();
	isInitialized_.store(false);
}

bool MissionSyncManager::RequestSyncNow() {
	nlohmann::json payload;
	if (!mcpClient_.Get(BuildLiveEndpoint(), payload)) {
		return false;
	}
	HandleIncomingPayload(payload);
	return true;
}

void MissionSyncManager::HandleIncomingPayload(const nlohmann::json& payload) {
	using namespace MissionSyncDefaults;

	if (!payload.is_object() || !payload.contains(kKeyMissionData)) {
		return;
	}

	int64_t incomingRev = kInitialRevision;
	if (payload.contains(kKeyRevision) && payload.at(kKeyRevision).is_number_integer()) {
		incomingRev = payload.at(kKeyRevision).get<int64_t>();
	}

	int64_t incomingResetRev = kInitialRevision;
	if (payload.contains(kKeyResetRevision) && payload.at(kKeyResetRevision).is_number_integer()) {
		incomingResetRev = payload.at(kKeyResetRevision).get<int64_t>();
	}

	const int64_t prevRev = lastReceivedRevision_.load();
	const int64_t prevResetRev = lastReceivedResetRevision_.load();
	if (incomingRev <= prevRev && incomingResetRev <= prevResetRev) {
		return;
	}

	try {
		const MissionData parsedData = payload.at(kKeyMissionData).get<MissionData>();
		std::string filePath;
		if (payload.contains(kKeyMissionFilePath) && payload.at(kKeyMissionFilePath).is_string()) {
			filePath = payload.at(kKeyMissionFilePath).get<std::string>();
		}

		const bool restartTriggered = (incomingResetRev > prevResetRev);
		lastReceivedRevision_.store(incomingRev);
		lastReceivedResetRevision_.store(incomingResetRev);

		std::lock_guard<std::mutex> lock(payloadMutex_);
		pendingMissionData_ = parsedData;
		pendingMissionFilePath_ = filePath;
		pendingRestartRequested_ = pendingRestartRequested_ || restartTriggered;
		hasPendingUpdate_ = true;
	}
	catch (...) {
		// 不完全なJSON受信時は安全に破棄して次回ポーリングを待機
	}
}

bool MissionSyncManager::ConsumePendingUpdate(
	MissionData& outMissionData,
	std::string& outFilePath,
	bool& outRestartRequested) {
	if (!autoApplyEnabled_.load()) {
		return false;
	}

	std::lock_guard<std::mutex> lock(payloadMutex_);
	if (!hasPendingUpdate_) {
		return false;
	}

	outMissionData = pendingMissionData_;
	outFilePath = pendingMissionFilePath_;
	outRestartRequested = pendingRestartRequested_;

	hasPendingUpdate_ = false;
	pendingRestartRequested_ = false;
	lastAppliedRevision_.store(lastReceivedRevision_.load());
	return true;
}

