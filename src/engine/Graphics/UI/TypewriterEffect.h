#pragma once
#include <string>

/// @brief テキストのタイピング文字送り演出＋点滅カーソル管理クラス
///        UTF-8マルチバイト（日本語等）に完全対応し、一文字ずつの安全な切り出しと点滅カーソル演出を提供
class TypewriterEffect {
public:
	TypewriterEffect() = default;
	explicit TypewriterEffect(const std::string& fullText, float charInterval = 0.030f, bool showCursor = true);

	/// @brief 対象テキストと速度の設定
	/// @param text 表示対象の全文
	/// @param charInterval 1文字あたりの表示間隔（秒）
	void SetText(const std::string& text, float charInterval = 0.030f);
	const std::string& GetTargetText() const { return targetText_; }

	/// @brief 毎フレーム更新
	/// @param dt デルタタイム（秒）
	void Update(float dt);

	/// @brief 現在表示すべき文字列（カーソル点滅込み）を取得
	std::string GetCurrentText() const;

	/// @brief カーソル無しの現在表示文字列を取得
	std::string GetRawVisibleText() const;

	/// @brief タイピングが完了したか
	bool IsComplete() const { return isComplete_; }

	/// @brief 演出を最初からリセット
	void Reset();

	/// @brief 演出をスキップして全文を一括表示
	void Skip();

	/// @brief カーソル文字の設定（デフォルト: "_"）
	void SetCursorChar(const std::string& cursor) { cursorChar_ = cursor; }
	/// @brief カーソル点滅の有無
	void SetShowCursor(bool show) { showCursor_ = show; }
	/// @brief 速度（1文字あたりの秒数）の動的変更
	void SetCharInterval(float interval) { charInterval_ = interval; }
	/// @brief カーソル点滅速度（Hz相当）
	void SetCursorBlinkRate(float hz) { cursorBlinkRate_ = hz; }

	/// @brief 現在表示されている文字数を取得
	size_t GetVisibleCharCount() const { return visibleCharCount_; }
	/// @brief 全文の文字数を取得
	size_t GetTotalCharCount() const { return totalCharCount_; }

	/// @brief 進捗率（0.0f〜1.0f）から直接タイピングテキストを生成する静的便利関数
	static std::string GetTextByProgress(const std::string& fullText, float progress, bool showCursor = true, float blinkTimer = 0.0f, float blinkRate = 16.0f, const std::string& cursor = "_");

	/// @brief UTF-8 文字列の文字数カウント
	static size_t CountUtf8Chars(const std::string& str);
	/// @brief UTF-8 文字列の部分取得（マルチバイト文字境界保護）
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
