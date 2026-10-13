#include "MCPClient.h"
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <sstream>
#include <chrono>

namespace {
// WinHTTPハンドルのRAIIラッパー（スレッド内完結で二重解放やリークを100%防止）
struct ScopedWinHttpHandle {
	HINTERNET handle = nullptr;
	ScopedWinHttpHandle(HINTERNET h = nullptr) : handle(h) {}
	~ScopedWinHttpHandle() {
		if (handle) {
			WinHttpCloseHandle(handle);
			handle = nullptr;
		}
	}
	void Reset(HINTERNET h = nullptr) {
		if (handle) {
			WinHttpCloseHandle(handle);
		}
		handle = h;
	}
	operator HINTERNET() const { return handle; }
	bool IsValid() const { return handle != nullptr; }

	// コピー禁止、ムーブ許可
	ScopedWinHttpHandle(const ScopedWinHttpHandle&) = delete;
	ScopedWinHttpHandle& operator=(const ScopedWinHttpHandle&) = delete;
	ScopedWinHttpHandle(ScopedWinHttpHandle&& other) noexcept : handle(other.handle) {
		other.handle = nullptr;
	}
	ScopedWinHttpHandle& operator=(ScopedWinHttpHandle&& other) noexcept {
		if (this != &other) {
			Reset(other.handle);
			other.handle = nullptr;
		}
		return *this;
	}
};
}

MCPClient::~MCPClient() {
	StopPolling();
}

void MCPClient::Configure(const std::string& host, int port) {
	std::lock_guard<std::mutex> lock(mutex_);
	host_ = host;
	port_ = port;
}

void MCPClient::StartPolling(int intervalMs, std::function<void(const nlohmann::json&)> callback) {
	StopPolling(); // 既存のポーリングがあれば安全に停止

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

	if (shouldStop_.load()) return false;

	// ホスト名をワイド文字列に変換
	std::wstring wHost(host_.begin(), host_.end());

	// 1. セッションのオープン
	ScopedWinHttpHandle hSession(WinHttpOpen(
		L"MCPClient/1.0",
		WINHTTP_ACCESS_TYPE_NO_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0
	));

	if (!hSession.IsValid()) {
		std::lock_guard<std::mutex> lock(mutex_);
		lastError_ = "WinHttpOpen failed";
		isConnected_.store(false);
		return false;
	}

	// タイムアウト設定（ローカルホスト専用: 50msで即時復帰）
	WinHttpSetTimeouts(hSession, 50, 50, 50, 50);

	if (shouldStop_.load()) return false;

	// 2. サーバーへの接続
	ScopedWinHttpHandle hConnect(WinHttpConnect(
		hSession,
		wHost.c_str(),
		static_cast<INTERNET_PORT>(port_),
		0
	));

	if (!hConnect.IsValid()) {
		std::lock_guard<std::mutex> lock(mutex_);
		lastError_ = "WinHttpConnect failed";
		isConnected_.store(false);
		return false;
	}

	if (shouldStop_.load()) return false;

	// 3. リクエストの作成
	std::wstring wPath(path.begin(), path.end());
	std::wstring wMethod(method.begin(), method.end());

	ScopedWinHttpHandle hRequest(WinHttpOpenRequest(
		hConnect,
		wMethod.c_str(),
		wPath.c_str(),
		NULL,
		WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		0
	));

	if (!hRequest.IsValid()) {
		std::lock_guard<std::mutex> lock(mutex_);
		lastError_ = "WinHttpOpenRequest failed";
		isConnected_.store(false);
		return false;
	}

	// ヘッダーの追加
	if (method == "POST") {
		WinHttpAddRequestHeaders(hRequest,
			L"Content-Type: application/json",
			-1L,
			WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
	}

	if (shouldStop_.load()) return false;

	// 4. リクエストの送信
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
		return false;
	}

	if (shouldStop_.load()) return false;

	// 5. レスポンスの受信
	bResults = WinHttpReceiveResponse(hRequest, NULL);
	if (!bResults) {
		std::lock_guard<std::mutex> lock(mutex_);
		lastError_ = "WinHttpReceiveResponse failed";
		isConnected_.store(false);
		return false;
	}

	// 6. レスポンスボディの読み取り
	std::string responseData;
	DWORD dwSize = 0;
	DWORD dwDownloaded = 0;

	while (!shouldStop_.load()) {
		dwSize = 0;
		if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;

		std::vector<char> buffer(dwSize + 1, 0);
		if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) break;

		responseData.append(buffer.data(), dwDownloaded);
	}

	outResponse = responseData;
	return !responseData.empty();
}

void MCPClient::PollingWorker() {
	while (!shouldStop_.load()) {
		nlohmann::json data;
		if (Get("/terrain/params", data)) {
			failureCount_.store(0);
			if (pollingCallback_) {
				pollingCallback_(data);
			}
		} else {
			failureCount_++;
		}

		if (shouldStop_.load()) break;

		// サーバーオフライン時は指数バックオフ（3秒待機）
		int waitMs = (failureCount_.load() > 0) ? 3000 : pollingIntervalMs_;

		// 10ms単位で小刻みにスリープし、shouldStop_ が立ったら即時脱出
		int elapsed = 0;
		while (elapsed < waitMs && !shouldStop_.load()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			elapsed += 10;
		}
	}
}
