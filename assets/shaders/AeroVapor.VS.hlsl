#include "AeroVapor.hlsli"

ConstantBuffer<VaporTransformCB> gTransform : register(b0);
ConstantBuffer<VaporParamsCB>    gParams    : register(b1);

VaporVSOutput main(VaporVertexInput input) {
    VaporVSOutput output;

    float4 localPos = input.position;
    float  u        = input.texcoord.x;
    float  v        = input.texcoord.y;
    float  domeH    = input.extra.y;
    float  sideSign = input.extra.z;
    float  chordNorm = input.extra.w;
    float  inten    = saturate(gParams.intensity);

    if (gParams.mode == 0u) {
        // 翼面凝縮シート：AoA・前縁剥離渦(LEV)コアの浮き上がり・横滑り偏向を計算
        float aoaNorm    = saturate(abs(gParams.aoaRad) / 0.45f);
        float sideWindU  = -gParams.sideslipRad * sideSign * 0.55f;
        float uCore      = saturate(0.08f + 0.34f * v + sideWindU * v * 0.75f);
        float coreRadius = lerp(0.08f, 0.25f, v) * (0.85f + 0.35f * aoaNorm);
        float coreDist   = abs(u - uCore) / max(0.04f, coreRadius);
        float coreRidge  = exp(-coreDist * coreDist * 1.4f) * smoothstep(0.0f, 0.12f, v);

        float liftScale  = (0.35f + 0.85f * aoaNorm + 0.35f * inten) * gParams.domeThickness;
        float coreBump   = coreRidge * (0.18f + 0.32f * aoaNorm) * chordNorm * gParams.domeThickness * gParams.vortexCoreStrength;
        float aoaWakeLift = sin(clamp(gParams.aoaRad, -0.4f, 0.7f)) * 0.48f * v * v * chordNorm * inten;
        float flutter    = sin(gParams.time * (22.0f * max(0.5f, gParams.airspeedScale)) - v * 12.0f + u * 6.0f) * 0.03f * v * inten * chordNorm;

        localPos.y += domeH * liftScale + coreBump + aoaWakeLift + flutter;
        localPos.x -= clamp(gParams.sideslipRad, -0.35f, 0.35f) * 1.15f * v * v * chordNorm;
    } else {
        // 遷音速ベイパーコーン：衝撃波ベルの厚み膨張＋マッハ脈動
        float inflation = (0.65f + 0.50f * inten) * gParams.domeThickness;
        float ripple = sin(gParams.time * 34.0f - v * 16.0f) * 0.06f * v * inten;
        localPos.xyz += normalize(input.normal) * (domeH * inflation + ripple);
    }

    float4 worldPos = mul(localPos, gTransform.World);
    output.position    = mul(localPos, gTransform.WVP);
    output.worldPos    = worldPos.xyz;
    output.worldNormal = normalize(mul(input.normal, (float3x3)gTransform.WorldInverseTranspose));
    output.texcoord    = input.texcoord;
    output.extra       = input.extra;

    return output;
}
