#include "ClearSceneInternal.h"

using namespace ClearSceneInternal;

// ============================================================
// 項目15: VHSテープのリワインド（巻き戻し）演出
// ============================================================
void ClearScene::DrawRewindEffectUI() {
	if (!isRewinding_ || !uploadSprite_) return;
	TextRenderer* tr = TextRenderer::GetInstance();
	float rewindProgress = rewindTimer_ / kRewindDuration;

	auto DrawRectHelper = [this](const Vector2& pos, const Vector2& size, const Vector4& col, BlendMode blend = BlendMode::kNormal) {
		if (!uploadSprite_) return;
		uploadSprite_->SetBlendMode(blend);
		uploadSprite_->SetPosition(pos);
		uploadSprite_->SetAnchorPoint({ 0.0f, 0.0f });
		uploadSprite_->SetSize(size);
		uploadSprite_->SetRotation(0.0f);
		uploadSprite_->SetColor(col);
		uploadSprite_->Update();
		uploadSprite_->Draw();
	};

	// 激しいVHS走査バー
	float noiseY = std::fmod(rewindTimer_ * 1800.0f, kScreenHeight);
	DrawRectHelper({ 0.0f, noiseY - 20.0f }, { kScreenWidth, 40.0f }, { 0.85f, 0.95f, 1.0f, 0.65f }, BlendMode::kAdd);

	for (int i = 0; i < 6; ++i) {
		float lineY = std::fmod(rewindTimer_ * 2600.0f + i * 140.0f, kScreenHeight);
		DrawRectHelper({ 0.0f, lineY }, { kScreenWidth, 3.0f }, { 1.0f, 1.0f, 1.0f, 0.80f }, BlendMode::kAdd);
	}

	if (rewindTimer_ < 0.55f) {
		tr->Print("HackGen", "<<< REWINDING MISSION DATA TAPE <<<",
			kScreenWidth * 0.5f, kScreenHeight * 0.5f - 25.0f, 28.0f, { 0.90f, 1.0f, 0.95f, 1.0f }, { 0.5f, 0.5f });
		char timeBuf[32];
		snprintf(timeBuf, sizeof(timeBuf), "TC -%02d:%02d:%02d", static_cast<int>(rewindTimer_ * 60.0f), static_cast<int>(rewindTimer_ * 3600.0f) % 60, static_cast<int>(rewindProgress * 100.0f));
		tr->Print("HackGen", timeBuf, kScreenWidth * 0.5f, kScreenHeight * 0.5f + 20.0f, 22.0f, { 0.30f, 1.0f, 0.60f, 0.95f }, { 0.5f, 0.5f });
	} else {
		float engageT = rewindTimer_ - 0.55f;
		const std::string fullEngage = "RAPID COMBAT RE-ENGAGE";
		int chars = static_cast<int>(engageT / 0.015f);
		if (chars > static_cast<int>(fullEngage.size())) chars = static_cast<int>(fullEngage.size());
		std::string dispEngage = fullEngage.substr(0, chars);
		if (chars < static_cast<int>(fullEngage.size())) dispEngage += "_";

		float ew = 560.0f;
		float eh = 90.0f;
		pseudoOS_.DrawPanel({ (kScreenWidth - ew) * 0.5f, (kScreenHeight - eh) * 0.5f },
			{ ew, eh }, { 0.01f, 0.06f, 0.10f, 0.92f }, { 0.35f, 1.0f, 0.65f, 1.0f }, 2.0f);

		tr->Print("HackGen", dispEngage.c_str(),
			kScreenWidth * 0.5f, kScreenHeight * 0.5f - 14.0f, 30.0f, { 0.40f, 1.0f, 0.70f, 1.0f }, { 0.5f, 0.5f });
		tr->Print("HackGen", "// SYSTEM RE-ARMED // IMMEDIATE DEPLOYMENT",
			kScreenWidth * 0.5f, kScreenHeight * 0.5f + 20.0f, 14.0f, { 0.85f, 1.0f, 0.95f, 0.90f }, { 0.5f, 0.5f });
	}

	if (rewindTimer_ >= 0.78f) {
		float fadeWhite = (rewindTimer_ - 0.78f) / 0.12f;
		DrawRectHelper({ 0.0f, 0.0f }, { kScreenWidth, kScreenHeight }, { 1.0f, 1.0f, 1.0f, fadeWhite * 0.75f }, BlendMode::kAdd);
	}
}

// ============================================================
// 項目16, 17, 18: データアップロードと帰還システムログブリッジ演出
// ============================================================
void ClearScene::DrawUploadSequenceUI() {
	if (!isUploading_ || !uploadSprite_) return;
	TextRenderer* tr = TextRenderer::GetInstance();
	float progress = (std::min)(1.0f, uploadTimer_ / 0.90f);

	auto DrawRectHelper = [this](const Vector2& pos, const Vector2& size, const Vector4& col, BlendMode blend = BlendMode::kNormal) {
		if (!uploadSprite_) return;
		uploadSprite_->SetBlendMode(blend);
		uploadSprite_->SetPosition(pos);
		uploadSprite_->SetAnchorPoint({ 0.0f, 0.0f });
		uploadSprite_->SetSize(size);
		uploadSprite_->SetRotation(0.0f);
		uploadSprite_->SetColor(col);
		uploadSprite_->Update();
		uploadSprite_->Draw();
	};

	float dialogW = 600.0f;
	float dialogH = 170.0f;
	float dialogX = (kScreenWidth - dialogW) * 0.5f;
	float dialogY = (kScreenHeight - dialogH) * 0.5f;

	pseudoOS_.DrawPanel({ dialogX, dialogY }, { dialogW, dialogH },
		{ 0.01f, 0.04f, 0.07f, 0.98f }, { 0.25f, 0.95f, 1.0f, 0.95f }, 1.5f);
	DrawRectHelper({ dialogX, dialogY }, { dialogW, 30.0f }, { 0.03f, 0.20f, 0.32f, 0.98f });

	tr->Print("HackGen", "DAWN_OS : TELEMETRY CLOUD UPLOAD & COMM LINK",
		dialogX + 14.0f, dialogY + 8.0f, 13.0f, { 0.40f, 0.95f, 1.0f, 1.0f });

	if (progress < 1.0f) {
		tr->Print("HackGen", "UPLOADING FLIGHT LOG TO BASE TACTICAL CLOUD...",
			dialogX + 22.0f, dialogY + 48.0f, 14.0f, { 0.85f, 0.95f, 1.0f, 0.95f });
		tr->Print("HackGen", "COMM_LINK: DISCONNECTING FROM COMBAT THEATER...",
			dialogX + 22.0f, dialogY + 70.0f, 12.0f, { 0.40f, 0.85f, 0.95f, 0.85f });
	} else {
		tr->Print("HackGen", "UPLOAD COMPLETED // BASE TERMINAL SYNCHRONIZED",
			dialogX + 22.0f, dialogY + 48.0f, 14.0f, { 0.35f, 1.0f, 0.65f, 1.0f });
		tr->Print("HackGen", "DISCONNECTED: ENTERING HANGAR TERMINAL ROOT...",
			dialogX + 22.0f, dialogY + 70.0f, 12.0f, { 0.60f, 1.0f, 0.80f, 0.95f });
	}

	float barW = dialogW - 44.0f;
	pseudoOS_.DrawGauge({ dialogX + 22.0f, dialogY + 98.0f }, { barW, 16.0f }, progress,
		{ 0.20f, 0.90f, 1.0f, 1.0f }, { 0.25f, 0.70f, 0.90f, 0.85f });

	char pctBuf[32];
	snprintf(pctBuf, sizeof(pctBuf), "%3.0f %%", progress * 100.0f);
	tr->Print("HackGen", pctBuf, dialogX + dialogW - 22.0f, dialogY + 122.0f, 14.0f, { 0.40f, 0.95f, 1.0f, 1.0f }, { 1.0f, 0.0f });
}
