#define NOMINMAX
#include "CombatBootSequence.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/System/DirectXCommon.h"
#include "../../engine/Graphics/System/BlendMode.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../Graphics/AircraftVisualModel.h"
#include "../../engine/Graphics/UI/TypewriterEffect.h"
#include "../../engine/base/WinApp.h"
#include "../../engine/io/Input.h"
#include "../System/MissionManager.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

CombatBootSequence::~CombatBootSequence() {
	Finalize();
}

void CombatBootSequence::Initialize(SpriteCommon* spriteCommon, DirectXCommon* dxCommon) {
	spriteCommon_ = spriteCommon;
	dxCommon_ = dxCommon ? dxCommon : DirectXCommon::GetInstance();

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

	// スプライトプール初期化（512枚：各Rect/Line/Borderの個別CBV確保）
	bootSpritePool_.clear();
	for (int i = 0; i < 512; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_ ? spriteCommon_ : SpriteCommon::GetInstance(), "assets/textures/white1x1.png");
		bootSpritePool_.push_back(std::move(sp));
	}
	bootSpritePoolIndex_ = 0;

	bootCirclePool_.clear();
	for (int i = 0; i < 32; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_ ? spriteCommon_ : SpriteCommon::GetInstance(), "assets/textures/circle2.png");
		bootCirclePool_.push_back(std::move(sp));
	}
	bootCirclePoolIndex_ = 0;

	// AC6風戦闘モード起動ボイス（不要のため無効化）

	timer_ = 0.0f;
	fadeAlpha_ = 1.0f;
	voicePlayed_ = false;

	// スキャンパイプライン（案1: 自機専用GSマテリアライズ）初期化
	InitializeScanPipeline(dxCommon_);
}

void CombatBootSequence::InitializeScanPipeline(DirectXCommon* dxCommon) {
	if (!dxCommon) {
		dxCommon = DirectXCommon::GetInstance();
	}
	dxCommon_ = dxCommon;
	if (!dxCommon_) return;

	// 定数バッファの作成
	scanSettingsResource_ = dxCommon_->CreateBufferResource(sizeof(CombatBootScanSettings));
	scanSettingsResource_->Map(0, nullptr, reinterpret_cast<void**>(&scanSettingsData_));
	scanSettingsData_->scanProgress = 0.0f;
	scanSettingsData_->scanBandWidth = 3.5f;
	scanSettingsData_->scanMaxRadius = 13.0f;
	scanSettingsData_->scanWaveFreq = 3.5f;
	scanSettingsData_->scanColor = { 0.15f, 1.0f, 0.55f, 1.0f }; // エメラルドグリーン強烈発光
	scanSettingsData_->scanTime = 0.0f;
	scanSettingsData_->centerOffset = { 0.0f, 0.0f, 0.0f };

	// 専用シェーダーのコンパイル (VS, GS, PS)
	Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(L"./assets/shaders/CombatBootScan.VS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> gsBlob = dxCommon_->CompileShader(L"./assets/shaders/CombatBootScan.GS.hlsl", L"gs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(L"./assets/shaders/CombatBootScan.PS.hlsl", L"ps_6_0");

	if (!vsBlob || !gsBlob || !psBlob) {
		OutputDebugStringA("[CombatBootSequence] Failed to compile CombatBootScan shaders!\n");
		return;
	}

	// 頂点入力レイアウト (Object3dCommon と完全互換)
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[5] = {};
	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[2] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[3] = { "WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[4] = { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, 64, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = Object3dCommon::GetInstance()->GetRootSignature();
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.GS = { gsBlob->GetBufferPointer(), gsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

	// ラスタライザ: ホログラムワイヤーフレームが裏面メッシュも透けて光るよう CullMode = NONE
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState = rasterizerDesc;

	// 深度ステンシル
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = dxCommon_->GetRTVFormat();
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 通常アルファブレンド
	psoDesc.BlendState = GetBlendDesc(BlendMode::kNormal);

	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&scanPipelineState_));
	if (FAILED(hr)) {
		OutputDebugStringA("[CombatBootSequence] Failed to create CombatBootScan PSO!\n");
	}
}

void CombatBootSequence::BeginAircraftScanDraw(AircraftVisualModel& visualModel, float scanProgress) {
	if (!scanPipelineState_ || !scanSettingsData_) return;

	// 定数バッファを更新（機体中心から広がるスキャン波形）
	scanSettingsData_->scanProgress = std::clamp(scanProgress, 0.0f, 1.0f);
	scanSettingsData_->scanBandWidth = 3.5f;
	scanSettingsData_->scanMaxRadius = 13.0f;
	scanSettingsData_->scanWaveFreq = 3.5f;
	scanSettingsData_->scanColor = { 0.15f, 1.0f, 0.55f, 1.0f }; // サイバーグリーン
	scanSettingsData_->scanTime = timer_;
	scanSettingsData_->centerOffset = { 0.0f, 0.0f, 0.0f };

	// 自機モデルの全パーツにスキャン専用PSOと定数バッファを適用
	visualModel.SetCustomPipelineState(scanPipelineState_.Get());
	visualModel.SetCustomLightingSettingsResource(scanSettingsResource_.Get());
}

void CombatBootSequence::EndAircraftScanDraw(AircraftVisualModel& visualModel) {
	// 自機モデルのカスタム設定を解除して通常状態に復元
	visualModel.SetCustomPipelineState(nullptr);
	visualModel.SetCustomLightingSettingsResource(nullptr);
}

void CombatBootSequence::Start() {
	if (!fadeSprite_ || !rectSprite_ || !ringSprite_) {
		Initialize(spriteCommon_ ? spriteCommon_ : SpriteCommon::GetInstance(),
		           dxCommon_ ? dxCommon_ : DirectXCommon::GetInstance());
	}
	timer_ = 0.0f;
	briefingTimer_ = 0.0f;
	isWaitingForSpace_ = !isQuickMode_;
	fadeAlpha_ = 0.95f;
	voicePlayed_ = false;

	if (fadeSprite_) {
		fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, fadeAlpha_ });
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

	if (scanSettingsResource_) {
		scanSettingsResource_->Unmap(0, nullptr);
		scanSettingsData_ = nullptr;
		scanSettingsResource_.Reset();
	}
	scanPipelineState_.Reset();
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
	// 高速リトライモード時の処理（0.9秒で完了）
	if (isQuickMode_) {
		timer_ += dt;
		if (timer_ <= 0.35f) {
			fadeAlpha_ = 1.0f;
		} else if (timer_ <= kQuickDuration) {
			float p = (timer_ - 0.35f) / (kQuickDuration - 0.35f);
			fadeAlpha_ = 1.0f - p;
		} else {
			fadeAlpha_ = 0.0f;
		}
		return;
	}

	// ブリーフィング中のSPACEキー入力待ち処理
	if (isWaitingForSpace_) {
		briefingTimer_ += dt;
		timer_ = 0.0f;
		fadeAlpha_ = 0.95f;

		Input* input = Input::GetInstance();
		if (input) {
			if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) || input->TriggerMouse(0)) {
				isWaitingForSpace_ = false;
				timer_ = 1.60f; // 1.60s〜2.00sでブリーフィングが中央へ圧縮されながら消滅し、2.00sでPhase 1(レーザー走査)へシームレス移行！
				input->LockCursor();
			}
		}
		return;
	}

	timer_ += dt;

	// 戦闘モード起動ボイスは無効化

	// ====================================================================
	// 背景の黒ディム推移（作戦ブリーフィング ＋ 戦闘モード起動 ＋ フェードアウト！）
	// ====================================================================
	// 0.00s - 2.10s: 作戦ブリーフィング（fadeAlpha = 0.95f: 高い可読性とシネマティック感！）
	// 2.10s - 2.60s: fadeAlpha = 0.95f -> 0.55f へスムーズにイージング
	// 2.60s - 4.20s: fadeAlpha = 0.55f（戦闘モードUIコントラスト維持）
	// 4.20s - kDuration: 0.55f -> 0.0f へフェードアウト（戦闘へシームレス移行！）
	if (timer_ <= 2.10f) {
		fadeAlpha_ = 0.95f;
	} else if (timer_ <= 2.60f) {
		float p = (timer_ - 2.10f) / 0.50f;
		p = std::clamp(p, 0.0f, 1.0f);
		float smoothP = p * (3.0f - 2.0f * p);
		fadeAlpha_ = 0.95f - smoothP * 0.40f; // 0.95f -> 0.55f へ
	} else if (timer_ <= 4.20f) {
		fadeAlpha_ = 0.55f; // 暗幕バイザーディムをキープ
	} else if (timer_ <= kDuration) {
		float p = (timer_ - 4.20f) / (kDuration - 4.20f);
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

void CombatBootSequence::DrawQuickBootUI(float cx, float cy) {
	if (timer_ >= kQuickDuration) return;

	float alpha = (timer_ < 0.65f) ? 1.0f : (1.0f - (timer_ - 0.65f) / 0.25f);
	DrawTacticalBracket({ cx, cy }, { 600.0f, 90.0f }, 30.0f, 2.0f, { 0.25f, 0.95f, 0.55f, alpha * 0.9f });

	TextRenderer* tr = TextRenderer::GetInstance();
	tr->Print(
		"HackGen",
		">> RAPID COMBAT RE-ENGAGE <<",
		cx,
		cy - 16.0f,
		32.0f,
		{ 0.35f, 1.0f, 0.65f, alpha },
		{ 0.5f, 0.5f }
	);

	tr->Print(
		"HackGen",
		"FLIGHT CONTROLS RESTORED // ALL SYSTEMS NOMINAL",
		cx,
		cy + 22.0f,
		16.0f,
		{ 0.85f, 0.95f, 0.90f, alpha * 0.85f },
		{ 0.5f, 0.5f }
	);
}

void CombatBootSequence::DrawLoadingDiagnostics(float cx, float cy) {
	float prog = std::clamp(timer_ / 0.80f, 0.0f, 1.0f);
	int pct = static_cast<int>(prog * 100.0f);

	// 回転するワイヤーフレームリング
	DrawCircleRing({ cx, cy - 30.0f }, 45.0f, { 0.20f, 0.80f, 0.45f, 0.60f }, BlendMode::kAdd);
	DrawCircleRing({ cx, cy - 30.0f }, 55.0f, { 0.15f, 0.50f, 0.35f, 0.40f }, BlendMode::kAdd);

	// 四隅ブラケット
	DrawTacticalBracket({ cx, cy - 30.0f }, { 130.0f, 130.0f }, 18.0f, 1.5f, { 0.25f, 0.90f, 0.50f, 0.70f });

	TextRenderer* tr = TextRenderer::GetInstance();
	tr->Print(
		"HackGen",
		"INITIALIZING COMBAT ENVIRONMENT...",
		cx,
		cy + 65.0f,
		20.0f,
		{ 0.35f, 1.0f, 0.60f, 0.95f },
		{ 0.5f, 0.5f }
	);

	// ダミープログレスバー
	float barW = 340.0f;
	float barH = 8.0f;
	float barX = cx - barW * 0.5f;
	float barY = cy + 95.0f;
	DrawRect({ barX, barY }, { barW, barH }, { 0.05f, 0.15f, 0.08f, 0.85f });
	DrawRect({ barX, barY }, { barW * prog, barH }, { 0.25f, 0.95f, 0.50f, 0.95f });

	char loadStatus[64];
	snprintf(loadStatus, sizeof(loadStatus), "PRE-FLIGHT DIAGNOSTICS: %d%% [OK]", pct);
	tr->Print(
		"HackGen",
		loadStatus,
		cx,
		cy + 120.0f,
		13.0f,
		{ 0.70f, 0.90f, 0.80f, 0.80f },
		{ 0.5f, 0.5f }
	);
}

void CombatBootSequence::DrawCinematicLetterbox(float sw, float sh) {
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
}

void CombatBootSequence::DrawScanningLaser(float sw, float cx, float cy) {
	float phaseT = timer_ - 2.00f;
	float scanProgress = std::clamp(phaseT / 0.28f, 0.0f, 1.0f);
	float beamLen = sw * scanProgress;
	float beamAlpha = 1.0f - std::clamp((phaseT - 0.28f) / 0.12f, 0.0f, 1.0f);

	// 水平高輝度レーザースキャンライン
	DrawRect({ 0.0f, cy - 2.0f }, { beamLen, 4.0f }, { 0.30f, 1.0f, 0.70f, 0.98f * beamAlpha }, { 0.0f, 0.0f }, BlendMode::kAdd);
	DrawRect({ beamLen - 180.0f, cy - 5.0f }, { 180.0f, 10.0f }, { 0.80f, 1.0f, 0.95f, 0.90f * beamAlpha }, { 0.0f, 0.0f }, BlendMode::kAdd);

	// 走査先端の強烈な閃光パルス
	DrawRect({ beamLen - 12.0f, cy - 30.0f }, { 24.0f, 60.0f }, { 1.0f, 1.0f, 1.0f, 0.98f * beamAlpha }, { 0.0f, 0.0f }, BlendMode::kAdd);

	// 上部ブート診断表示（特大＆高コントラスト）
	bool blink = (static_cast<int>(timer_ * 20.0f) % 2) == 0;
	Vector4 alertCol = blink ? Vector4{ 0.40f, 1.0f, 0.70f, 1.00f } : Vector4{ 0.20f, 0.75f, 0.45f, 0.75f };
	TextRenderer* tr = TextRenderer::GetInstance();
	tr->Print("HackGen", "[ INITIALIZING COMBAT AVIONICS MATRIX... CONNECTING ]", cx, cy - 65.0f, 15.0f, alertCol, { 0.5f, 0.5f });
	tr->Flush();
}

void CombatBootSequence::DrawTacticalBanner(float cx, float cy, float bannerW, float overallAlpha) {
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
}

void CombatBootSequence::DrawTacticalReticle(float cx, float cy, float easeExpand, float overallAlpha) {
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
}

void CombatBootSequence::DrawBootText(float cx, float cy, float topY, float overallAlpha) {
	if (timer_ < 2.40f) return;

	TextRenderer* tr = TextRenderer::GetInstance();
	bool isConfirmed = (timer_ >= 3.20f);

	if (!isConfirmed) {
		const std::string fullEnText = "MAIN SYSTEM : ACTIVATING COMBAT MODE";
		const std::string fullJpText = "メインシステム、戦闘モード起動...";

		float typingDuration = 0.65f; // 0.65秒で全文字タイピング
		float typingProgress = (timer_ - 2.40f) / typingDuration;
		std::string dispEn = TypewriterEffect::GetTextByProgress(fullEnText, typingProgress, true, timer_, 16.0f, "_");

		// 日本語タイピング（2.50sから開始）
		std::string dispJp = "";
		if (timer_ >= 2.50f) {
			float jpProgress = (timer_ - 2.50f) / (typingDuration * 0.9f);
			dispJp = TypewriterEffect::GetTextByProgress(fullJpText, jpProgress, true, timer_, 16.0f, "_");
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
		float pulse = 0.85f + 0.15f * std::sin((timer_ - 3.20f) * 14.0f);
		Vector4 confirmedCol = { 0.50f * pulse, 1.0f, 0.80f * pulse, 1.00f * overallAlpha };

		std::string enNominal = "ALL SYSTEMS NOMINAL /// COMBAT MODE ONLINE";
		std::string jpNominal = "全システム正常 ・ 戦闘モード起動完了";

		// 4.20s以降は退場フェーズ：グリッチノイズとフェードアウト
		if (timer_ > 4.20f) {
			float fadeT = (timer_ - 4.20f) / 0.80f; // 0.0 -> 1.0
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

void CombatBootSequence::DrawTelemetryLogs(float cx, float cy, float bannerW, float overallAlpha) {
	if (timer_ < 2.50f || bannerW <= 400.0f) return;

	TextRenderer* tr = TextRenderer::GetInstance();
	float logAlpha = std::clamp((timer_ - 2.50f) / 0.50f, 0.0f, 1.0f) * overallAlpha;
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

void CombatBootSequence::DrawPilotPrompt(float cx, float overallAlpha) {
	if (timer_ < 4.00f) return;

	float promptAlpha = std::clamp((timer_ - 4.00f) / 0.40f, 0.0f, 1.0f);
	if (timer_ > 4.80f) {
		promptAlpha = std::clamp(1.0f - (timer_ - 4.80f) / 0.40f, 0.0f, 1.0f);
	}
	bool blink = (static_cast<int>(timer_ * 10.0f) % 2) == 0;
	Vector4 promptCol = blink
		? Vector4{ 0.45f, 1.0f, 0.75f, 0.98f * promptAlpha }
		: Vector4{ 0.25f, 0.85f, 0.55f, 0.80f * promptAlpha };
	TextRenderer::GetInstance()->Print("HackGen", ">>> FLIGHT CONTROL ASSIGNED TO PILOT >>>", cx, 98.0f, 15.0f, promptCol, { 0.5f, 0.0f });
}

void CombatBootSequence::DrawBootUI() {
	const float sw = static_cast<float>(WinApp::kClientWidth);
	const float sh = static_cast<float>(WinApp::kClientHeight);
	const float cx = sw * 0.5f;
	const float cy = sh * 0.46f; // ミリタリーHUD視線中央

	// スプライトプールのフレーム先頭リセット & パイプライン共通ステート適用
	bootSpritePoolIndex_ = 0;
	bootCirclePoolIndex_ = 0;
	SpriteCommon::GetInstance()->SetupCommonState();

	// リトライ時高速モードの描画（0.9秒でキビキビと出撃復帰）
	if (isQuickMode_) {
		DrawQuickBootUI(cx, cy);
		return;
	}

	if (timer_ >= kDuration) {
		return;
	}

	// ====================================================================
	// Phase 0: TACTICAL MISSION BRIEFING (0.00s 〜 2.00s)
	// ステージシーン冒頭の作戦ブリーフィング表示
	// ====================================================================
	if (timer_ < 2.00f) {
		const auto& mission = MissionManager::GetInstance().GetCurrentMission();
		TextRenderer* tr = TextRenderer::GetInstance();

		// ブリーフィング終了時の「真ん中に圧縮されるように消滅する」アニメーション計算
		float closeProgress = 0.0f;
		if (!isWaitingForSpace_) {
			closeProgress = std::clamp((timer_ - 1.60f) / 0.40f, 0.0f, 1.0f);
		}

		// 圧縮スケール: イーズイン(加速)しながら 1.0 -> 0.0 へ中央(cx, cy)に凝縮
		float compressScale = 1.0f - (closeProgress * closeProgress);

		// ブリーフィングパネルフェードイン/フェードアウト
		float briefAlpha = 1.0f;
		if (isWaitingForSpace_) {
			briefAlpha = std::clamp(briefingTimer_ / 0.25f, 0.0f, 1.0f);
		} else {
			// 前半(0.0〜0.40)は文字が中央に高速圧縮されるダイナミックな動きを鮮明に見せ、後半(0.40〜1.0)でフェードアウト
			float fadeOutT = std::clamp((closeProgress - 0.40f) / 0.60f, 0.0f, 1.0f);
			briefAlpha = 1.0f - (fadeOutT * fadeOutT);
		}

		float animTime = isWaitingForSpace_ ? briefingTimer_ : timer_;

		// 画面中央 (すべての文字・ウィンドウがこの点へ圧縮収束する)
		// cx, cy は既に CombatBootSequence::Render() で cx = sw * 0.5f, cy = sh * 0.46f として定義済み

		// 圧縮対応テキスト描画ヘルパー: 各テキストの描画中心を (cx, cy) に向けてスケールし、フォントサイズも縮小
		auto printBriefText = [&](const std::string& fontName, const std::string& text, float origX, float origY, float origSize, Vector4 color, Vector2 anchor = { 0.0f, 0.0f }) {
			if (closeProgress >= 1.0f || compressScale <= 0.015f) return;
			Vector2 tSize = tr->CalculateTextSize(fontName, text, origSize);
			float textCenterX = origX + tSize.x * (0.5f - anchor.x);
			float textCenterY = origY + tSize.y * (0.5f - anchor.y);

			float newCenterX = cx + (textCenterX - cx) * compressScale;
			float newCenterY = cy + (textCenterY - cy) * compressScale;
			float newSize = origSize * compressScale;
			if (newSize < 1.0f) return;

			// 圧縮時に粒子凝縮のような高エネルギー白発光をブレンド
			if (closeProgress > 0.0f) {
				float glowMix = closeProgress * 0.40f;
				color.x = (1.0f - glowMix) * color.x + glowMix * 0.95f;
				color.y = (1.0f - glowMix) * color.y + glowMix * 1.00f;
				color.z = (1.0f - glowMix) * color.z + glowMix * 0.95f;
			}
			color.w *= briefAlpha;
			if (color.w <= 0.001f) return;

			tr->Print(fontName, text, newCenterX, newCenterY, newSize, color, { 0.5f, 0.5f });
		};

		// 圧縮対応ライン描画ヘルパー
		auto drawBriefLine = [&](Vector2 p1, Vector2 p2, float thickness, Vector4 color, BlendMode blendMode = BlendMode::kNormal) {
			if (closeProgress >= 1.0f || compressScale <= 0.015f) return;
			Vector2 np1 = { cx + (p1.x - cx) * compressScale, cy + (p1.y - cy) * compressScale };
			Vector2 np2 = { cx + (p2.x - cx) * compressScale, cy + (p2.y - cy) * compressScale };
			color.w *= briefAlpha;
			if (color.w <= 0.001f) return;
			DrawLine(np1, np2, (std::max)(1.0f, thickness * compressScale), color, blendMode);
		};

		// 圧縮対応矩形描画ヘルパー
		auto drawBriefRect = [&](Vector2 pos, Vector2 size, Vector4 color, Vector2 anchor = { 0.0f, 0.0f }, BlendMode blendMode = BlendMode::kNormal) {
			if (closeProgress >= 1.0f || compressScale <= 0.015f) return;
			Vector2 rCenter = { pos.x + size.x * (0.5f - anchor.x), pos.y + size.y * (0.5f - anchor.y) };
			Vector2 newCenter = { cx + (rCenter.x - cx) * compressScale, cy + (rCenter.y - cy) * compressScale };
			Vector2 newSize = { size.x * compressScale, size.y * compressScale };
			color.w *= briefAlpha;
			if (color.w <= 0.001f || newSize.x < 1.0f || newSize.y < 1.0f) return;
			DrawRect(newCenter, newSize, color, { 0.5f, 0.5f }, blendMode);
		};

		// 圧縮対応タクティカルブラケット描画ヘルパー
		auto drawBriefBracket = [&](Vector2 center, Vector2 size, float bracketLen, float thickness, Vector4 color) {
			if (closeProgress >= 1.0f || compressScale <= 0.015f) return;
			Vector2 newCenter = { cx + (center.x - cx) * compressScale, cy + (center.y - cy) * compressScale };
			Vector2 newSize = { size.x * compressScale, size.y * compressScale };
			float newLen = bracketLen * compressScale;
			color.w *= briefAlpha;
			if (color.w <= 0.001f || newLen < 1.0f) return;
			DrawTacticalBracket(newCenter, newSize, newLen, (std::max)(1.0f, thickness * compressScale), color);
		};

		// 圧縮対応円形リング描画ヘルパー
		auto drawBriefCircleRing = [&](Vector2 center, float radius, Vector4 color, BlendMode blendMode = BlendMode::kNormal) {
			if (closeProgress >= 1.0f || compressScale <= 0.015f) return;
			Vector2 newCenter = { cx + (center.x - cx) * compressScale, cy + (center.y - cy) * compressScale };
			float newRadius = radius * compressScale;
			color.w *= briefAlpha;
			if (color.w <= 0.001f || newRadius < 1.0f) return;
			DrawCircleRing(newCenter, newRadius, color, blendMode);
		};

		// 上下シネマティック黒帯（黒帯自体は画面端に留まり、境界線と文字が中央へ圧縮）
		float lbH = 46.0f;
		DrawRect({ 0.0f, 0.0f }, { sw, lbH }, { 0.0f, 0.0f, 0.0f, 0.90f * (isWaitingForSpace_ ? briefAlpha : 1.0f) });
		DrawRect({ 0.0f, sh - lbH }, { sw, lbH }, { 0.0f, 0.0f, 0.0f, 0.90f * (isWaitingForSpace_ ? briefAlpha : 1.0f) });
		drawBriefLine({ 0.0f, lbH }, { sw, lbH }, 1.5f, { 0.35f, 1.0f, 0.65f, 0.85f }, BlendMode::kAdd);
		drawBriefLine({ 0.0f, sh - lbH }, { sw, sh - lbH }, 1.5f, { 0.35f, 1.0f, 0.65f, 0.85f }, BlendMode::kAdd);

		// 上部帯タイトル
		printBriefText("HackGen", "[ TACTICAL SORTIE BRIEFING // OPERATION DIRECTIVE ]", 28.0f, 12.0f, 15.0f, { 0.40f, 1.0f, 0.70f, 0.95f });
		printBriefText("HackGen", "CLEARANCE: LEVEL 5 // COMBAT PROTOCOL LOADED", sw - 440.0f, 12.0f, 14.0f, { 0.25f, 0.75f, 0.45f, 0.80f });

		// 中央大型ブリーフィングウィンドウ
		const float panelW = 1080.0f;
		const float panelH = 500.0f;
		const float px = cx - panelW * 0.5f;
		const float py = cy - panelH * 0.5f - 4.0f;

		// ウィンドウ背景
		drawBriefRect({ px, py }, { panelW, panelH }, { 0.01f, 0.04f, 0.02f, 0.94f });
		drawBriefRect({ px + 2.0f, py + 2.0f }, { panelW - 4.0f, panelH - 4.0f }, { 0.005f, 0.02f, 0.01f, 0.90f });

		// 二重枠線 ＆ タクティカルブラケット
		Vector4 pBorder = { 0.35f, 0.95f, 0.60f, 0.95f };
		drawBriefLine({ px, py }, { px + panelW, py }, 2.0f, pBorder, BlendMode::kAdd);
		drawBriefLine({ px, py + panelH }, { px + panelW, py + panelH }, 2.0f, pBorder, BlendMode::kAdd);
		drawBriefLine({ px, py }, { px, py + panelH }, 2.0f, pBorder, BlendMode::kAdd);
		drawBriefLine({ px + panelW, py }, { px + panelW, py + panelH }, 2.0f, pBorder, BlendMode::kAdd);
		drawBriefBracket({ cx, py + panelH * 0.5f }, { panelW + 24.0f, panelH + 24.0f }, 36.0f, 2.0f, { 0.45f, 1.0f, 0.75f, 0.90f });

		// パネルタイトルバー
		float tbH = 38.0f;
		drawBriefRect({ px, py }, { panelW, tbH }, { 0.04f, 0.18f, 0.09f, 0.95f });
		drawBriefLine({ px, py + tbH }, { px + panelW, py + tbH }, 1.5f, pBorder, BlendMode::kAdd);

		// 点滅LED
		bool blink = (std::fmod(animTime, 0.6f) < 0.35f);
		Vector4 ledCol = blink ? Vector4{ 0.40f, 1.0f, 0.65f, 1.0f } : Vector4{ 0.15f, 0.45f, 0.25f, 0.6f };
		drawBriefRect({ px + 16.0f, py + 14.0f }, { 10.0f, 10.0f }, ledCol);
		printBriefText("HackGen", "MISSION FILE: SORTIE_DIRECTIVE_ARCHIVE", px + 36.0f, py + 9.0f, 15.5f, { 0.90f, 1.0f, 0.95f, 1.0f });

		// 作戦名（特大ハイライト表示）
		float contentY = py + tbH + 16.0f;
		std::string opHeader = mission.aliasName.empty() ? "// OPERATION NAME" : ("// OPERATION PROTOCOL : " + mission.aliasName);
		printBriefText("HackGen", opHeader, px + 36.0f, contentY, 15.0f, { 0.35f, 0.85f, 0.55f, 0.85f });
		contentY += 22.0f;

		std::string opTitle = mission.name.empty() ? "OPERATION COMBINED STRIKE" : mission.name;
		std::string opSubTitle = (mission.subTitle.empty() || mission.subTitle == mission.name) ? "" : (" // " + mission.subTitle);
		printBriefText("HackGen", opTitle + opSubTitle, px + 36.0f, contentY, 28.0f, { 0.95f, 1.0f, 0.98f, 1.0f });
		contentY += 38.0f;

		// セパレータライン
		drawBriefLine({ px + 28.0f, contentY }, { px + panelW - 28.0f, contentY }, 1.5f, { 0.20f, 0.60f, 0.35f, 0.50f });
		contentY += 16.0f;

		// 下部フッターセパレータ位置
		float footLineY = py + panelH - 68.0f;

		// ====================================================================
		// 左カラム: 主目標 ＋ 作戦指示 ＋ 戦況・パラメーター (幅 450px)
		// ====================================================================
		float colLeftX = px + 36.0f;
		float curY = contentY;

		// 1. 主目標 (PRIMARY OBJECTIVES)
		printBriefText("HackGen", "[ PRIMARY OBJECTIVES ]", colLeftX, curY, 15.5f, { 0.45f, 1.0f, 0.70f, 0.95f });
		curY += 24.0f;

		if (!mission.objectives.empty()) {
			for (size_t o = 0; o < (std::min)(mission.objectives.size(), size_t(2)); ++o) {
				const auto& obj = mission.objectives[o];
				std::string objTxt = "> " + obj.description;
				printBriefText("HackGen", objTxt, colLeftX + 8.0f, curY, 14.5f, { 0.85f, 0.95f, 0.90f, 0.95f });
				curY += 22.0f;
			}
		} else {
			printBriefText("HackGen", "> DESTROY ALL HOSTILE AIR & GROUND CONTACTS", colLeftX + 8.0f, curY, 14.5f, { 0.85f, 0.95f, 0.90f, 0.95f });
			curY += 22.0f;
		}

		// 2. 作戦指示 (DIRECTIVE / SITUATION)
		curY += 8.0f;
		printBriefText("HackGen", "[ SITUATION & DIRECTIVE ]", colLeftX, curY, 15.5f, { 0.45f, 1.0f, 0.70f, 0.95f });
		curY += 24.0f;
		std::string desc = mission.description.empty() ? "Engage all designated hostile targets. Exercise extreme caution in dense combat zones." : mission.description;
		printBriefText("HackGen", desc, colLeftX + 8.0f, curY, 14.5f, { 0.75f, 0.92f, 0.82f, 0.92f });
		curY += 26.0f;

		// 3. 戦況パラメーター (TACTICAL CONDITIONS & INTEL)
		curY += 8.0f;
		printBriefText("HackGen", "[ TACTICAL CONDITIONS & INTEL ]", colLeftX, curY, 15.5f, { 0.45f, 1.0f, 0.70f, 0.95f });
		curY += 24.0f;

		if (mission.enemies.empty() && mission.groundEnemies.empty()) {
			printBriefText("HackGen", "HOSTILE UNITS    : NONE (RECONNAISSANCE ONLY)", colLeftX + 8.0f, curY, 14.0f, { 0.35f, 0.85f, 0.55f, 0.90f });
			curY += 21.0f;
		} else {
			char airBuf[64];
			snprintf(airBuf, sizeof(airBuf), "HOSTILE AIRBORNE : %zu UNITS", mission.enemies.size());
			printBriefText("HackGen", airBuf, colLeftX + 8.0f, curY, 14.0f, { 0.95f, 0.40f, 0.35f, 0.95f });
			curY += 21.0f;

			char gndBuf[64];
			snprintf(gndBuf, sizeof(gndBuf), "HOSTILE GROUND   : %zu SITES", mission.groundEnemies.size());
			printBriefText("HackGen", gndBuf, colLeftX + 8.0f, curY, 14.0f, { 0.95f, 0.70f, 0.30f, 0.95f });
			curY += 21.0f;
		}

		char areaBuf[128];
		snprintf(areaBuf, sizeof(areaBuf), "COMBAT AIRSPACE  : %s", mission.areaName.c_str());
		printBriefText("HackGen", areaBuf, colLeftX + 8.0f, curY, 14.0f, { 0.35f, 0.85f, 0.55f, 0.90f });
		curY += 21.0f;

		char timeBuf[64];
		if (mission.timeLimit > 0.0f) {
			int tSec = static_cast<int>(mission.timeLimit);
			snprintf(timeBuf, sizeof(timeBuf), "TIME LIMIT       : %02d:%02d", tSec / 60, tSec % 60);
		} else {
			snprintf(timeBuf, sizeof(timeBuf), "TIME LIMIT       : UNLIMITED");
		}
		printBriefText("HackGen", timeBuf, colLeftX + 8.0f, curY, 14.0f, { 0.45f, 1.0f, 0.70f, 0.95f });

		// ====================================================================
		// 右カラム: 作戦空域タクティカルマップ (四角い枠のみ)
		// ====================================================================
		float mapX = px + 515.0f;
		float mapY = contentY - 4.0f;
		float mapW = panelW - 550.0f; // 530px
		float mapH = footLineY - mapY - 8.0f; // 280px

		// マップ背景 & 枠線 (四角い枠)
		drawBriefRect({ mapX, mapY }, { mapW, mapH }, { 0.005f, 0.035f, 0.018f, 0.98f });
		Vector4 mapBorderCol = { 0.45f, 1.0f, 0.75f, 1.0f };
		drawBriefLine({ mapX, mapY }, { mapX + mapW, mapY }, 2.0f, mapBorderCol, BlendMode::kAdd);
		drawBriefLine({ mapX, mapY + mapH }, { mapX + mapW, mapY + mapH }, 2.0f, mapBorderCol, BlendMode::kAdd);
		drawBriefLine({ mapX, mapY }, { mapX, mapY + mapH }, 2.0f, mapBorderCol, BlendMode::kAdd);
		drawBriefLine({ mapX + mapW, mapY }, { mapX + mapW, mapY + mapH }, 2.0f, mapBorderCol, BlendMode::kAdd);
		drawBriefBracket({ mapX + mapW * 0.5f, mapY + mapH * 0.5f }, { mapW + 12.0f, mapH + 12.0f }, 20.0f, 2.0f, { 0.55f, 1.0f, 0.85f, 1.0f });

		// マップヘッダーバー
		drawBriefRect({ mapX, mapY }, { mapW, 26.0f }, { 0.04f, 0.16f, 0.08f, 0.95f });
		drawBriefLine({ mapX, mapY + 26.0f }, { mapX + mapW, mapY + 26.0f }, 1.5f, mapBorderCol, BlendMode::kAdd);
		printBriefText("HackGen", "[ THEATER TACTICAL MAP ]", mapX + 12.0f, mapY + 5.0f, 13.0f, { 0.45f, 1.0f, 0.75f, 0.95f });

		// ====================================================================
		// フッターエリア: ステータス ＆ PRESS SPACE TO START
		// ====================================================================
		drawBriefLine({ px + 24.0f, footLineY }, { px + panelW - 24.0f, footLineY }, 1.5f, { 0.20f, 0.65f, 0.38f, 0.55f });

		// 左側: ステータスインジケーター
		float indY = footLineY + 22.0f;
		bool indBlink = (std::fmod(animTime, 0.5f) < 0.30f);
		Vector4 indCol = blink ? Vector4{ 0.35f, 1.0f, 0.60f, 0.95f } : Vector4{ 0.15f, 0.50f, 0.25f, 0.60f };
		drawBriefRect({ px + 32.0f, indY + 3.0f }, { 10.0f, 10.0f }, indCol);
		printBriefText("HackGen", "SORTIE STANDBY // ALL WEAPONS ARMED", px + 50.0f, indY, 13.5f, { 0.40f, 0.85f, 0.55f, 0.85f });

		// 右側: サブ入力ガイド
		printBriefText("HackGen", "[ENTER / CLICK] ALSO ACCEPTED", px + panelW - 32.0f, indY, 13.0f, { 0.35f, 0.70f, 0.45f, 0.75f }, { 1.0f, 0.0f });

		// 中央: メイン出撃誘導ボタン (PRESS SPACE TO START MISSION)
		float btnW = 540.0f;
		float btnH = 40.0f;
		float btnX = cx - btnW * 0.5f;
		float btnY = footLineY + 12.0f;

		if (isWaitingForSpace_) {
			bool isHover = (cursorPos_.x >= btnX && cursorPos_.x <= btnX + btnW &&
			                cursorPos_.y >= btnY && cursorPos_.y <= btnY + btnH);
			float pulse = 0.65f + 0.35f * std::sin(animTime * 4.5f);
			Vector4 textCol = isHover ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : Vector4{ 0.95f, 1.0f, 0.98f, (0.90f + 0.10f * pulse) };
			std::string promptText = isHover ? ">> CLICK OR PRESS [ SPACE ] TO START MISSION <<" : ">> PRESS [ SPACE ] TO START MISSION <<";
			printBriefText("HackGen", promptText, cx, btnY + 9.0f, 17.5f, textCol, { 0.5f, 0.0f });
		} else {
			printBriefText("HackGen", ">> SORTIE INITIATED // STARTING COMBAT MODE <<", cx, btnY + 9.0f, 17.5f, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.5f, 0.0f });
		}

		// 中央圧縮時の高密度エネルギーライン・凝縮フレア（テキスト・窓枠が吸い込まれるエフェクト）
		if (closeProgress > 0.05f) {
			float coreFlare = std::sin(closeProgress * 3.14159f);
			float beamW = 280.0f * (1.0f - closeProgress * 0.7f) + 80.0f * coreFlare;
			float beamH = 2.0f + 6.0f * coreFlare;
			DrawRect({ cx - beamW * 0.5f, cy - beamH * 0.5f }, { beamW, beamH }, { 0.50f, 1.0f, 0.80f, 0.95f * coreFlare }, { 0.0f, 0.0f }, BlendMode::kAdd);
			DrawRect({ cx - 25.0f * compressScale, cy - 10.0f * compressScale }, { 50.0f * compressScale, 20.0f * compressScale }, { 1.0f, 1.0f, 1.0f, 0.90f * coreFlare }, { 0.0f, 0.0f }, BlendMode::kAdd);
		}

		tr->Flush();
		return;
	}

	// シネマティック上下レターボックス黒帯
	DrawCinematicLetterbox(sw, sh);

	// Phase 1: 高速走査レーザービーム (2.00s 〜 2.50s)
	if (timer_ < 2.50f) {
		DrawScanningLaser(sw, cx, cy);
		return;
	}

	// Phase 2〜4: AC6風システム起動バナー & レティクル展開 (2.40s 〜 5.20s)
	float overallAlpha = 1.0f;
	if (timer_ > 4.20f) {
		overallAlpha = std::clamp(1.0f - (timer_ - 4.20f) / 0.90f, 0.0f, 1.0f);
	}

	float expandT = std::clamp((timer_ - 2.40f) / 0.70f, 0.0f, 1.0f);
	float easeExpand = 1.0f - std::pow(1.0f - expandT, 3.0f);

	float bannerW = 860.0f * std::clamp((timer_ - 2.40f) / 0.40f, 0.0f, 1.0f);
	float bannerH = 112.0f;
	float topY = cy - bannerH * 0.5f;

	(void)easeExpand;
	(void)bannerW;
	// メインシステム起動の四角枠(DrawTacticalBanner)は不要のため非描画
	DrawBootText(cx, cy, topY, overallAlpha);
	DrawTelemetryLogs(cx, cy, bannerW, overallAlpha);
	DrawPilotPrompt(cx, overallAlpha);

	TextRenderer::GetInstance()->Flush();
}

// ====================================================================
// 2D 描画ヘルパー
// ====================================================================

void CombatBootSequence::DrawLine(const Vector2& p1, const Vector2& p2, float thickness, const Vector4& color, BlendMode blendMode) {
	float dx = p2.x - p1.x;
	float dy = p2.y - p1.y;
	float length = std::sqrt(dx * dx + dy * dy);
	if (length < 0.001f) return;

	float angle = std::atan2(dy, dx);
	Sprite* sp = GetBootSprite();
	if (!sp) return;
	sp->SetBlendMode(blendMode);
	sp->SetPosition(p1);
	sp->SetAnchorPoint({ 0.0f, 0.5f });
	sp->SetSize({ length, thickness });
	sp->SetRotation(angle);
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

void CombatBootSequence::DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& anchor, BlendMode blendMode) {
	if (size.x <= 0.001f || size.y <= 0.001f) return;
	Sprite* sp = GetBootSprite();
	if (!sp) return;
	sp->SetBlendMode(blendMode);
	sp->SetPosition(pos);
	sp->SetAnchorPoint(anchor);
	sp->SetSize(size);
	sp->SetRotation(0.0f);
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
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
	if (radius <= 0.001f) return;
	Sprite* sp = GetBootCircleSprite();
	if (!sp) return;
	sp->SetBlendMode(blendMode);
	sp->SetPosition(center);
	sp->SetAnchorPoint({ 0.5f, 0.5f });
	sp->SetSize({ radius * 2.0f, radius * 2.0f });
	sp->SetRotation(timer_ * 0.6f); // ゆったり回転
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

Sprite* CombatBootSequence::GetBootSprite() {
	if (bootSpritePoolIndex_ >= bootSpritePool_.size()) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_ ? spriteCommon_ : SpriteCommon::GetInstance(), "assets/textures/white1x1.png");
		bootSpritePool_.push_back(std::move(sp));
	}
	return bootSpritePool_[bootSpritePoolIndex_++].get();
}

Sprite* CombatBootSequence::GetBootCircleSprite() {
	if (bootCirclePoolIndex_ >= bootCirclePool_.size()) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_ ? spriteCommon_ : SpriteCommon::GetInstance(), "assets/textures/circle2.png");
		bootCirclePool_.push_back(std::move(sp));
	}
	return bootCirclePool_[bootCirclePoolIndex_++].get();
}
