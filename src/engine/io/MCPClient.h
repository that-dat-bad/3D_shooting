#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include "json.hpp"

/// @brief MCP (Model Context Protocol) HTTPクライアント
/// Blender側のMCPサーバーと通信し、地形パラメータを取得する。
/// WinHTTPを使用した軽量実装。
class MCPClient {
public:
	MCPClient() = default;
	~MCPClient();

	/// @brief サーバーへの接続を設定
	/// @param host ホスト名 (例: "localhost")
	/// @param port ポート番号 (例: 8765)
	void Configure(const std::string& host, int port);

	/// @brief 定期ポーリングの開始
	/// @param intervalMs ポーリング間隔（ミリ秒）
	/// @param callback データ受信時のコールバック
	void StartPolling(int intervalMs, std::function<void(const nlohmann::json&)> callback);

	/// @brief ポーリングの停止
	void StopPolling();

	/// @brief 単発のGETリクエスト
	/// @param path リクエストパス (例: "/terrain/params")
	/// @param outJson 受信したJSONデータ
	/// @return 成功した場合 true
	bool Get(const std::string& path, nlohmann::json& outJson);

	/// @brief 単発のPOSTリクエスト
	/// @param path リクエストパス
	/// @param body 送信するJSONデータ
	/// @param outJson レスポンスのJSONデータ
	/// @return 成功した場合 true
	bool Post(const std::string& path, const nlohmann::json& body, nlohmann::json& outJson);

	/// @brief 接続状態の取得
	bool IsConnected() const { return isConnected_.load(); }

	/// @brief ポーリング中かどうか
	bool IsPolling() const { return isPolling_.load(); }

	/// @brief 最後のエラーメッセージを取得
	const std::string& GetLastError() const { return lastError_; }

private:
	/// @brief HTTPリクエストを送信して結果を取得する（内部実装）
	/// @param method "GET" or "POST"
	/// @param path パス
	/// @param requestBody POST時のボディ（GETの場合は空）
	/// @param outResponse レスポンス文字列
	/// @return 成功した場合 true
	bool SendRequest(const std::string& method, const std::string& path,
		const std::string& requestBody, std::string& outResponse);

	/// @brief ポーリング用のワーカースレッド関数
	void PollingWorker();

	std::string host_ = "localhost";
	int port_ = 8765;

	std::atomic<bool> isConnected_{ false };
	std::atomic<bool> isPolling_{ false };
	std::atomic<bool> shouldStop_{ false };

	std::thread pollingThread_;
	int pollingIntervalMs_ = 1000;
	std::function<void(const nlohmann::json&)> pollingCallback_;

	mutable std::mutex mutex_;
	std::string lastError_;
};
