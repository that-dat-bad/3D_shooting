#include "Particle.hlsli"

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET;
};

// 深度値を線形に変換する関数
float LinearizeDepth(float depth)
{
    // Projection行列がパースペクティブで [0, 1] にマップされると仮定した場合の計算
    return (nearClip * farClip) / (farClip - depth * (farClip - nearClip));
}

PixelShaderOutput main(VSOutput input)
{
    PixelShaderOutput output;

    // テクスチャから色を取り出す
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // テクスチャの色と、頂点シェーダーから送られてきたパーティクルの色を乗算
    output.color = textureColor * input.color;
    
    // アルファ値が0なら棄却 (必要に応じて)
    if (output.color.a == 0.0f)
    {
        discard;
    }

    // --- Soft Particle (Depth Fade) ---
    // 背景の深度をサンプリング (SV_POSITION.xy を使ってピクセルを直接フェッチ)
    float bgDepth = gDepthTexture.Load(int3(input.svpos.xy, 0));
    
    // 深度を線形化
    float linearBgDepth = LinearizeDepth(bgDepth);
    float linearParticleDepth = LinearizeDepth(input.svpos.z);
    
    // 差分からフェード係数を計算
    float fadeDistance = 5.0f; // ぼかす距離(ワールド空間の距離)
    float depthDiff = max(0.0f, linearBgDepth - linearParticleDepth);
    float softAlpha = saturate(depthDiff / fadeDistance);
    
    // 最終的なアルファに適用
    output.color.a *= softAlpha;

    return output;
}