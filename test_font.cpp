#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cassert>
#include <algorithm>

#define STB_TRUETYPE_IMPLEMENTATION
#include "external/stb/stb_truetype.h"

int main() {
    std::string filePath = "C:/Windows/Fonts/msgothic.ttc";
    float pixelHeight = 96.0f;

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open font file.\n";
        return 1;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> ttfBuffer(size);
    if (!file.read((char*)ttfBuffer.data(), size)) {
        std::cerr << "Failed to read font file.\n";
        return 1;
    }
    std::cout << "File size: " << size << "\n";

    stbtt_pack_context spc;
    int texWidth = 4096;
    int texHeight = 4096;
    std::vector<unsigned char> tempBitmap(texWidth * texHeight);

    stbtt_PackBegin(&spc, tempBitmap.data(), texWidth, texHeight, 0, 1, nullptr);
    stbtt_PackSetOversampling(&spc, 1, 1);

    std::vector<stbtt_packedchar> asciiData(96);
    std::vector<stbtt_packedchar> hiraganaData(96);
    std::vector<stbtt_packedchar> katakanaData(96);
    std::vector<stbtt_packedchar> fullwidthData(240);
    std::vector<stbtt_packedchar> cjkSymbolsData(64);

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

    std::cout << "Packing fonts...\n";
    int res = stbtt_PackFontRanges(&spc, ttfBuffer.data(), 0, ranges, 6);
    std::cout << "PackFontRanges returned " << res << "\n";

    stbtt_PackEnd(&spc);
    return 0;
}
