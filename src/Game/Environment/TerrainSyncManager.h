#pragma once
#include <string>
#include <mutex>
#include "../../engine/io/MCPClient.h"
#include "../../engine/io/json.hpp"

// 地形同期管理クラス
class TerrainSyncManager {
public:
	static TerrainSyncManager* GetInstance();

	// 初期化
	// host: ホスト名
	// port: ポート番号
	void Initialize(const std::string& host = "localhost", int port = 8765);

	// 終了処理
	void Finalize();

	// 更新
	void Update();

	// 同期開始
	void StartSync();

	// 同期停止
	void StopSync();

	bool IsSyncing() const { return mcpClient_.IsPolling(); }
	bool IsConnected() const { return mcpClient_.IsConnected(); }

	// --- 地形パラメータアクセサ ---
	float GetGridSizeX() const { return gridSizeX_; }
	float GetGridSizeY() const { return gridSizeY_; }
	int GetSubdivisionsX() const { return subdivisionsX_; }
	int GetSubdivisionsY() const { return subdivisionsY_; }
	float GetNoiseScale() const { return noiseScale_; }
	float GetTerrainHeight() const { return terrainHeight_; }
	float GetMountainHeight() const { return mountainHeight_; }
	float GetMountainRadius() const { return mountainRadius_; }
	float GetValleyDepth() const { return valleyDepth_; }
	float GetValleyRadius() const { return valleyRadius_; }

private:
	TerrainSyncManager() = default;
	~TerrainSyncManager() = default;
	TerrainSyncManager(const TerrainSyncManager&) = delete;
	TerrainSyncManager& operator=(const TerrainSyncManager&) = delete;

	// 受信コールバック
	void OnTerrainDataReceived(const nlohmann::json& data);

	// ImGui描画
	void DrawImGui();

	MCPClient mcpClient_;

	// 地形パラメータ（スレッドセーフに更新）
	mutable std::mutex paramMutex_;
	float gridSizeX_ = 200.0f;
	float gridSizeY_ = 200.0f;
	int subdivisionsX_ = 10;
	int subdivisionsY_ = 10;
	float noiseScale_ = 0.05f;
	float terrainHeight_ = 10.0f;
	float mountainHeight_ = 15.0f;
	float mountainRadius_ = 10.0f;
	float valleyDepth_ = 8.0f;
	float valleyRadius_ = 8.0f;
	float flatRadius_ = 3.0f;
	float flatBlend_ = 2.0f;
	float holeSize_ = 0.0f;

	// 接続設定
	std::string host_ = "localhost";
	int port_ = 8765;
	int pollingIntervalMs_ = 500; // 0.5秒間隔

	// ステータス
	bool hasReceivedData_ = false;
	std::string lastUpdateTime_;
};
