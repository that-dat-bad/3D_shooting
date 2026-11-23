struct GSInput
{
    float32_t4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : WORLDPOSITION0;
    float32_t3 localPosition : LOCALPOSITION0;
};

struct GSOutput
{
    float32_t4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : WORLDPOSITION0;
    float32_t3 localPosition : LOCALPOSITION0;
    float32_t3 barycentric : BARYCENTRIC0; // 重心座標 , ,
};

[maxvertexcount(3)]
void main(triangle GSInput input[3], inout TriangleStream<GSOutput> triStream)
{
    // 重心座標: ピクセルシェーダーで完全な稜線ワイヤーフレームを抽出
    const float32_t3 barycentrics[3] = {
        float32_t3(1.0f, 0.0f, 0.0f),
        float32_t3(0.0f, 1.0f, 0.0f),
        float32_t3(0.0f, 0.0f, 1.0f)
    };

    [unroll]
    for (uint j = 0; j < 3; j++)
    {
        GSOutput output;
        output.position = input[j].position;
        output.texcoord = input[j].texcoord;
        output.normal = input[j].normal;
        output.worldPosition = input[j].worldPosition;
        output.localPosition = input[j].localPosition;
        output.barycentric = barycentrics[j];

        triStream.Append(output);
    }
    triStream.RestartStrip();
}
