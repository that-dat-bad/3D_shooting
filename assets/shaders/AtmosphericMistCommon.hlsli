// ============================================================================
// AtmosphericMistCommon.hlsli
// 空力ベイパー・雲海・3D積雲 共通シェーダーライブラリ
// （2層ドメインワープノイズ・マスク侵食・雲ライティング・深度ソフトフェード）
// ============================================================================

#ifndef ATMOSPHERIC_MIST_COMMON_HLSLI
#define ATMOSPHERIC_MIST_COMMON_HLSLI

// 透視投影深度バッファ値 [0,1] をビュー空間距離（m）へ線形化
float LinearizeSceneDepth(float depth, float nearClip, float farClip) {
    float n = max(0.01f, nearClip);
    float f = max(n + 1.0f, farClip);
    return (n * f) / max(1e-5f, f - depth * (f - n));
}

// 機体・地形との硬いポリゴン交差境界を消す深度ソフトフェード
float ComputeDepthSoftFade(
    Texture2D<float> depthTex,
    int2 pixelPos,
    float fragDepth,
    float nearClip,
    float farClip,
    float softDistance,
    uint hasDepthTexture) {

    if (hasDepthTexture == 0u) {
        return 1.0f;
    }

    float bgDepth = depthTex.Load(int3(pixelPos, 0));
    if (bgDepth >= 0.99999f) {
        return 1.0f;
    }

    float linearBg = LinearizeSceneDepth(bgDepth, nearClip, farClip);
    float linearPart = LinearizeSceneDepth(fragDepth, nearClip, farClip);
    float depthDiff = max(0.0f, linearBg - linearPart);
    return saturate(depthDiff / max(0.05f, softDistance));
}

// 2層スクロール＋ドメインワープによる有機的クラウド／気流ノイズサンプリング
// （Wrapサンプラー前提。雲海では等方スケール、ベイパーでは異方性スケールを指定）
float SampleWarpedCloudNoise(
    Texture2D<float4> noiseTex,
    SamplerState wrapSampler,
    float2 uv,
    float2 flowSpeedA,
    float2 flowSpeedB,
    float2 scaleA,
    float2 scaleB,
    float warpStrength,
    float timeSec) {

    float2 uvA = uv * scaleA + flowSpeedA * timeSec;
    float nA = noiseTex.Sample(wrapSampler, uvA).r;

    float2 warp = float2(nA - 0.5f, (nA - 0.5f) * 0.65f) * warpStrength;
    float2 uvB = uv * scaleB + flowSpeedB * timeSec + warp;
    float nB = noiseTex.Sample(wrapSampler, uvB).r;

    return saturate(lerp(nA, nB, 0.58f));
}

// ベース形状マスクからノイズで輪郭を侵食（千切れ・ほつれ表現）
float ComputeErodedMask(
    float baseMask,
    float noiseVal,
    float erodeStrength,
    float threshold,
    float softness) {

    float eroded = baseMask - (1.0f - noiseVal) * erodeStrength;
    return smoothstep(threshold, threshold + max(0.001f, softness), eroded);
}

// 物理ベース近似の雲／凝縮蒸気ライティング
// （Wrap Diffuse + 内部自己陰影 + 逆光前方散乱シルバーライニング）
float3 ComputeCloudLighting(
    float3 worldNormal,
    float3 viewDir,
    float3 sunDir,
    float3 sunColor,
    float sunIntensity,
    float3 shadowTint,
    float density,
    float layerFactor) {

    // 太陽向きのラップディフューズ（柔らかく光が回り込む）
    float ndotl = saturate((dot(worldNormal, sunDir) + 0.55f) / 1.55f);

    // 内側層・高密度コアにわずかな自己陰影をつけて立体感を出す
    float coreOcclusion = lerp(0.84f, 1.0f, layerFactor) * lerp(1.0f, 0.88f, density * (1.0f - ndotl));

    // 太陽光による白い銀縁（Henyey-Greenstein 近似の前方散乱）
    float forwardDot = saturate(dot(-viewDir, sunDir));
    float silverLining = pow(forwardDot, 4.0f) * (1.0f - density * 0.55f) * 0.45f;

    float3 sunlitWhite = lerp(float3(1.0f, 1.0f, 1.0f), saturate(sunColor), 0.35f);
    float3 baseLit = lerp(shadowTint, sunlitWhite, saturate(ndotl * 0.85f + 0.15f)) * coreOcclusion;

    return baseLit + sunColor * (silverLining * max(0.6f, sunIntensity));
}

#endif // ATMOSPHERIC_MIST_COMMON_HLSLI
