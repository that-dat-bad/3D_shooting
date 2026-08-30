#pragma once
#include <string>
#include <unordered_map>
#include <vector>

/// @brief 汎用CFG / INI設定ファイルパーサー
/// [Section] 構造、Key = Value ペア、# / ; / // コメントに対応
class ConfigFile {
public:
	ConfigFile() = default;
	~ConfigFile() = default;

	/// @brief ファイルからCFGデータを読み込む
	/// @param filepath ファイルパス
	/// @return 読み込み成功時 true
	bool LoadFromFile(const std::string& filepath);

	/// @brief CFGデータをファイルに保存する
	/// @param filepath ファイルパス
	/// @return 保存成功時 true
	bool SaveToFile(const std::string& filepath) const;

	// === ゲッター ===
	std::string GetString(const std::string& section, const std::string& key, const std::string& defaultValue = "") const;
	float GetFloat(const std::string& section, const std::string& key, float defaultValue = 0.0f) const;
	int GetInt(const std::string& section, const std::string& key, int defaultValue = 0) const;
	bool GetBool(const std::string& section, const std::string& key, bool defaultValue = false) const;

	// === セッター ===
	void SetString(const std::string& section, const std::string& key, const std::string& value);
	void SetFloat(const std::string& section, const std::string& key, float value);
	void SetInt(const std::string& section, const std::string& key, int value);
	void SetBool(const std::string& section, const std::string& key, bool value);

	/// @brief セクションが存在するか
	bool HasSection(const std::string& section) const;

	/// @brief キーが存在するか
	bool HasKey(const std::string& section, const std::string& key) const;

	/// @brief 全セクション名を取得
	std::vector<std::string> GetSections() const;

	/// @brief クリア
	void Clear();

private:
	// 文字列のトリム
	static std::string Trim(const std::string& str);
	// コメントの除去
	static std::string RemoveComments(const std::string& line);

private:
	// section -> (key -> value)
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data_;
	// セクションの順序を保持（保存時の見栄えのため）
	std::vector<std::string> sectionOrder_;
	std::unordered_map<std::string, std::vector<std::string>> keyOrder_;
};
