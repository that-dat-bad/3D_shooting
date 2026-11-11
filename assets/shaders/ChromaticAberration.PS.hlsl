#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PostEffectParams {
    int32_t kernelSize;
    float intensity;      // 色収差のズレの大きさ (Chromatic Aberration Intensity)
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
    
    // 画面中心 (0.5, 0.5) からの方向ベクトル
    float2 uvCenter = float2(0.5f, 0.5f);
    float2 dir = input.texcoord - uvCenter;
    
    // 赤(R)と青(B)のサンプリング座標を放射状に逆方向にずらす
    // 緑(G)は歪み・ズレなしの基準値
    float2 uvR = input.texcoord - dir * gParams.intensity;
    float2 uvG = input.texcoord;
    float2 uvB = input.texcoord + dir * gParams.intensity;
    
    // サンプリングが画面外に出るのを防ぐために saturate
    uvR = saturate(uvR);
    uvB = saturate(uvB);
    
    // RGBの各チャンネルを個別にサンプリングして合成
    float r = gTexture.Sample(gSampler, uvR).r;
    float g = gTexture.Sample(gSampler, uvG).g;
    float b = gTexture.Sample(gSampler, uvB).b;
    float a = gTexture.Sample(gSampler, uvG).a; // アルファ値はズレなしのものを採用
    
    output.color = float32_t4(r, g, b, a);
    return output;
}
