#include "GPUParticle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
RWStructuredBuffer<uint32_t> gAliveList : register(u3);
RWStructuredBuffer<uint32_t> gIndirectArgs : register(u4);

ConstantBuffer<PerFrame> gPerFrame : register(b0);

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    uint32_t particleIndex = DTid.x;
    if (particleIndex >= kMaxParticles) {
        return;
    }

    ParticleCS p = gParticles[particleIndex];
    if (p.lifeTime <= 0.0f) {
        return;
    }

    float32_t dt = clamp(gPerFrame.deltaTime, 0.0f, 0.1f);
    float32_t frameScale = dt * 60.0f;

    // 上昇上限高さによる加速度・速度減衰（CPU版と一致）
    float32_t3 effectiveAccel = p.acceleration;
    if (p.maxRiseHeight > 0.0f) {
        float32_t currentRise = p.translate.y - p.spawnY;
        float32_t heightRatio = saturate(currentRise / p.maxRiseHeight);
        if (effectiveAccel.y > 0.0f) {
            effectiveAccel.y *= (1.0f - heightRatio);
        }
    }

    p.velocity += effectiveAccel * frameScale;

    if (p.maxRiseHeight > 0.0f) {
        float32_t currentRise = p.translate.y - p.spawnY;
        float32_t heightRatio = saturate(currentRise / p.maxRiseHeight);
        if (heightRatio > 0.45f && p.velocity.y > 0.0f) {
            float32_t brakeT = saturate((heightRatio - 0.45f) / 0.55f);
            p.velocity.y *= (1.0f - brakeT);
        }
    }

    p.translate += p.velocity * frameScale;

    if (p.maxRiseHeight > 0.0f) {
        float32_t ceilingY = p.spawnY + p.maxRiseHeight;
        if (p.translate.y > ceilingY) {
            p.translate.y = ceilingY;
            if (p.velocity.y > 0.0f) {
                p.velocity.y = 0.0f;
            }
        }
    }

    p.rotation += p.rotationSpeed * dt;
    p.currentTime += dt;

    if (p.currentTime >= p.lifeTime) {
        p.lifeTime = 0.0f;
        p.scale = float32_t3(0.0f, 0.0f, 0.0f);
        p.color.a = 0.0f;
        gParticles[particleIndex] = p;

        int32_t freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        if ((freeListIndex + 1) < (int32_t)kMaxParticles) {
            gFreeList[freeListIndex + 1] = particleIndex;
        } else {
            InterlockedAdd(gFreeListIndex[0], -1);
        }
        return;
    }

    float32_t t = saturate(p.currentTime / p.lifeTime);
    float32_t easedT = pow(t, max(0.01f, p.scaleEasing));
    float32_t currentScale = lerp(p.startScale, p.endScale, easedT);
    p.scale = float32_t3(currentScale, currentScale, currentScale);
    p.color.a = ((p.flags & kParticleFlagFadeOut) != 0u) ? (p.startAlpha * (1.0f - t)) : p.startAlpha;

    gParticles[particleIndex] = p;

    uint32_t aliveIndex;
    InterlockedAdd(gIndirectArgs[1], 1u, aliveIndex);
    if (aliveIndex < kMaxParticles) {
        gAliveList[aliveIndex] = particleIndex;
    }
}
