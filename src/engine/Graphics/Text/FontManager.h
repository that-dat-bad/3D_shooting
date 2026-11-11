#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <future>
#include <memory>
#include <mutex>
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
/// フォントロードリクエスト定義
/// </summary>
struct FontLoadRequest {
    std::string fontName;
    std::string filePath;
    float pixelHeight = 48.0f;
};

/// <summary>
/// バックグラウンドラスタライズの中間結果
/// </summary>
struct FontRasterResult {
    std::string fontName;
    float pixelHeight = 0.0f;
    int texWidth = 4096;
    int texHeight = 4096;
    std::vector<uint32_t> rgbaBitmap;
    CharacterInfo info;
    bool success = false;
};

/// <summary>
/// フォントの読み込みと管理を行うクラス（マルチスレッド非同期ロード対応）
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
    /// フレーム毎更新（非同期ロードが完了したフォントテクスチャをGPUへシームレスにアップロード）
    /// </summary>
    void Update();

	/// <summary>
	/// フォントファイルを同期的に読み込んでテクスチャ化する
	/// </summary>
	/// <param name="fontName">登録するフォント名</param>
	/// <param name="filePath">フォントファイルのパス(.ttf等)</param>
	/// <param name="pixelHeight">文字のピクセル高</param>
    void LoadFont(const std::string& fontName, const std::string& filePath, float pixelHeight);

    /// <summary>
    /// フォントファイルをバックグラウンドスレッドで非同期に読み込み・ラスタライズする（非同期処理による軽量化）
    /// </summary>
    void LoadFontAsync(const std::string& fontName, const std::string& filePath, float pixelHeight);

    /// <summary>
    /// 複数フォントをCPU全コアで並列・非同期に一括ラスタライズする（起動・シーン遷移の爆速化）
    /// </summary>
    void LoadFontsParallel(const std::vector<FontLoadRequest>& requests);

    /// <summary>
    /// 指定フォントのロードが完了しているか判定
    /// </summary>
    bool IsFontLoaded(const std::string& fontName) const;

    /// <summary>
    /// 未完了の非同期フォントロードタスクの完了を待機
    /// </summary>
    void WaitForAllPendingFonts();

	/// <summary>
	/// 登録済みのフォント文字情報を取得する（未ロード時は利用可能な別フォントへ自動フォールバック）
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

    // CPU処理のみを行う純粋ラスタライズ関数（ワーカースレッドから呼び出し可能）
    static FontRasterResult RasterizeFontInternal(const std::string& fontName, const std::string& filePath, float pixelHeight);

    mutable std::mutex fontsMutex_;
    std::unordered_map<std::string, CharacterInfo> fonts_;

    // 非同期ロード中のフューチャータスクリスト
    std::vector<std::future<FontRasterResult>> pendingFontFutures_;
};
