#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer PostEffectParams : register(b0)
{
    int32_t kernelSize;   // 未使用
    float intensity;      // ビネットの強さ (pow の指数, 大きいほど暗い)
    float dirX;           // カラーオーバーレイの強度 (0.0〜)
    float dirY;           // 未使用
    float time;           // 未使用
    float colorR;         // オーバーレイ色 R
    float colorG;         // オーバーレイ色 G
    float colorB;         // オーバーレイ色 B
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);

    // 周囲を0に、中心になるほど明るくなるように
    float32_t2 correct = input.texcoord * (1.0f - input.texcoord.yx);
    float vignette = correct.x * correct.y * 16.0f;
    
    // 1. ビネット暗転処理
    // intensity で pow の指数を制御: 大きい値ほどビネット（暗転）が強くなる
    float blackoutFactor = saturate(pow(vignette, intensity));
    output.color.rgb *= blackoutFactor;

    // 2. カラーオーバーレイ処理 (RGB対応)
    // dirX はオーバーレイの強度（0.0〜）
    if (dirX > 0.0f) {
        // 画面の端ほど強く色が乗り、中央も色味がかかるようにする
        float edgeFactor = saturate(1.0f - vignette);
        
        // オーバーレイの強さに応じて、基本の色＋端の色をブレンド
        float totalOverlay = saturate(dirX * (0.3f + edgeFactor * 0.7f));
        
        float3 overlayColor = float3(colorR, colorG, colorB);
        output.color.rgb = lerp(output.color.rgb, overlayColor, totalOverlay);
    }
    
    return output;
}