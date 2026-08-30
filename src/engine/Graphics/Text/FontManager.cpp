#include "FontManager.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../external/stb/stb_truetype.h"

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
    fonts_.clear();
}

void FontManager::LoadFont(const std::string& fontName, const std::string& filePath, float pixelHeight) {
    if (fonts_.contains(fontName)) { return; }

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    assert(file.is_open() && "Failed to open font file.");
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> ttfBuffer(size);
    if (!file.read((char*)ttfBuffer.data(), size)) {
        assert(false && "Failed to read font file.");
    }

    // 高解像度パッキング（4096 x 4096テクスチャで大文字サイズも極めてシャープに描画）
    stbtt_pack_context spc;
    int texWidth = 4096;
    int texHeight = 4096;
    std::vector<unsigned char> tempBitmap(texWidth * texHeight);

    stbtt_PackBegin(&spc, tempBitmap.data(), texWidth, texHeight, 0, 1, nullptr);
    stbtt_PackSetOversampling(&spc, 1, 1);

    // ベイクする範囲の定義
    // 1. ASCII (32-126)
    // 2. ひらがな (0x3040-0x309F)
    // 3. カタカナ (0x30A0-0x30FF)
    // 4. 全角記号 (0xFF00-0xFFEF)
    // 5. CJK記号・句読点 (0x3000-0x303F)
    // 6. 常用・ゲーム主要漢字
    std::vector<stbtt_packedchar> asciiData(96);
    std::vector<stbtt_packedchar> hiraganaData(96);
    std::vector<stbtt_packedchar> katakanaData(96);
    std::vector<stbtt_packedchar> fullwidthData(240);
    std::vector<stbtt_packedchar> cjkSymbolsData(64);

    // 主要漢字のリスト
    static const std::u32string kCommonKanji =
        U"戦雷撃破残数時間操作設定終了勝敗点次戻再開始面難易度音量効果主要任務完成失敗機体敵弾速度高力残量命中率得点最新記録結果状態確認情報選択決定戻る"
        U"一二三四五六七八九十百千万億円日月火水木金土年月日日時分秒前後左右上下東西南北"
        U"無有正誤入出大小中長短多少高低新旧良悪強弱勝負生死光暗炎氷雷風水土毒盾剣銃"
        U"自他友敵王神魔竜人女男子供赤青緑黄黒白金銀"
        U"攻撃防御回避回復移動飛行加速減速照準索敵通信警告危険注意停止開始設定初期"
        U"画面解像度音質言語全般表示描画処理速度"
        U"第章節段級位界宇宙星空陸海川山森都市街建物道路橋"
        U"自機僚機標的目標迎撃索敵追尾離脱接近交戦戦果作戦指令司令部防衛制圧";

    std::vector<int> kanjiCodepoints;
    for (char32_t c : kCommonKanji) {
        kanjiCodepoints.push_back(static_cast<int>(c));
    }
    std::sort(kanjiCodepoints.begin(), kanjiCodepoints.end());
    kanjiCodepoints.erase(std::unique(kanjiCodepoints.begin(), kanjiCodepoints.end()), kanjiCodepoints.end());

    std::vector<stbtt_packedchar> kanjiData(kanjiCodepoints.size());

    stbtt_pack_range ranges[6] = {};
    
    ranges[0].font_size = pixelHeight;
    ranges[0].first_unicode_codepoint_in_range = 32;
    ranges[0].num_chars = 96;
    ranges[0].chardata_for_range = asciiData.data();

    ranges[1].font_size = pixelHeight;
    ranges[1].first_unicode_codepoint_in_range = 0x3040;
    ranges[1].num_chars = 96;
    ranges[1].chardata_for_range = hiraganaData.data();

    ranges[2].font_size = pixelHeight;
    ranges[2].first_unicode_codepoint_in_range = 0x30A0;
    ranges[2].num_chars = 96;
    ranges[2].chardata_for_range = katakanaData.data();

    ranges[3].font_size = pixelHeight;
    ranges[3].first_unicode_codepoint_in_range = 0xFF00;
    ranges[3].num_chars = 240;
    ranges[3].chardata_for_range = fullwidthData.data();

    ranges[4].font_size = pixelHeight;
    ranges[4].first_unicode_codepoint_in_range = 0x3000;
    ranges[4].num_chars = 64;
    ranges[4].chardata_for_range = cjkSymbolsData.data();

    ranges[5].font_size = pixelHeight;
    ranges[5].first_unicode_codepoint_in_range = 0;
    ranges[5].array_of_unicode_codepoints = kanjiCodepoints.data();
    ranges[5].num_chars = static_cast<int>(kanjiCodepoints.size());
    ranges[5].chardata_for_range = kanjiData.data();

    // 0は最初のフォントインデックス (TTCの場合も0を指定)
    stbtt_PackFontRanges(&spc, ttfBuffer.data(), 0, ranges, 6);
    stbtt_PackEnd(&spc);

    CharacterInfo info;
    info.size = pixelHeight;
    info.textureName = fontName + "_Tex";

    // ハッシュマップに格納
    for (int i = 0; i < 96; ++i) info.glyphs[32 + i] = asciiData[i];
    for (int i = 0; i < 96; ++i) info.glyphs[0x3040 + i] = hiraganaData[i];
    for (int i = 0; i < 96; ++i) info.glyphs[0x30A0 + i] = katakanaData[i];
    for (int i = 0; i < 240; ++i) info.glyphs[0xFF00 + i] = fullwidthData[i];
    for (int i = 0; i < 64; ++i) info.glyphs[0x3000 + i] = cjkSymbolsData[i];
    for (size_t i = 0; i < kanjiCodepoints.size(); ++i) {
        info.glyphs[kanjiCodepoints[i]] = kanjiData[i];
    }

    std::vector<uint32_t> rgbaBitmap(texWidth * texHeight);
    for (int i = 0; i < texWidth * texHeight; ++i) {
        uint8_t alpha = tempBitmap[i];
        rgbaBitmap[i] = (alpha << 24) | (255 << 16) | (255 << 8) | 255;
    }

    TextureManager::GetInstance()->LoadTextureFromRawPixels(
        info.textureName, texWidth, texHeight, DXGI_FORMAT_R8G8B8A8_UNORM, rgbaBitmap.data()
    );

    info.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(info.textureName);

    fonts_[fontName] = info;
}

const CharacterInfo* FontManager::GetCharacterInfo(const std::string& fontName) const {
    auto it = fonts_.find(fontName);
    if (it != fonts_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> FontManager::GetFontNames() const {
    std::vector<std::string> names;
    names.reserve(fonts_.size());
    for (const auto& [name, _] : fonts_) {
        names.push_back(name);
    }
    return names;
}
