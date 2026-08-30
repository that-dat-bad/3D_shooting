#include "MCPClient.h"
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <sstream>

MCPClient::~MCPClient() {
	StopPolling();
}

void MCPClient::Configure(const std::string& host, int port) {
	std::lock_guard<std::mutex> lock(mutex_);
	host_ = host;
	port_ = port;
}

void MCPClient::StartPolling(int intervalMs, std::function<void(const nlohmann::json&)> callback) {
	StopPolling(); // 既存のポーリングがあれば停止

	pollingIntervalMs_ = intervalMs;
	pollingCallback_ = callback;
	shouldStop_.store(false);
	isPolling_.store(true);

	pollingThread_ = std::thread(&MCPClient::PollingWorker, this);
}

void MCPClient::StopPolling() {
	shouldStop_.store(true);
	if (pollingThread_.joinable()) {
		pollingThread_.join();
	}
	isPolling_.store(false);
}

bool MCPClient::Get(const std::string& path, nlohmann::json& outJson) {
	std::string response;
	if (!SendRequest("GET", path, "", response)) {
		return false;
	}

	try {
		outJson = nlohmann::json::parse(response);
		isConnected_.store(true);
		return true;
	} catch (const nlohmann::json::parse_error& e) {
		std::lock_guard<std::mutex> lock(mutex_);
		lastError_ = std::string("JSON parse error: ") + e.what();
		return false;
	}
}

bool MCPClient::Post(const std::string& path, const nlohmann::json& body, nlohmann::json& outJson) {
	std::string bodyStr = body.dump();
	std::string response;
	if (!SendRequest("POST", path, bodyStr, response)) {
		return false;
	}

	try {
		outJson = nlohmann::json::parse(response);
		isConnected_.store(true);
		return true;
	} catch (const nlohmann::json::parse_error& e) {
		std::lock_guard<std::mutex> lock(mutex_);
		lastError_ = std::string("JSON parse error: ") + e.what();
		return false;
	}
}

bool MCPClient::SendRequest(const std::string& method, const std::string& path,
	const std::string& requestBody, std::string& outResponse) {

	// ホスト名をワイド文字列に変換
	std::wstring wHost(host_.begin(), host_.end());

	// WinHTTPセッションのオープン
	HINTERNET hSession = WinHttpOpen(
		L"MCPClient/1.0",
		WINHTTP_ACCESS_TYPE_NO_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0
	);

	if (!hSession) {
		std::lock_guard<std::mutex> lock(mutex_);
		lastError_ = "WinHttpOpen failed";
		isConnected_.store(false);
		return false;
	}

	// タイムアウト設定（接続: 2秒、送受信: 3秒）
	WinHttpSetTimeouts(hSession, 2000, 2000, 3000, 3000);

	// サーバーへの接続
	HINTERNET hConnect = WinHttpConnect(
		hSession,
		wHost.c_str(),
		static_cast<INTERNET_PORT>(port_),
		0
	);

	if (!hConnect) {
		std::lock_guard<std::mutex> lock(mutex_);
		lastError_ = "WinHttpConnect failed";
		isConnected_.store(false);
		WinHttpCloseHandle(hSession);
		return false;
	}

	// リクエストの作成
	std::wstring wPath(path.begin(), path.end());
	std::wstring wMethod(method.begin(), method.end());

	HINTERNET hRequest = WinHttpOpenRequest(
		hConnect,
		wMethod.c_str(),
		wPath.c_str(),
		NULL,
		WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		0
	);

	if (!hRequest) {
		std::lock_guard<std::mutex> lock(mutex_);
		lastError_ = "WinHttpOpenRequest failed";
		isConnected_.store(false);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	// ヘッダーの追加
	if (method == "POST") {
		WinHttpAddRequestHeaders(hRequest,
			L"Content-Type: application/json",
			-1L,
			WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
	}

	// リクエストの送信
	BOOL bResults = WinHttpSendRequest(
		hRequest,
		WINHTTP_NO_ADDITIONAL_HEADERS,
		0,
		method == "POST" ? (LPVOID)requestBody.c_str() : WINHTTP_NO_REQUEST_DATA,
		method == "POST" ? static_cast<DWORD>(requestBody.size()) : 0,
		method == "POST" ? static_cast<DWORD>(requestBody.size()) : 0,
		0
	);

	if (!bResults) {
		std::lock_guard<std::mutex> lock(mutex_);
		lastError_ = "WinHttpSendRequest failed (server may be offline)";
		isConnected_.store(false);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	// レスポンスの受信
	bResults = WinHttpReceiveResponse(hRequest, NULL);
	if (!bResults) {
		std::lock_guard<std::mutex> lock(mutex_);
		lastError_ = "WinHttpReceiveResponse failed";
		isConnected_.store(false);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	// レスポンスボディの読み取り
	std::string responseData;
	DWORD dwSize = 0;
	DWORD dwDownloaded = 0;

	do {
		dwSize = 0;
		if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
		if (dwSize == 0) break;

		std::vector<char> buffer(dwSize + 1, 0);
		if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) break;

		responseData.append(buffer.data(), dwDownloaded);
	} while (dwSize > 0);

	outResponse = responseData;

	// クリーンアップ
	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);

	return true;
}

void MCPClient::PollingWorker() {
	while (!shouldStop_.load()) {
		nlohmann::json data;
		if (Get("/terrain/params", data)) {
			if (pollingCallback_) {
				pollingCallback_(data);
			}
		}

		// ポーリング間隔分スリープ（中断可能に小刻みに）
		int slept = 0;
		while (slept < pollingIntervalMs_ && !shouldStop_.load()) {
			Sleep(100);
			slept += 100;
		}
	}
}
