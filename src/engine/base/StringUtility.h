#pragma once
#include <string>
/// <summary>
/// 文字列変換ユーティリティ名前空間
/// </summary>
namespace StringUtility {
	/// <summary>
	/// std::string を std::wstring に変換する
	/// </summary>
	/// <param name="str">変換元の文字列</param>
	/// <returns>変換後のワイド文字列</returns>
	std::wstring ConvertString(const std::string& str);

	/// <summary>
	/// std::wstring を std::string に変換する
	/// </summary>
	/// <param name="str">変換元のワイド文字列</param>
	/// <returns>変換後の文字列</returns>
	std::string ConvertString(const std::wstring& str);
};
