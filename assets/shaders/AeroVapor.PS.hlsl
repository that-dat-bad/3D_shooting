#include "AeroVapor.hlsli"
#include "AtmosphericMistCommon.hlsli"

ConstantBuffer<VaporParamsCB> gParams : register(b1);

Texture2D<float4> gNoiseTex : register(t0);
Texture2D<float>  gDepthTex : register(t1);
SamplerState      gWrapSampler : register(s0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VaporVSOutput input) {
    PixelShaderOutput output;

    float u        = input.texcoord.x;
    float v        = input.texcoord.y;
    float layer    = input.extra.x;
    float sideSign = input.extra.z;
    float inten    = saturate(gParams.intensity);

    float density       = 0.0f;
    float layerWeight   = 1.0f;
    float intensityFade = smoothstep(0.0f, 0.25f, inten);

    if (gParams.mode == 0u) {
        // ----------------------------------------------------------------
        // Mode 0: 主翼前縁・ストレーキ凝縮シート（空力LEV渦コア＋流線スクロール）
        // ・AoA/横滑り/ロール角速度から実気流の筋と左右非対称を計算
        // ・前縁剥離渦コア沿いに濃く立ち上がり、後縁は流線沿いに細く千切れる
        // ----------------------------------------------------------------
        float aoaNorm    = saturate(abs(gParams.aoaRad) / 0.45f);
        float wingAsym   = clamp(1.0f + sideSign * gParams.rollAsymmetry, 0.65f, 1.35f);
        float localInten = saturate(inten * wingAsym);
        intensityFade    = smoothstep(0.0f, 0.25f, localInten);

        float curveShift = 0.05f * u * u;
        float vEff = saturate((v - curveShift) / max(0.01f, 1.0f - curveShift));

        float leadWidth = max(0.02f, gParams.leadingEdgeSharpness * (0.75f + 0.55f * layer));
        float leadMask  = smoothstep(0.0f, leadWidth, vEff);
        float trailMask = 1.0f - smoothstep(0.42f, 0.96f, vEff);

        float spanLimit = lerp(0.32f, 0.92f, saturate(localInten * gParams.wingSpanExpand));
        float rootMask  = smoothstep(0.0f, 0.06f + 0.03f * layer, u);
        float tipMask   = 1.0f - smoothstep(max(0.08f, spanLimit - 0.22f), spanLimit, u);

        float shapeMask = leadMask * trailMask * rootMask * tipMask;

        // 翼根ストレーキ起点→斜め後方へ伸びる前縁剥離渦(LEV)コア軸（横滑りで偏流）
        float sideWindU  = -gParams.sideslipRad * sideSign * 0.55f;
        float uCore      = saturate(0.08f + 0.34f * vEff + sideWindU * vEff * 0.80f);
        float coreRadius = lerp(0.065f, 0.23f, vEff) * (0.80f + 0.40f * aoaNorm);
        float distToCore = abs(u - uCore) / max(0.035f, coreRadius);
        float vortexCore = exp(-distToCore * distToCore * 1.75f)
                         * smoothstep(0.0f, 0.07f, vEff)
                         * (1.0f - smoothstep(0.55f, 0.98f, vEff));

        // 螺旋巻き込み＋横滑り偏向を反映した気流ストリームライン座標
        float curlPull = (u - uCore) * exp(-abs(u - uCore) * 3.6f) * 0.42f * (0.35f + 0.65f * aoaNorm) * gParams.streamlineTwist;
        float sAcross  = (u - sideWindU * vEff * 0.90f) - curlPull * vEff + 0.08f * sin(vEff * 3.14159f) * (1.0f - u) * gParams.streamlineTwist;
        float sAlong   = vEff;
        float effFlow  = gParams.flowSpeed * clamp(gParams.airspeedScale, 0.45f, 2.4f);

        // 流線に沿って細長く引き伸ばした2層異方性ノイズ＋微細フィラメント筋
        float2 flowUv = float2(sAcross + layer * 0.37f, sAlong);
        float flowNoise = SampleWarpedCloudNoise(
            gNoiseTex,
            gWrapSampler,
            flowUv,
            float2(0.0f, -effFlow),
            float2(0.0f, -effFlow * 1.85f),
            float2(2.8f, 0.75f),
            float2(9.5f, 1.15f),
            0.18f,
            gParams.time
        );
        float fineStreak = gNoiseTex.Sample(
            gWrapSampler,
            float2(sAcross * 14.0f + layer * 0.41f, sAlong * 0.55f - effFlow * 2.1f * gParams.time)
        ).r;

        float combinedMask = saturate(shapeMask * 0.82f + vortexCore * gParams.vortexCoreStrength * (0.55f + 0.55f * aoaNorm));
        float erode = lerp(0.12f, gParams.trailingErosion, smoothstep(0.10f, 0.90f, vEff)) * lerp(1.0f, 0.58f, vortexCore);
        density     = ComputeErodedMask(combinedMask, lerp(flowNoise, fineStreak, 0.30f), erode, 0.07f, 0.32f);
        layerWeight = lerp(1.0f, 0.48f, layer);
    } else {
        // ----------------------------------------------------------------
        // Mode 1: 遷音速プラントル＝グロワート ベイパーコーン
        // ・円周方向は整数スケールでサンプリングしシーム（継ぎ目）を完全に防ぐ
        // ----------------------------------------------------------------
        float leadMask  = smoothstep(0.0f, 0.06f + 0.04f * layer, v);
        float trailMask = 1.0f - smoothstep(0.35f, 1.0f, v);
        float shapeMask = leadMask * trailMask;

        float2 flowUv = float2(u + layer * 0.25f, v);
        float flowNoise = SampleWarpedCloudNoise(
            gNoiseTex,
            gWrapSampler,
            flowUv,
            float2(0.0f, -gParams.flowSpeed * 1.35f),
            float2(0.0f, -gParams.flowSpeed * 2.20f),
            float2(3.0f, 0.90f),
            float2(9.0f, 1.50f),
            0.18f,
            gParams.time
        );

        float erode = lerp(0.12f, gParams.trailingErosion * 1.05f, smoothstep(0.08f, 0.92f, v));
        density     = ComputeErodedMask(shapeMask, flowNoise, erode, 0.06f, 0.32f);
        layerWeight = lerp(0.92f, 0.45f, layer);
    }

    // 視線角フェード（メッシュ端の板ポリ感を消しつつ、アーチドームで全方向から厚み維持）
    float3 V    = normalize(gParams.cameraPosition - input.worldPos);
    float3 geomN = normalize(input.worldNormal);
    float3 faceN = (dot(geomN, V) < 0.0f) ? -geomN : geomN;
    float  ndotv = abs(dot(faceN, V));
    float  viewFade = pow(smoothstep(0.06f, 0.52f, ndotv), gParams.fresnelPower);

    // 機体胴体・主翼・キャノピーとの交差境界のみをソフトフェード
    float depthFade = ComputeDepthSoftFade(
        gDepthTex,
        int2(input.position.xy),
        input.position.z,
        gParams.nearClip,
        gParams.farClip,
        gParams.softDistance,
        gParams.hasDepthTexture
    );

    // 太陽方向を向いた面を明るくし、裏面・内部に淡い青灰の雲陰影を乗せる
    float3 sunDir = (length(gParams.sunDirection) > 1e-4f)
        ? normalize(gParams.sunDirection)
        : normalize(float3(0.45f, 0.80f, 0.35f));

    float3 litColor = ComputeCloudLighting(
        geomN,
        V,
        sunDir,
        gParams.sunColor,
        gParams.sunIntensity,
        gParams.shadowTint,
        density,
        layer
    );

    float alpha = saturate(
        density * layerWeight * viewFade * depthFade *
        gParams.densityScale * gParams.opacityBoost * intensityFade * gParams.baseColor.a
    );

    if (alpha <= 0.003f) {
        discard;
    }

    output.color = float4(litColor * gParams.baseColor.rgb, alpha);
    return output;
}
