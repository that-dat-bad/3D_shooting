#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSamplerLinear : register(s0);

cbuffer PostEffectParams : register(b0)
{
    int32_t kernelSize;
    float intensity;      // 輪郭検出の感度・乗算値 (Multiplier)
    float dirX;
    float dirY;
    float time;
    float3 colorTint;     // 輪郭線のカラー (R, G, B)
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

static const float32_t kPrewittHorizontalKernel[3][3] = {
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
};

static const float32_t kPrewittVerticalKernel[3][3] = {
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    {  0.0f,         0.0f,         0.0f },
    {  1.0f / 6.0f,  1.0f / 6.0f,  1.0f / 6.0f },
};

float32_t Luminance(float32_t3 v) {
    return dot(v, float32_t3(0.2125f, 0.7154f, 0.0721f));
}

PixelShaderOutput main(VertexShaderOutput input)
{
    uint32_t width, height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp((float32_t)width), rcp((float32_t)height));

    float32_t2 difference = float32_t2(0.0f, 0.0f);
    
    for (int32_t y = 0; y < 3; ++y) {
        for (int32_t x = 0; x < 3; ++x) {
            float32_t2 texcoord = input.texcoord + float32_t2(x - 1, y - 1) * uvStepSize;
            float32_t3 fetchColor = gTexture.Sample(gSamplerLinear, texcoord).rgb;
            float32_t luminance = Luminance(fetchColor);
            difference.x += luminance * kPrewittHorizontalKernel[y][x];
            difference.y += luminance * kPrewittVerticalKernel[y][x];
        }
    }

    float32_t weight = length(difference);
    float32_t mult = (intensity > 0.0f) ? intensity : 6.0f;
    weight = saturate(weight * mult);

    PixelShaderOutput output;
    float32_t3 baseColor = gTexture.Sample(gSamplerLinear, input.texcoord).rgb;
    // 輪郭部分(weight)に指定されたカラー (colorTint) を合成
    output.color.rgb = lerp(baseColor, colorTint, weight);
    output.color.a = 1.0f;
    return output;
}
