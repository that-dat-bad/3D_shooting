#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <mutex>
#include "BenchmarkConfig.h"

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

/// @brief フレームタイム監視・ヒッチ検知・ログ記録マネージャー
class HitchDetector {
public:
	static HitchDetector* GetInstance() {
		static HitchDetector instance;
		return &instance;
	}

	/// @brief 毎フレームの開始時に呼び出し、DeltaTimeとヒッチを監視
	void OnBeginFrame() {
		auto now = std::chrono::steady_clock::now();
		if (hasLastFrame_) {
			double deltaMs = std::chrono::duration<double, std::milli>(now - lastFrameTime_).count();
			lastDeltaMs_ = deltaMs;

			// 初回または直後の異常値（起動時の初期化スパイク）を記録
			if (frameCount_ > 10) { // 最初の10フレームはエンジン初期化・ウィンドウ生成なので除外
				avgFrameTimeMs_ = (avgFrameTimeMs_ <= 0.0) ? deltaMs : (avgFrameTimeMs_ * 0.95 + deltaMs * 0.05);

				if (deltaMs > maxFrameTimeMs_) {
					maxFrameTimeMs_ = deltaMs;
				}

				// ヒッチ判定（25.0ms以上 = 60FPSの16.6msより半フレーム以上遅延）
				if (deltaMs >= hitchThresholdMs_) {
					hitchCount_++;
					double spikeMs = deltaMs - 16.666;
					double lostFrames = (deltaMs / 16.666) - 1.0;

					char logBuf[256];
					sprintf_s(logBuf, "[HITCH DETECTED] Frame #%llu: FrameTime = %.2f ms (+%.1f ms spike / ~%.1f dropped frames)",
						frameCount_, deltaMs, spikeMs, lostFrames);
					Benchmark::Log(logBuf);
				}

				// 重度ヒッチ判定（50.0ms以上 = 3フレーム以上画面停止）
				if (deltaMs >= severeThresholdMs_) {
					severeHitchCount_++;
				}
			}

			// グラフ用履歴更新
			frameHistory_[historyIndex_] = static_cast<float>(deltaMs);
			historyIndex_ = (historyIndex_ + 1) % kHistorySize;
		}
		lastFrameTime_ = now;
		hasLastFrame_ = true;
		frameCount_++;
	}

	/// @brief 統計データのリセット
	void ResetStats() {
		maxFrameTimeMs_ = 0.0;
		hitchCount_ = 0;
		severeHitchCount_ = 0;
		avgFrameTimeMs_ = 16.66;
		frameHistory_.assign(kHistorySize, 16.66f);
		Benchmark::Log("[HitchDetector] Statistics reset.");
	}

	double GetLastDeltaMs() const { return lastDeltaMs_; }
	double GetAvgFrameTimeMs() const { return avgFrameTimeMs_; }
	double GetMaxFrameTimeMs() const { return maxFrameTimeMs_; }
	uint64_t GetFrameCount() const { return frameCount_; }
	int GetHitchCount() const { return hitchCount_; }
	int GetSevereHitchCount() const { return severeHitchCount_; }

	/// @brief ImGuiによるヒッチ＆フレームタイム監視パネルの描画
	void DrawImGui() {
#ifdef USE_IMGUI
		if (ImGui::Begin("Hitch & Performance Monitor", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Frame #%llu", frameCount_);
			double curFps = (lastDeltaMs_ > 0.0) ? (1000.0 / lastDeltaMs_) : 0.0;
			ImGui::Text("Current Delta: %.2f ms (%.1f FPS)", lastDeltaMs_, curFps);
			ImGui::Text("Avg FrameTime: %.2f ms", avgFrameTimeMs_);

			// ピークヒッチ時間の評価表示
			if (maxFrameTimeMs_ >= 50.0) {
				ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Max Hitch Peak: %.2f ms (SEVERE STUTTER)", maxFrameTimeMs_);
			} else if (maxFrameTimeMs_ >= 25.0) {
				ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Max Hitch Peak: %.2f ms (Noticeable Hitch)", maxFrameTimeMs_);
			} else {
				ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Max Hitch Peak: %.2f ms (Smooth 60FPS)", maxFrameTimeMs_);
			}

			ImGui::Separator();
			ImGui::Text("Hitch Count (>25ms): %d", hitchCount_);
			ImGui::Text("Severe Hitch (>50ms): %d", severeHitchCount_);

			// フレーム時間グラフ
			ImGui::PlotLines("FrameTime (ms)", frameHistory_.data(), kHistorySize, historyIndex_,
				"Target: 16.6ms | Limit: 33.3ms", 0.0f, 80.0f, ImVec2(300, 70));

			if (ImGui::Button("Reset Stats")) {
				ResetStats();
			}
		}
		ImGui::End();
#endif
	}

private:
	HitchDetector() : frameHistory_(kHistorySize, 16.66f) {}

	static constexpr int kHistorySize = 120;
	std::vector<float> frameHistory_;
	int historyIndex_ = 0;

	std::chrono::steady_clock::time_point lastFrameTime_;
	bool hasLastFrame_ = false;
	uint64_t frameCount_ = 0;

	double lastDeltaMs_ = 16.66;
	double avgFrameTimeMs_ = 16.66;
	double maxFrameTimeMs_ = 0.0;

	float hitchThresholdMs_ = 25.0f;    // 1.5フレーム遅延
	float severeThresholdMs_ = 50.0f;   // 3フレーム遅延

	int hitchCount_ = 0;
	int severeHitchCount_ = 0;
};
