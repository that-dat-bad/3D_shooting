#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class UIText;

/// @brief 全シーンのUIText要素を名前付きで登録・管理するレジストリ。
///        UITextEditorから参照するために使用する。
class UITextRegistry {
public:
	static UITextRegistry* GetInstance();

	/// @brief UITextを名前付きで登録する
	/// @param name 一意な識別名（例: "Title_Main"）
	/// @param text 登録するUITextへのポインタ
	void Register(const std::string& name, UIText* text);

	/// @brief 指定した名前のUITextの登録を解除する
	void Unregister(const std::string& name);

	/// @brief 全ての登録を解除する（シーン遷移時に呼ぶ）
	void Clear();

	/// @brief 登録されている全UITextを取得する
	const std::unordered_map<std::string, UIText*>& GetAll() const { return registry_; }

	/// @brief 登録名の一覧をソート済みで取得する（エディタ表示用）
	std::vector<std::string> GetSortedNames() const;

	/// @brief INIファイルから保存済みスタイルを読み込む
	void LoadConfig(const std::string& filePath = "assets/ui/text_styles.ini");

	/// @brief 現在の全登録テキストのスタイルをINIファイルに保存する
	void SaveConfig(const std::string& filePath = "assets/ui/text_styles.ini");

	/// @brief 指定したUITextにINIの保存済みスタイルを適用する
	void ApplyConfig(const std::string& name, UIText* text);

private:
	UITextRegistry() = default;
	~UITextRegistry() = default;
	UITextRegistry(const UITextRegistry&) = delete;
	UITextRegistry& operator=(const UITextRegistry&) = delete;

	std::unordered_map<std::string, UIText*> registry_;
};
