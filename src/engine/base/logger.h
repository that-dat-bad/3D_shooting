#pragma once
#include<string>
/// <summary>
/// ログ出力用名前空間
/// </summary>
namespace logger {
	/// <summary>
	/// デバッグ用のログを出力する
	/// </summary>
	/// <param name="message">出力するメッセージ</param>
	void Log(const std::string& message);
}
