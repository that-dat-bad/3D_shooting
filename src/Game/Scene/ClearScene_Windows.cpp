#include "ClearSceneInternal.h"

void ClearScene::DrawDebriefWindow(const PseudoOS::Window& win) {
	TextRenderer* tr = TextRenderer::GetInstance();
	float cx = win.pos.x + win.size.x * 0.5f;
	float cy = win.pos.y + win.titleBarHeight + 14.0f;

	// 1. MISSION COMPLETE 大見出し
	missionCompleteText_.Draw();
	cy += 44.0f;

	tr->Print("HackGen", ">> COMBAT SORTIE REPORT : AIRSPACE SECURED <<",
		cx, cy, 13.5f, { 0.35f, 0.95f, 0.75f, 0.95f }, { 0.5f, 0.0f });
	cy += 24.0f;

	// 2. リザルト情報パネル
	float frameW = win.size.x - 36.0f;
	float frameH = 224.0f;
	pseudoOS_.DrawPanel({ win.pos.x + 18.0f, cy }, { frameW, frameH },
		{ 0.01f, 0.03f, 0.05f, 0.92f }, { 0.20f, 0.65f, 0.85f, 0.75f }, 1.0f);

	float rowStartY = cy + 18.0f;
	float rowGap = 46.0f;
	float labelX = win.pos.x + 36.0f;
	float valX = win.pos.x + win.size.x - 36.0f;

	// 行1: 撃破数
	enemiesDestroyedLabel_.SetPosition({ labelX, rowStartY });
	enemiesDestroyedLabel_.Draw();
	enemiesDestroyedValue_.SetPosition({ valX, rowStartY - 2.0f });
	enemiesDestroyedValue_.Draw();
	pseudoOS_.DrawRect({ labelX, rowStartY + 28.0f }, { frameW - 36.0f, 1.0f }, { 0.15f, 0.40f, 0.55f, 0.30f });

	// 行2: クリアタイム
	clearTimeLabel_.SetPosition({ labelX, rowStartY + rowGap });
	clearTimeLabel_.Draw();
	clearTimeValue_.SetPosition({ valX, rowStartY + rowGap - 2.0f });
	clearTimeValue_.Draw();
	pseudoOS_.DrawRect({ labelX, rowStartY + rowGap + 28.0f }, { frameW - 36.0f, 1.0f }, { 0.15f, 0.40f, 0.55f, 0.30f });

	// 行3: 残存HP
	hpRemainingLabel_.SetPosition({ labelX, rowStartY + rowGap * 2.0f });
	hpRemainingLabel_.Draw();
	hpRemainingValue_.SetPosition({ valX, rowStartY + rowGap * 2.0f - 2.0f });
	hpRemainingValue_.Draw();
	pseudoOS_.DrawRect({ labelX, rowStartY + rowGap * 2.0f + 28.0f }, { frameW - 36.0f, 1.0f }, { 0.15f, 0.40f, 0.55f, 0.30f });

	// 行4: 作戦評価
	tr->Print("HackGen", "OPERATIONAL EVALUATION:", labelX, rowStartY + rowGap * 3.0f, 18.0f, { 0.55f, 0.85f, 0.95f, 0.95f });
	tr->Print("HackGen", "RANK [S]  100% CLEAR", valX, rowStartY + rowGap * 3.0f - 2.0f, 22.0f, { 1.0f, 0.95f, 0.35f, 1.0f }, { 1.0f, 0.0f });

	cy += frameH + 16.0f;

	// 3. 作戦達成率ゲージ
	float pulseRatio = 0.96f + 0.04f * std::sin(animTimer_ * 4.0f);
	Vector4 gaugeCol = { 0.25f, 0.95f * pulseRatio, 0.85f * pulseRatio, 0.95f };
	tr->Print("HackGen", "MISSION OBJECTIVE COMPLETION RATE : 100% [VERIFIED]", win.pos.x + 24.0f, cy, 11.5f, { 0.35f, 0.90f, 1.0f, 0.98f });
	cy += 16.0f;
	pseudoOS_.DrawGauge({ win.pos.x + 24.0f, cy }, { frameW - 12.0f, 10.0f }, 1.0f, gaugeCol, { 0.20f, 0.65f, 0.85f, 0.75f });
	cy += 20.0f;

	tr->Print("HackGen", "FLIGHT DATA ARCHIVED // SELECT PROTOCOL TO CONTINUE",
		cx, cy, 12.0f, { 0.40f, 0.90f, 0.70f, 0.95f }, { 0.5f, 0.0f });

	// 4. ボタン描画
	replayButton_.Draw();
	titleButton_.Draw();
}

void ClearScene::DrawFlightLogWindow(const PseudoOS::Window& win) {
	TextRenderer* tr = TextRenderer::GetInstance();
	float startX = win.pos.x + 16.0f;
	float cy = win.pos.y + win.titleBarHeight + 12.0f;

	tr->Print("HackGen", "SORTIE THEATER : SECTOR 04 [TUNNEL & MOUNTAIN]", startX, cy, 11.5f, { 0.45f, 0.90f, 1.0f, 0.98f });
	cy += 18.0f;
	tr->Print("HackGen", "FLIGHT AIRFRAME: MiG-21Bis TACTICAL JET", startX, cy, 11.0f, { 0.35f, 0.80f, 0.90f, 0.90f });
	cy += 18.0f;

	// 点滅データインテグリティLED
	bool validBlink = (std::fmod(animTimer_, 1.2f) < 0.95f);
	Vector4 validCol = validBlink ? Vector4{ 0.35f, 1.0f, 0.60f, 0.95f } : Vector4{ 0.15f, 0.50f, 0.30f, 0.70f };
	tr->Print("HackGen", "DATA INTEGRITY : SHA-256 SIGNED [VALID]", startX, cy, 11.0f, validCol);
	cy += 22.0f;

	// テレメトリ情報枠
	float frameW = win.size.x - 32.0f;
	pseudoOS_.DrawPanel({ startX, cy }, { frameW, 94.0f }, { 0.01f, 0.02f, 0.04f, 0.82f }, { 0.15f, 0.45f, 0.65f, 0.65f }, 1.0f);

	float machNoise = 1.85f + 0.015f * std::sin(animTimer_ * 5.2f);
	char machBuf[48];
	snprintf(machBuf, sizeof(machBuf), "MAX AIR SPEED : MACH %.2f [CRUISE]", machNoise);
	tr->Print("HackGen", machBuf, startX + 8.0f, cy + 8.0f, 10.5f, { 0.45f, 0.95f, 0.85f, 0.90f });

	float gNoise = 7.8f + 0.1f * std::sin(animTimer_ * 3.1f);
	char gBuf[48];
	snprintf(gBuf, sizeof(gBuf), "PEAK G-LOAD   : +%.1f G [STRUCTURAL OK]", gNoise);
	tr->Print("HackGen", gBuf, startX + 8.0f, cy + 24.0f, 10.5f, { 0.45f, 0.95f, 0.85f, 0.90f });

	int fuelVal = 420 - static_cast<int>(std::fmod(animTimer_ * 0.4f, 4.0f));
	char fuelBuf[48];
	snprintf(fuelBuf, sizeof(fuelBuf), "FUEL REMAINING: %d L [SAFE MARGIN]", fuelVal);
	tr->Print("HackGen", fuelBuf, startX + 8.0f, cy + 40.0f, 10.5f, { 0.45f, 0.95f, 0.85f, 0.90f });

	int egtVal = 638 + static_cast<int>(6.0f * std::sin(animTimer_ * 4.5f));
	char egtBuf[48];
	snprintf(egtBuf, sizeof(egtBuf), "AFTERBURNER   : %d C [EGT COOLDOWN]", egtVal);
	tr->Print("HackGen", egtBuf, startX + 8.0f, cy + 56.0f, 10.5f, { 0.35f, 0.85f, 0.65f, 0.90f });

	// 動的センサーバッファプログレス
	float bufRatio = 0.985f + 0.012f * std::sin(animTimer_ * 6.0f);
	char bufStr[48];
	snprintf(bufStr, sizeof(bufStr), "SENSOR BUFFER : %.1f%% SYNCHRONIZED", bufRatio * 100.0f);
	tr->Print("HackGen", bufStr, startX + 8.0f, cy + 74.0f, 10.0f, { 0.30f, 0.85f, 0.65f, 0.95f });
	cy += 104.0f;

	// フライトスペクトラム
	pseudoOS_.DrawSpectrumBars({ startX, cy }, frameW, 22.0f, 14, 5.0f, { 0.30f, 0.88f, 1.0f, 0.90f });
	cy += 28.0f;

	// 点滅通信キャリアリンクLED
	bool carBlink = (std::fmod(animTimer_, 0.8f) < 0.60f);
	Vector4 carLed = carBlink ? Vector4{ 0.35f, 1.0f, 0.65f, 1.0f } : Vector4{ 0.15f, 0.40f, 0.25f, 0.6f };
	pseudoOS_.DrawRect({ startX, cy + 3.0f }, { 6.0f, 6.0f }, carLed);
	tr->Print("HackGen", "CARRIER LINK  : ENCRYPTED RF LINK [ONLINE]", startX + 12.0f, cy, 10.5f, { 0.35f, 0.90f, 0.70f, 0.90f });
}

void ClearScene::DrawIntelAssessmentWindow(const PseudoOS::Window& win) {
	TextRenderer* tr = TextRenderer::GetInstance();
	float startX = win.pos.x + 16.0f;
	float cy = win.pos.y + win.titleBarHeight + 12.0f;
	float frameW = win.size.x - 32.0f;

	tr->Print("HackGen", "COMBAT ASSESSMENT REPORT:", startX, cy, 11.5f, { 0.45f, 0.95f, 0.85f, 0.98f });
	cy += 20.0f;

	// 戦果ステータス
	const char* const intelStatus[] = {
		"ENEMY FIGHTERS : [INTERCEPTORS NEUTRALIZED]",
		"RADAR JAMMING  : [AIRSPACE RESTORED]",
		"SAM INSTALL    : [DESTROYED / MISSION ACCORD]",
		"FORWARD BASE   : [TACTICAL RETREAT TRIGGERED]"
	};
	for (int i = 0; i < 4; ++i) {
		tr->Print("HackGen", intelStatus[i], startX, cy, 10.5f, { 0.35f, 0.85f, 0.65f, 0.90f });
		cy += 16.0f;
	}
	cy += 8.0f;

	// 空域掌握率ゲージ
	float supRatio = 0.95f + 0.05f * std::sin(animTimer_ * 3.5f);
	tr->Print("HackGen", "AIR SUPERIORITY DOMINANCE: 100 % [ABSOLUTE]", startX, cy, 10.5f, { 0.35f, 0.95f, 0.85f, 0.98f });
	cy += 14.0f;
	pseudoOS_.DrawGauge({ startX, cy }, { frameW, 8.0f }, 1.0f, { 0.25f, 0.95f * supRatio, 0.65f * supRatio, 0.95f }, { 0.15f, 0.65f, 0.45f, 0.75f });
	cy += 18.0f;

	// 機体再出撃可能ゲージ
	float readyJitter = 0.92f + 0.008f * std::sin(animTimer_ * 4.8f);
	char readyBuf[48];
	snprintf(readyBuf, sizeof(readyBuf), "COMBAT READINESS FOR RE-SORTIE: %d %%", static_cast<int>(readyJitter * 100.0f));
	tr->Print("HackGen", readyBuf, startX, cy, 10.5f, { 0.35f, 0.85f, 1.0f, 0.95f });
	cy += 14.0f;
	pseudoOS_.DrawGauge({ startX, cy }, { frameW, 8.0f }, readyJitter, { 0.25f, 0.85f, 1.0f, 0.95f }, { 0.15f, 0.55f, 0.75f, 0.75f });
	cy += 24.0f;

	// 作戦承認完了ステータスバッジ
	pseudoOS_.DrawRect({ startX, cy }, { frameW, 26.0f }, { 0.01f, 0.04f, 0.03f, 0.40f });
	pseudoOS_.DrawBorder({ startX, cy }, { frameW, 26.0f }, { 0.20f, 0.75f, 0.50f, 0.60f }, 1.0f);
	const float cL = 6.0f;
	pseudoOS_.DrawRect({ startX, cy }, { cL, 2.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX, cy }, { 2.0f, cL }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX + frameW - cL, cy }, { cL, 2.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX + frameW - 2.0f, cy }, { 2.0f, cL }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX, cy + 24.0f }, { cL, 2.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX, cy + 26.0f - cL }, { 2.0f, cL }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX + frameW - cL, cy + 24.0f }, { cL, 2.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
	pseudoOS_.DrawRect({ startX + frameW - 2.0f, cy + 26.0f - cL }, { 2.0f, cL }, { 0.40f, 1.0f, 0.70f, 0.95f });

	bool authBlink = (std::fmod(animTimer_, 0.9f) < 0.65f);
	Vector4 authLedCol = authBlink ? Vector4{ 0.35f, 1.0f, 0.65f, 1.0f } : Vector4{ 0.15f, 0.50f, 0.30f, 0.60f };
	pseudoOS_.DrawRect({ startX + 10.0f, cy + 9.0f }, { 8.0f, 8.0f }, authLedCol);
	tr->Print("HackGen", "[STATUS: CERTIFIED] AUTH // MISSION DEBRIEF", startX + 24.0f, cy + 6.0f, 10.5f, { 0.40f, 1.0f, 0.75f, 0.98f });
}

void ClearScene::DrawTheaterReconWindow(const PseudoOS::Window& win) {
	TextRenderer* tr = TextRenderer::GetInstance();
	float startX = win.pos.x + 14.0f;
	float cy = win.pos.y + win.titleBarHeight + 10.0f;
	float frameW = win.size.x - 28.0f;

	tr->Print("HackGen", "SECTOR TOPOGRAPHY [SURFACE RECON]:", startX, cy, 11.5f, { 0.45f, 0.95f, 0.85f, 0.98f });
	cy += 18.0f;

	// 1. 3Dワイヤーフレーム作戦マップ (h = 160)
	float mapH = 160.0f;
	pseudoOS_.DrawPanel({ startX, cy }, { frameW, mapH }, { 0.01f, 0.03f, 0.05f, 0.88f }, { 0.20f, 0.60f, 0.80f, 0.70f }, 1.0f);

	float gridTime = animTimer_ * 0.4f;
	for (int gy = 1; gy < 6; ++gy) {
		float lineY = cy + mapH * (gy * 0.166f);
		float wave = 4.0f * std::sin(gridTime + gy * 0.8f);
		pseudoOS_.DrawRect({ startX + 4.0f, lineY + wave }, { frameW - 8.0f, 1.0f }, { 0.15f, 0.45f, 0.60f, 0.40f });
	}
	for (int gx = 1; gx < 5; ++gx) {
		float lineX = startX + frameW * (gx * 0.20f);
		pseudoOS_.DrawRect({ lineX, cy + 4.0f }, { 1.0f, mapH - 8.0f }, { 0.15f, 0.45f, 0.60f, 0.35f });
	}

	float sweepAngle = animTimer_ * 2.4f;
	float scx = startX + frameW * 0.5f;
	float scy = cy + mapH * 0.5f;
	float sLen = 54.0f;
	float endX = scx + sLen * std::cos(sweepAngle);
	float endY = scy + sLen * std::sin(sweepAngle);
	pseudoOS_.DrawRect({ scx - 2.0f, scy - 2.0f }, { 4.0f, 4.0f }, { 0.35f, 1.0f, 0.80f, 1.0f });
	pseudoOS_.DrawRect({ (scx + endX) * 0.5f - 18.0f, (scy + endY) * 0.5f }, { 36.0f, 1.0f }, { 0.20f, 0.90f, 1.0f, 0.75f });

	bool tgtBlink = (std::fmod(animTimer_, 0.7f) < 0.45f);
	Vector4 tgtCol = tgtBlink ? Vector4{ 1.0f, 0.30f, 0.25f, 0.95f } : Vector4{ 0.50f, 0.15f, 0.10f, 0.60f };
	tr->Print("HackGen", "[X] BOGEY-01 (K.I.A.)", scx - 70.0f, scy - 35.0f, 10.0f, tgtCol);
	tr->Print("HackGen", "[X] BOGEY-02 (K.I.A.)", scx + 10.0f, scy + 25.0f, 10.0f, tgtCol);
	tr->Print("HackGen", ">> AIRSPACE SANITIZED // ALL HOSTILES DOWN", startX + 8.0f, cy + mapH - 18.0f, 10.0f, { 0.35f, 1.0f, 0.70f, 0.95f });
	cy += mapH + 16.0f;

	// 2. 自機 MiG-21 戦闘被害評価
	tr->Print("HackGen", "AIRFRAME BDA [MiG-21Bis]:", startX, cy, 11.5f, { 0.45f, 0.95f, 0.85f, 0.98f });
	cy += 18.0f;

	struct ComponentStatus {
		const char* name;
		const char* status;
		bool isOk;
	};
	const ComponentStatus bdaList[] = {
		{ "RADOME / SENSORS", "[100% NOMINAL]", true },
		{ "CANOPY PRESSURE ", "[SEALED // OK]", true },
		{ "PORT WING STRUC ", "[INTACT // OK]", true },
		{ "STBD WING STRUC ", "[INTACT // OK]", true },
		{ "R-25 TURBOJET   ", "[EGT COOLDOWN]", true },
		{ "HYDRAULIC SYS   ", "[20.8 MPa OK ]", true },
		{ "AVIONICS BUS    ", "[LINK SECURE ]", true }
	};

	float bdaH = 176.0f;
	pseudoOS_.DrawPanel({ startX, cy }, { frameW, bdaH }, { 0.01f, 0.025f, 0.04f, 0.85f }, { 0.15f, 0.50f, 0.70f, 0.65f }, 1.0f);
	float bdaRowY = cy + 8.0f;
	for (const auto& comp : bdaList) {
		pseudoOS_.DrawRect({ startX + 8.0f, bdaRowY + 4.0f }, { 6.0f, 6.0f }, { 0.35f, 1.0f, 0.65f, 0.95f });
		tr->Print("HackGen", comp.name, startX + 20.0f, bdaRowY, 10.5f, { 0.40f, 0.85f, 0.95f, 0.90f });
		tr->Print("HackGen", comp.status, startX + frameW - 8.0f, bdaRowY, 10.5f, { 0.35f, 1.0f, 0.65f, 0.95f }, { 1.0f, 0.0f });
		bdaRowY += 22.0f;
	}
	cy += bdaH + 14.0f;

	// 再出撃認可スタンプ
	pseudoOS_.DrawPanel({ startX, cy }, { frameW, 28.0f }, { 0.02f, 0.12f, 0.08f, 0.85f }, { 0.30f, 0.95f, 0.65f, 0.90f }, 1.0f);
	tr->Print("HackGen", "AIRFRAME READY FOR NEXT COMBAT SORTIE", startX + frameW * 0.5f, cy + 7.0f, 11.0f, { 0.40f, 1.0f, 0.75f, 0.98f }, { 0.5f, 0.0f });
}
