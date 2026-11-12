#include "TypewriterEffect.h"
#include <cmath>

TypewriterEffect::TypewriterEffect(const std::string& fullText, float charInterval, bool showCursor)
	: charInterval_(charInterval), showCursor_(showCursor)
{
	SetText(fullText, charInterval);
}

void TypewriterEffect::SetText(const std::string& text, float charInterval) {
	targetText_ = text;
	charInterval_ = charInterval;
	timer_ = 0.0f;
	visibleCharCount_ = 0;
	isComplete_ = false;
	totalCharCount_ = CountUtf8Chars(targetText_);
}

void TypewriterEffect::Update(float dt) {
	if (isComplete_) return;

	timer_ += dt;
	if (charInterval_ <= 0.0f) {
		visibleCharCount_ = totalCharCount_;
		isComplete_ = true;
		return;
	}

	visibleCharCount_ = static_cast<size_t>(timer_ / charInterval_);
	if (visibleCharCount_ >= totalCharCount_) {
		visibleCharCount_ = totalCharCount_;
		isComplete_ = true;
	}
}

std::string TypewriterEffect::GetRawVisibleText() const {
	if (visibleCharCount_ >= totalCharCount_) {
		return targetText_;
	}
	return GetUtf8Substr(targetText_, visibleCharCount_);
}

std::string TypewriterEffect::GetCurrentText() const {
	std::string result = GetRawVisibleText();
	if (!isComplete_ && showCursor_) {
		// 指定周波数でカーソルを点滅（偶数フレームで表示）
		if (static_cast<int>(timer_ * cursorBlinkRate_) % 2 == 0) {
			result += cursorChar_;
		}
	}
	return result;
}

void TypewriterEffect::Reset() {
	timer_ = 0.0f;
	visibleCharCount_ = 0;
	isComplete_ = false;
}

void TypewriterEffect::Skip() {
	visibleCharCount_ = totalCharCount_;
	isComplete_ = true;
}

std::string TypewriterEffect::GetTextByProgress(
	const std::string& fullText, float progress, bool showCursor,
	float blinkTimer, float blinkRate, const std::string& cursor)
{
	float clampedProgress = (std::max)(0.0f, (std::min)(1.0f, progress));
	size_t totalChars = CountUtf8Chars(fullText);
	size_t charCount = static_cast<size_t>(clampedProgress * static_cast<float>(totalChars));

	std::string result = GetUtf8Substr(fullText, charCount);
	if (clampedProgress < 1.0f && showCursor) {
		bool cursorBlink = (static_cast<int>(blinkTimer * blinkRate) % 2) == 0;
		if (cursorBlink) {
			result += cursor;
		}
	}
	return result;
}

size_t TypewriterEffect::CountUtf8Chars(const std::string& str) {
	size_t count = 0;
	for (size_t i = 0; i < str.size(); ) {
		unsigned char c = static_cast<unsigned char>(str[i]);
		if (c <= 0x7F) i += 1;
		else if ((c & 0xE0) == 0xC0) i += 2;
		else if ((c & 0xF0) == 0xE0) i += 3;
		else if ((c & 0xF8) == 0xF0) i += 4;
		else i += 1;
		count++;
	}
	return count;
}

std::string TypewriterEffect::GetUtf8Substr(const std::string& str, size_t charCount) {
	size_t current = 0;
	size_t byteIdx = 0;
	while (byteIdx < str.size() && current < charCount) {
		unsigned char c = static_cast<unsigned char>(str[byteIdx]);
		if (c <= 0x7F) byteIdx += 1;
		else if ((c & 0xE0) == 0xC0) byteIdx += 2;
		else if ((c & 0xF0) == 0xE0) byteIdx += 3;
		else if ((c & 0xF8) == 0xF0) byteIdx += 4;
		else byteIdx += 1;
		current++;
	}
	return str.substr(0, byteIdx);
}
