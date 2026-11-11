#include "Skybox.hlsli"

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET;
};

// マテリアル情報
struct Material {
    float32_t4 color;
    int enableLighting;
    float shininess;
    float32_t2 padding;
    matrix uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);

// Cubemapテクスチャではなく2Dテクスチャとして equirectangular (正距円筒図法) サンプリングする
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    // 3次元方向ベクトルを正規化
    float3 dir = normalize(input.texcoord);
    
    // 方向ベクトルから球面座標(経度・緯度)を計算し、UV座標に変換
    float u = atan2(dir.x, dir.z) / (2.0f * 3.14159265f) + 0.5f;
    float v = 0.5f - asin(dir.y) / 3.14159265f;
    
    float2 uv = float2(u, v);
    
    float32_t4 textureColor = gTexture.Sample(gSampler, uv);
    output.color = textureColor * gMaterial.color;
    
    return output;
}
