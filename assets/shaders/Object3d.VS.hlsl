// 頂点データの入力構造体
struct VertexInput
{
    float32_t4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 weight : WEIGHT0;
    int4 indices : BLENDINDICES0;
};

// ピクセルシェーダーへの出力構造体
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
    matrix WorldInverseTranspose; // WorldInverseTranspose for non-uniform scaling
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    // スキニング計算は Compute Shader で事前計算済み
    float32_t4 skinnedPosition = input.position;
    
    // 変形した座標に、カメラなどのWVP行列を掛けて画面に出力
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    
    // UV座標をそのまま渡す
    output.texcoord = input.texcoord;
    
    float32_t3 skinnedNormal = input.normal;
    output.normal = normalize(mul(skinnedNormal, (float32_t3x3) gTransformationMatrix.WorldInverseTranspose));
    
    // 頂点位置をワールド空間に変換
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    return output;
}

