#include "GPUParticle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
RWStructuredBuffer<uint32_t> gAliveListAlpha : register(u3);
RWStructuredBuffer<uint32_t> gAliveListAdd : register(u4);
RWStructuredBuffer<uint32_t> gIndirectArgs : register(u5);

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    uint32_t particleIndex = DTid.x;
    if (particleIndex < kMaxParticles) {
        gParticles[particleIndex] = (ParticleCS)0;
        gFreeList[particleIndex] = particleIndex;
        gAliveListAlpha[particleIndex] = 0u;
        gAliveListAdd[particleIndex] = 0u;
    }

    if (particleIndex == 0u) {
        gFreeListIndex[0] = (int32_t)(kMaxParticles - 1u);

        static const uint32_t kVertexCount = 4u;
        // Bucket 0: Normal Alpha
        gIndirectArgs[0] = kVertexCount;
        gIndirectArgs[1] = 0u;
        gIndirectArgs[2] = 0u;
        gIndirectArgs[3] = 0u;
        // Bucket 1: Additive HDR
        gIndirectArgs[4] = kVertexCount;
        gIndirectArgs[5] = 0u;
        gIndirectArgs[6] = 0u;
        gIndirectArgs[7] = 0u;
    }
}
