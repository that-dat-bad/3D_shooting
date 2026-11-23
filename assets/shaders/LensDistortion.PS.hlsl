#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PostEffectParams {
    int32_t kernelSize;
    float intensity;      // 歪み強度
    float dirX;
    float dirY;
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
    
    // UV座標を中心基準にする
    float2 uv = input.texcoord - 0.5f;
    
    // 自動ズーム率の適用
    float scale = 1.0f;
    if (gParams.intensity > 0.0f) {
        // intensity = 1.0 のときに約 0.77 倍にズームインして黒枠をトリミングする
        scale = 1.0f / (1.0f + 0.3f * gParams.intensity);
    } else if (gParams.intensity < 0.0f) {
        scale = 1.0f + 0.3f * abs(gParams.intensity);
    }
    
    // スケールされた座標
    float2 scaledUV = uv * scale;
    float r2 = dot(scaledUV, scaledUV);
    
    // 魚眼レンズ歪み適用
    float2 distortedUV = scaledUV * (1.0f + gParams.intensity * r2);
    
    // 元の範囲に戻す
    distortedUV += 0.5f;
    
    // 万が一の範囲外や境界部分の処理
    if (distortedUV.x < 0.0f || distortedUV.x > 1.0f || distortedUV.y < 0.0f || distortedUV.y > 1.0f) {
        output.color = float32_t4(0.0f, 0.0f, 0.0f, 1.0f);
    } else {
        output.color = gTexture.Sample(gSampler, distortedUV);
    }
    
    return output;
}
