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

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

struct CombatBootScanSettings
{
    float scanProgress;      // 0.0: 未起動 -> 1.0: スキャン完了
    float scanBandWidth;     // スキャン波形帯の幅
    float scanMaxRadius;     // 機体中心からの最大走査半径
    float scanWaveFreq;      // 波形干渉縞の周波数
    float32_t4 scanColor;    // ネオン発光色
    float scanTime;          // 脈動・波形アニメーションタイマー
    float32_t3 centerOffset; // スキャン起点
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<CombatBootScanSettings> gScanSettings : register(b3);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelInput
{
    float32_t4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : WORLDPOSITION0;
    float32_t3 localPosition : LOCALPOSITION0;
    float32_t3 barycentric : BARYCENTRIC0;
};

PixelShaderOutput main(PixelInput input)
{
    PixelShaderOutput output;

    // 基本テクスチャサンプリング
    float32_t4 uv = float32_t4(input.texcoord, 0.0f, 1.0f);
    uv = mul(uv, gMaterial.uvTransform);
    float32_t4 texColor = gTexture.Sample(gSampler, uv.xy);
    if (texColor.a < 0.3f) {
        discard;
    }

    // 機体中心からの距離計算
    float32_t3 localPos = input.localPosition - gScanSettings.centerOffset;
    float dist = length(localPos);

    // スキャン波の到達半径
    float totalRadius = gScanSettings.scanMaxRadius + gScanSettings.scanBandWidth;
    float currentRadius = saturate(gScanSettings.scanProgress) * totalRadius;
    float bandWidth = max(0.5f, gScanSettings.scanBandWidth);

    // 波の先端からの距離
    float waveDist = currentRadius - dist;

    // 未到達領域の破棄
    if (waveDist < 0.0f)
    {
        discard;
    }

    // 基本ライティング計算
    float32_t3 N = normalize(input.normal);
    float32_t3 L = normalize(-gDirectionalLight.direction);
    float NdotL = dot(N, L);
    float cosFactor = saturate(NdotL * 0.5f + 0.5f); // Half-Lambert
    float hemi = N.y * 0.5f + 0.5f;
    float32_t3 ambient = lerp(float32_t3(0.18f, 0.16f, 0.14f), float32_t3(0.30f, 0.35f, 0.42f), hemi) * 0.16f;
    float32_t3 diffuse = gMaterial.color.rgb * texColor.rgb * (gDirectionalLight.color.rgb * (cosFactor * 0.84f) * gDirectionalLight.intensity + ambient);

    // ワイヤーフレーム構築
    float32_t3 d = fwidth(input.barycentric);
    float32_t3 a3 = smoothstep(float32_t3(0.0f, 0.0f, 0.0f), d * 1.35f, input.barycentric);
    float edge = 1.0f - min(a3.x, min(a3.y, a3.z));

    // スキャン波帯の遷移係数
    float wireFactor = 1.0f - smoothstep(0.0f, bandWidth, waveDist);

    // 実体装甲の出現ファクター
    float solidFactor = smoothstep(0.0f, bandWidth * 0.85f, waveDist);

    // スキャン波形: 中心から外側へ伝播する同心円パルス波
    float wavePhase = (dist * gScanSettings.scanWaveFreq) - (gScanSettings.scanTime * 18.0f);
    float wavePulse = sin(wavePhase) * 0.5f + 0.5f;
    float waveFrontPeak = pow(wireFactor, 1.8f); // 波の先端直上が最も鋭く発光

    // ネオン発光色
    float32_t4 wireCol = gScanSettings.scanColor;
    float pulseBlink = 0.88f + 0.12f * sin(gScanSettings.scanTime * 28.0f);

    // ワイヤーフレーム稜線は超強烈発光、面の内側は波形干渉縞
    float32_t3 neonGlow = wireCol.rgb * (edge * 4.2f + waveFrontPeak * 2.2f + wavePulse * 0.8f * wireFactor) * pulseBlink;

    // 実体装甲へのブレンド
    float32_t3 finalColor = diffuse * solidFactor + neonGlow * wireFactor;
    if (any(isnan(finalColor)) || any(isinf(finalColor)))
    {
        finalColor = gMaterial.color.rgb * texColor.rgb * 0.35f;
    }
    float finalAlpha = lerp(edge * 0.95f + waveFrontPeak * 0.85f, 1.0f, solidFactor) * gMaterial.color.a * texColor.a;

    output.color = float32_t4(finalColor, saturate(finalAlpha));
    return output;
}
