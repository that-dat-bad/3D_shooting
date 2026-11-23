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

    float32_t c = cos(particle.rotation);
    float32_t s = sin(particle.rotation);
    float32_t2 rotatedLocal = float32_t2(
        input.position.x * c - input.position.y * s,
        input.position.x * s + input.position.y * c
    );

    float3 xAxis = gPerView.billboardMatrix[0].xyz;
    float3 yAxis = gPerView.billboardMatrix[1].xyz;

    float3 worldPos = particle.translate;
    worldPos += xAxis * (rotatedLocal.x * particle.scale.x);
    worldPos += yAxis * (rotatedLocal.y * particle.scale.y);

    output.position = mul(float4(worldPos, 1.0f), gPerView.viewProjection);
    output.texcoord = input.texcoord;
    output.color = particle.color;

    return output;
}
