#include "TerrainSyncManager.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

TerrainSyncManager* TerrainSyncManager::GetInstance() {
	static TerrainSyncManager instance;
	return &instance;
}

void TerrainSyncManager::Initialize(const std::string& host, int port) {
	host_ = host;
	port_ = port;
	mcpClient_.Configure(host, port);
}

void TerrainSyncManager::Finalize() {
	StopSync();
}

void TerrainSyncManager::StartSync() {
	mcpClient_.Configure(host_, port_);
	mcpClient_.StartPolling(pollingIntervalMs_, [this](const nlohmann::json& data) {
		OnTerrainDataReceived(data);
	});
}

void TerrainSyncManager::StopSync() {
	mcpClient_.StopPolling();
}

void TerrainSyncManager::Update() {
#ifdef USE_IMGUI
	DrawImGui();
#endif
}

void TerrainSyncManager::OnTerrainDataReceived(const nlohmann::json& data) {
	std::lock_guard<std::mutex> lock(paramMutex_);

	if (data.contains("params") && !data["params"].is_null()) {
		auto& params = data["params"];

		if (params.contains("Grid Size X")) gridSizeX_ = params["Grid Size X"].get<float>();
		if (params.contains("Grid Size Y")) gridSizeY_ = params["Grid Size Y"].get<float>();
		if (params.contains("Subdivisions X")) subdivisionsX_ = params["Subdivisions X"].get<int>();
		if (params.contains("Subdivisions Y")) subdivisionsY_ = params["Subdivisions Y"].get<int>();
		if (params.contains("Noise Scale")) noiseScale_ = params["Noise Scale"].get<float>();
		if (params.contains("Terrain Height")) terrainHeight_ = params["Terrain Height"].get<float>();
		if (params.contains("Mountain Height")) mountainHeight_ = params["Mountain Height"].get<float>();
		if (params.contains("Mountain Radius")) mountainRadius_ = params["Mountain Radius"].get<float>();
		if (params.contains("Valley Depth")) valleyDepth_ = params["Valley Depth"].get<float>();
		if (params.contains("Valley Radius")) valleyRadius_ = params["Valley Radius"].get<float>();
		if (params.contains("Flat Radius")) flatRadius_ = params["Flat Radius"].get<float>();
		if (params.contains("Flat Blend")) flatBlend_ = params["Flat Blend"].get<float>();
		if (params.contains("Hole Size")) holeSize_ = params["Hole Size"].get<float>();

		hasReceivedData_ = true;

		// 更新時刻の記録
		auto now = std::chrono::system_clock::now();
		auto t = std::chrono::system_clock::to_time_t(now);
		std::tm local_tm;
		localtime_s(&local_tm, &t);
		std::ostringstream oss;
		oss << std::put_time(&local_tm, "%H:%M:%S");
		lastUpdateTime_ = oss.str();
	}
}

void TerrainSyncManager::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Terrain MCP Sync");

	// --- 接続コントロール ---
	ImGui::Text("Server: %s:%d", host_.c_str(), port_);

	bool isSyncing = mcpClient_.IsPolling();
	bool isConnected = mcpClient_.IsConnected();

	// 接続ステータスの色付き表示
	if (isConnected) {
		ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "Status: CONNECTED");
	} else if (isSyncing) {
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Status: CONNECTING...");
	} else {
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Status: DISCONNECTED");
	}

	if (!isSyncing) {
		if (ImGui::Button("Start Sync")) {
			StartSync();
		}
	} else {
		if (ImGui::Button("Stop Sync")) {
			StopSync();
		}
	}

	ImGui::SameLine();
	ImGui::Text("Interval: %dms", pollingIntervalMs_);

	if (!mcpClient_.GetLastError().empty()) {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", mcpClient_.GetLastError().c_str());
	}

	ImGui::Separator();

	// --- 受信済みパラメータの表示 ---
	if (hasReceivedData_) {
		ImGui::Text("Last Update: %s", lastUpdateTime_.c_str());
		ImGui::Separator();

		std::lock_guard<std::mutex> lock(paramMutex_);

		ImGui::Text("=== Grid ===");
		ImGui::Text("  Size X: %.1f", gridSizeX_);
		ImGui::Text("  Size Y: %.1f", gridSizeY_);
		ImGui::Text("  Subdivisions X: %d", subdivisionsX_);
		ImGui::Text("  Subdivisions Y: %d", subdivisionsY_);

		ImGui::Separator();
		ImGui::Text("=== Terrain ===");
		ImGui::Text("  Noise Scale: %.4f", noiseScale_);
		ImGui::Text("  Terrain Height: %.1f", terrainHeight_);

		ImGui::Separator();
		ImGui::Text("=== Mountain ===");
		ImGui::Text("  Height: %.1f", mountainHeight_);
		ImGui::Text("  Radius: %.1f", mountainRadius_);

		ImGui::Separator();
		ImGui::Text("=== Valley ===");
		ImGui::Text("  Depth: %.1f", valleyDepth_);
		ImGui::Text("  Radius: %.1f", valleyRadius_);

		ImGui::Separator();
		ImGui::Text("=== Flat ===");
		ImGui::Text("  Radius: %.1f", flatRadius_);
		ImGui::Text("  Blend: %.1f", flatBlend_);

		ImGui::Separator();
		ImGui::Text("=== Hole ===");
		ImGui::Text("  Size: %.1f", holeSize_);
	} else {
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No data received yet...");
	}

	ImGui::End();
#endif
}
