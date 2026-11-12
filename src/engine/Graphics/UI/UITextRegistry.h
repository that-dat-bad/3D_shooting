#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class UIText;

// UITextのレジストリクラス
class UITextRegistry {
public:
	static UITextRegistry* GetInstance();

	// テキストの登録
	// name: 識別名
	// text: 登録するテキスト
	void Register(const std::string& name, UIText* text);

	// 登録解除
	void Unregister(const std::string& name);

	// 全登録解除
	void Clear();

	// 全テキストの取得
	const std::unordered_map<std::string, UIText*>& GetAll() const { return registry_; }

	// 登録名一覧の取得
	std::vector<std::string> GetSortedNames() const;

	// 設定読み込み
	void LoadConfig(const std::string& filePath = "assets/ui/text_styles.ini");

	// 設定保存
	void SaveConfig(const std::string& filePath = "assets/ui/text_styles.ini");

	// 設定の適用
	void ApplyConfig(const std::string& name, UIText* text);

private:
	UITextRegistry() = default;
	~UITextRegistry() = default;
	UITextRegistry(const UITextRegistry&) = delete;
	UITextRegistry& operator=(const UITextRegistry&) = delete;

	std::unordered_map<std::string, UIText*> registry_;
};
