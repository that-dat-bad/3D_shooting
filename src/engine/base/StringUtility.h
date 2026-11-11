#pragma once
#include <string>
/// <summary>
/// 文字列変換ユーティリティ名前空間
/// </summary>
namespace StringUtility {
	/// <summary>
	/// std::string (マルチバイト文字列) を std::wstring (ワイド文字列) に変換する
	/// </summary>
	/// <param name="str">変換元の文字列 (std::string)</param>
	/// <returns>変換後のワイド文字列 (std::wstring)</returns>
	std::wstring ConvertString(const std::string& str);

	/// <summary>
	/// std::wstring (ワイド文字列) を std::string (マルチバイト文字列) に変換する
	/// </summary>
	/// <param name="str">変換元のワイド文字列 (std::wstring)</param>
	/// <returns>変換後の文字列 (std::string)</returns>
	std::string ConvertString(const std::wstring& str);
};
