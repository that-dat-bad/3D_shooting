#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PostEffectParams {
    int32_t kernelSize;
    float intensity;      // 光量増幅 (Light Amplification / Gain)
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
    output.color = gTexture.Sample(gSampler, input.texcoord);
    
    // 輝度計算
    float32_t value = dot(output.color.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
    
    // intensity を用いて光量を増幅 (露出/ゲイン調整)
    value *= gParams.intensity;
    
    // 目標とする色 (colorTint) を乗算して着色する
    output.color.rgb = value * gParams.colorTint;
    
    return output;
}
