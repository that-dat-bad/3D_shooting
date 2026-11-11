// =========================================================================
// CombatBootScan.PS.hlsl - 戦闘モード起動スキャン ピクセルシェーダー
// 機体中心から広がるスキャン波形に沿って：
// 【無（完全ステルス） → ネオンワイヤーフレーム＆波紋パルス → 3D実体装甲】へと連続遷移
// =========================================================================

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
    float scanBandWidth;     // スキャン波形帯の幅 (約 3.5f)
    float scanMaxRadius;     // 機体中心からの最大走査半径 (約 13.0f)
    float scanWaveFreq;      // 波形干渉縞の周波数
    float32_t4 scanColor;    // ネオン発光色 (サイバーグリーン)
    float scanTime;          // 脈動・波形アニメーションタイマー
    float32_t3 centerOffset; // スキャン起点（中心オフセット）
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

    // スキャン波の到達半径（機体中心から球状・波紋状に広がる）
    // scanProgress 0.0 -> 半径 0.0 (未到達・完全ステルス)
    // scanProgress 1.0 -> 半径 maxRadius + bandWidth (全パーツ完全実体化)
    float totalRadius = gScanSettings.scanMaxRadius + gScanSettings.scanBandWidth;
    float currentRadius = saturate(gScanSettings.scanProgress) * totalRadius;
    float bandWidth = max(0.5f, gScanSettings.scanBandWidth);

    // 波の先端からの距離（通過後の進行深度）
    float waveDist = currentRadius - dist;

    // 1. 【無（完全不可視・ステルス）フェーズ】
    // スキャン波がまだ到達していない外側領域はピクセル単位でディスカード！
    // 頂点単位ではなくピクセル単位で綺麗な同心球状にカットされるため、パッと出る違和感ゼロ！
    if (waveDist < 0.0f)
    {
        discard;
    }

    // 基本ライティング計算（Half-Lambert + 半球アンビエント）
    float32_t3 N = normalize(input.normal);
    float32_t3 L = normalize(-gDirectionalLight.direction);
    float NdotL = dot(N, L);
    float cosFactor = saturate(NdotL * 0.5f + 0.5f); // Half-Lambert
    float hemi = N.y * 0.5f + 0.5f;
    float32_t3 ambient = lerp(float32_t3(0.18f, 0.16f, 0.14f), float32_t3(0.30f, 0.35f, 0.42f), hemi) * 0.16f;
    float32_t3 diffuse = gMaterial.color.rgb * texColor.rgb * (gDirectionalLight.color.rgb * (cosFactor * 0.84f) * gDirectionalLight.intensity + ambient);

    // 2. 【スキャン波形＆ネオンワイヤーフレーム構築フェーズ】
    // 重心座標からエッジまでの最短距離をスクリーンスペースでシャープに計算
    float32_t3 d = fwidth(input.barycentric);
    float32_t3 a3 = smoothstep(float32_t3(0.0f, 0.0f, 0.0f), d * 1.35f, input.barycentric);
    float edge = 1.0f - min(a3.x, min(a3.y, a3.z));

    // スキャン波帯（waveDist: 0.0 〜 bandWidth）における遷移ファクター
    // wireFactor: 波の先端（waveDist=0）で1.0、中心側（waveDist=bandWidth）に向かって滑らかに0.0へ減衰
    float wireFactor = 1.0f - smoothstep(0.0f, bandWidth, waveDist);

    // 実体装甲の出現ファクター（波が通り過ぎるにつれて0.0から1.0へ徐々に実体化）
    float solidFactor = smoothstep(0.0f, bandWidth * 0.85f, waveDist);

    // スキャン波形（波紋パルス干渉縞）: 中心から外側へ伝播する同心円パルス波
    float wavePhase = (dist * gScanSettings.scanWaveFreq) - (gScanSettings.scanTime * 18.0f);
    float wavePulse = sin(wavePhase) * 0.5f + 0.5f;
    float waveFrontPeak = pow(wireFactor, 1.8f); // 波の先端直上が最も鋭く発光

    // ネオン発光色（サイバーグリーン）
    float32_t4 wireCol = gScanSettings.scanColor;
    float pulseBlink = 0.88f + 0.12f * sin(gScanSettings.scanTime * 28.0f);

    // ワイヤーフレーム稜線は超強烈発光（×4.2）、面の内側は波形干渉縞（ホログラムパルス）
    float32_t3 neonGlow = wireCol.rgb * (edge * 4.2f + waveFrontPeak * 2.2f + wavePulse * 0.8f * wireFactor) * pulseBlink;

    // 3. 【実写（3D装甲）へのシームレスな融合】
    // 波面先端ではワイヤーフレーム＆透過ホログラムがメインで実体は薄く、
    // 波が通り過ぎるにつれて実体装甲（テクスチャ＋ライティング）が徐々に現れて完全実写へ！
    float32_t3 finalColor = diffuse * solidFactor + neonGlow * wireFactor;
    if (any(isnan(finalColor)) || any(isinf(finalColor)))
    {
        finalColor = gMaterial.color.rgb * texColor.rgb * 0.35f;
    }
    float finalAlpha = lerp(edge * 0.95f + waveFrontPeak * 0.85f, 1.0f, solidFactor) * gMaterial.color.a * texColor.a;

    output.color = float32_t4(finalColor, saturate(finalAlpha));
    return output;
}
