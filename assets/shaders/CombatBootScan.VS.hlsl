// =========================================================================
// CombatBootScan.VS.hlsl - 戦闘モード起動スキャン専用 頂点シェーダー
// =========================================================================

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
    float32_t3 localPosition : LOCALPOSITION0; // 機体モデル空間のローカル座標
};

struct TransformationMatrix
{
    matrix WVP;
    matrix World;
    matrix WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    float32_t4 skinnedPosition = input.position;
    
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    
    float32_t3 skinnedNormal = input.normal;
    output.normal = normalize(mul(skinnedNormal, (float32_t3x3)gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    output.localPosition = input.position.xyz;
    
    return output;
}
