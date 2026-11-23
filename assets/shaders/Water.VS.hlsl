struct VertexInput
{
    float32_t4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 weight : WEIGHT0;
    int4 indices : BLENDINDICES0;
};

struct VertexOutput
{
    float32_t4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : WORLDPOSITION0;
};

struct Material
{
    float32_t4 color;
    int enableLighting;
    float shininess;               // < 50: 海モード, >= 50: 川モード
    float environmentCoefficient;
    float specularIntensity;
    float32_t3 emissiveColor;
    float emissiveIntensity;       // timeSec として使用
    matrix uvTransform;
};

struct TransformationMatrix
{
    matrix WVP;
    matrix World;
    matrix WorldInverseTranspose;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

// 4オクターブの方向性波
float EvaluateCascadeWave(float2 xz, float timeSec, out float3 outNormal)
{
    const float2 dirs[4] = {
        float2(0.866f, 0.500f),
        float2(0.422f, -0.906f),
        float2(-0.707f, 0.707f),
        float2(0.965f, -0.258f)
    };
    const float freqs[4] = { 0.018f, 0.037f, 0.082f, 0.175f };
    const float amps[4]  = { 0.85f,  0.42f,  0.18f,  0.07f };
    const float speeds[4]= { 1.15f,  1.65f,  2.40f,  3.20f };

    float h = 0.0f;
    float dx = 0.0f;
    float dz = 0.0f;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float phase = dot(xz, dirs[i]) * freqs[i] + timeSec * speeds[i];
        float s = sin(phase);
        float c = cos(phase);
        h += amps[i] * s;
        float deriv = amps[i] * freqs[i] * c;
        dx += deriv * dirs[i].x;
        dz += deriv * dirs[i].y;
    }

    outNormal = normalize(float3(-dx, 1.0f, -dz));
    return h;
}

VertexOutput main(VertexInput input)
{
    VertexOutput output;

    float4 worldPos = mul(input.position, gTransformationMatrix.World);
    float3 worldNormal = normalize(mul(input.normal, (float3x3)gTransformationMatrix.WorldInverseTranspose));

    float timeSec = gMaterial.emissiveIntensity;
    bool isRiver = (gMaterial.emissiveColor.x > 0.5f);

    if (!isRiver)
    {
        float3 waveN;
        float waveH = EvaluateCascadeWave(worldPos.xz, timeSec, waveN);
        // 遠景でのエイリアシングを抑えるため距離減衰
        float distFade = saturate(1.0f - length(worldPos.xz) / 60000.0f);
        worldPos.y += waveH * distFade;
        worldNormal = normalize(lerp(float3(0.0f, 1.0f, 0.0f), waveN, distFade));
    }

    output.worldPosition = worldPos.xyz;
    output.normal = worldNormal;
    output.texcoord = input.texcoord;

    // ローカル変位を適用してWVP変換
    float4 localPos = input.position;
    if (!isRiver)
    {
        float3 dummyN;
        float waveH = EvaluateCascadeWave(worldPos.xz, timeSec, dummyN);
        float distFade = saturate(1.0f - length(worldPos.xz) / 60000.0f);
        localPos.y += waveH * distFade;
    }
    output.position = mul(localPos, gTransformationMatrix.WVP);
    return output;
}
