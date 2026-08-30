#include "TextRenderer.h"
#include "FontManager.h"

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

Sprite* TextRenderer::GetNextSprite() {
    if (usedSpriteCount_ >= spritePool_.size()) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(spriteCommon_, "");
        spritePool_.push_back(std::move(sprite));
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
        }
    }
    return {totalWidth, size};
}

void TextRenderer::Print(const std::string& fontName, const std::string& text, float x, float y, float size, const Vector4& color, const Vector2& anchorPoint) {
    const CharacterInfo* info = FontManager::GetInstance()->GetCharacterInfo(fontName);
    if (!info) { return; }

    float scale = size / info->size;
    
    Vector2 textSize = CalculateTextSize(fontName, text, size);
    float startX = x - textSize.x * anchorPoint.x;
    // テキスト全体の高さをsizeとし、ベースラインはその80%の位置にあると仮定する
    float startY = y + size * 0.8f - size * anchorPoint.y;

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
        }
    }
}

void TextRenderer::Draw() {
    for (uint32_t i = 0; i < usedSpriteCount_; ++i) {
        spritePool_[i]->Draw();
    }
    usedSpriteCount_ = 0; // 次のフレーム用にリセット
}
