#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PostEffectParams {
    int32_t kernelSize;
    float intensity;      // ブルームの広がり半径 (Bloom Blur Radius)
    float dirX;           // ブルームの強さ (Bloom Strength)
    float dirY;           // 輝度しきい値 (Bloom Brightness Threshold)
    float time;
    float3 colorTint;
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
    
    // ブルーム抽出のしきい値 (dirY) と強度 (dirX)
    float threshold = gParams.dirY;
    float strength = gParams.dirX;
    
    // サンプリングステップの決定 (解像度に合わせる)
    float2 step = float2(1.0f / 1280.0f, 1.0f / 720.0f) * gParams.intensity * 2.0f;
    
    // 9タップガウシアンブラーの重み
    float weights[5] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };
    
    float32_t3 blurColor = float32_t3(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;
    
    // 1. 中心ピクセルの高輝度判定と蓄積
    float32_t3 val = baseColor.rgb;
    float luminance = dot(val, float32_t3(0.2125f, 0.7154f, 0.0721f));
    if (luminance > threshold) {
        blurColor += val * weights[0];
    }
    totalWeight += weights[0];
    
    // 2. 十字（水平・垂直）方向に隣接する周辺ピクセルをサンプリング
    for (int i = 1; i < 5; ++i) {
        float2 offsetH = float2(i, 0.0f) * step;
        float2 offsetV = float2(0.0f, i) * step;
        
        float32_t3 c1 = gTexture.Sample(gSampler, input.texcoord + offsetH).rgb;
        float32_t3 c2 = gTexture.Sample(gSampler, input.texcoord - offsetH).rgb;
        float32_t3 c3 = gTexture.Sample(gSampler, input.texcoord + offsetV).rgb;
        float32_t3 c4 = gTexture.Sample(gSampler, input.texcoord - offsetV).rgb;
        
        float l1 = dot(c1, float32_t3(0.2125f, 0.7154f, 0.0721f));
        float l2 = dot(c2, float32_t3(0.2125f, 0.7154f, 0.0721f));
        float l3 = dot(c3, float32_t3(0.2125f, 0.7154f, 0.0721f));
        float l4 = dot(c4, float32_t3(0.2125f, 0.7154f, 0.0721f));
        
        if (l1 > threshold) blurColor += c1 * weights[i];
        if (l2 > threshold) blurColor += c2 * weights[i];
        if (l3 > threshold) blurColor += c3 * weights[i];
        if (l4 > threshold) blurColor += c4 * weights[i];
        
        totalWeight += weights[i] * 4.0f;
    }
    
    // 平均化されたブルームの眩しさを強度(strength)を乗算して元の色に加算
    float32_t3 bloomColor = (blurColor / totalWeight) * strength;
    output.color = float32_t4(baseColor.rgb + bloomColor, baseColor.a);
    
    return output;
}
