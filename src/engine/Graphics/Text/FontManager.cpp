#include "FontManager.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../external/stb/stb_truetype.h"

#include "../System/TextureManager.h"
#include <fstream>
#include <cassert>
#include <vector>

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

    // パッキングコンテキストの準備
    stbtt_pack_context spc;
    int texWidth = 2048;
    int texHeight = 2048;
    std::vector<unsigned char> tempBitmap(texWidth * texHeight);

    stbtt_PackBegin(&spc, tempBitmap.data(), texWidth, texHeight, 0, 1, nullptr);
    stbtt_PackSetOversampling(&spc, 1, 1);

    // ベイクする範囲の定義
    // 1. ASCII (32-126)
    // 2. ひらがな (0x3040-0x309F)
    // 3. カタカナ (0x30A0-0x30FF)
    // 4. 全角記号 (0xFF00-0xFFEF)
    // 5. CJK記号・句読点 (0x3000-0x303F)
    std::vector<stbtt_packedchar> asciiData(96);
    std::vector<stbtt_packedchar> hiraganaData(96);
    std::vector<stbtt_packedchar> katakanaData(96);
    std::vector<stbtt_packedchar> fullwidthData(240);
    std::vector<stbtt_packedchar> cjkSymbolsData(64);

    stbtt_pack_range ranges[5] = {};
    
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

    // 0は最初のフォントインデックス (TTCの場合も0を指定)
    stbtt_PackFontRanges(&spc, ttfBuffer.data(), 0, ranges, 5);
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
