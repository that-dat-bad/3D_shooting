#include "GPUParticle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
RWStructuredBuffer<uint32_t> gAliveList : register(u3);
RWStructuredBuffer<uint32_t> gIndirectArgs : register(u4);

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    uint32_t particleIndex = DTid.x;
    if (particleIndex < kMaxParticles) {
        gParticles[particleIndex] = (ParticleCS)0;
        gFreeList[particleIndex] = particleIndex;
    }

    if (particleIndex == 0) {
        gFreeListIndex[0] = (int32_t)(kMaxParticles - 1u);

        static const uint32_t kVertexCount = 4u;
        gIndirectArgs[0] = kVertexCount; // VertexCountPerInstance
        gIndirectArgs[1] = 0u;           // InstanceCount
        gIndirectArgs[2] = 0u;           // StartVertexLocation
        gIndirectArgs[3] = 0u;           // StartInstanceLocation
    }
}
