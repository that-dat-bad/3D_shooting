// ============================================================================
// AeroVapor.hlsli
// 専用3D多層空力ベイパーシェル 共通構造体・定数バッファ定義
// （C++: AeroVaporMeshRenderer とバイナリレイアウト一致）
// ============================================================================

#ifndef AERO_VAPOR_HLSLI
#define AERO_VAPOR_HLSLI

struct VaporVertexInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0; // x: span/angle u [0..1], y: chord/axial v [0..1]
    float3 normal   : NORMAL0;
    float4 extra    : TEXCOORD1; // x: layerFactor [0..1], y: domeHeight (m), z: sideSign (-1/+1), w: chordNorm [0..1]
};

struct VaporVSOutput {
    float4 position    : SV_POSITION;
    float2 texcoord    : TEXCOORD0;
    float3 worldNormal : NORMAL0;
    float3 worldPos    : WORLDPOSITION0;
    float4 extra       : TEXCOORD1;
};

struct VaporTransformCB {
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

struct VaporParamsCB {
    float  time;
    float  intensity;
    float  flowSpeed;
    float  softDistance;

    float  leadingEdgeSharpness;
    float  trailingErosion;
    float  densityScale;
    float  fresnelPower;

    float3 sunDirection;
    float  sunIntensity;

    float3 sunColor;
    float  domeThickness;

    float3 shadowTint;
    uint   mode; // 0: 翼面/ストレーキ凝縮シェル, 1: 遷音速ベイパーコーン

    float3 cameraPosition;
    float  nearClip;

    float  farClip;
    uint   hasDepthTexture;
    float  wingSpanExpand;
    float  opacityBoost;

    float4 baseColor;

    float  aoaRad;
    float  sideslipRad;
    float  rollAsymmetry;
    float  airspeedScale;

    float  vortexCoreStrength;
    float  streamlineTwist;
    float  pad0;
    float  pad1;
};

#endif // AERO_VAPOR_HLSLI
