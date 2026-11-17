// タクティカル等高線マップ ピクセルシェーダー (ClearScene 背景用)
// 地形の標高 (worldPosition.y) に基づくアンチエイリアス等高線のみを描画

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET;
};

struct Material
{
    float32_t4 color;
    int enableLighting;
    float shininess;
    float environmentCoefficient;
    float specularIntensity;
    float32_t3 emissiveColor;
    float emissiveIntensity;
    matrix uvTransform;
};

struct LightingSettings
{
    int shadingModel;
    int specularModel;
    int lightType;
    float padding;
    float32_t3 cameraPosition;
    float emissiveIntensityScale;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<LightingSettings> gLightingSettings : register(b3);

struct PixelInput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : WORLDPOSITION0;
};

float ContourLine(float value, float interval, float widthPx)
{
    float scaled = value / interval;
    float dist = abs(frac(scaled - 0.5f) - 0.5f) / max(fwidth(scaled), 1e-5f);
    return 1.0f - smoothstep(0.0f, widthPx, dist);
}

PixelShaderOutput main(PixelInput input)
{
    PixelShaderOutput output;
    float3 wp = input.worldPosition;

    // 1. 等高線 (40m 間隔の細線 + 200m 間隔の主線)
    float minorLine = ContourLine(wp.y, 40.0f, 1.35f);
    float majorLine = ContourLine(wp.y, 200.0f, 1.85f);

    // 2. 海岸線 (標高 0m 付近) の強調
    float shoreBand = 1.0f - smoothstep(0.0f, 12.0f, abs(wp.y));

    // 3. 2000m 間隔の控えめなタクティカルグリッド
    float gx = ContourLine(wp.x, 2000.0f, 1.0f);
    float gz = ContourLine(wp.z, 2000.0f, 1.0f);
    float grid = max(gx, gz) * 0.22f;

    // 4. ベース背景色（深いタクティカルネイビー）
    float3 baseCol = float3(0.008f, 0.022f, 0.045f);
    float3 minorCol = float3(0.10f, 0.48f, 0.72f);
    float3 majorCol = float3(0.28f, 0.85f, 1.00f);
    float3 shoreCol = float3(0.35f, 0.92f, 1.00f);

    float3 col = baseCol;
    col += minorCol * (minorLine * 0.52f);
    col += majorCol * (majorLine * 0.85f);
    col += shoreCol * (shoreBand * 0.45f);
    col += float3(0.10f, 0.35f, 0.55f) * grid;

    // カメラからの距離フェード（遠景を滑らかに暗く溶かす）
    float camDist = length(gLightingSettings.cameraPosition - wp);
    float fade = saturate(1.0f - smoothstep(14000.0f, 34000.0f, camDist));
    col = lerp(float3(0.004f, 0.010f, 0.022f), col, fade);

    output.color = float4(col, 1.0f);
    return output;
}
