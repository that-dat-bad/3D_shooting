#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "../../../external/imgui/imstb_truetype.h"
#include "../base/Math/MyMath.h"

using namespace MyMath;

/// <summary>
/// フォントの文字情報(グリフデータ等)を保持する構造体
/// </summary>
struct CharacterInfo {
    std::unordered_map<char32_t, stbtt_packedchar> glyphs;
    std::string textureName;
    uint32_t textureIndex = 0;
    float size = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float lineGap = 0.0f;
};

/// <summary>
/// フォントの読み込みと管理を行うクラス
/// </summary>
class FontManager {
public:
	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns>インスタンスポインタ</returns>
    static FontManager* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
    void Initialize();
	
	/// <summary>
	/// 終了処理
	/// </summary>
    void Finalize();

	/// <summary>
	/// フォントファイルを読み込んでテクスチャ化する
	/// </summary>
	/// <param name="fontName">登録するフォント名</param>
	/// <param name="filePath">フォントファイルのパス(.ttf等)</param>
	/// <param name="pixelHeight">文字のピクセル高</param>
    void LoadFont(const std::string& fontName, const std::string& filePath, float pixelHeight);

	/// <summary>
	/// 登録済みのフォント文字情報を取得する
	/// </summary>
	/// <param name="fontName">フォント名</param>
	/// <returns>文字情報のポインタ (存在しない場合はnullptr)</returns>
    const CharacterInfo* GetCharacterInfo(const std::string& fontName) const;

	/// <summary>
	/// 登録済みの全フォント名を取得する（エディタ用）
	/// </summary>
	/// <returns>フォント名のリスト</returns>
	std::vector<std::string> GetFontNames() const;

private:
    FontManager() = default;
    ~FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    std::unordered_map<std::string, CharacterInfo> fonts_;
};
