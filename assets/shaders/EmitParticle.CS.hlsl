#include "GPUParticle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

ConstantBuffer<PerFrame> gPerFrame : register(b0);
StructuredBuffer<GpuEmitRequest> gEmitRequests : register(t0);

[numthreads(64, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    uint32_t emitIndex = DTid.x;
    if (emitIndex >= gPerFrame.totalEmitCount) {
        return;
    }

    // 自身が属する発生リクエストを検索
    GpuEmitRequest req = (GpuEmitRequest)0;
    bool found = false;
    uint32_t reqCount = min(gPerFrame.requestCount, kMaxEmitRequests);
    for (uint32_t r = 0u; r < reqCount; ++r) {
        uint32_t start = gEmitRequests[r].startOffset;
        uint32_t end = start + gEmitRequests[r].count;
        if (emitIndex >= start && emitIndex < end) {
            req = gEmitRequests[r];
            found = true;
            break;
        }
    }
    if (!found) {
        return;
    }

    int32_t freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0 || freeListIndex >= (int32_t)kMaxParticles) {
        InterlockedAdd(gFreeListIndex[0], 1);
        return;
    }

    uint32_t particleIndex = gFreeList[freeListIndex];
    uint rng = PcgHash(emitIndex ^ (req.seedSalt * 747796405u) ^ asuint(gPerFrame.time));

    ParticleCS p = (ParticleCS)0;
    float32_t3 randomOffset = Rand3(rng) * 2.0f - 1.0f;
    p.translate = req.origin + randomOffset * req.randomPositionRange;
    p.velocity = lerp(req.minVelocity, req.maxVelocity, Rand3(rng));
    p.acceleration = req.acceleration;
    p.lifeTime = max(0.01f, lerp(req.minLifeTime, req.maxLifeTime, Rand01(rng)));
    p.currentTime = 0.0f;

    float32_t s = lerp(req.minScale, req.maxScale, Rand01(rng));
    p.scale = float32_t3(s, s, s);
    p.startScale = s;
    p.endScale = req.endScale;
    p.scaleEasing = max(0.01f, req.scaleEasing);

    float32_t4 col = lerp(req.minColor, req.maxColor, float32_t4(Rand01(rng), Rand01(rng), Rand01(rng), Rand01(rng)));
    p.color = col;
    p.startAlpha = col.a;

    p.rotation = lerp(req.minRotation, req.maxRotation, Rand01(rng));
    p.rotationSpeed = lerp(req.minRotationSpeed, req.maxRotationSpeed, Rand01(rng));
    p.spawnY = p.translate.y;
    p.maxRiseHeight = req.maxRiseHeight;
    p.flags = req.flags;
    p.padding = 0u;

    gParticles[particleIndex] = p;
}
