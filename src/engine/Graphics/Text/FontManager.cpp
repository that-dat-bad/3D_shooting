#include "FontManager.h"

#define STBRP_STATIC
#define STB_RECT_PACK_IMPLEMENTATION
#include "../../../external/imgui/imstb_rectpack.h"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../external/imgui/imstb_truetype.h"

#include "../System/TextureManager.h"
#include <fstream>
#include <cassert>
#include <vector>
#include <algorithm>
#include <string>

FontManager* FontManager::GetInstance() {
    static FontManager instance;
    return &instance;
}

void FontManager::Initialize() {
}

void FontManager::Finalize() {
    WaitForAllPendingFonts();
    std::lock_guard<std::mutex> lock(fontsMutex_);
    fonts_.clear();
}

void FontManager::Update() {
    // 完了した非同期タスクを取り出して GPU にテクスチャ転送
    if (pendingFontFutures_.empty()) { return; }

    for (auto it = pendingFontFutures_.begin(); it != pendingFontFutures_.end(); ) {
        if (it->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            FontRasterResult res = it->get();
            if (res.success) {
                TextureManager::GetInstance()->LoadTextureFromRawPixels(
                    res.info.textureName, res.texWidth, res.texHeight, DXGI_FORMAT_R8G8B8A8_UNORM, res.rgbaBitmap.data()
                );
                res.info.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(res.info.textureName);

                std::lock_guard<std::mutex> lock(fontsMutex_);
                fonts_[res.fontName] = res.info;
            }
            it = pendingFontFutures_.erase(it);
        } else {
            ++it;
        }
    }
}

FontRasterResult FontManager::RasterizeFontInternal(const std::string& fontName, const std::string& filePath, float pixelHeight) {
    FontRasterResult result;
    result.fontName = fontName;
    result.pixelHeight = pixelHeight;
    result.success = false;

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return result;
    }
    std::streamsize size = file.tellg();
    if (size <= 0) {
        return result;
    }
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> ttfBuffer(size);
    if (!file.read((char*)ttfBuffer.data(), size)) {
        return result;
    }

    // 高解像度パッキング（4096 x 4096テクスチャ）
    stbtt_pack_context spc;
    result.texWidth = 4096;
    result.texHeight = 4096;
    std::vector<unsigned char> tempBitmap(result.texWidth * result.texHeight);

    if (!stbtt_PackBegin(&spc, tempBitmap.data(), result.texWidth, result.texHeight, 0, 1, nullptr)) {
        return result;
    }
    stbtt_PackSetOversampling(&spc, 1, 1);

    // 主要漢字のリスト
    static const std::u32string kCommonKanji =
        U"戦雷撃破残数時間操作設定終了勝敗点次戻再開始面難易度音量効果主要任務完成失敗機体敵弾速度高力残量命中率得点最新記録結果状態確認情報選択決定戻る"
        U"一二三四五六七八九十百千万億円日月火水木金土年月日日時分秒前後左右上下東西南北"
        U"無有正誤入出大小中長短多少高低新旧良悪強弱勝負生死光暗炎氷雷風水土毒盾剣銃"
        U"自他友敵王神魔竜人女男子供赤青緑黄黒白金銀"
        U"攻撃防御回避回復移動飛行加速減速照準索敵通信警告危険注意停止開始設定初期起動式換"
        U"画面解像度音質言語全般表示描画処理速度"
        U"第章節段級位界宇宙星空陸海川山森都市街建物道路橋"
        U"自機僚機標的目標迎撃索敵追尾離脱接近交戦戦果作戦指令司令部防衛制圧";

    // 必要な全コードポイントを1つの配列に集約
    std::vector<int> codepoints;
    codepoints.reserve(1024);

    // 1. ASCII (32-126)
    for (int c = 32; c <= 126; ++c) codepoints.push_back(c);

    // 2. ひらがな (0x3040-0x309F)
    for (int c = 0x3040; c <= 0x309F; ++c) codepoints.push_back(c);

    // 3. カタカナ (0x30A0-0x30FF)
    for (int c = 0x30A0; c <= 0x30FF; ++c) codepoints.push_back(c);

    // 4. CJK記号・句読点 (0x3000-0x303F)
    for (int c = 0x3000; c <= 0x303F; ++c) codepoints.push_back(c);

    // 5. 全角記号・英数 (0xFF01-0xFF5E)
    for (int c = 0xFF01; c <= 0xFF5E; ++c) codepoints.push_back(c);

    // 6. 主要漢字
    for (char32_t c : kCommonKanji) {
        if (c > 0 && c <= 0x10FFFF) {
            codepoints.push_back(static_cast<int>(c));
        }
    }

    std::sort(codepoints.begin(), codepoints.end());
    codepoints.erase(std::unique(codepoints.begin(), codepoints.end()), codepoints.end());

    std::vector<stbtt_packedchar> packedChars(codepoints.size());

    stbtt_pack_range range{};
    range.font_size = pixelHeight;
    range.first_unicode_codepoint_in_range = 0;
    range.array_of_unicode_codepoints = codepoints.data();
    range.num_chars = static_cast<int>(codepoints.size());
    range.chardata_for_range = packedChars.data();

    stbtt_PackFontRanges(&spc, ttfBuffer.data(), 0, &range, 1);
    stbtt_PackEnd(&spc);

    result.info.size = pixelHeight;
    result.info.textureName = fontName + "_Tex";
    result.info.textureIndex = 0;

    // フォントの垂直メトリクス取得
    stbtt_fontinfo fontInfo;
    int fontOffset = stbtt_GetFontOffsetForIndex(ttfBuffer.data(), 0);
    if (fontOffset >= 0 && stbtt_InitFont(&fontInfo, ttfBuffer.data(), fontOffset)) {
        int ascent = 0, descent = 0, lineGap = 0;
        stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
        float vScale = stbtt_ScaleForPixelHeight(&fontInfo, pixelHeight);
        result.info.ascent = static_cast<float>(ascent) * vScale;
        result.info.descent = static_cast<float>(descent) * vScale;
        result.info.lineGap = static_cast<float>(lineGap) * vScale;
    } else {
        result.info.ascent = pixelHeight * 0.8f;
        result.info.descent = -pixelHeight * 0.2f;
        result.info.lineGap = 0.0f;
    }

    for (size_t i = 0; i < codepoints.size(); ++i) {
        result.info.glyphs[codepoints[i]] = packedChars[i];
    }

    result.rgbaBitmap.resize(result.texWidth * result.texHeight);
    for (int i = 0; i < result.texWidth * result.texHeight; ++i) {
        uint8_t alpha = tempBitmap[i];
        result.rgbaBitmap[i] = (alpha << 24) | (255 << 16) | (255 << 8) | 255;
    }

    result.success = true;
    return result;
}

void FontManager::LoadFont(const std::string& fontName, const std::string& filePath, float pixelHeight) {
    {
        std::lock_guard<std::mutex> lock(fontsMutex_);
        if (fonts_.contains(fontName)) { return; }
    }

    FontRasterResult res = RasterizeFontInternal(fontName, filePath, pixelHeight);
    if (res.success) {
        TextureManager::GetInstance()->LoadTextureFromRawPixels(
            res.info.textureName, res.texWidth, res.texHeight, DXGI_FORMAT_R8G8B8A8_UNORM, res.rgbaBitmap.data()
        );
        res.info.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(res.info.textureName);

        std::lock_guard<std::mutex> lock(fontsMutex_);
        fonts_[fontName] = res.info;
    }
}

void FontManager::LoadFontAsync(const std::string& fontName, const std::string& filePath, float pixelHeight) {
    {
        std::lock_guard<std::mutex> lock(fontsMutex_);
        if (fonts_.contains(fontName)) { return; }
    }

    // バックグラウンドスレッドで非同期にラスタライズを実行
    pendingFontFutures_.push_back(
        std::async(std::launch::async, &FontManager::RasterizeFontInternal, fontName, filePath, pixelHeight)
    );
}

void FontManager::LoadFontsParallel(const std::vector<FontLoadRequest>& requests) {
    std::vector<std::future<FontRasterResult>> futures;
    futures.reserve(requests.size());

    // CPU全コアを使って一斉に非同期ラスタライズ開始
    for (const auto& req : requests) {
        {
            std::lock_guard<std::mutex> lock(fontsMutex_);
            if (fonts_.contains(req.fontName)) continue;
        }
        futures.push_back(
            std::async(std::launch::async, &FontManager::RasterizeFontInternal, req.fontName, req.filePath, req.pixelHeight)
        );
    }

    // 全タスクの完了を待機し、メインスレッドで一括GPUテクスチャ化
    for (auto& fut : futures) {
        FontRasterResult res = fut.get();
        if (res.success) {
            TextureManager::GetInstance()->LoadTextureFromRawPixels(
                res.info.textureName, res.texWidth, res.texHeight, DXGI_FORMAT_R8G8B8A8_UNORM, res.rgbaBitmap.data()
            );
            res.info.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(res.info.textureName);

            std::lock_guard<std::mutex> lock(fontsMutex_);
            fonts_[res.fontName] = res.info;
        }
    }
}

bool FontManager::IsFontLoaded(const std::string& fontName) const {
    std::lock_guard<std::mutex> lock(fontsMutex_);
    return fonts_.contains(fontName);
}

void FontManager::WaitForAllPendingFonts() {
    for (auto& fut : pendingFontFutures_) {
        if (fut.valid()) {
            FontRasterResult res = fut.get();
            if (res.success) {
                TextureManager::GetInstance()->LoadTextureFromRawPixels(
                    res.info.textureName, res.texWidth, res.texHeight, DXGI_FORMAT_R8G8B8A8_UNORM, res.rgbaBitmap.data()
                );
                res.info.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(res.info.textureName);

                std::lock_guard<std::mutex> lock(fontsMutex_);
                fonts_[res.fontName] = res.info;
            }
        }
    }
    pendingFontFutures_.clear();
}

const CharacterInfo* FontManager::GetCharacterInfo(const std::string& fontName) const {
    std::lock_guard<std::mutex> lock(fontsMutex_);
    auto it = fonts_.find(fontName);
    if (it != fonts_.end()) {
        return &it->second;
    }
    // ロード未完了時の安全なフォールバック（クラッシュ防止）
    if (!fonts_.empty()) {
        return &fonts_.begin()->second;
    }
    return nullptr;
}

std::vector<std::string> FontManager::GetFontNames() const {
    std::lock_guard<std::mutex> lock(fontsMutex_);
    std::vector<std::string> names;
    names.reserve(fonts_.size());
    for (const auto& [name, _] : fonts_) {
        names.push_back(name);
    }
    return names;
}
