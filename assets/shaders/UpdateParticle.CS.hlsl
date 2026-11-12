#include "GPUParticle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
RWStructuredBuffer<uint32_t> gAliveList : register(u3);
RWStructuredBuffer<uint32_t> gIndirectArgs : register(u4);

ConstantBuffer<PerFrame> gPerFrame : register(b1);

[numthreads(1024, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    if (DTid.x == 0) {
        gIndirectArgs[1] = 0; // Reset InstanceCount
    }
    DeviceMemoryBarrierWithGroupSync();

    uint32_t particleIndex = DTid.x;
    if (particleIndex < kMaxParticles) {
        if (gParticles[particleIndex].color.a != 0) {
            gParticles[particleIndex].translate += gParticles[particleIndex].velocity;
            gParticles[particleIndex].currentTime += gPerFrame.deltaTime;
            
            float32_t alpha = 1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
            gParticles[particleIndex].color.a = saturate(alpha);
            
            if (gParticles[particleIndex].color.a == 0) {
                gParticles[particleIndex].scale = float32_t3(0.0f, 0.0f, 0.0f);
                
                int32_t freeListIndex;
                InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
                if ((freeListIndex + 1) < kMaxParticles) {
                    gFreeList[freeListIndex + 1] = particleIndex;
                } else {
                    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
                }
            } else {
                // 生きているパーティクルをAliveListに追加
                uint32_t aliveIndex;
                InterlockedAdd(gIndirectArgs[1], 1, aliveIndex);
                gAliveList[aliveIndex] = particleIndex;
            }
        }
    }
}
