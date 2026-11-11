#define NOMINMAX
#include "RadioCommunicator.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

void RadioCommunicator::Initialize(SpriteCommon* spriteCommon) {
	spriteCommon_ = spriteCommon;
	typewriter_.SetShowCursor(false);
	typewriter_.SetCursorChar(RadioDefaults::kEmptyCursor);
	Clear();
}

void RadioCommunicator::Enqueue(
	const std::string& speakerName,
	const std::string& dialogueText,
	RadioFaction faction,
	float holdDuration)
{
	RadioMessage message{};
	message.speakerName = speakerName;
	message.dialogueText = dialogueText;
	message.faction = faction;
	message.holdDuration = holdDuration;
	message.charInterval = timingConfig_.defaultCharIntervalSeconds;
	message.interrupt = false;
	Enqueue(message);
}

void RadioCommunicator::Enqueue(const RadioMessage& message) {
	if (message.dialogueText.empty() && message.speakerName.empty()) {
		return;
	}

	RadioMessage sanitized = message;
	if (sanitized.holdDuration <= RadioDefaults::kZero) {
		sanitized.holdDuration = timingConfig_.defaultHoldSeconds;
	}
	if (sanitized.charInterval <= RadioDefaults::kZero) {
		sanitized.charInterval = timingConfig_.defaultCharIntervalSeconds;
	}

	if (sanitized.interrupt) {
		messageQueue_.push_front(sanitized);
		if (phase_ == Phase::Idle) {
			StartNextMessage();
		} else if (phase_ != Phase::Closing) {
			phase_ = Phase::Closing;
			phaseTimer_ = RadioDefaults::kZero;
			activeCloseDuration_ = (std::max)(RadioDefaults::kEpsilon, timingConfig_.interruptCloseSeconds);
		} else {
			activeCloseDuration_ = (std::min)(
				activeCloseDuration_,
				(std::max)(RadioDefaults::kEpsilon, timingConfig_.interruptCloseSeconds));
		}
		return;
	}

	messageQueue_.push_back(sanitized);
	if (phase_ == Phase::Idle) {
		StartNextMessage();
	}
}

void RadioCommunicator::Interrupt(
	const std::string& speakerName,
	const std::string& dialogueText,
	RadioFaction faction,
	float holdDuration)
{
	RadioMessage message{};
	message.speakerName = speakerName;
	message.dialogueText = dialogueText;
	message.faction = faction;
	message.holdDuration = holdDuration;
	message.charInterval = timingConfig_.defaultCharIntervalSeconds;
	message.interrupt = true;
	Enqueue(message);
}

void RadioCommunicator::SkipCurrent() {
	if (phase_ == Phase::Opening || phase_ == Phase::Typing) {
		typewriter_.Skip();
		phase_ = Phase::Hold;
		phaseTimer_ = RadioDefaults::kZero;
	} else if (phase_ == Phase::Hold) {
		phase_ = Phase::Closing;
		phaseTimer_ = RadioDefaults::kZero;
		activeCloseDuration_ = (std::max)(RadioDefaults::kEpsilon, timingConfig_.interruptCloseSeconds);
	}
}

void RadioCommunicator::Clear() {
	messageQueue_.clear();
	currentMessage_ = RadioMessage{};
	cleanSpeakerName_.clear();
	cleanDialogueBody_.clear();
	dialogueLines_.clear();
	typewriter_.Reset();
	phase_ = Phase::Idle;
	phaseTimer_ = RadioDefaults::kZero;
	activeCloseDuration_ = (std::max)(RadioDefaults::kEpsilon, timingConfig_.closeDurationSeconds);
}

bool RadioCommunicator::IsActive() const {
	return phase_ != Phase::Idle || !messageQueue_.empty();
}

void RadioCommunicator::Update(float deltaTime) {
	const float safeDt = (std::max)(RadioDefaults::kZero, deltaTime);

	switch (phase_) {
	case Phase::Idle:
		if (!messageQueue_.empty()) {
			StartNextMessage();
		}
		break;

	case Phase::Opening: {
		phaseTimer_ += safeDt;
		const float openDuration = (std::max)(RadioDefaults::kEpsilon, timingConfig_.openDurationSeconds);
		if (phaseTimer_ >= openDuration) {
			phaseTimer_ = RadioDefaults::kZero;
			phase_ = timingConfig_.enableTypewriter ? Phase::Typing : Phase::Hold;
		}
		break;
	}

	case Phase::Typing:
		typewriter_.Update(safeDt);
		if (typewriter_.IsComplete()) {
			phase_ = Phase::Hold;
			phaseTimer_ = RadioDefaults::kZero;
		}
		break;

	case Phase::Hold: {
		phaseTimer_ += safeDt;
		const float holdDuration = (std::max)(RadioDefaults::kMinHoldSeconds, currentMessage_.holdDuration);
		if (phaseTimer_ >= holdDuration) {
			phase_ = Phase::Closing;
			phaseTimer_ = RadioDefaults::kZero;
			activeCloseDuration_ = (std::max)(RadioDefaults::kEpsilon, timingConfig_.closeDurationSeconds);
		}
		break;
	}

	case Phase::Closing:
		phaseTimer_ += safeDt;
		if (phaseTimer_ >= activeCloseDuration_) {
			if (!messageQueue_.empty()) {
				StartNextMessage();
			} else {
				phase_ = Phase::Idle;
				phaseTimer_ = RadioDefaults::kZero;
			}
		}
		break;
	}
}

void RadioCommunicator::Draw(float screenWidth, float screenHeight) {
	if (phase_ == Phase::Idle || dialogueLines_.empty()) {
		return;
	}
	if (screenWidth <= RadioDefaults::kZero || screenHeight <= RadioDefaults::kZero) {
		return;
	}

	TextRenderer* textRenderer = TextRenderer::GetInstance();
	if (!textRenderer) {
		return;
	}

	// フェーズに応じたフェード不透明度の算出
	float alpha = RadioDefaults::kUnit;
	if (phase_ == Phase::Opening) {
		const float openDuration = (std::max)(RadioDefaults::kEpsilon, timingConfig_.openDurationSeconds);
		const float progress = std::clamp(phaseTimer_ / openDuration, RadioDefaults::kZero, RadioDefaults::kUnit);
		alpha = ComputeSmoothStep(progress);
	} else if (phase_ == Phase::Closing) {
		const float progress = std::clamp(phaseTimer_ / activeCloseDuration_, RadioDefaults::kZero, RadioDefaults::kUnit);
		alpha = RadioDefaults::kUnit - ComputeSmoothStep(progress);
	}

	if (alpha <= RadioDefaults::kEpsilon) {
		return;
	}

	const float scale = std::clamp(
		screenHeight / RadioDefaults::kReferenceScreenHeight,
		RadioDefaults::kMinResolutionScale,
		RadioDefaults::kMaxResolutionScale);

	const float centerX = screenWidth * RadioDefaults::kCenterAnchorRatioX;
	const float speakerTopY = screenHeight * layoutConfig_.topAnchorRatioY;
	const float speakerFontPx = layoutConfig_.speakerFontSize * scale;
	const float dialogueFontPx = layoutConfig_.dialogueFontSize * scale;
	const float speakerGapY = layoutConfig_.speakerToDialogueGap * scale;
	const float lineGapY = layoutConfig_.dialogueLineGap * scale;

	// 話者名および << >> 括弧のカラー（AC7準拠の陣営カラー）
	Vector4 speakerColor = ResolveSpeakerColor(currentMessage_.faction);
	speakerColor.w *= alpha;

	// セリフ本文のカラー（ホワイト）
	Vector4 bodyColor = RadioDefaults::kDialogueBodyColor;
	bodyColor.w *= alpha;

	// 1. 1行目: 話者名（例: Joker 2 Coster）を中央揃えで直接フローティング描画
	PrintFloatingText(
		RadioDefaults::kFontName,
		cleanSpeakerName_,
		centerX,
		speakerTopY,
		speakerFontPx,
		scale,
		speakerColor,
		{ RadioDefaults::kHalf, RadioDefaults::kZero });

	// 2. 2行目以降: << 会話内容 >>（<< と >> は話者と同色、会話内容はホワイト）
	const float dialogueStartY = speakerTopY + speakerFontPx + speakerGapY;
	const float openDelimWidth = textRenderer->CalculateTextSize(
		RadioDefaults::kFontName, RadioDefaults::kDialogueOpenDelimiter, dialogueFontPx).x;
	const float closeDelimWidth = textRenderer->CalculateTextSize(
		RadioDefaults::kFontName, RadioDefaults::kDialogueCloseDelimiter, dialogueFontPx).x;

	const bool showFullText = (!timingConfig_.enableTypewriter || typewriter_.IsComplete());
	std::vector<std::string> visibleBodyLines;
	if (showFullText) {
		visibleBodyLines = dialogueLines_;
	} else {
		visibleBodyLines = SplitLines(typewriter_.GetRawVisibleText());
	}

	for (size_t i = RadioDefaults::kInitialIndex; i < dialogueLines_.size(); ++i) {
		const bool isFirstLine = (i == RadioDefaults::kInitialIndex);
		const bool isLastLine = (i + RadioDefaults::kIndexStep == dialogueLines_.size());

		const float fullBodyWidth = textRenderer->CalculateTextSize(
			RadioDefaults::kFontName, dialogueLines_[i], dialogueFontPx).x;
		const float linePrefixWidth = isFirstLine ? openDelimWidth : RadioDefaults::kZero;
		const float lineSuffixWidth = isLastLine ? closeDelimWidth : RadioDefaults::kZero;
		const float totalLineWidth = linePrefixWidth + fullBodyWidth + lineSuffixWidth;

		float cursorX = centerX - totalLineWidth * RadioDefaults::kHalf;
		const float lineY = dialogueStartY + static_cast<float>(i) * (dialogueFontPx + lineGapY);

		// 左側 "<< "（話者名と同色）
		if (isFirstLine) {
			PrintFloatingText(
				RadioDefaults::kFontName,
				RadioDefaults::kDialogueOpenDelimiter,
				cursorX,
				lineY,
				dialogueFontPx,
				scale,
				speakerColor,
				{ RadioDefaults::kZero, RadioDefaults::kZero });
			cursorX += openDelimWidth;
		}

		// 中央 セリフ本文（ホワイト）
		if (i < visibleBodyLines.size() && !visibleBodyLines[i].empty()) {
			PrintFloatingText(
				RadioDefaults::kFontName,
				visibleBodyLines[i],
				cursorX,
				lineY,
				dialogueFontPx,
				scale,
				bodyColor,
				{ RadioDefaults::kZero, RadioDefaults::kZero });
		}
		cursorX += fullBodyWidth;

		// 右側 " >>"（話者名と同色）
		if (isLastLine && showFullText) {
			PrintFloatingText(
				RadioDefaults::kFontName,
				RadioDefaults::kDialogueCloseDelimiter,
				cursorX,
				lineY,
				dialogueFontPx,
				scale,
				speakerColor,
				{ RadioDefaults::kZero, RadioDefaults::kZero });
		}
	}
}

std::string RadioCommunicator::SanitizeSpeakerName(const std::string& rawSpeaker) {
	std::string trimmed = TrimSpaces(rawSpeaker);
	if (trimmed.size() >= RadioDefaults::kDoubleBracketPairMinLength &&
		trimmed.compare(
			RadioDefaults::kInitialIndex,
			RadioDefaults::kDoubleBracketCharCount,
			RadioDefaults::kDoubleOpenAngle) == static_cast<int>(RadioDefaults::kInitialIndex) &&
		trimmed.compare(
			trimmed.size() - RadioDefaults::kDoubleBracketCharCount,
			RadioDefaults::kDoubleBracketCharCount,
			RadioDefaults::kDoubleCloseAngle) == static_cast<int>(RadioDefaults::kInitialIndex)) {
		trimmed = TrimSpaces(trimmed.substr(
			RadioDefaults::kDoubleBracketCharCount,
			trimmed.size() - RadioDefaults::kDoubleBracketPairMinLength));
	} else if (trimmed.size() >= RadioDefaults::kEnclosingPairCharCount &&
		trimmed.front() == RadioDefaults::kSingleOpenAngleChar &&
		trimmed.back() == RadioDefaults::kSingleCloseAngleChar) {
		trimmed = TrimSpaces(trimmed.substr(
			RadioDefaults::kIndexStep,
			trimmed.size() - RadioDefaults::kEnclosingPairCharCount));
	}

	if (trimmed.empty()) {
		return std::string(RadioDefaults::kDefaultSpeakerName);
	}
	return trimmed;
}

std::string RadioCommunicator::SanitizeDialogueBody(const std::string& rawDialogue) {
	std::string trimmed = TrimSpaces(rawDialogue);
	if (trimmed.size() >= RadioDefaults::kDoubleBracketPairMinLength &&
		trimmed.compare(
			RadioDefaults::kInitialIndex,
			RadioDefaults::kDoubleBracketCharCount,
			RadioDefaults::kDoubleOpenAngle) == static_cast<int>(RadioDefaults::kInitialIndex) &&
		trimmed.compare(
			trimmed.size() - RadioDefaults::kDoubleBracketCharCount,
			RadioDefaults::kDoubleBracketCharCount,
			RadioDefaults::kDoubleCloseAngle) == static_cast<int>(RadioDefaults::kInitialIndex)) {
		return TrimSpaces(trimmed.substr(
			RadioDefaults::kDoubleBracketCharCount,
			trimmed.size() - RadioDefaults::kDoubleBracketPairMinLength));
	}

	if (trimmed.size() >= RadioDefaults::kEnclosingPairCharCount &&
		trimmed.front() == RadioDefaults::kDoubleQuoteChar &&
		trimmed.back() == RadioDefaults::kDoubleQuoteChar) {
		return trimmed.substr(
			RadioDefaults::kIndexStep,
			trimmed.size() - RadioDefaults::kEnclosingPairCharCount);
	}
	return rawDialogue;
}

void RadioCommunicator::StartNextMessage() {
	if (messageQueue_.empty()) {
		phase_ = Phase::Idle;
		return;
	}

	currentMessage_ = messageQueue_.front();
	messageQueue_.pop_front();

	cleanSpeakerName_ = SanitizeSpeakerName(currentMessage_.speakerName);
	cleanDialogueBody_ = SanitizeDialogueBody(currentMessage_.dialogueText);
	dialogueLines_ = SplitLines(cleanDialogueBody_);

	const float interval = (currentMessage_.charInterval > RadioDefaults::kZero)
		? currentMessage_.charInterval
		: timingConfig_.defaultCharIntervalSeconds;

	typewriter_.SetShowCursor(false);
	typewriter_.SetCursorChar(RadioDefaults::kEmptyCursor);
	typewriter_.SetText(cleanDialogueBody_, interval);
	if (!timingConfig_.enableTypewriter) {
		typewriter_.Skip();
	}

	phase_ = Phase::Opening;
	phaseTimer_ = RadioDefaults::kZero;
	activeCloseDuration_ = (std::max)(RadioDefaults::kEpsilon, timingConfig_.closeDurationSeconds);
}

Vector4 RadioCommunicator::ResolveSpeakerColor(RadioFaction faction) {
	switch (faction) {
	case RadioFaction::Ally:
		return RadioDefaults::kAllySpeakerColor;
	case RadioFaction::Awacs:
		return RadioDefaults::kAwacsSpeakerColor;
	case RadioFaction::Enemy:
		return RadioDefaults::kEnemySpeakerColor;
	case RadioFaction::System:
		return RadioDefaults::kSystemSpeakerColor;
	default:
		return RadioDefaults::kAllySpeakerColor;
	}
}

float RadioCommunicator::ComputeSmoothStep(float progress) {
	const float t = std::clamp(progress, RadioDefaults::kZero, RadioDefaults::kUnit);
	return t * t * (RadioDefaults::kSmoothStepFactorA - RadioDefaults::kSmoothStepFactorB * t);
}

std::vector<std::string> RadioCommunicator::SplitLines(const std::string& text) {
	std::vector<std::string> lines;
	std::string currentLine;
	for (char ch : text) {
		if (ch == RadioDefaults::kCarriageReturnChar) {
			continue;
		}
		if (ch == RadioDefaults::kNewlineChar) {
			lines.push_back(currentLine);
			currentLine.clear();
		} else {
			currentLine.push_back(ch);
		}
	}
	lines.push_back(currentLine);
	return lines;
}

std::string RadioCommunicator::TrimSpaces(const std::string& text) {
	size_t first = RadioDefaults::kInitialIndex;
	while (first < text.size() && text[first] == RadioDefaults::kSpaceChar) {
		first += RadioDefaults::kIndexStep;
	}
	if (first >= text.size()) {
		return std::string();
	}

	size_t last = text.size() - RadioDefaults::kIndexStep;
	while (last > first && text[last] == RadioDefaults::kSpaceChar) {
		last -= RadioDefaults::kIndexStep;
	}
	return text.substr(first, (last - first) + RadioDefaults::kIndexStep);
}

void RadioCommunicator::PrintFloatingText(
	const std::string& fontName,
	const std::string& text,
	float x,
	float y,
	float fontSize,
	float scale,
	const Vector4& textColor,
	const Vector2& anchorPoint)
{
	TextRenderer* textRenderer = TextRenderer::GetInstance();
	if (!textRenderer || text.empty()) {
		return;
	}

	Vector4 outerShadow = RadioDefaults::kOuterShadowColor;
	outerShadow.w *= textColor.w;
	textRenderer->Print(
		fontName,
		text,
		x + RadioDefaults::kOuterShadowOffsetX * scale,
		y + RadioDefaults::kOuterShadowOffsetY * scale,
		fontSize,
		outerShadow,
		anchorPoint);

	Vector4 innerShadow = RadioDefaults::kInnerShadowColor;
	innerShadow.w *= textColor.w;
	textRenderer->Print(
		fontName,
		text,
		x + RadioDefaults::kInnerShadowOffsetX * scale,
		y + RadioDefaults::kInnerShadowOffsetY * scale,
		fontSize,
		innerShadow,
		anchorPoint);

	textRenderer->Print(fontName, text, x, y, fontSize, textColor, anchorPoint);
}
