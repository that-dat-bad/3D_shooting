struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET;
};

// マテリアル情報を格納する定数バッファ (register b0)
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

// 平行光源の情報を格納する定数バッファ (register b2)
struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

// ライティング設定定数バッファ (register b3)
struct LightingSettings
{
    int shadingModel;
    int specularModel;
    int lightType;
    float padding;
    float32_t3 cameraPosition;
    float emissiveIntensityScale;
};

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    float decay;
    float32_t2 padding; 
};

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float32_t3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
    float padding;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<LightingSettings> gLightingSettings : register(b3);
ConstantBuffer<PointLight> gPointLight : register(b4);
ConstantBuffer<SpotLight> gSpotLight : register(b5);

Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvTexture : register(t1);
SamplerState gSampler : register(s0);

// 頂点シェーダーからの入力
struct PixelInput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : WORLDPOSITION0;
};

// --- プロシージャル地形ノイズ生成ヘルパー ---
float TerrainHash21(float2 p)
{
    p = frac(p * float2(234.34f, 435.345f));
    p += dot(p, p + 34.23f);
    return frac(p.x * p.y);
}

float TerrainNoise2D(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float a = TerrainHash21(i);
    float b = TerrainHash21(i + float2(1.0f, 0.0f));
    float c = TerrainHash21(i + float2(0.0f, 1.0f));
    float d = TerrainHash21(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float TerrainFbm(float2 p)
{
    float v = 0.0f;
    v += TerrainNoise2D(p) * 0.5000f;
    v += TerrainNoise2D(p * 2.03f) * 0.2500f;
    v += TerrainNoise2D(p * 4.09f) * 0.1250f;
    v += TerrainNoise2D(p * 8.17f) * 0.0625f;
    return v;
}

PixelShaderOutput main(PixelInput input)
{
    PixelShaderOutput output;

    float3 worldPos = input.worldPosition;
    
    // 法線の安全な正規化
    float3 rawN = input.normal;
    float nLen = length(rawN);
    rawN = (nLen > 0.001f) ? (rawN / nLen) : float3(0.0f, 1.0f, 0.0f);

    // 1. 地形起伏の陰影コントラスト強調（緩やかな傾斜でも山肌・稜線の陰影をクッキリ描出）
    float3 steepN = normalize(float3(rawN.x * 2.8f, rawN.y, rawN.z * 2.8f));
    float slope = saturate(1.0f - rawN.y); // 0 = 水平な平地, 1 = 垂直な絶壁

    // プロシージャルマイクロバンプ（地面との高度感・立体感を強調する微細凹凸）
    float2 bumpCoord = worldPos.xz * 0.18f;
    float bumpNoise = TerrainNoise2D(bumpCoord);
    steepN.xz += (float2(TerrainNoise2D(bumpCoord + float2(0.1f, 0.0f)), TerrainNoise2D(bumpCoord + float2(0.0f, 0.1f))) - bumpNoise) * 0.35f;
    steepN = normalize(steepN);

    // 2. 太陽光・山肌シャドウ計算
    float dirLen = length(gDirectionalLight.direction);
    float3 L = (dirLen > 0.001f) ? normalize(-gDirectionalLight.direction) : normalize(float3(0.5f, 0.8f, 0.4f));
    float NdotL = dot(steepN, L);

    // 日向と日陰の境界を明確にし、深い陰影（シャドウ）を生成
    float sunLit = smoothstep(-0.02f, 0.42f, NdotL);

    // 2.5 機体の地形投影シャドウ (Aircraft Ground Shadow)
    float aircraftShadow = 0.0f;
    if (gSpotLight.color.x > 0.5f) // シャドウ有効
    {
        float3 acPos = gSpotLight.position;
        float3 acForward = normalize(gSpotLight.direction);
        float3 acRight = normalize(float3(gSpotLight.distance, gSpotLight.decay, gSpotLight.cosAngle));
        
        // 地表ピクセルから太陽(L)に向かうレイに沿って、地表から機体への距離 t を求める
        float3 toAircraft = acPos - worldPos;
        float t = dot(toAircraft, L);

        // 機体より下にある地表、かつ最大投影高度以内
        float maxShadowAlt = gSpotLight.color.w; // 1200m
        if (t > 0.0f && t < maxShadowAlt)
        {
            // 太陽光レイ上の機体投影中心点
            float3 rayPoint = worldPos + L * t;
            float3 diff = rayPoint - acPos;

            // 機体ローカル座標系への投影（前後 fwd, 左右 lateral）
            float fwd = dot(diff, acForward);
            float lateral = abs(dot(diff, acRight));

            // 高度に応じた自然な半影（ペナンブラ・ソフトブラー）
            float penumbra = 0.35f + t * 0.008f;

            // ジェット戦闘機のシルエット形状（デルタ翼・胴体・水平尾翼）
            // 全長: ~15m (前方 +7.5m 〜 後方 -7.5m)
            // 翼幅: ~8.4m (左右 ±4.2m)
            
            // (1) 胴体 (Fuselage)
            float bodyHalfWidth = 1.1f * saturate(1.0f - abs(fwd) / 7.5f) + 0.25f;
            float bodyDist = max(lateral - bodyHalfWidth, abs(fwd) - 7.5f);

            // (2) デルタ主翼 (Delta Main Wings)
            float wingHalfSpan = 0.0f;
            if (fwd < 2.0f && fwd > -4.5f) {
                wingHalfSpan = saturate((2.0f - fwd) / 6.5f) * gSpotLight.color.y; // 4.2m
            }
            float wingDist = max(lateral - wingHalfSpan, max(fwd - 2.0f, -4.5f - fwd));

            // (3) 水平尾翼 (Tail Stabilizers)
            float tailHalfSpan = 0.0f;
            if (fwd <= -4.5f && fwd >= -7.5f) {
                tailHalfSpan = saturate((-4.5f - fwd) / 3.0f) * 2.8f;
            }
            float tailDist = max(lateral - tailHalfSpan, max(fwd + 4.5f, -7.5f - fwd));

            // 形状の結合 (SDF 最小距離)
            float shapeDist = min(bodyDist, min(wingDist, tailDist));

            // 境界のソフトアンチエイリアシング & 半影ブラー
            float shadowMask = smoothstep(penumbra, -penumbra, shapeDist);

            // 高度による自然な減衰（低空ではクッキリ濃く、高空では淡く拡散）
            float altFade = saturate(1.0f - (t / maxShadowAlt));
            altFade = pow(altFade, 0.65f);

            aircraftShadow = shadowMask * altFade * gSpotLight.intensity;
        }
    }

    // 昼夜スケール
    float dayFactor = saturate(gDirectionalLight.intensity * 1.3f);

    // 大気・天空光による冷涼なアンビエントシャドウ色（日陰側の陰影色）
    float3 skyAmbient = lerp(float3(0.04f, 0.05f, 0.08f), float3(0.18f, 0.22f, 0.28f), dayFactor);
    // 太陽光直射（日向側の温かい直射日光）
    float3 sunLightColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity * 1.35f;

    // 谷底・低地のアンビエントオクルージョン (AO: 峡谷の深部ほど光が届きにくく暗い)
    float valleyAO = saturate((worldPos.y + 600.0f) / 800.0f) * 0.35f + 0.65f;
    
    // 機体の影による遮蔽率（直射日光を最大85%遮断、アンビエント光も最大40%減光）
    float shadowFactor = (1.0f - aircraftShadow * 0.85f);
    float ambientShadow = (1.0f - aircraftShadow * 0.40f);
    float3 lighting = (skyAmbient * valleyAO * ambientShadow) + (sunLightColor * sunLit * shadowFactor);

    // 3. プロシージャル地形テクスチャリング（極寒冷地峡谷・岩石・地層・砂利・泥土・霜の多層マテリアル）
    // (A) マルチスケールノイズサンプリング
    float macroNoise = TerrainFbm(worldPos.xz * 0.0032f);   // 300mスケール（地質帯・断層の斑）
    float midNoise   = TerrainFbm(worldPos.xz * 0.024f);    // 40mスケール（岩肌・砂利の分布）
    float microNoise = TerrainNoise2D(worldPos.xz * 0.18f); // 5mスケール（砂利・岩のザラつき）
    float cliffNoise = TerrainFbm(float2(worldPos.x + worldPos.z, worldPos.y) * 0.048f); // 垂直崖面のゴツゴツ感

    // (B) 谷底・低空平坦部のリアルな地面（冷たい暗灰褐色の泥土・頁岩・砂利グラベル）
    float3 soilDark  = float3(0.18f, 0.16f, 0.14f); // 湿った泥土・頁岩（シェール）
    float3 gravel    = float3(0.27f, 0.25f, 0.23f); // 砕石・粗い砂利
    float3 soilClay  = float3(0.22f, 0.19f, 0.16f); // 粘土質・風化岩粉
    float3 lichen    = float3(0.19f, 0.22f, 0.16f); // 寒冷地帯特有の暗色地衣類・微小枯草
    
    float3 groundBase = lerp(soilDark, gravel, midNoise);
    groundBase = lerp(groundBase, soilClay, smoothstep(0.45f, 0.75f, microNoise));
    groundBase = lerp(groundBase, lichen, smoothstep(0.60f, 0.90f, macroNoise) * 0.25f);
    groundBase *= (0.78f + 0.44f * microNoise); // 砂利・地面の細かな起伏明暗

    // (C) 峡谷・崖面の玄武岩・花崗岩（リアルな地層ストラータ＋ゴツゴツした岩肌）
    float3 rockDark  = float3(0.15f, 0.14f, 0.16f); // 暗色玄武岩
    float3 rockLight = float3(0.30f, 0.28f, 0.29f); // 露出した白っぽい花崗岩層
    float strata = sin(worldPos.y * 0.18f + cliffNoise * 3.6f) * 0.5f + 0.5f;
    float3 cliffRock = lerp(rockDark, rockLight, strata);
    cliffRock *= (0.72f + 0.55f * microNoise); // 岩肌の細かな明暗・ざらつき

    // (D) 高所・尾根の岩肌と雪・霜
    float3 peakRock = float3(0.24f, 0.24f, 0.26f);
    float heightRock = saturate((worldPos.y - 40.0f) / 220.0f);
    cliffRock = lerp(cliffRock, peakRock, heightRock);

    // 極夜・寒冷地の山頂薄雪・霜（高所かつ上向き面に付着）
    float snowMask = saturate((worldPos.y - 120.0f) / 180.0f) * saturate(rawN.y * 1.5f - 0.2f);
    snowMask = smoothstep(0.35f, 0.65f, snowMask + midNoise * 0.25f);
    float3 snowColor = float3(0.82f, 0.88f, 0.94f);

    // 斜面傾斜に応じた岩肌・断崖の露出（崖は岩肌、緩斜面は土・砂利）
    float rockBlend = smoothstep(0.04f, 0.22f, slope + midNoise * 0.08f);
    float3 albedo = lerp(groundBase, cliffRock, rockBlend);
    albedo = lerp(albedo, snowColor, snowMask * 0.65f);

    // マテリアル基本色の反映
    float3 matTint = gMaterial.color.rgb;
    if (matTint.r > 0.01f || matTint.g > 0.01f || matTint.b > 0.01f) {
        albedo = lerp(albedo, albedo * (matTint / float3(0.24f, 0.35f, 0.20f)), 0.20f);
    }

    // 4. 飛行 HUD / 空間把握用の等高線・距離グリッド（微細なディテール）
    float2 gridPos = worldPos.xz * 0.005f; // 200mグリッド
    float2 grid = abs(frac(gridPos - 0.5f) - 0.5f) / max(fwidth(gridPos), 0.0001f);
    float gridLine = 1.0f - min(min(grid.x, grid.y), 1.0f);
    
    // 50m間隔の等高線
    float contourPos = worldPos.y * 0.02f;
    float contour = 1.0f - min(abs(frac(contourPos - 0.5f) - 0.5f) / max(fwidth(contourPos), 0.0001f), 1.0f);
    
    // 速度感・高度感を高めるグリッドをわずかにブレンド (約5%)
    albedo = lerp(albedo, albedo * 1.25f + float3(0.04f, 0.06f, 0.05f), (gridLine * 0.5f + contour * 0.5f) * 0.08f);

    // 5. ライティング適用
    float3 finalRgb = albedo * lighting;

    // 山肌の微細なスペキュラハイライト（日向側の岩肌の立体感を強調、機体の影の中は遮蔽）
    float3 toEye = normalize(gLightingSettings.cameraPosition - worldPos);
    float3 H = normalize(L + toEye);
    float spec = pow(saturate(dot(steepN, H)), 24.0f) * 0.12f * sunLit * rockBlend * shadowFactor;
    finalRgb += gDirectionalLight.color.rgb * spec;

    // 6. 大気遠景フォグ（距離に応じた空気遠近法で遠くの山を霞ませ、山々の重なりと奥行きを強調）
    float dist = length(gLightingSettings.cameraPosition - worldPos);
    float fogFactor = saturate((dist - 1500.0f) / 22000.0f);
    fogFactor = pow(fogFactor, 1.4f);

    // 大気の霞・地平線色
    float3 fogColor = lerp(float3(0.05f, 0.06f, 0.10f), float3(0.18f, 0.24f, 0.34f), dayFactor);
    finalRgb = lerp(finalRgb, fogColor, fogFactor);

    output.color = float32_t4(finalRgb, 1.0f);
    return output;
}
