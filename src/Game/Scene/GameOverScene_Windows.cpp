#include "GameOverSceneInternal.h"

using namespace GameOverSceneInternal;

void GameOverScene::DrawCrashRecoveryWindow(const PseudoOS::Window& win) {
	TextRenderer* tr = TextRenderer::GetInstance();
	float cx = win.pos.x + win.size.x * 0.5f;
	float cy = win.pos.y + win.titleBarHeight + 14.0f;

	// 1. カーネルパニック大見出し
	gameOverText_.Draw();
	cy += 48.0f;

	if (GameResult::GetInstance().gameOverReason == GameOverReason::TimeOver) {
		tr->Print("HackGen", "*** OPERATION TIMEOUT: EXCEPTION_TIME_EXPIRED (0x00000042) ***",
			cx, cy, 14.0f, { 1.0f, 0.75f, 0.35f, 0.95f }, { 0.5f, 0.0f });
	} else {
		tr->Print("HackGen", "*** CRITICAL KERNEL PANIC: EXCEPTION_STAGE_FAILURE (0xC0000005) ***",
			cx, cy, 14.0f, { 1.0f, 0.75f, 0.35f, 0.95f }, { 0.5f, 0.0f });
	}
	cy += 24.0f;

	// 2. メモリダンプ表示枠（深紅のターミナル枠）
	float dumpFrameW = win.size.x - 36.0f;
	float dumpFrameH = 216.0f;
	pseudoOS_.DrawPanel({ win.pos.x + 18.0f, cy }, { dumpFrameW, dumpFrameH },
		{ 0.02f, 0.005f, 0.005f, 0.90f }, { 0.55f, 0.15f, 0.15f, 0.70f }, 1.0f);

	float dumpStartX = win.pos.x + 28.0f;
	float dumpStartY = cy + 12.0f;
	int displayLineCount = visibleLogCount_;

	for (int i = 0; i < displayLineCount; ++i) {
		int idx = (dumpScrollIndex_ + i) % static_cast<int>(memoryDumpPool_.size());
		float lineY = dumpStartY + i * 26.0f;
		float lineAlpha = 0.95f - (static_cast<float>(i) / 6.0f) * 0.15f;

		Vector4 textColor = (i == 0 || i == 3)
			? Vector4{ 1.0f, 0.90f, 0.40f, lineAlpha }
			: Vector4{ 0.98f, 0.40f, 0.35f, lineAlpha };

		std::string lineStr = memoryDumpPool_[idx];
		if (visibleLogCount_ < 6 && i == visibleLogCount_ - 1) {
			lineStr += "_";
		}

		tr->Print("HackGen", lineStr.c_str(), dumpStartX, lineY, 14.0f, textColor);
	}

	if (visibleLogCount_ < 6) {
		float nextY = dumpStartY + visibleLogCount_ * 26.0f;
		float cursorPulse = (std::fmod(animTimer_, 0.20f) < 0.10f) ? 1.0f : 0.0f;
		tr->Print("HackGen", "> _", dumpStartX, nextY, 14.0f, { 1.0f, 0.60f, 0.60f, cursorPulse });
	}

	cy += dumpFrameH + 16.0f;

	// 3. システム情報 & アクション案内
	pseudoOS_.DrawSpectrumBars({ win.pos.x + 24.0f, cy }, dumpFrameW - 12.0f, 18.0f, 20, 4.0f, { 0.85f, 0.25f, 0.20f, 0.85f });
	cy += 28.0f;

	tr->Print("HackGen", "CORE MEMORY DUMP SAVED // SELECT RECOVERY PROTOCOL TO CONTINUE",
		cx, cy, 12.5f, { 1.0f, 0.80f, 0.40f, 0.95f }, { 0.5f, 0.0f });

	// 4. ボタン描画
	retryButton_.Draw();
	titleButton_.Draw();
}

void GameOverScene::DrawMemoryExceptionWindow(const PseudoOS::Window& win) {
	TextRenderer* tr = TextRenderer::GetInstance();
	float startX = win.pos.x + 16.0f;
	float cy = win.pos.y + win.titleBarHeight + 12.0f;

	tr->Print("HackGen", "FAULT ADDR : 0x00007FF7B4C82A10", startX, cy, 11.5f, { 1.0f, 0.45f, 0.40f, 0.98f });
	cy += 18.0f;
	tr->Print("HackGen", "FAULT CODE : STATUS_ACCESS_VIOLATION", startX, cy, 11.0f, { 1.0f, 0.85f, 0.35f, 0.90f });
	cy += 18.0f;
	tr->Print("HackGen", "EXCEPTION  : EXCEPTION_STAGE_FAILURE", startX, cy, 11.0f, { 1.0f, 0.35f, 0.30f, 0.90f });
	cy += 22.0f;

	// レジスタダンプ枠
	float frameW = win.size.x - 32.0f;
	pseudoOS_.DrawPanel({ startX, cy }, { frameW, 88.0f }, { 0.02f, 0.005f, 0.005f, 0.80f }, { 0.50f, 0.15f, 0.12f, 0.65f }, 1.0f);
	tr->Print("HackGen", "RAX: 0x00000000  RBX: 0x00007FFF", startX + 8.0f, cy + 8.0f, 10.5f, { 0.90f, 0.60f, 0.35f, 0.85f });
	tr->Print("HackGen", "RCX: 0xC0000005  RDX: 0x00000000", startX + 8.0f, cy + 24.0f, 10.5f, { 0.90f, 0.60f, 0.35f, 0.85f });
	tr->Print("HackGen", "RSP: 0x00EFF8B0  RBP: 0x00EFF950", startX + 8.0f, cy + 40.0f, 10.5f, { 0.90f, 0.60f, 0.35f, 0.85f });
	tr->Print("HackGen", "RIP: 0x7FF7B4C8  EFL: 0x00010202", startX + 8.0f, cy + 56.0f, 10.5f, { 1.0f, 0.45f, 0.35f, 0.90f });
	tr->Print("HackGen", "STACK CORRUPTION DETECTED", startX + 8.0f, cy + 72.0f, 10.0f, { 1.0f, 0.25f, 0.20f, 0.95f });
	cy += 98.0f;

	// 障害スペクトラム
	pseudoOS_.DrawSpectrumBars({ startX, cy }, frameW, 20.0f, 14, 5.0f, { 1.0f, 0.30f, 0.25f, 0.85f });
	cy += 26.0f;

	tr->Print("HackGen", "CRASH DUMP: /var/log/crash.dmp (OK)", startX, cy, 10.5f, { 0.35f, 0.85f, 0.55f, 0.85f });
}

void GameOverScene::DrawHardwareFailureWindow(const PseudoOS::Window& win) {
	TextRenderer* tr = TextRenderer::GetInstance();
	float startX = win.pos.x + 16.0f;
	float cy = win.pos.y + win.titleBarHeight + 12.0f;
	float frameW = win.size.x - 32.0f;

	tr->Print("HackGen", "SUBSYSTEM HEALTH ANALYSIS:", startX, cy, 11.5f, { 1.0f, 0.45f, 0.40f, 0.98f });
	cy += 20.0f;

	// サブシステム停止ステータス
	const char* const sysStatus[] = {
		"AVIONICS HUD  : [OFFLINE / UNRESPONSIVE]",
		"WEAPON BUS    : [POWER SEVERED]",
		"RADAR CARRIER : [CARRIER SIGNAL LOST]",
		"FLIGHT CTRL   : [WATCHDOG TIMEOUT]"
	};
	for (int i = 0; i < 4; ++i) {
		tr->Print("HackGen", sysStatus[i], startX, cy, 10.5f, (i == 0 || i == 3) ? Vector4{ 1.0f, 0.35f, 0.30f, 0.95f } : Vector4{ 1.0f, 0.70f, 0.25f, 0.85f });
		cy += 16.0f;
	}
	cy += 8.0f;

	// CPU温度異常ゲージ (99℃)
	tr->Print("HackGen", "MAIN ENGINE CPU CORE TEMP: 99.4 ℃ [OVERHEAT!]", startX, cy, 10.5f, { 1.0f, 0.30f, 0.25f, 0.98f });
	cy += 14.0f;
	pseudoOS_.DrawGauge({ startX, cy }, { frameW, 8.0f }, 0.99f, { 1.0f, 0.25f, 0.20f, 0.95f }, { 0.65f, 0.15f, 0.15f, 0.75f });
	cy += 18.0f;

	// 緊急電力残量ゲージ (8%)
	tr->Print("HackGen", "EMERGENCY BATTERY RESERVE: 8 % [CRITICAL]", startX, cy, 10.5f, { 1.0f, 0.80f, 0.25f, 0.95f });
	cy += 14.0f;
	pseudoOS_.DrawGauge({ startX, cy }, { frameW, 8.0f }, 0.08f, { 1.0f, 0.65f, 0.15f, 0.95f }, { 0.55f, 0.30f, 0.15f, 0.75f });
	cy += 24.0f;

	// DEFCON 1 警告パネル
	pseudoOS_.DrawPanel({ startX, cy }, { frameW, 26.0f }, { 0.25f, 0.04f, 0.04f, 0.90f }, { 1.0f, 0.25f, 0.20f, 0.95f }, 1.0f);
	tr->Print("HackGen", "DEFCON 1 // EMERGENCY SYSTEM SHUTDOWN", win.pos.x + win.size.x * 0.5f, cy + 6.0f, 11.5f, { 1.0f, 0.35f, 0.30f, 0.98f }, { 0.5f, 0.0f });
}
