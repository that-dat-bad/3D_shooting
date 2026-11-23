#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../base/Math/MyMath.h"
#include "../Sprite/Sprite.h"

using namespace MyMath;
class SpriteCommon;

class TextRenderer {
public:
    static TextRenderer* GetInstance();

    void Initialize(SpriteCommon* spriteCommon);
    void Finalize();

    // 画面に文字を描画するために積む
    void Print(const std::string& fontName, const std::string& text, float x, float y, float size, const Vector4& color = {1.0f, 1.0f, 1.0f, 1.0f}, const Vector2& anchorPoint = {0.0f, 0.0f});

    // 指定したフォントとサイズでの文字列の描画サイズを計算する
    Vector2 CalculateTextSize(const std::string& fontName, const std::string& text, float size);

    // スプライトプールの事前確保
    void PrewarmPool(size_t count);

    // 積まれた文字を実際に描画する
    void Draw();

    // 未描画の文字を途中描画する
    void Flush();

private:
    TextRenderer() = default;
    ~TextRenderer() = default;
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    SpriteCommon* spriteCommon_ = nullptr;
    
    // スプライトのプール
    std::vector<std::unique_ptr<Sprite>> spritePool_;
    uint32_t usedSpriteCount_ = 0;
    uint32_t flushedSpriteCount_ = 0; // このフレームで Flush 済みのスプライト数
    uint32_t dynamicAllocCount_ = 0;   // このフレームで動的生成されたスプライト数
    double dynamicAllocTimeMs_ = 0.0;  // 動的バッファ確保にかかった累計時間(ms)

    Sprite* GetNextSprite();
};
