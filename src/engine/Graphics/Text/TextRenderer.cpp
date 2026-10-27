#include "TextRenderer.h"
#include "FontManager.h"
#include "../../Game/System/BenchmarkConfig.h"
#include <chrono>

TextRenderer* TextRenderer::GetInstance() {
    static TextRenderer instance;
    return &instance;
}

void TextRenderer::Initialize(SpriteCommon* spriteCommon) {
    spriteCommon_ = spriteCommon;
    usedSpriteCount_ = 0;
}

void TextRenderer::Finalize() {
    spritePool_.clear();
}

void TextRenderer::PrewarmPool(size_t count) {
    if (!spriteCommon_) { return; }
#if BENCHMARK_DISABLE_PREWARM
    Benchmark::Log("[Benchmark:TextRenderer] PrewarmPool DISABLED -> Skipping pre-allocation. (Dynamic allocations will occur during rendering)");
    return;
#else
    auto start = std::chrono::high_resolution_clock::now();
    size_t prevCount = spritePool_.size();
    while (spritePool_.size() < count) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(spriteCommon_, "");
        spritePool_.push_back(std::move(sprite));
    }
    auto end = std::chrono::high_resolution_clock::now();
    double timeMs = std::chrono::duration<double, std::milli>(end - start).count();
    char buf[256];
    sprintf_s(buf, "[Benchmark:TextRenderer] PrewarmPool ENABLED -> Pre-allocated %zu sprites (total %zu) in %.2f ms (Zero-Allocation ready)", count > prevCount ? count - prevCount : size_t(0), spritePool_.size(), timeMs);
    Benchmark::Log(buf);
#endif
}

Sprite* TextRenderer::GetNextSprite() {
    if (usedSpriteCount_ >= spritePool_.size()) {
        auto start = std::chrono::high_resolution_clock::now();
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(spriteCommon_, "");
        spritePool_.push_back(std::move(sprite));
        auto end = std::chrono::high_resolution_clock::now();

        dynamicAllocCount_++;
        dynamicAllocTimeMs_ += std::chrono::duration<double, std::milli>(end - start).count();
    }
    return spritePool_[usedSpriteCount_++].get();
}

Vector2 TextRenderer::CalculateTextSize(const std::string& fontName, const std::string& text, float size) {
    const CharacterInfo* info = FontManager::GetInstance()->GetCharacterInfo(fontName);
    if (!info) { return {0.0f, 0.0f}; }

    float scale = size / info->size;
    float totalWidth = 0.0f;

    for (size_t i = 0; i < text.length(); ) {
        char32_t codepoint = 0;
        unsigned char c = text[i];
        if (c <= 0x7F) {
            codepoint = c; i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= text.length()) break;
            codepoint = ((c & 0x1F) << 6) | (text[i+1] & 0x3F); i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= text.length()) break;
            codepoint = ((c & 0x0F) << 12) | ((text[i+1] & 0x3F) << 6) | (text[i+2] & 0x3F); i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= text.length()) break;
            codepoint = ((c & 0x07) << 18) | ((text[i+1] & 0x3F) << 12) | ((text[i+2] & 0x3F) << 6) | (text[i+3] & 0x3F); i += 4;
        } else {
            i += 1; continue;
        }

        auto it = info->glyphs.find(codepoint);
        if (it != info->glyphs.end()) {
            totalWidth += it->second.xadvance * scale;
        } else {
            totalWidth += (codepoint > 0x7F) ? size : (size * 0.5f);
        }
    }
    float fontHeight = (info->ascent - info->descent) * scale;
    if (fontHeight <= 0.0f) { fontHeight = size; }
    return {totalWidth, fontHeight};
}

void TextRenderer::Print(const std::string& fontName, const std::string& text, float x, float y, float size, const Vector4& color, const Vector2& anchorPoint) {
    const CharacterInfo* info = FontManager::GetInstance()->GetCharacterInfo(fontName);
    if (!info) { return; }

    float scale = size / info->size;
    
    // anchorPoint.x が 0.0f（左揃え）の場合はテキスト幅の事前計算をスキップして高速化
    float startX = x;
    if (std::abs(anchorPoint.x) > 0.0001f) {
        Vector2 textSize = CalculateTextSize(fontName, text, size);
        startX = x - textSize.x * anchorPoint.x;
    }
    
    // フォントのascentとdescentに基づき、アンカーに応じたベースライン位置を正確に決定
    // anchorPoint.y = 0.0f: 上揃え (行上端が y) -> startY = y + ascent * scale
    // anchorPoint.y = 0.5f: 上下中央揃え (上下中心が y) -> startY = y + (ascent + descent) * 0.5f * scale
    // anchorPoint.y = 1.0f: 下揃え (行下端が y) -> startY = y + descent * scale
    float scaledAscent = info->ascent * scale;
    float scaledDescent = info->descent * scale;
    float startY = y + scaledAscent * (1.0f - anchorPoint.y) + scaledDescent * anchorPoint.y;

    for (size_t i = 0; i < text.length(); ) {
        char32_t codepoint = 0;
        unsigned char c = text[i];
        if (c <= 0x7F) {
            codepoint = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= text.length()) { break; }
            codepoint = ((c & 0x1F) << 6) | (text[i+1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= text.length()) { break; }
            codepoint = ((c & 0x0F) << 12) | ((text[i+1] & 0x3F) << 6) | (text[i+2] & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= text.length()) { break; }
            codepoint = ((c & 0x07) << 18) | ((text[i+1] & 0x3F) << 12) | ((text[i+2] & 0x3F) << 6) | (text[i+3] & 0x3F);
            i += 4;
        } else {
            i += 1;
            continue;
        }

        auto it = info->glyphs.find(codepoint);
        if (it != info->glyphs.end()) {
            const stbtt_packedchar* b = &it->second;
            
            // 四角形の生成
            float xpos = startX + b->xoff * scale;
            float ypos = startY + b->yoff * scale;
            float width = (b->x1 - b->x0) * scale;
            float height = (b->y1 - b->y0) * scale;

            Sprite* sprite = GetNextSprite();
            sprite->SetTextureIndex(info->textureIndex);
            
            // テクスチャのピクセル座標指定
            sprite->SetTextureLeftTop({(float)b->x0, (float)b->y0});
            sprite->SetTextureSize({(float)(b->x1 - b->x0), (float)(b->y1 - b->y0)});
            
            // 描画サイズと位置
            sprite->SetSize({width, height});
            sprite->SetPosition({xpos, ypos});
            sprite->SetAnchorPoint({0.0f, 0.0f}); // 左上基準
            sprite->SetColor(color);
            
            sprite->Update();

            // 次の文字へ進める
            startX += b->xadvance * scale;
        } else {
            // 未登録文字または特殊スペース時の文字送りフォールバック
            float adv = (codepoint > 0x7F) ? size : (size * 0.5f);
            startX += adv;
        }
    }
}

void TextRenderer::Flush() {
    for (uint32_t i = flushedSpriteCount_; i < usedSpriteCount_; ++i) {
        spritePool_[i]->Draw();
    }
    flushedSpriteCount_ = usedSpriteCount_;
}

void TextRenderer::Draw() {
    Flush();

    // ベンチマークログ出力
    if (dynamicAllocCount_ > 0) {
        char buf[256];
        sprintf_s(buf, "[Benchmark:TextRenderer] Dynamic Allocations: %u sprites (Hitch / D3D12 Spike: %.2f ms)", dynamicAllocCount_, dynamicAllocTimeMs_);
        Benchmark::Log(buf);
        dynamicAllocCount_ = 0;
        dynamicAllocTimeMs_ = 0.0;
    }

    usedSpriteCount_ = 0; // 次のフレーム用にリセット
    flushedSpriteCount_ = 0;
}
