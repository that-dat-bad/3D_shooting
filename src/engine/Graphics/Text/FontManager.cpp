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

    // このゲームで実際に使用されている全漢字（901文字）の事前登録リスト
    static const std::u32string kUsedGameKanji =
        U"一七万三上下不与両並中丸主久乗九乱了予事二互五井亡交人今仕他付代令以仮仰件任伏会伴伸似位低体何余作使例供依価便係保信修俯"
        U"個倍倒候値偏停側偶備傷傾働像僚億優元充先光克入全八公六共具典内円再冒写冷処出分切列初判別利到制刷刺刻則削前剛剣副剰割力功"
        U"加助効動務勝勢化北区十千半南単印危即厚原厳去参友双反収取受口句可台右号司各合同名向含吸吹告周味呼命和哀品問善喪器噴四回囲"
        U"図固土圧在地均垂型埃埋域基堂報場塔塗填境増墜壁壊声変夕外多夜大天太央失奇奥奨女始委姿威子字存学孫宇安完宙定宜実宣害容寂寄"
        U"密寸対寿専射将導小少尾屈届屋展属層履山崩嵐川巡左巨差巻市布帯帰帳常幅幕干平年幾広床序底度座庫廃延建式引弦弧弱張強弾当形影"
        U"役往径待後徐従得御復微徹心必応念思急性恐悪情想愁意感態慣慮成戒戦戻所手打扱承抑投抗折抜択抵押抽担招拠拡括持指挙挫振捉捗掌"
        U"排掛探接控推揃描提揚換握揺損搭携摘摩撃撤操擦擬支改攻放故敗散数整敵敷文斉料斜斬断新方施旋既日旦旧早昇明昏易星映昼時普景暖"
        U"暗暮曲更書替最月有期木未末本材束条来東板析枚果枠枯染柔柱査栄核根格案梁棄森検楕業極概構様標権横樽橋機次止正武歪歴死殊残殲"
        U"段毎毒比気水氷求汎汚決沈沢油沿況法波注洗活派流浮海消淡深渇済減渡温測湾満源準溜溝滅滑滞漂漆漏演漢潰激濃瀬火灯災炎点烈無焦"
        U"然焼煙煤照熱燃爆片版物特状独狭猛猶率王珠現球理璧環生産用由甲男画界留略番異疑症痕発登白百的盛監盤目直相盾省真着瞬瞰矢知矩"
        U"短砂砲破確示神票禁秒称移程種稼積穏穴空突立竜章端第等策箇算管箱節範築簡粉粋粒精系約紅紋納純級素索細終組経結統絶継続維綺緊"
        U"総緑線編緩練縁縦縮繊繋繰置美群義翼考耐肌背胴能脅脈脚脱腹膜膨臨自致航般舵良色芒花若英茶荒荷莢落蒸蓄蔵蔽薄薬虚行術街衛衝表"
        U"衰被装裏裕補複西要覆見規視覚覧親観角解触言計記訪設許診証評詰該詳認誘語誤説読課調識警譲護象負費質赤走起超足距跡路跳身車軌"
        U"軍転軸軽較載輝輪辺込辿迎近返迫追退送逆透途這通速造連進遅遊運過道達遠適遮遷選避還部郭都配重野量金鉄銀銃銅鋭鋼錐録鍮鎖鏡長"
        U"閃閉開間関閾闘防降限除陸険陽隅階随隔隙際障隠隣集離難雲雷電震青静非面音響頂項順須領頭頻題類風飛食飾首駄駆骨骸高魔鮮鶏麗黄"
        U"黒";

    // 必要な全コードポイントを1つの配列に集約
    std::vector<int> codepoints;
    codepoints.reserve(2048);

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

    // 6. 一般的な追加特殊記号 (度数、矢印、幾何学記号、三点リーダー等)
    static const char32_t kCommonSymbols[] = {
        U'°', U'±', U'×', U'÷', U'…', U'※', U'★', U'☆',
        U'■', U'□', U'▲', U'△', U'▼', U'▽', U'◆', U'◇',
        U'○', U'●', U'◎', U'→', U'←', U'↑', U'↓'
    };
    for (char32_t c : kCommonSymbols) {
        codepoints.push_back(static_cast<int>(c));
    }

    // 7. このゲームで使われている全漢字 (901文字)
    for (char32_t c : kUsedGameKanji) {
        if (c > 0 && c <= 0x10FFFF) {
            codepoints.push_back(static_cast<int>(c));
        }
    }

    std::sort(codepoints.begin(), codepoints.end());
    codepoints.erase(std::unique(codepoints.begin(), codepoints.end()), codepoints.end());

    // 高解像度パッキング（基本は 4096 x 4096 テクスチャ、必要時に自動拡張）
    int texDim = 4096;
    stbtt_pack_context spc;
    std::vector<unsigned char> tempBitmap;
    std::vector<stbtt_packedchar> packedChars(codepoints.size());

    for (int attempt = 0; attempt < 2; ++attempt) {
        tempBitmap.assign(texDim * texDim, 0);
        if (!stbtt_PackBegin(&spc, tempBitmap.data(), texDim, texDim, 0, 1, nullptr)) {
            return result;
        }
        stbtt_PackSetOversampling(&spc, 1, 1);

        stbtt_pack_range range{};
        range.font_size = pixelHeight;
        range.first_unicode_codepoint_in_range = 0;
        range.array_of_unicode_codepoints = codepoints.data();
        range.num_chars = static_cast<int>(codepoints.size());
        range.chardata_for_range = packedChars.data();

        int ret = stbtt_PackFontRanges(&spc, ttfBuffer.data(), 0, &range, 1);
        stbtt_PackEnd(&spc);

        if (ret == 1) {
            break;
        }
        // 万が一文字サイズが巨大で収まらなかった場合は 8192 に拡張してリトライ
        texDim = 8192;
    }

    result.texWidth = texDim;
    result.texHeight = texDim;
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
