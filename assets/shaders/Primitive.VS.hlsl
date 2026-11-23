struct VertexInput
{
    float32_t4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

struct VertexOutput
{
    float32_t4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : WORLDPOSITION0;
};

// 変換行列を格納する定数バッファ
struct TransformationMatrix
{
    matrix WVP; // World * View * Projection
    matrix World; // World
    matrix WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    
    // ボーン計算を完全にスキップして、そのままWVP行列を掛ける
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    // UV座標
    output.texcoord = input.texcoord;
    
    // 法線のワールド変換
    output.normal = normalize(mul(input.normal, (float32_t3x3) gTransformationMatrix.WorldInverseTranspose));
    
    // ワールド座標
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;
    
    return output;
}
