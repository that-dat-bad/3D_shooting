#define NOMINMAX
#include "CombatBootSequence.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/base/WinApp.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

CombatBootSequence::~CombatBootSequence() {
	Finalize();
}

void CombatBootSequence::Initialize(SpriteCommon* spriteCommon) {
	spriteCommon_ = spriteCommon;

	// テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture("assets/textures/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("assets/textures/circle2.png");

	// 全画面黒フェード用スプライト
	fadeSprite_ = std::make_unique<Sprite>();
	fadeSprite_->Initialize(spriteCommon_, "assets/textures/white1x1.png");
	fadeSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	fadeSprite_->SetPosition({ 0.0f, 0.0f });
	fadeSprite_->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
	fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	fadeSprite_->Update();

	// 矩形・ライン用スプライト
	rectSprite_ = std::make_unique<Sprite>();
	rectSprite_->Initialize(spriteCommon_, "assets/textures/white1x1.png");

	// 円形リング用スプライト
	ringSprite_ = std::make_unique<Sprite>();
	ringSprite_->Initialize(spriteCommon_, "assets/textures/circle2.png");

	// AC6風戦闘モード起動ボイス
	voiceData_ = AudioManager::GetInstance()->SoundLoadFile("assets/sounds/combat_mode_jp.wav");

	timer_ = 0.0f;
	fadeAlpha_ = 1.0f;
	voicePlayed_ = false;
}

void CombatBootSequence::Start() {
	timer_ = 0.0f;
	fadeAlpha_ = 1.0f;
	voicePlayed_ = false;

	if (fadeSprite_) {
		fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		fadeSprite_->Update();
	}
}

void CombatBootSequence::Finalize() {
	if (!voiceData_.buffer.empty()) {
		AudioManager::GetInstance()->SoundUnload(&voiceData_);
	}
	fadeSprite_.reset();
	rectSprite_.reset();
	ringSprite_.reset();
}

// UTF-8 文字列の部分取得（マルチバイト文字境界保護）
static std::string GetUtf8Substr(const std::string& str, size_t charCount) {
	size_t byteIdx = 0;
	size_t count = 0;
	while (byteIdx < str.size() && count < charCount) {
		unsigned char c = static_cast<unsigned char>(str[byteIdx]);
		if (c < 0x80) byteIdx += 1;
		else if ((c & 0xE0) == 0xC0) byteIdx += 2;
		else if ((c & 0xF0) == 0xE0) byteIdx += 3;
		else if ((c & 0xF8) == 0xF0) byteIdx += 4;
		else byteIdx += 1;
		count++;
	}
	return str.substr(0, byteIdx);
}

// UTF-8 文字数カウント
static size_t GetUtf8CharCount(const std::string& str) {
	size_t count = 0;
	for (size_t i = 0; i < str.size();) {
		unsigned char c = static_cast<unsigned char>(str[i]);
		if (c < 0x80) i += 1;
		else if ((c & 0xE0) == 0xC0) i += 2;
		else if ((c & 0xF0) == 0xE0) i += 3;
		else if ((c & 0xF8) == 0xF0) i += 4;
		else i += 1;
		count++;
	}
	return count;
}

// ランダムサイバーグリッチ（文字化けデコード演出）
static std::string MakeGlitchText(const std::string& base, float glitchIntensity, uint32_t seed) {
	if (glitchIntensity <= 0.001f) return base;
	static const char kGlitchChars[] = "ABCDEF0123456789<>_#*!/?%$&";
	const size_t numGlitchChars = sizeof(kGlitchChars) - 1;
	std::string result = base;
	for (size_t i = 0; i < result.size(); ++i) {
		if (result[i] == ' ' || result[i] == ':' || result[i] == '/' || result[i] == '.') continue;
		uint32_t hash = (seed * 1103515245 + static_cast<uint32_t>(i) * 12345) & 0x7FFFFFFF;
		float r = static_cast<float>(hash % 1000) / 1000.0f;
		if (r < glitchIntensity) {
			result[i] = kGlitchChars[hash % numGlitchChars];
		}
	}
	return result;
}

void CombatBootSequence::Update(float dt) {
	timer_ += dt;

	// 音声再生（完全暗転中の静寂0.40秒後にピピッチャイムから再生開始：項目1）
	if (!voicePlayed_ && timer_ >= 0.40f) {
		if (!voiceData_.buffer.empty()) {
			AudioManager::GetInstance()->SoundPlayWave(voiceData_);
		}
		voicePlayed_ = true;
	}

	// ====================================================================
	// 背景の黒ディム推移（項目1: 暗転タメ 0.85s ＋ 背景ディム）
	// ====================================================================
	// 0.00s - 0.85s: 完全暗転（fadeAlpha = 1.0f: システム再起動の重厚なタメ）
	// 0.85s - 1.40s: fadeAlpha = 1.0f（走査レーザー中も暗転維持）
	// 1.40s - 2.10s: 完全透過にはせず、fadeAlpha = 0.55f（重厚なダークディム）へスムーズにイージング！
	// 2.10s - 3.40s: fadeAlpha = 0.55f を維持！背景を暗く落として中央UIのコントラストを極大化！
	// 3.40s - 4.80s: 演出完了に伴い、0.55f から 0.0f へ滑らかにフェードアウト！
	if (timer_ <= 1.40f) {
		fadeAlpha_ = 1.0f;
	} else if (timer_ <= 2.10f) {
		float p = (timer_ - 1.40f) / 0.70f;
		p = std::clamp(p, 0.0f, 1.0f);
		float smoothP = p * (3.0f - 2.0f * p);
		fadeAlpha_ = 1.0f - smoothP * 0.45f; // 1.0f -> 0.55f へ
	} else if (timer_ <= 3.40f) {
		fadeAlpha_ = 0.55f; // 暗幕バイザーディムをキープ
	} else if (timer_ <= kDuration) {
		float p = (timer_ - 3.40f) / (kDuration - 3.40f);
		p = std::clamp(p, 0.0f, 1.0f);
		float smoothP = p * (3.0f - 2.0f * p);
		fadeAlpha_ = 0.55f * (1.0f - smoothP); // 0.55f -> 0.0f へフェードアウト
	} else {
		fadeAlpha_ = 0.0f;
	}
}

void CombatBootSequence::DrawBackgroundFade() {
	if (fadeAlpha_ > 0.001f && fadeSprite_) {
		fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, fadeAlpha_ });
		fadeSprite_->Update();
		fadeSprite_->Draw();
	}
}

void CombatBootSequence::DrawBootUI() {
	if (timer_ >= kDuration) {
		return;
	}

	// 項目1: 0.00s 〜 0.85s は完全暗転（UI描画なし、暗転のタメ）
	if (timer_ < 0.85f) {
		return;
	}

	const float sw = static_cast<float>(WinApp::kClientWidth);
	const float sh = static_cast<float>(WinApp::kClientHeight);
	const float cx = sw * 0.5f;
	const float cy = sh * 0.46f; // ミリタリーHUD視線中央

	TextRenderer* tr = TextRenderer::GetInstance();

	// ====================================================================
	// 0. シネマティック上下レターボックス黒帯（AC6映画的演出）
	// ====================================================================
	float letterboxH = 42.0f;
	float letterboxAlpha = 1.0f;
	if (timer_ > 3.40f) {
		letterboxAlpha = std::clamp(1.0f - (timer_ - 3.40f) / 1.30f, 0.0f, 1.0f);
	}
	if (letterboxAlpha > 0.001f) {
		DrawRect({ 0.0f, 0.0f }, { sw, letterboxH }, { 0.0f, 0.0f, 0.0f, 0.85f * letterboxAlpha });
		DrawRect({ 0.0f, sh - letterboxH }, { sw, letterboxH }, { 0.0f, 0.0f, 0.0f, 0.85f * letterboxAlpha });
		DrawLine({ 0.0f, letterboxH }, { sw, letterboxH }, 1.5f, { 0.20f, 0.80f, 0.45f, 0.60f * letterboxAlpha }, BlendMode::kAdd);
		DrawLine({ 0.0f, sh - letterboxH }, { sw, sh - letterboxH }, 1.5f, { 0.20f, 0.80f, 0.45f, 0.60f * letterboxAlpha }, BlendMode::kAdd);
	}

	// ====================================================================
	// Phase 1: 高速走査レーザービーム (0.85s 〜 1.35s)
	// ====================================================================
	if (timer_ < 1.35f) {
		float phaseT = timer_ - 0.85f;
		float scanProgress = std::clamp(phaseT / 0.36f, 0.0f, 1.0f);
		float beamLen = sw * scanProgress;
		float beamAlpha = 1.0f - std::clamp((phaseT - 0.36f) / 0.14f, 0.0f, 1.0f);

		// 水平高輝度レーザースキャンライン
		DrawRect({ 0.0f, cy - 2.0f }, { beamLen, 4.0f }, { 0.30f, 1.0f, 0.70f, 0.98f * beamAlpha }, { 0.0f, 0.0f }, BlendMode::kAdd);
		DrawRect({ beamLen - 180.0f, cy - 5.0f }, { 180.0f, 10.0f }, { 0.80f, 1.0f, 0.95f, 0.90f * beamAlpha }, { 0.0f, 0.0f }, BlendMode::kAdd);

		// 走査先端の強烈な閃光パルス
		DrawRect({ beamLen - 12.0f, cy - 30.0f }, { 24.0f, 60.0f }, { 1.0f, 1.0f, 1.0f, 0.98f * beamAlpha }, { 0.0f, 0.0f }, BlendMode::kAdd);

		// 上部ブート診断表示（特大＆高コントラスト）
		bool blink = (static_cast<int>(timer_ * 20.0f) % 2) == 0;
		Vector4 alertCol = blink ? Vector4{ 0.40f, 1.0f, 0.70f, 1.00f } : Vector4{ 0.20f, 0.75f, 0.45f, 0.75f };
		tr->Print("HackGen", "[ INITIALIZING COMBAT AVIONICS MATRIX... CONNECTING ]", cx, cy - 65.0f, 15.0f, alertCol, { 0.5f, 0.5f });
		tr->Flush();
		return;
	}

	// ====================================================================
	// Phase 2〜4: AC6風システム起動バナー & レティクル展開 (1.35s 〜 4.80s)
	// ====================================================================

	// 全体のアルファフェードアウト（3.40s 〜 4.40s で滑らかに退場：項目10）
	float overallAlpha = 1.0f;
	if (timer_ > 3.40f) {
		overallAlpha = std::clamp(1.0f - (timer_ - 3.40f) / 1.00f, 0.0f, 1.0f);
	}

	// レティクル展開進行度（1.35s〜2.10sでシャキッと拡大・展開）
	float expandT = std::clamp((timer_ - 1.35f) / 0.75f, 0.0f, 1.0f);
	float easeExpand = 1.0f - std::pow(1.0f - expandT, 3.0f);

	// --------------------------------------------------------------------
	// 1. 中央タクティカルバナー（大型化・高コントラスト化）
	// --------------------------------------------------------------------
	float bannerW = 860.0f * std::clamp((timer_ - 1.35f) / 0.45f, 0.0f, 1.0f);
	float bannerH = 112.0f;
	float topY = cy - bannerH * 0.5f;
	float botY = cy + bannerH * 0.5f;

	// バナー背景（ダークメッシュ帯：背景の黒ディムと相まって文字が超鮮明に！）
	DrawRect({ cx - bannerW * 0.5f, topY }, { bannerW, bannerH }, { 0.00f, 0.02f, 0.01f, 0.88f * overallAlpha });
	DrawRect({ cx - bannerW * 0.5f, topY + 2.0f }, { bannerW, bannerH - 4.0f }, { 0.02f, 0.08f, 0.04f, 0.75f * overallAlpha }, { 0.0f, 0.0f }, BlendMode::kAdd);

	// 二重タクティカルボーダーライン（シアン〜ライムグリーン高輝度）
	Vector4 borderCol = { 0.35f, 1.0f, 0.70f, 0.98f * overallAlpha };
	Vector4 subBorderCol = { 0.20f, 0.70f, 0.45f, 0.60f * overallAlpha };

	// メインライン（厚さ 2.5f）
	DrawLine({ cx - bannerW * 0.5f, topY }, { cx + bannerW * 0.5f, topY }, 2.5f, borderCol, BlendMode::kAdd);
	DrawLine({ cx - bannerW * 0.5f, botY }, { cx + bannerW * 0.5f, botY }, 2.5f, borderCol, BlendMode::kAdd);

	// サブライン（外側に 5px 離した細線）
	DrawLine({ cx - bannerW * 0.48f, topY - 5.0f }, { cx + bannerW * 0.48f, topY - 5.0f }, 1.0f, subBorderCol, BlendMode::kAdd);
	DrawLine({ cx - bannerW * 0.48f, botY + 5.0f }, { cx + bannerW * 0.48f, botY + 5.0f }, 1.0f, subBorderCol, BlendMode::kAdd);

	// 端部のタクティカル45度カット装飾
	DrawRect({ cx - bannerW * 0.5f, topY - 3.0f }, { 10.0f, 8.0f }, borderCol, { 0.0f, 0.0f }, BlendMode::kAdd);
	DrawRect({ cx + bannerW * 0.5f - 10.0f, topY - 3.0f }, { 10.0f, 8.0f }, borderCol, { 0.0f, 0.0f }, BlendMode::kAdd);
	DrawRect({ cx - bannerW * 0.5f, botY - 5.0f }, { 10.0f, 8.0f }, borderCol, { 0.0f, 0.0f }, BlendMode::kAdd);
	DrawRect({ cx + bannerW * 0.5f - 10.0f, botY - 5.0f }, { 10.0f, 8.0f }, borderCol, { 0.0f, 0.0f }, BlendMode::kAdd);

	// ミリタリーチックマーク（等間隔の目盛り）
	if (bannerW > 300.0f) {
		for (float ox = -bannerW * 0.46f; ox <= bannerW * 0.46f; ox += 36.0f) {
			DrawLine({ cx + ox, topY }, { cx + ox, topY - 5.0f }, 1.5f, { 0.35f, 1.0f, 0.65f, 0.70f * overallAlpha }, BlendMode::kAdd);
			DrawLine({ cx + ox, botY }, { cx + ox, botY + 5.0f }, 1.5f, { 0.35f, 1.0f, 0.65f, 0.70f * overallAlpha }, BlendMode::kAdd);
		}
	}

	// --------------------------------------------------------------------
	// 2. 中央照準レティクル（特大化 & 発光演出）
	// --------------------------------------------------------------------
	float ringRadius = 38.0f + 92.0f * easeExpand;
	float ringAlpha = (0.80f + 0.20f * std::sin(timer_ * 10.0f)) * overallAlpha;

	// 外周大リング ＋ 内周小リング
	DrawCircleRing({ cx, cy }, ringRadius, { 0.25f, 1.0f, 0.65f, ringAlpha }, BlendMode::kAdd);
	DrawCircleRing({ cx, cy }, ringRadius * 0.50f, { 0.45f, 1.0f, 0.75f, ringAlpha * 0.85f }, BlendMode::kAdd);

	// 十字マイクロクロスヘア（照準線）
	const float chLen = 22.0f;
	DrawLine({ cx - ringRadius - chLen, cy }, { cx - ringRadius + 6.0f, cy }, 2.0f, { 0.40f, 1.0f, 0.75f, overallAlpha }, BlendMode::kAdd);
	DrawLine({ cx + ringRadius - 6.0f, cy }, { cx + ringRadius + chLen, cy }, 2.0f, { 0.40f, 1.0f, 0.75f, overallAlpha }, BlendMode::kAdd);
	DrawLine({ cx, cy - ringRadius - chLen }, { cx, cy - ringRadius + 6.0f }, 2.0f, { 0.40f, 1.0f, 0.75f, overallAlpha }, BlendMode::kAdd);
	DrawLine({ cx, cy + ringRadius - 6.0f }, { cx, cy + ringRadius + chLen }, 2.0f, { 0.40f, 1.0f, 0.75f, overallAlpha }, BlendMode::kAdd);

	// レティクル四隅大型ブラケット（外周 130px）
	float bracketDist = 135.0f * easeExpand;
	DrawTacticalBracket({ cx, cy }, { bracketDist * 2.0f, bracketDist * 2.0f }, 24.0f, 2.5f, { 0.45f, 1.0f, 0.75f, 0.90f * overallAlpha });

	// --------------------------------------------------------------------
	// 3. テキスト表示（タイピング演出 ＆ グリッチ消去：項目2, 10）
	// --------------------------------------------------------------------
	if (timer_ >= 1.35f) {
		bool isConfirmed = (timer_ >= 2.20f);

		if (!isConfirmed) {
			// ============================================================
			// 項目2: 起動テキストのタイピング演出（1.35s 〜 2.10s）
			// ============================================================
			const std::string fullEnText = "MAIN SYSTEM : ACTIVATING COMBAT MODE";
			const std::string fullJpText = "メインシステム、戦闘モード起動...";

			float typingDuration = 0.65f; // 0.65秒で全文字タイピング
			float typingProgress = std::clamp((timer_ - 1.35f) / typingDuration, 0.0f, 1.0f);

			// 英語タイピング文字数
			size_t enCharCount = static_cast<size_t>(typingProgress * static_cast<float>(fullEnText.size()));
			std::string dispEn = fullEnText.substr(0, enCharCount);

			// 末尾ブロックカーソルの点滅
			bool cursorBlink = (static_cast<int>(timer_ * 16.0f) % 2) == 0;
			if (typingProgress < 1.0f && cursorBlink) {
				dispEn += "_";
			}

			// 日本語タイピング（1.45sから開始）
			std::string dispJp = "";
			if (timer_ >= 1.45f) {
				float jpProgress = std::clamp((timer_ - 1.45f) / (typingDuration * 0.9f), 0.0f, 1.0f);
				size_t jpTotalChars = GetUtf8CharCount(fullJpText);
				size_t jpCharCount = static_cast<size_t>(jpProgress * static_cast<float>(jpTotalChars));
				dispJp = GetUtf8Substr(fullJpText, jpCharCount);
				if (jpProgress < 1.0f && cursorBlink) {
					dispJp += "_";
				}
			}

			// タイピング風微小点滅カラー
			bool blinkFast = (static_cast<int>(timer_ * 12.0f) % 2) == 0;
			Vector4 mainTextCol = blinkFast
				? Vector4{ 1.0f, 1.0f, 1.0f, 1.00f * overallAlpha }
				: Vector4{ 0.30f, 1.0f, 0.65f, 0.98f * overallAlpha };

			// 上部サブインフォ（13.5f）
			tr->Print("HackGen", "[ TACTICAL FCS // COMBAT PROTOCOL LOADED ]", cx, topY - 26.0f, 13.5f, { 0.40f, 1.0f, 0.65f, 0.85f * overallAlpha }, { 0.5f, 0.5f });

			// メイン英語タイトル（特大 26.0f！ 発光感を出すため多重描画）
			tr->Print("HackGen", dispEn, cx + 1.0f, cy - 20.0f, 26.0f, { 0.15f, 0.80f, 0.40f, 0.60f * overallAlpha }, { 0.5f, 0.5f });
			tr->Print("HackGen", dispEn, cx, cy - 20.0f, 26.0f, mainTextCol, { 0.5f, 0.5f });

			// メイン日本語サブタイトル（拡大 19.0f！ 白源フォント）
			if (!dispJp.empty()) {
				Vector4 subJpCol = { 0.40f, 1.0f, 0.70f, 0.95f * overallAlpha };
				tr->Print("HackGen", dispJp, cx + 1.0f, cy + 22.0f, 19.0f, { 0.10f, 0.60f, 0.30f, 0.60f * overallAlpha }, { 0.5f, 0.5f });
				tr->Print("HackGen", dispJp, cx, cy + 22.0f, 19.0f, subJpCol, { 0.5f, 0.5f });
			}

		} else {
			// ============================================================
			// 項目10: 戦闘モード確立後の確認表示 ＆ スムーズなフェードアウト/グリッチ消去
			// ============================================================
			float pulse = 0.85f + 0.15f * std::sin((timer_ - 2.20f) * 14.0f);
			Vector4 confirmedCol = { 0.50f * pulse, 1.0f, 0.80f * pulse, 1.00f * overallAlpha };

			std::string enNominal = "ALL SYSTEMS NOMINAL /// COMBAT MODE ONLINE";
			std::string jpNominal = "全システム正常 ・ 戦闘モード起動完了";

			// 3.40s以降は退場フェーズ：グリッチノイズとフェードアウト（項目10）
			if (timer_ > 3.40f) {
				float fadeT = (timer_ - 3.40f) / 0.80f; // 0.0 -> 1.0
				float glitchChance = std::clamp(fadeT * 0.45f, 0.0f, 0.45f);
				uint32_t glitchSeed = static_cast<uint32_t>(timer_ * 60.0f);
				enNominal = MakeGlitchText(enNominal, glitchChance, glitchSeed);
			}

			// 上部サブインフォ
			tr->Print("HackGen", "[ MISSION AUTHORIZATION : CONFIRMED // AIRSPACE OPEN ]", cx, topY - 26.0f, 13.5f, { 0.50f, 1.0f, 0.75f, 0.90f * overallAlpha }, { 0.5f, 0.5f });

			// メイン英語タイトル（特大 25.0f）
			tr->Print("HackGen", enNominal, cx + 1.0f, cy - 20.0f, 25.0f, { 0.20f, 0.85f, 0.50f, 0.60f * overallAlpha }, { 0.5f, 0.5f });
			tr->Print("HackGen", enNominal, cx, cy - 20.0f, 25.0f, confirmedCol, { 0.5f, 0.5f });

			// メイン日本語サブタイトル（特大 18.5f）
			tr->Print("HackGen", jpNominal, cx + 1.0f, cy + 22.0f, 18.5f, { 0.20f, 0.60f, 0.40f, 0.60f * overallAlpha }, { 0.5f, 0.5f });
			tr->Print("HackGen", jpNominal, cx, cy + 22.0f, 18.5f, { 0.90f, 1.0f, 0.95f, 1.00f * overallAlpha }, { 0.5f, 0.5f });
		}
	}

	// --------------------------------------------------------------------
	// 4. 左右のタクティカルブートログ（バーゲージ ＆ ステータスブロック付き）
	// --------------------------------------------------------------------
	if (timer_ >= 1.50f && bannerW > 400.0f) {
		float logAlpha = std::clamp((timer_ - 1.50f) / 0.50f, 0.0f, 1.0f) * overallAlpha;
		const Vector4 logCol = { 0.35f, 0.95f, 0.65f, 0.88f * logAlpha };
		const Vector4 logValCol = { 0.90f, 1.0f, 0.95f, 0.95f * logAlpha };

		// 左翼テレメトリ（幅を広くしてサイズ 13.0f）
		float leftX = cx - 440.0f;
		float logY = cy - 36.0f;
		const float rowH = 26.0f;

		tr->Print("HackGen", "// CORE FIRMWARE : V5.84-R", leftX, logY, 13.0f, logCol, { 1.0f, 0.5f });
		tr->Print("HackGen", "[ ONLINE ]", leftX + 10.0f, logY, 12.0f, logValCol, { 0.0f, 0.5f });

		tr->Print("HackGen", "// FCS LOCK-ON MATRIX ...", leftX, logY + rowH, 13.0f, logCol, { 1.0f, 0.5f });
		tr->Print("HackGen", "[ ARMED ]", leftX + 10.0f, logY + rowH, 12.0f, logValCol, { 0.0f, 0.5f });

		tr->Print("HackGen", "// THRUST VECTOR DRIVE ..", leftX, logY + rowH * 2.0f, 13.0f, logCol, { 1.0f, 0.5f });
		tr->Print("HackGen", "[ 100% ]", leftX + 10.0f, logY + rowH * 2.0f, 12.0f, logValCol, { 0.0f, 0.5f });

		// 右翼テレメトリ
		float rightX = cx + 440.0f;
		tr->Print("HackGen", "[ VERIFIED ]", rightX - 10.0f, logY, 12.0f, logValCol, { 1.0f, 0.5f });
		tr->Print("HackGen", "// AP INTEGRITY : 100%", rightX, logY, 13.0f, logCol, { 0.0f, 0.5f });

		tr->Print("HackGen", "[ SYNC 100% ]", rightX - 10.0f, logY + rowH, 12.0f, logValCol, { 1.0f, 0.5f });
		tr->Print("HackGen", "// PILOT NEURAL LINK .", rightX, logY + rowH, 13.0f, logCol, { 0.0f, 0.5f });

		tr->Print("HackGen", "[ AUTHORIZED ]", rightX - 10.0f, logY + rowH * 2.0f, 12.0f, logValCol, { 1.0f, 0.5f });
		tr->Print("HackGen", "// AIRSPACE ENTRY ....", rightX, logY + rowH * 2.0f, 13.0f, logCol, { 0.0f, 0.5f });
	}

	// --------------------------------------------------------------------
	// 5. 終盤のパイロット操縦権移行アナウンス（3.60s〜4.80s）
	// --------------------------------------------------------------------
	if (timer_ >= 3.60f) {
		float promptAlpha = std::clamp((timer_ - 3.60f) / 0.40f, 0.0f, 1.0f);
		if (timer_ > 4.40f) {
			promptAlpha = std::clamp(1.0f - (timer_ - 4.40f) / 0.40f, 0.0f, 1.0f);
		}
		bool blink = (static_cast<int>(timer_ * 10.0f) % 2) == 0;
		Vector4 promptCol = blink
			? Vector4{ 0.45f, 1.0f, 0.75f, 0.98f * promptAlpha }
			: Vector4{ 0.25f, 0.85f, 0.55f, 0.80f * promptAlpha };
		tr->Print("HackGen", ">>> FLIGHT CONTROL ASSIGNED TO PILOT >>>", cx, 98.0f, 15.0f, promptCol, { 0.5f, 0.0f });
	}

	tr->Flush();
}

// ====================================================================
// 2D 描画ヘルパー
// ====================================================================

void CombatBootSequence::DrawLine(const Vector2& p1, const Vector2& p2, float thickness, const Vector4& color, BlendMode blendMode) {
	if (!rectSprite_) return;
	float dx = p2.x - p1.x;
	float dy = p2.y - p1.y;
	float length = std::sqrt(dx * dx + dy * dy);
	if (length < 0.001f) return;

	float angle = std::atan2(dy, dx);
	rectSprite_->SetBlendMode(blendMode);
	rectSprite_->SetPosition(p1);
	rectSprite_->SetAnchorPoint({ 0.0f, 0.5f });
	rectSprite_->SetSize({ length, thickness });
	rectSprite_->SetRotation(angle);
	rectSprite_->SetColor(color);
	rectSprite_->Update();
	rectSprite_->Draw();
}

void CombatBootSequence::DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& anchor, BlendMode blendMode) {
	if (!rectSprite_) return;
	rectSprite_->SetBlendMode(blendMode);
	rectSprite_->SetPosition(pos);
	rectSprite_->SetAnchorPoint(anchor);
	rectSprite_->SetSize(size);
	rectSprite_->SetRotation(0.0f);
	rectSprite_->SetColor(color);
	rectSprite_->Update();
	rectSprite_->Draw();
}

void CombatBootSequence::DrawTacticalBracket(const Vector2& center, const Vector2& size, float bracketLen, float thickness, const Vector4& color) {
	float halfW = size.x * 0.5f;
	float halfH = size.y * 0.5f;

	// 左上
	DrawLine({ center.x - halfW, center.y - halfH }, { center.x - halfW + bracketLen, center.y - halfH }, thickness, color, BlendMode::kAdd);
	DrawLine({ center.x - halfW, center.y - halfH }, { center.x - halfW, center.y - halfH + bracketLen }, thickness, color, BlendMode::kAdd);

	// 右上
	DrawLine({ center.x + halfW, center.y - halfH }, { center.x + halfW - bracketLen, center.y - halfH }, thickness, color, BlendMode::kAdd);
	DrawLine({ center.x + halfW, center.y - halfH }, { center.x + halfW, center.y - halfH + bracketLen }, thickness, color, BlendMode::kAdd);

	// 左下
	DrawLine({ center.x - halfW, center.y + halfH }, { center.x - halfW + bracketLen, center.y + halfH }, thickness, color, BlendMode::kAdd);
	DrawLine({ center.x - halfW, center.y + halfH }, { center.x - halfW, center.y + halfH - bracketLen }, thickness, color, BlendMode::kAdd);

	// 右下
	DrawLine({ center.x + halfW, center.y + halfH }, { center.x + halfW - bracketLen, center.y + halfH }, thickness, color, BlendMode::kAdd);
	DrawLine({ center.x + halfW, center.y + halfH }, { center.x + halfW, center.y + halfH - bracketLen }, thickness, color, BlendMode::kAdd);
}

void CombatBootSequence::DrawCircleRing(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode) {
	if (!ringSprite_) return;
	ringSprite_->SetBlendMode(blendMode);
	ringSprite_->SetPosition(center);
	ringSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	ringSprite_->SetSize({ radius * 2.0f, radius * 2.0f });
	ringSprite_->SetRotation(timer_ * 0.6f); // ゆったり回転
	ringSprite_->SetColor(color);
	ringSprite_->Update();
	ringSprite_->Draw();
}
