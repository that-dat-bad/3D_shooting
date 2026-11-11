#include "InfoUI.h"
#include <cstdio>
#include <cmath>

void InfoUI::SetupTextElement(UIText& textElem, const std::string& initialText) {
	textElem.Initialize(InfoUIDefaults::kFontName, initialText, InfoUIDefaults::kFontSize);
	textElem.SetAnchorPoint({ 0.0f, 0.0f });
	textElem.SetColor(InfoUIDefaults::kTextColor);
	// 会話UI（RadioCommunicator）と完全同一の2層ソフトドロップシャドウを適用
	textElem.SetDualDropShadow(
		true,
		InfoUIDefaults::kInnerShadowOffset,
		InfoUIDefaults::kInnerShadowColor,
		InfoUIDefaults::kOuterShadowOffset,
		InfoUIDefaults::kOuterShadowColor
	);
}

void InfoUI::Initialize(SpriteCommon* spriteCommon) {
	spriteCommon_ = spriteCommon;
	isVisible_ = true;

	// 既存の文字UIクラス UIText を初期化し、会話UI風2層ソフトドロップシャドウを流用設定
	SetupTextElement(labelThrottle_, "出力");
	SetupTextElement(valThrottle_, "0 %");

	SetupTextElement(labelSpeed_, "速度");
	SetupTextElement(valSpeed_, "0 km/h");

	SetupTextElement(labelAlt_, "高度");
	SetupTextElement(valAlt_, "0 m");

	SetupTextElement(labelRalt_, "RALT");
	SetupTextElement(valRalt_, "0 m");

	SetupTextElement(labelAmmo_, "機関砲");
	SetupTextElement(valAmmo_, "0");
}

void InfoUI::SetVisible(bool visible) {
	isVisible_ = visible;
	labelThrottle_.SetVisible(visible);
	valThrottle_.SetVisible(visible);
	labelSpeed_.SetVisible(visible);
	valSpeed_.SetVisible(visible);
	labelAlt_.SetVisible(visible);
	valAlt_.SetVisible(visible);
	labelRalt_.SetVisible(visible);
	valRalt_.SetVisible(visible);
	labelAmmo_.SetVisible(visible);
	valAmmo_.SetVisible(visible);
}

void InfoUI::Update(float dt, const FlightInfoUIData& data) {
	(void)dt;
	data_ = data;

	if (!isVisible_ || data_.alpha <= 0.001f) {
		return;
	}

	const float screenH = data_.screenHeight > 0.0f ? data_.screenHeight : InfoUIDefaults::kReferenceScreenHeight;
	const float scale = std::clamp(
		screenH / InfoUIDefaults::kReferenceScreenHeight,
		InfoUIDefaults::kMinResolutionScale,
		InfoUIDefaults::kMaxResolutionScale);

	const float baseX = InfoUIDefaults::kMarginLeft * scale;
	float currentY = InfoUIDefaults::kMarginTop * scale;
	const float fontSize = InfoUIDefaults::kFontSize * scale;
	const float lineSpacing = InfoUIDefaults::kLineSpacing * scale;
	const float valueOffsetX = InfoUIDefaults::kValueColumnOffset * scale;
	const float valueX = baseX + valueOffsetX;

	const float currentAlpha = std::clamp(data_.alpha, 0.0f, 1.0f);
	Vector4 baseTextColor = InfoUIDefaults::kTextColor;
	baseTextColor.w *= currentAlpha;

	// -------------------------------------------------------------
	// 1. エンジン出力 (THR / 出力)
	// -------------------------------------------------------------
	{
		const int throttlePct = static_cast<int>(std::round(data_.throttle * InfoUIDefaults::kThrottlePercentScale));
		const bool isWep = (data_.throttle > 1.01f);

		Vector4 valCol = isWep ? InfoUIDefaults::kWepColor : baseTextColor;
		valCol.w *= currentAlpha;

		char valBuf[32];
		if (isWep) {
			std::snprintf(valBuf, sizeof(valBuf), "WEP %d %%", throttlePct);
		} else {
			std::snprintf(valBuf, sizeof(valBuf), "%d %%", (std::max)(0, throttlePct));
		}

		labelThrottle_.SetPosition({ baseX, currentY });
		labelThrottle_.SetFontSize(fontSize);
		labelThrottle_.SetColor(baseTextColor);

		valThrottle_.SetText(valBuf);
		valThrottle_.SetPosition({ valueX, currentY });
		valThrottle_.SetFontSize(fontSize);
		valThrottle_.SetColor(valCol);

		currentY += lineSpacing;
	}

	// -------------------------------------------------------------
	// 2. 速度 (SPD / 速度, km/h)
	// -------------------------------------------------------------
	{
		const int speedKmh = static_cast<int>(std::round(data_.speed * InfoUIDefaults::kMpsToKmh));
		char valBuf[32];
		std::snprintf(valBuf, sizeof(valBuf), "%d km/h", (std::max)(0, speedKmh));

		labelSpeed_.SetPosition({ baseX, currentY });
		labelSpeed_.SetFontSize(fontSize);
		labelSpeed_.SetColor(baseTextColor);

		valSpeed_.SetText(valBuf);
		valSpeed_.SetPosition({ valueX, currentY });
		valSpeed_.SetFontSize(fontSize);
		valSpeed_.SetColor(baseTextColor);

		currentY += lineSpacing;
	}

	// -------------------------------------------------------------
	// 3. 絶対高度 (ALT / 高度, 海抜高度 MSL)
	// -------------------------------------------------------------
	{
		const int altM = static_cast<int>(std::round(data_.absoluteAltitude));
		char valBuf[32];
		std::snprintf(valBuf, sizeof(valBuf), "%d m", altM);

		labelAlt_.SetPosition({ baseX, currentY });
		labelAlt_.SetFontSize(fontSize);
		labelAlt_.SetColor(baseTextColor);

		valAlt_.SetText(valBuf);
		valAlt_.SetPosition({ valueX, currentY });
		valAlt_.SetFontSize(fontSize);
		valAlt_.SetColor(baseTextColor);

		currentY += lineSpacing;
	}

	// -------------------------------------------------------------
	// 4. 相対高度 (RALT / 対地高度 AGL)
	// -------------------------------------------------------------
	{
		const int raltM = static_cast<int>(std::round((std::max)(0.0f, data_.relativeAltitude)));
		char valBuf[32];
		std::snprintf(valBuf, sizeof(valBuf), "%d m", raltM);

		labelRalt_.SetPosition({ baseX, currentY });
		labelRalt_.SetFontSize(fontSize);
		labelRalt_.SetColor(baseTextColor);

		valRalt_.SetText(valBuf);
		valRalt_.SetPosition({ valueX, currentY });
		valRalt_.SetFontSize(fontSize);
		valRalt_.SetColor(baseTextColor);

		currentY += lineSpacing;
	}

	// -------------------------------------------------------------
	// 5. 弾数 (AMMO / 機関砲) - WT画像準拠で空行セクションギャップを挿入
	// -------------------------------------------------------------
	currentY += InfoUIDefaults::kSectionGap * scale;
	{
		const int currentAmmo = (std::max)(0, data_.currentAmmo);
		const bool isLowAmmo = (data_.maxAmmo > 0 &&
			static_cast<float>(currentAmmo) / static_cast<float>(data_.maxAmmo) <= InfoUIDefaults::kLowAmmoThresholdRatio);

		Vector4 ammoCol = isLowAmmo ? InfoUIDefaults::kLowAmmoColor : baseTextColor;
		ammoCol.w *= currentAlpha;

		char valBuf[32];
		std::snprintf(valBuf, sizeof(valBuf), "%d", currentAmmo);

		labelAmmo_.SetPosition({ baseX, currentY });
		labelAmmo_.SetFontSize(fontSize);
		labelAmmo_.SetColor(baseTextColor);

		valAmmo_.SetText(valBuf);
		valAmmo_.SetPosition({ valueX, currentY });
		valAmmo_.SetFontSize(fontSize);
		valAmmo_.SetColor(ammoCol);
	}

	// 全 UIText の更新処理
	labelThrottle_.Update();
	valThrottle_.Update();
	labelSpeed_.Update();
	valSpeed_.Update();
	labelAlt_.Update();
	valAlt_.Update();
	labelRalt_.Update();
	valRalt_.Update();
	labelAmmo_.Update();
	valAmmo_.Update();
}

void InfoUI::Draw() {
	if (!isVisible_ || data_.alpha <= 0.001f) {
		return;
	}

	// 既存の文字UIクラス（UIText）の描画処理を一括呼び出し
	labelThrottle_.Draw();
	valThrottle_.Draw();

	labelSpeed_.Draw();
	valSpeed_.Draw();

	labelAlt_.Draw();
	valAlt_.Draw();

	labelRalt_.Draw();
	valRalt_.Draw();

	labelAmmo_.Draw();
	valAmmo_.Draw();
}
