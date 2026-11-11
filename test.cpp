#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#define STB_TRUETYPE_IMPLEMENTATION
#include "c:\Users\gamer\source\repos\25B_GE_CG\external\stb\stb_truetype.h"

int main() {
    std::ifstream file("C:/Windows/Fonts/msgothic.ttc", std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> ttfBuffer(size);
    file.read((char*)ttfBuffer.data(), size);

    stbtt_pack_context spc;
    int texWidth = 4096;
    int texHeight = 4096;
    std::vector<unsigned char> tempBitmap(texWidth * texHeight);

    stbtt_PackBegin(&spc, tempBitmap.data(), texWidth, texHeight, 0, 1, nullptr);
    stbtt_PackSetOversampling(&spc, 1, 1);

    static const std::u32string kCommonKanji = U"戦雷撃破残数時間操作設定終了勝敗点次戻再開始面難易度音量効果主要任務完成失敗機体敵弾速度高力残量命中率得点最新記録結果状態確認情報選択決定戻る";
    std::vector<int> kanjiCodepoints;
    for (char32_t c : kCommonKanji) {
        kanjiCodepoints.push_back(static_cast<int>(c));
    }
    std::vector<stbtt_packedchar> kanjiData(kanjiCodepoints.size());

    stbtt_pack_range ranges[6] = {};
    ranges[5].font_size = 96.0f;
    ranges[5].first_unicode_codepoint_in_range = 0;
    ranges[5].array_of_unicode_codepoints = kanjiCodepoints.data();
    ranges[5].num_chars = (int)kanjiCodepoints.size();
    ranges[5].chardata_for_range = kanjiData.data();

    std::cout << "Packing..." << std::endl;
    stbtt_PackFontRanges(&spc, ttfBuffer.data(), 0, ranges, 6);
    std::cout << "Done!" << std::endl;
    return 0;
}
