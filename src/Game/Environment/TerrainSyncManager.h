#pragma once
#include <string>
#include <mutex>
#include "../../engine/io/MCPClient.h"
#include "../../engine/io/json.hpp"

/// @brief Blender地形ツールとのMCPリアルタイム同期を管理するクラス
/// MCPClientを使用して地形パラメータを定期的に取得し、
/// ImGuiウィンドウで表示・編集可能にする。
class TerrainSyncManager {
public:
	static TerrainSyncManager* GetInstance();

	/// @brief 初期化（MCP接続設定）
	/// @param host MCPサーバーのホスト名
	/// @param port MCPサーバーのポート番号
	void Initialize(const std::string& host = "localhost", int port = 8765);

	/// @brief 終了処理
	void Finalize();

	/// @brief 毎フレームの更新（ImGuiウィンドウの描画含む）
	void Update();

	/// @brief MCP同期の開始
	void StartSync();

	/// @brief MCP同期の停止
	void StopSync();

	/// @brief 同期中かどうか
	bool IsSyncing() const { return mcpClient_.IsPolling(); }

	/// @brief 接続されているかどうか
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

	/// @brief MCPからの受信コールバック
	void OnTerrainDataReceived(const nlohmann::json& data);

	/// @brief ImGuiウィンドウの描画
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
