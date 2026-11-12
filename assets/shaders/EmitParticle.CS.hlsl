#include "GPUParticle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

[numthreads(1, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    if (gEmitter.emit != 0) {
        RandomGenerator generator;
        generator.seed = (DTid + gPerFrame.time) * gPerFrame.time;

        for (uint32_t countIndex = 0; countIndex < gEmitter.count; ++countIndex) {
            int32_t freeListIndex;
            InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
            
            if (0 <= freeListIndex && freeListIndex < kMaxParticles) {
                uint32_t particleIndex = gFreeList[freeListIndex];
                
                // Initialize Particle
                gParticles[particleIndex].scale = float32_t3(0.5f, 0.5f, 0.5f);
                float32_t3 randomOffset = generator.Generate3d() * 2.0f - 1.0f; // -1 to 1
                gParticles[particleIndex].translate = gEmitter.translate + randomOffset * gEmitter.radius;
                
                float32_t3 randomVelocity = generator.Generate3d() * 2.0f - 1.0f;
                gParticles[particleIndex].velocity = randomVelocity * 0.1f; // speed
                gParticles[particleIndex].color = float32_t4(1.0f, 1.0f, 1.0f, 1.0f);
                gParticles[particleIndex].lifeTime = 1.0f + generator.Generate1d(); // 1~2 seconds
                gParticles[particleIndex].currentTime = 0.0f;
                
            } else {
                InterlockedAdd(gFreeListIndex[0], 1);
                break;
            }
        }
    }
}
