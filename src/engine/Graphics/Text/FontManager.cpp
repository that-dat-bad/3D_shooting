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
    fonts_.clear();
}

void FontManager::LoadFont(const std::string& fontName, const std::string& filePath, float pixelHeight) {
    if (fonts_.contains(fontName)) { return; }

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        assert(false && "Failed to open font file.");
        return;
    }
    std::streamsize size = file.tellg();
    if (size <= 0) {
        assert(false && "Invalid font file size.");
        return;
    }
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> ttfBuffer(size);
    if (!file.read((char*)ttfBuffer.data(), size)) {
        assert(false && "Failed to read font file.");
        return;
    }

    // 高解像度パッキング（4096 x 4096テクスチャで大文字サイズも極めてシャープに描画）
    stbtt_pack_context spc;
    int texWidth = 4096;
    int texHeight = 4096;
    std::vector<unsigned char> tempBitmap(texWidth * texHeight);

    if (!stbtt_PackBegin(&spc, tempBitmap.data(), texWidth, texHeight, 0, 1, nullptr)) {
        assert(false && "Failed to begin packing.");
        return;
    }
    stbtt_PackSetOversampling(&spc, 1, 1);

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

    // 必要な全コードポイントを1つの配列に集約（レンジ分割による stb_truetype の missing_glyph インデックス不整合バグを回避）
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

    // 単一レンジでパッキング（stb_truetype の複数レンジ時 missing_glyph バッファオーバーランを完全に防止）
    stbtt_PackFontRanges(&spc, ttfBuffer.data(), 0, &range, 1);
    stbtt_PackEnd(&spc);

    CharacterInfo info;
    info.size = pixelHeight;
    info.textureName = fontName + "_Tex";
    info.textureIndex = 0;

    // ハッシュマップに格納
    for (size_t i = 0; i < codepoints.size(); ++i) {
        info.glyphs[codepoints[i]] = packedChars[i];
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
