#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PostEffectParams {
    int32_t kernelSize;
    float intensity;      // 抽出カラーの許容判定範囲 (Tolerance / Range)
    float dirX;           // トランジションの境界の滑らかさ (Smoothness)
    float dirY;           // 非対象カラーのモノクロ化率 (Desaturation, 1.0 = 完全モノクロ)
    float time;
    float3 targetColor;   // 抽出したい目標カラー (R, G, B)
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
    
    // 目標カラー (R, G, B) と現在のピクセル色との距離を計算
    float32_t dist = length(baseColor.rgb - gParams.targetColor);
    
    // 輝度（モノクロ）計算
    float32_t luminance = dot(baseColor.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
    float32_t3 grayColor = float32_t3(luminance, luminance, luminance);
    
    // 許容範囲 (intensity) と滑らかさ (dirX) によるフェード率
    float32_t tolerance = max(gParams.intensity, 0.01f);
    float32_t smoothness = max(gParams.dirX, 0.001f);
    float32_t factor = smoothstep(tolerance, tolerance - smoothness, dist);
    
    // モノクロ化率 (dirY) に応じた非対象領域の脱色
    float32_t desatAmount = saturate(gParams.dirY);
    float32_t3 nonTargetColor = lerp(baseColor.rgb, grayColor, desatAmount);
    
    // 目標色付近は鮮やかな元の色、それ以外は脱色カラーを合成
    output.color.rgb = lerp(nonTargetColor, baseColor.rgb, factor);
    output.color.a = baseColor.a;
    
    return output;
}
