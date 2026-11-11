#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PostEffectParams {
    int32_t kernelSize;   // モード切り替え (0: 通常カラー, 1: グレースケール+カラー指定)
    float intensity;      // 走査線の暗さ (0.0: 効果なし, 1.0: 最大)
    float dirX;           // 走査線の密度 (細さ)
    float dirY;           // 走査線のスクロール速度
    float time;           // 時間
    float3 colorTint;     // 輝度に掛ける色
};
ConstantBuffer<PostEffectParams> gParams : register(b0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t4 baseColor = gTexture.Sample(gSampler, input.texcoord);
    
    float32_t3 finalColor;
    if (gParams.kernelSize == 1) {
        // グレースケール化してから指定の色を掛ける (暗視モード)
        float32_t luminance = dot(baseColor.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
        finalColor = luminance * gParams.colorTint;
    } else {
        // 通常カラーに指定の色を掛ける（デフォルトは 1.0, 1.0, 1.0 で変化なし）
        finalColor = baseColor.rgb * gParams.colorTint;
    }
    
    // 走査線の密度 (デフォルトは 600)
    float density = gParams.dirX > 0.0f ? gParams.dirX : 600.0f;
    float scroll = gParams.time * gParams.dirY;
    
    // サイン波で走査線の明暗を計算
    float scanline = sin(input.texcoord.y * density + scroll) * 0.5f + 0.5f;
    
    // 走査線強度に応じて色を暗くブレンドする
    finalColor *= lerp(1.0f, scanline, gParams.intensity);
    
    output.color = float32_t4(finalColor, baseColor.a);
    return output;
}
