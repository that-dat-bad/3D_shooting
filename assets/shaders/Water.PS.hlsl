// Dagor / War Thunder 準拠 水面ピクセルシェーダー
// 海: Shore Distance Field (浅瀬減衰/白波) + 多層波法線 + Schlickフレネル + Skybox反射
// 川: 2相フェーズシフト Flowmap 流水スクロール + 岸辺フェード

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET;
};

struct Material
{
    float32_t4 color;
    int enableLighting;
    float shininess;               // < 50: 海モード, >= 50: 川モード
    float environmentCoefficient;
    float specularIntensity;
    float32_t3 emissiveColor;
    float emissiveIntensity;       // timeSec
    matrix uvTransform;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
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
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<LightingSettings> gLightingSettings : register(b3);

Texture2D<float32_t4> gTexture : register(t0);     // shore_sdf.png (海) または汎用テクスチャ
TextureCube<float32_t4> gEnvTexture : register(t1);
SamplerState gSampler : register(s0);

struct PixelInput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : WORLDPOSITION0;
};

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float ValueNoise2D(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float a = Hash21(i);
    float b = Hash21(i + float2(1.0f, 0.0f));
    float c = Hash21(i + float2(0.0f, 1.0f));
    float d = Hash21(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float3 ComputeMicroWaveNormal(float2 uv, float timeSec, float strength)
{
    float e = 0.15f;
    float h0 = ValueNoise2D(uv * 0.18f + float2(timeSec * 0.45f, timeSec * 0.25f))
             + 0.5f * ValueNoise2D(uv * 0.43f - float2(timeSec * 0.65f, -timeSec * 0.35f));
    float hx = ValueNoise2D((uv + float2(e, 0.0f)) * 0.18f + float2(timeSec * 0.45f, timeSec * 0.25f))
             + 0.5f * ValueNoise2D((uv + float2(e, 0.0f)) * 0.43f - float2(timeSec * 0.65f, -timeSec * 0.35f));
    float hz = ValueNoise2D((uv + float2(0.0f, e)) * 0.18f + float2(timeSec * 0.45f, timeSec * 0.25f))
             + 0.5f * ValueNoise2D((uv + float2(0.0f, e)) * 0.43f - float2(timeSec * 0.65f, -timeSec * 0.35f));
    float dx = (hx - h0) / e;
    float dz = (hz - h0) / e;
    return normalize(float3(-dx * strength, 1.0f, -dz * strength));
}

PixelShaderOutput main(PixelInput input)
{
    PixelShaderOutput output;

    float timeSec = gMaterial.emissiveIntensity;
    bool isRiver = (gMaterial.emissiveColor.x > 0.5f);

    float3 V = normalize(gLightingSettings.cameraPosition - input.worldPosition);
    float3 L = normalize(-gDirectionalLight.direction);
    if (length(gDirectionalLight.direction) < 0.001f)
    {
        L = normalize(float3(0.5f, 0.8f, 0.4f));
    }

    float3 N;
    float3 waterBodyColor;
    float foam = 0.0f;
    float alpha = 0.92f;

    if (isRiver)
    {
        // Dagor 式 2相フェーズシフト Flowmap スクロール (UV.y が上流 -> 下流)
        float flowSpeed = 0.42f;
        float phase0 = frac(timeSec * flowSpeed);
        float phase1 = frac(timeSec * flowSpeed + 0.5f);
        float blendW = abs(2.0f * phase0 - 1.0f);

        float2 uv0 = float2(input.texcoord.x * 3.0f, input.texcoord.y * 2.0f - phase0 * 2.0f);
        float2 uv1 = float2(input.texcoord.x * 3.0f + 0.37f, input.texcoord.y * 2.0f - phase1 * 2.0f);

        float3 n0 = ComputeMicroWaveNormal(uv0 * 14.0f, timeSec * 0.3f, 0.38f);
        float3 n1 = ComputeMicroWaveNormal(uv1 * 14.0f, timeSec * 0.3f, 0.38f);
        N = normalize(lerp(n0, n1, blendW));

        // 岸辺 (uv.x が 0 または 1 に近い) の白泡とフェード
        float edgeDist = min(input.texcoord.x, 1.0f - input.texcoord.x);
        float bankMask = saturate(edgeDist / 0.14f);
        float foamNoise = lerp(ValueNoise2D(uv0 * 6.0f), ValueNoise2D(uv1 * 6.0f), blendW);
        foam = saturate((1.0f - bankMask) * foamNoise * 1.4f);

        waterBodyColor = lerp(float3(0.05f, 0.24f, 0.26f), float3(0.03f, 0.14f, 0.20f), bankMask);
        alpha = saturate(0.55f + 0.40f * bankMask);
    }
    else
    {
        // 海モード: Shore Distance Field による浅瀬・白波 + 多層波法線
        float shoreDist = gTexture.Sample(gSampler, saturate(input.texcoord)).r;
        // 岸に近いほど細波を減衰 (Shoaling)
        float waveDamp = lerp(0.35f, 1.0f, saturate(shoreDist * 2.5f));
        float3 microN = ComputeMicroWaveNormal(input.worldPosition.xz * 0.22f, timeSec, 0.32f * waveDamp);
        N = normalize(lerp(normalize(input.normal), microN, 0.65f));

        // 水深グラデーション（浅瀬エメラルド -> 外洋ディープネイビー）
        float3 shallowCol = float3(0.06f, 0.32f, 0.36f);
        float3 deepCol    = float3(0.015f, 0.065f, 0.14f);
        waterBodyColor = lerp(shallowCol, deepCol, saturate(shoreDist * 2.2f));

        // 岸辺の波打ち際白波 (Shore Foam)
        float shoreWave = sin(shoreDist * 38.0f - timeSec * 2.4f) * 0.5f + 0.5f;
        float foamNoise = ValueNoise2D(input.worldPosition.xz * 0.04f + float2(timeSec * 0.2f, 0.0f));
        foam = saturate((1.0f - saturate(shoreDist / 0.16f)) * shoreWave * foamNoise * 1.6f);
        alpha = saturate(0.75f + 0.23f * saturate(shoreDist * 4.0f));
    }

    // Schlick フレネル (F0 = 0.02 for water)
    float NdotV = saturate(dot(N, V));
    float fresnel = 0.02f + 0.98f * pow(1.0f - NdotV, 5.0f);

    // Skybox 環境反射
    float3 R = reflect(-V, N);
    R.y = abs(R.y);
    float3 skyReflect = gEnvTexture.Sample(gSampler, R).rgb;
    if (dot(skyReflect, skyReflect) < 0.001f)
    {
        skyReflect = float3(0.45f, 0.62f, 0.85f);
    }

    // 太陽スペキュラ (Blinn-Phong)
    float3 H = normalize(L + V);
    float spec = pow(saturate(dot(N, H)), 180.0f) * 1.4f;
    float NdotL = saturate(dot(N, L));

    float3 litWater = waterBodyColor * (0.35f + 0.65f * NdotL) * gDirectionalLight.color.rgb * max(gDirectionalLight.intensity, 0.8f);
    float3 finalCol = lerp(litWater, skyReflect, fresnel * 0.85f) + gDirectionalLight.color.rgb * spec;
    finalCol = lerp(finalCol, float3(0.92f, 0.96f, 0.99f), foam * 0.75f);

    output.color = float4(finalCol, alpha);
    return output;
}
