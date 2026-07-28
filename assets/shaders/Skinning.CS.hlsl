struct VertexData
{
    float4 position;
    float2 texcoord;
    float3 normal;
    float3 padding;
    float4 weight;
    int4 indices;
};

struct BoneMatrix
{
    matrix matrices[100];
};

struct SkinningParams
{
    uint numVertices;
    float3 padding;
};

ConstantBuffer<SkinningParams> gSkinningParams : register(b0);
ConstantBuffer<BoneMatrix> gBones : register(b1);
StructuredBuffer<VertexData> gInputVertices : register(t0);
RWStructuredBuffer<VertexData> gOutputVertices : register(u0);

[numthreads(128, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexIndex = DTid.x;
    if (vertexIndex >= gSkinningParams.numVertices)
    {
        return;
    }

    VertexData input = gInputVertices[vertexIndex];
    VertexData output = input;

    // 4つのボーン行列をウェイト（影響度）でブレンド
    matrix skinnedMatrix =
        gBones.matrices[input.indices[0]] * input.weight.x +
        gBones.matrices[input.indices[1]] * input.weight.y +
        gBones.matrices[input.indices[2]] * input.weight.z +
        gBones.matrices[input.indices[3]] * input.weight.w;

    // ボーン変形を座標と法線に適用
    output.position = mul(input.position, skinnedMatrix);
    output.normal = mul(input.normal, (float3x3)skinnedMatrix);

    gOutputVertices[vertexIndex] = output;
}
