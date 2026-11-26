#include "GPUParticle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

ConstantBuffer<PerFrame> gPerFrame : register(b0);
StructuredBuffer<GpuEmitRequest> gEmitRequests : register(t0);

void BuildOrthoBasis(float32_t3 axis, out float32_t3 tangent, out float32_t3 bitangent) {
    float32_t3 n = (length(axis) > 1e-4f) ? normalize(axis) : float32_t3(0.0f, 1.0f, 0.0f);
    float32_t3 refVec = (abs(n.y) < 0.95f) ? float32_t3(0.0f, 1.0f, 0.0f) : float32_t3(1.0f, 0.0f, 0.0f);
    tangent = normalize(cross(refVec, n));
    bitangent = normalize(cross(n, tangent));
}

[numthreads(64, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    uint32_t emitIndex = DTid.x;
    if (emitIndex >= gPerFrame.totalEmitCount) {
        return;
    }

    // 自身が属する発生リクエストを二分探索で高速検索（startOffset単調増加）
    GpuEmitRequest req = (GpuEmitRequest)0;
    bool found = false;
    uint32_t reqCount = min(gPerFrame.requestCount, kMaxEmitRequests);
    uint32_t lo = 0u;
    uint32_t hi = reqCount;
    while (lo < hi) {
        uint32_t mid = (lo + hi) >> 1u;
        uint32_t start = gEmitRequests[mid].startOffset;
        uint32_t end = start + gEmitRequests[mid].count;
        if (emitIndex < start) {
            hi = mid;
        } else if (emitIndex >= end) {
            lo = mid + 1u;
        } else {
            req = gEmitRequests[mid];
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
    uint rng = PcgHash(emitIndex ^ (req.seedSalt * 747796405u) ^ asuint(gPerFrame.time) ^ (particleIndex * 2654435761u));

    ParticleCS p = (ParticleCS)0;

    // 発生形状に応じた座標＆初期速度の生成
    float32_t3 baseVel = lerp(req.minVelocity, req.maxVelocity, Rand3(rng));
    if ((req.flags & kParticleFlagRingEmit) != 0u) {
        float32_t3 tAxis, bAxis;
        BuildOrthoBasis(req.emitAxis, tAxis, bAxis);
        float32_t angle = Rand01(rng) * 6.2831853f;
        float32_t3 radialDir = tAxis * cos(angle) + bAxis * sin(angle);
        float32_t radiusJitter = req.ringRadius + (Rand01(rng) * 2.0f - 1.0f) * req.randomPositionRange;
        float32_t3 axisJitter = normalize(length(req.emitAxis) > 1e-4f ? req.emitAxis : float32_t3(0.0f, 1.0f, 0.0f)) *
                                ((Rand01(rng) * 2.0f - 1.0f) * req.randomPositionRange * 0.35f);
        p.translate = req.origin + radialDir * max(0.0f, radiusJitter) + axisJitter;

        float32_t radialSpeed = lerp(req.radialSpeedMin, req.radialSpeedMax, Rand01(rng));
        p.velocity = baseVel + radialDir * radialSpeed;
    } else if ((req.flags & kParticleFlagSphereEmit) != 0u) {
        float32_t3 dir = RandUnitSphere(rng);
        float32_t r = pow(Rand01(rng), 0.45f) * req.randomPositionRange;
        p.translate = req.origin + dir * r;

        float32_t radialSpeed = lerp(req.radialSpeedMin, req.radialSpeedMax, Rand01(rng));
        p.velocity = baseVel + dir * radialSpeed;
    } else {
        float32_t3 randomOffset = Rand3(rng) * 2.0f - 1.0f;
        p.translate = req.origin + randomOffset * req.randomPositionRange;
        p.velocity = baseVel;
    }

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
    p.shapeType = req.shapeType;

    p.drag = max(0.0f, req.drag);
    p.curlStrength = req.curlStrength;
    p.curlFrequency = max(0.001f, req.curlFrequency);
    p.stretchFactor = req.stretchFactor;
    p.temperature = lerp(req.minTemperature, req.maxTemperature, Rand01(rng));
    p.coolingRate = max(0.0f, req.coolingRate);
    p.floorY = req.floorY;
    p.restitution = saturate(req.restitution);
    p.emissiveIntensity = max(0.0f, req.emissiveIntensity);
    p.softDistance = max(0.05f, req.softDistance);
    p.randomSeed = Rand01(rng);
    p.wakeSensitivity = req.wakeSensitivity;

    // 黒体放射が有効な場合、初フレームから温度色を設定
    if ((p.flags & kParticleFlagBlackbody) != 0u) {
        p.color.rgb = BlackbodyColorRamp(p.temperature);
    }

    gParticles[particleIndex] = p;
}
