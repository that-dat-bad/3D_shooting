// 頂点シェーダーからの入力
struct GSInput
{
    float32_t4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : WORLDPOSITION0;
    float32_t3 localPosition : LOCALPOSITION0;
    float32_t3 barycentric : BARYCENTRIC0;
    float scanFactor : SCANFACTOR0;
};

// ピクセルシェーダーへの出力
struct GSOutput
{
    float32_t4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : WORLDPOSITION0;
    float32_t3 localPosition : LOCALPOSITION0;
    float32_t3 barycentric : BARYCENTRIC0;
    float scanFactor : SCANFACTOR0;
};

// ライティング・スキャン設定定数バッファ
struct LightingSettings
{
    int shadingModel;
    int specularModel;
    int lightType;
    int scanEnabled;              // 0: 通常パススルー, 1: スキャン演出有効
    float32_t3 cameraPosition;
    float emissiveIntensityScale;
    float scanProgress;           // 0.0: 未起動 -> 1.0: スキャン完了
    float scanBandWidth;          // ネオンワイヤーフレーム帯の幅
    float scanMinZ;               // 機首ノーズZ
    float scanMaxZ;               // 尾翼ノズルZ
    float32_t4 scanColor;         // ネオン発光色
};

ConstantBuffer<LightingSettings> gLightingSettings : register(b3);

[maxvertexcount(3)]
void main(triangle GSInput input[3], inout TriangleStream<GSOutput> triStream)
{
    // スキャン演出が無効のときはパススルー
    if (gLightingSettings.scanEnabled == 0)
    {
        [unroll]
        for (uint i = 0; i < 3; i++)
        {
            GSOutput output;
            output.position = input[i].position;
            output.texcoord = input[i].texcoord;
            output.normal = input[i].normal;
            output.worldPosition = input[i].worldPosition;
            output.localPosition = input[i].localPosition;
            output.barycentric = float32_t3(0.0f, 0.0f, 0.0f);
            output.scanFactor = 0.0f;
            triStream.Append(output);
        }
        triStream.RestartStrip();
        return;
    }

    // 戦闘モード起動スキャン演出
    float currentScanZ = gLightingSettings.scanMinZ + (gLightingSettings.scanMaxZ - gLightingSettings.scanMinZ) * saturate(gLightingSettings.scanProgress);
    float bandWidth = max(0.5f, gLightingSettings.scanBandWidth);

    // 三角形のローカルZ範囲
    float minZ = min(input[0].localPosition.z, min(input[1].localPosition.z, input[2].localPosition.z));
    float maxZ = max(input[0].localPosition.z, max(input[1].localPosition.z, input[2].localPosition.z));

    // 未到達の三角形を破棄
    if (minZ > currentScanZ)
    {
        return;
    }

    // 重心座標: ピクセルシェーダーで完全なワイヤーフレームを抽出
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

        // スキャン境界帯の距離計算
        float distToScan = currentScanZ - input[j].localPosition.z;
        if (distToScan <= bandWidth)
        {
            // ワイヤーフレーム構築フェーズ
            output.scanFactor = saturate(1.0f - (distToScan / bandWidth));
        }
        else
        {
            // 3D装甲実体化フェーズ
            output.scanFactor = 0.0f;
        }

        triStream.Append(output);
    }
    triStream.RestartStrip();
}
