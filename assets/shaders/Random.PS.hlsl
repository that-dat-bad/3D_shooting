#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PostEffectParams {
    int32_t kernelSize;
    float intensity;      // ノイズの強さ (Noise Strength / Scale)
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

// 2D座標を用いた高品質ハッシュ
float rand2dTo1d(float2 value) {
    float2 smallValue = sin(value);
    float random = dot(smallValue, float2(12.9898, 78.233));
    random = frac(sin(random) * 43758.5453);
    return random;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t4 baseColor = gTexture.Sample(gSampler, input.texcoord);
    
    // 時間による時間的シード変化を加えてノイズを動かす (gParams.time)
    // ノイズのテクスチャ解像度を上げるため、座標に大きい値をかける
    // sin/cosの組み合わせにより、毎フレームごとにランダムに座標オフセットをずらす
    float2 offset = float2(sin(gParams.time * 29.35f) * 100.0f, cos(gParams.time * 37.17f) * 100.0f);
    float2 noiseCoord = input.texcoord * float2(960.0f, 540.0f) + offset;
    float noise = rand2dTo1d(noiseCoord);
    
    // ノイズを -0.5 ~ +0.5 に正規化し、強度(intensity)を乗算して加算する
    float32_t3 noiseFactor = (noise - 0.5f) * gParams.intensity;
    
    // intensity <= 0.15 の時は従来の微小粒子ノイズ
    // intensity > 0.15 からは信号減衰が進み、ベース画像が徐々にアナログ砂嵐（スノーノイズ）へ埋没していく
    float snowBlend = saturate((gParams.intensity - 0.15f) / 0.60f); // 0.15〜0.75で 0.0 -> 1.0
    float3 snowColor = float3(noise, noise, noise);
    float3 fadedBase = lerp(baseColor.rgb, snowColor, snowBlend * 0.90f);
    
    output.color.rgb = saturate(fadedBase + noiseFactor);
    output.color.a = baseColor.a;
    
    return output;
}
