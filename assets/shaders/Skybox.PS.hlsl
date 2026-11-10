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

// Cubemapテクスチャとしてサンプリング + 極夜・高高度成層圏プロシージャル演出
TextureCube<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 高周波ハッシュ関数（星空生成用）
float Hash31(float3 p) {
    p = frac(p * float3(443.897f, 441.423f, 437.195f));
    p += dot(p, p.yzx + 19.19f);
    return frac((p.x + p.y) * p.z);
}

// 3Dバリューノイズ（オーロラ・薄雲用）
float ValueNoise3D(float3 p) {
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float n000 = Hash31(i + float3(0, 0, 0));
    float n100 = Hash31(i + float3(1, 0, 0));
    float n010 = Hash31(i + float3(0, 1, 0));
    float n110 = Hash31(i + float3(1, 1, 0));
    float n001 = Hash31(i + float3(0, 0, 1));
    float n101 = Hash31(i + float3(1, 0, 1));
    float n011 = Hash31(i + float3(0, 1, 1));
    float n111 = Hash31(i + float3(1, 1, 1));
    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);
    float nxy0 = lerp(nx00, nx10, f.y);
    float nxy1 = lerp(nx01, nx11, f.y);
    return lerp(nxy0, nxy1, f.z);
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    // 3次元方向ベクトルを正規化
    float3 dir = normalize(input.texcoord);
    
    // Cubemapサンプリング
    float32_t4 texSample = gTexture.Sample(gSampler, dir);
    float3 skyRgb = texSample.rgb * gMaterial.color.rgb;
    
    // 極夜・薄暮大気グラデーション（天頂の深い濃紺・漆黒〜地平線の淡い蒼白トワイライトグロウ）
    // ※天球が過度に白飛び・明るくなりすぎないよう、トーンを引き締める
    float horizonGlow = pow(1.0f - abs(dir.y), 4.2f);
    float3 zenithTint = float3(0.015f, 0.025f, 0.055f);
    float3 horizonTint = float3(0.065f, 0.110f, 0.180f);
    float3 baseAtmosphere = lerp(zenithTint, horizonTint, horizonGlow);
    
    // キューブマップテクスチャを極夜の落ち着いた明度にスケーリング
    skyRgb = max(skyRgb * 0.32f, baseAtmosphere) + skyRgb * 0.08f;
    
    // 薄暗い高層雲海ディテール（地平線〜中高度）
    if (dir.y > -0.15f) {
        float3 cloudCoord = dir * float3(4.5f, 12.0f, 4.5f);
        float cloudNoise = ValueNoise3D(cloudCoord) * 0.65f + ValueNoise3D(cloudCoord * 2.3f) * 0.35f;
        float cloudBand = smoothstep(0.02f, 0.25f, dir.y) * smoothstep(0.65f, 0.15f, dir.y);
        float3 cloudCol = float3(0.08f, 0.12f, 0.18f) * smoothstep(0.42f, 0.78f, cloudNoise) * cloudBand * 0.40f;
        skyRgb += cloudCol;
    }
    
    // 極夜のオーロラカーテン（上空に揺らめく淡いエメラルド・シアンの帯）
    if (dir.y > 0.08f) {
        float2 auroraUv = dir.xz / (dir.y + 0.35f);
        float wave = sin(auroraUv.x * 3.8f + auroraUv.y * 1.6f) * 0.45f
                   + cos(auroraUv.y * 5.2f - auroraUv.x * 2.1f) * 0.35f;
        float curtain = exp(-pow((auroraUv.y + wave * 0.35f) * 2.2f, 2.0f));
        float streak = ValueNoise3D(float3(auroraUv * 6.5f, wave));
        float auroraMask = curtain * smoothstep(0.32f, 0.78f, streak) * smoothstep(0.08f, 0.38f, dir.y) * smoothstep(0.92f, 0.45f, dir.y);
        float3 auroraColor = lerp(float3(0.08f, 0.65f, 0.40f), float3(0.12f, 0.42f, 0.75f), saturate(dir.y * 1.4f));
        skyRgb += auroraColor * auroraMask * 0.22f;
    }
    
    // 成層圏の星々（天頂付近ほど鮮明に瞬く）
    if (dir.y > 0.05f) {
        float3 starGrid = floor(dir * 260.0f);
        float starVal = Hash31(starGrid);
        if (starVal > 0.9945f) {
            float3 cellCenter = (starGrid + 0.5f) / 260.0f;
            float distToCenter = length(dir - normalize(cellCenter)) * 260.0f;
            float starSpot = smoothstep(0.45f, 0.0f, distToCenter);
            float brightness = pow(saturate((starVal - 0.9945f) / 0.0055f), 1.5f) * smoothstep(0.05f, 0.40f, dir.y);
            float3 starCol = lerp(float3(0.75f, 0.88f, 1.0f), float3(1.0f, 0.95f, 0.85f), frac(starVal * 97.0f));
            skyRgb += starCol * starSpot * brightness * 0.95f;
        }
    }
    
    output.color = float32_t4(skyRgb, gMaterial.color.a);
    return output;
}
