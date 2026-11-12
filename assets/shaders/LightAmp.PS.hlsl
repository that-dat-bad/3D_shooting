#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PostEffectParams {
    int32_t kernelSize;
    float intensity;      // 光量増幅倍率 (Exposure / Gain multiplier)
    float dirX;           // コントラスト / ガンマ調整
    float dirY;
    float time;
    float3 colorTint;     // 基本着色カラー (NVD用)
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
    
    // 1. 入力カラーの輝度 (Luminance) を計算
    float luminance = dot(baseColor.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
    
    // 2. 光量増幅 (intensity倍)
    float gain = gParams.intensity > 0.0f ? gParams.intensity : 3.0f;
    float ampLuminance = luminance * gain;
    
    // 3. 暗視グリーンカラーの決定 (colorTintが設定されていれば使用)
    float32_t3 nvdTint = (gParams.colorTint.r > 0.0f || gParams.colorTint.g > 0.0f || gParams.colorTint.b > 0.0f)
                       ? gParams.colorTint
                       : float32_t3(0.15f, 1.0f, 0.3f);
                       
    // 4. 増幅輝度に基づく着色
    float32_t3 greenColored = nvdTint * ampLuminance;
    
    // 5. NVD白飛び (Whiteout) 処理: 輝度が増幅されて一定値を超えると暗視管が飽和し真っ白(1,1,1)へ移行
    float whiteoutFactor = saturate((ampLuminance - 0.65f) * 2.2f);
    float32_t3 finalColor = lerp(greenColored, float32_t3(1.0f, 1.0f, 1.0f), whiteoutFactor);
    
    output.color = float32_t4(finalColor, baseColor.a);
    return output;
}
