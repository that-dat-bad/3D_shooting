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

    // 画面に文字を描画するために積む（毎フレーム呼ぶ想定）
    void Print(const std::string& fontName, const std::string& text, float x, float y, float size, const Vector4& color = {1.0f, 1.0f, 1.0f, 1.0f}, const Vector2& anchorPoint = {0.0f, 0.0f});

    // 指定したフォントとサイズでの文字列の描画サイズを計算する
    Vector2 CalculateTextSize(const std::string& fontName, const std::string& text, float size);

    // 積まれた文字を実際に描画する（SceneのDraw等で一括で呼ぶ）
    void Draw();

private:
    TextRenderer() = default;
    ~TextRenderer() = default;
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    SpriteCommon* spriteCommon_ = nullptr;
    
    // スプライトのプール
    std::vector<std::unique_ptr<Sprite>> spritePool_;
    uint32_t usedSpriteCount_ = 0;

    Sprite* GetNextSprite();
};
