#pragma once
#include <string>

// タイプライター演出
class TypewriterEffect {
public:
	TypewriterEffect() = default;
	explicit TypewriterEffect(const std::string& fullText, float charInterval = 0.030f, bool showCursor = true);

	// テキストと速度の設定
	// text: 表示テキスト
	// charInterval: 表示間隔
	void SetText(const std::string& text, float charInterval = 0.030f);
	const std::string& GetTargetText() const { return targetText_; }

	// 更新
	// dt: デルタタイム
	void Update(float dt);

	// 表示用文字列の取得
	std::string GetCurrentText() const;

	// カーソル抜きの表示文字列取得
	std::string GetRawVisibleText() const;

	// 完了判定
	bool IsComplete() const { return isComplete_; }

	// リセット
	void Reset();

	// スキップ
	void Skip();

	void SetCursorChar(const std::string& cursor) { cursorChar_ = cursor; }
	void SetShowCursor(bool show) { showCursor_ = show; }
	void SetCharInterval(float interval) { charInterval_ = interval; }
	void SetCursorBlinkRate(float hz) { cursorBlinkRate_ = hz; }

	size_t GetVisibleCharCount() const { return visibleCharCount_; }
	size_t GetTotalCharCount() const { return totalCharCount_; }

	// 進捗率からテキストを生成
	static std::string GetTextByProgress(const std::string& fullText, float progress, bool showCursor = true, float blinkTimer = 0.0f, float blinkRate = 16.0f, const std::string& cursor = "_");

	// UTF-8文字数のカウント
	static size_t CountUtf8Chars(const std::string& str);
	// UTF-8部分文字列の取得
	static std::string GetUtf8Substr(const std::string& str, size_t charCount);

private:

	std::string targetText_;
	float charInterval_ = 0.030f;
	float timer_ = 0.0f;
	size_t visibleCharCount_ = 0;
	size_t totalCharCount_ = 0;
	bool isComplete_ = false;
	bool showCursor_ = true;
	std::string cursorChar_ = "_";
	float cursorBlinkRate_ = 12.0f;
};
