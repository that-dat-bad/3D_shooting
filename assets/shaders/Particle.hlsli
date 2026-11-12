struct VSOutput
{
    float32_t4 svpos : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
};

struct ParticleForGPU
{
    float32_t4x4 WVP;
    float32_t4 color;
};

// カメラ・スクリーン定数バッファ
cbuffer CameraData : register(b0)
{
    float32_t nearClip;
    float32_t farClip;
    float32_t screenWidth;
    float32_t screenHeight;
}

//テクスチャ
Texture2D<float32_t4> gTexture : register(t0);

//インスタンシングデータ (StructuredBuffer)
StructuredBuffer<ParticleForGPU> gParticleData : register(t1);

//深度テクスチャ
Texture2D<float32_t> gDepthTexture : register(t2);

//サンプラー
SamplerState gSampler : register(s0);