#include "GPUParticle.hlsli"

struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

struct VertexShaderOutput {
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
};

StructuredBuffer<ParticleCS> gParticles : register(t0);
StructuredBuffer<uint32_t> gAliveList : register(t1);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID) {
    VertexShaderOutput output;
    
    uint32_t particleIndex = gAliveList[instanceId];
    ParticleCS particle = gParticles[particleIndex];
    
    // Extract camera axes directly from the rows of the billboard matrix.
    // With -Zpr (row-major), the rows map exactly to the C++ matrix rows.
    float3 xAxis = gPerView.billboardMatrix[0].xyz;
    float3 yAxis = gPerView.billboardMatrix[1].xyz;
    float3 zAxis = gPerView.billboardMatrix[2].xyz;
    
    // Compute world position by scaling and adding along the camera axes
    float3 worldPos = particle.translate;
    worldPos += xAxis * (input.position.x * particle.scale.x);
    worldPos += yAxis * (input.position.y * particle.scale.y);
    worldPos += zAxis * (input.position.z * particle.scale.z);
    
    output.position = mul(float4(worldPos, 1.0f), gPerView.viewProjection);
    output.texcoord = input.texcoord;
    output.color = particle.color;
    
    return output;
}
