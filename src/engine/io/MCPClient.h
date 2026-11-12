#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include "json.hpp"

// MCPクライアント
class MCPClient {
public:
	MCPClient() = default;
	~MCPClient();

	// 接続設定
	// host: ホスト名
	// port: ポート番号
	void Configure(const std::string& host, int port);

	// 定期ポーリング開始
	// intervalMs: ポーリング間隔（ミリ秒）
	// callback: 受信時コールバック
	void StartPolling(
		int intervalMs,
		std::function<void(const nlohmann::json&)> callback,
		const std::string& endpointPath = "/terrain/params");

	// ポーリング停止
	void StopPolling();

	// GETリクエスト
	// path: リクエストパス
	// outJson: 受信データ
	bool Get(const std::string& path, nlohmann::json& outJson);

	// POSTリクエスト
	// path: リクエストパス
	// body: 送信データ
	bool Post(const std::string& path, const nlohmann::json& body, nlohmann::json& outJson);

	bool IsConnected() const { return isConnected_.load(); }
	bool IsPolling() const { return isPolling_.load(); }
	const std::string& GetLastError() const { return lastError_; }

private:
	// HTTPリクエスト送信
	// method: メソッド名
	// path: パス
	bool SendRequest(const std::string& method, const std::string& path,
		const std::string& requestBody, std::string& outResponse);

	// ワーカースレッド関数
	void PollingWorker();

	std::string host_ = "localhost";
	int port_ = 8765;

	std::atomic<bool> isConnected_{ false };
	std::atomic<bool> isPolling_{ false };
	std::atomic<bool> shouldStop_{ false };

	std::thread pollingThread_;
	int pollingIntervalMs_ = 1000;
	std::string pollingEndpoint_ = "/terrain/params";
	std::function<void(const nlohmann::json&)> pollingCallback_;

	std::atomic<int> failureCount_{ 0 };

	mutable std::mutex mutex_;
	std::string lastError_;
};
