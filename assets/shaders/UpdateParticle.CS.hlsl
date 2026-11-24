#include "GPUParticle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
RWStructuredBuffer<uint32_t> gAliveListAlpha : register(u3);
RWStructuredBuffer<uint32_t> gAliveListAdd : register(u4);
RWStructuredBuffer<uint32_t> gIndirectArgs : register(u5);

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
    bool isWorldSI = ((p.flags & kParticleFlagWorldUnitsPerSec) != 0u);
    float32_t stepScale = isWorldSI ? dt : (dt * 60.0f);

    // 1. 上昇上限高さによる加速度減衰（既存CPU/GPU互換）
    float32_t3 effectiveAccel = p.acceleration;
    if (p.maxRiseHeight > 0.0f) {
        float32_t currentRise = p.translate.y - p.spawnY;
        float32_t heightRatio = saturate(currentRise / p.maxRiseHeight);
        if (effectiveAccel.y > 0.0f) {
            effectiveAccel.y *= (1.0f - heightRatio);
        }
    }

    p.velocity += effectiveAccel * stepScale;

    // 2. 3D カールノイズ乱流（発散なし渦場）
    if ((p.flags & kParticleFlagCurlNoise) != 0u && p.curlStrength > 0.0001f) {
        float32_t3 curl = ComputeCurlNoise3D(p.translate * p.curlFrequency, gPerFrame.time * gPerFrame.globalCurlTimeScale);
        p.velocity += curl * (p.curlStrength * dt);
    }

    // 3. 航空機ウェイク（翼端渦＆ジェット排気ブラスト）との相互作用
    if ((p.flags & kParticleFlagWakeInteract) != 0u && gPerFrame.activeWakeCount > 0u) {
        float32_t3 wakeAccel = float32_t3(0.0f, 0.0f, 0.0f);
        uint32_t wakeLimit = min(gPerFrame.activeWakeCount, kMaxWakeFields);
        for (uint32_t w = 0u; w < wakeLimit; ++w) {
            wakeAccel += EvaluateAircraftWakeForce(p.translate, gPerFrame.wakeFields[w]);
        }
        p.velocity += wakeAccel * (dt * p.wakeSensitivity);
    }

    // 4. 空気抵抗（指数減衰ドラッグ）
    if (p.drag > 0.0001f) {
        if ((p.flags & kParticleFlagCameraWrap) != 0u && gPerFrame.ambient.active != 0u) {
            // 環境粒子はウェイクで乱された後、滑らかに基準の吹雪風速へ収束する
            float32_t blendRate = 1.0f - exp(-p.drag * dt);
            p.velocity = lerp(p.velocity, gPerFrame.ambient.windVelocity, blendRate);
        } else {
            p.velocity *= exp(-p.drag * dt);
        }
    }

    if (p.maxRiseHeight > 0.0f) {
        float32_t currentRise = p.translate.y - p.spawnY;
        float32_t heightRatio = saturate(currentRise / p.maxRiseHeight);
        if (heightRatio > 0.45f && p.velocity.y > 0.0f) {
            float32_t brakeT = saturate((heightRatio - 0.45f) / 0.55f);
            p.velocity.y *= (1.0f - brakeT);
        }
    }

    // 5. 位置積分
    p.translate += p.velocity * stepScale;

    if (p.maxRiseHeight > 0.0f) {
        float32_t ceilingY = p.spawnY + p.maxRiseHeight;
        if (p.translate.y > ceilingY) {
            p.translate.y = ceilingY;
            if (p.velocity.y > 0.0f) {
                p.velocity.y = 0.0f;
            }
        }
    }

    // 6. 地面・地形高さでのバウンド＆摩擦（火花・破片スパーク用）
    if ((p.flags & kParticleFlagFloorBounce) != 0u) {
        if (p.translate.y < p.floorY) {
            p.translate.y = p.floorY;
            if (p.velocity.y < 0.0f) {
                p.velocity.y = -p.velocity.y * p.restitution;
                // 接地摩擦とわずかなスキッター揺らぎ
                p.velocity.x *= 0.78f;
                p.velocity.z *= 0.78f;
                // 接地時にわずかに温度を保ちつつ回転を反転
                p.rotationSpeed = -p.rotationSpeed * 0.65f;
            }
        }
    }

    // 7. カメラ追従ラップアラウンド（極地吹雪・氷晶ダスト用）
    if ((p.flags & kParticleFlagCameraWrap) != 0u && gPerFrame.ambient.active != 0u) {
        float32_t halfExt = max(5.0f, gPerFrame.ambient.halfExtent);
        float32_t fullSpan = halfExt * 2.0f;
        float32_t3 rel = p.translate - gPerFrame.ambient.center;
        // [-halfExt, +halfExt] に滑らかにラップ
        float32_t3 wrapped = frac((rel + halfExt) / fullSpan) * fullSpan - halfExt;
        p.translate = gPerFrame.ambient.center + wrapped;
    }

    p.rotation += p.rotationSpeed * dt;
    p.currentTime += dt;

    // 8. 寿命チェック → FreeList へ返却
    if (p.currentTime >= p.lifeTime) {
        p.lifeTime = 0.0f;
        p.scale = float32_t3(0.0f, 0.0f, 0.0f);
        p.color.a = 0.0f;
        gParticles[particleIndex] = p;

        int32_t freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        if ((freeListIndex + 1) >= 0 && (freeListIndex + 1) < (int32_t)kMaxParticles) {
            gFreeList[freeListIndex + 1] = particleIndex;
        } else {
            InterlockedAdd(gFreeListIndex[0], -1);
        }
        return;
    }

    // 9. スケール・黒体温度・アルファの更新
    float32_t t = saturate(p.currentTime / p.lifeTime);
    float32_t easedT = pow(t, max(0.01f, p.scaleEasing));
    float32_t currentScale = lerp(p.startScale, p.endScale, easedT);
    p.scale = float32_t3(currentScale, currentScale, currentScale);

    if ((p.flags & kParticleFlagBlackbody) != 0u) {
        p.temperature = max(0.0f, p.temperature - p.coolingRate * dt);
        // 炎のちらつきをわずかに付加
        float32_t flicker = 0.94f + 0.06f * sin(gPerFrame.time * 28.0f + p.randomSeed * 62.8f);
        p.color.rgb = BlackbodyColorRamp(p.temperature * flicker);
    }

    // 環境ラップ粒子はボックス端と出現/消滅で滑らかにフェード
    float32_t alpha = p.startAlpha;
    if ((p.flags & kParticleFlagCameraWrap) != 0u && gPerFrame.ambient.active != 0u) {
        float32_t fadeIn = saturate(t * 8.0f);
        float32_t fadeOut = saturate((1.0f - t) * 5.0f);
        float32_t3 rel = abs(p.translate - gPerFrame.ambient.center);
        float32_t maxAxis = max(rel.x, max(rel.y, rel.z));
        float32_t edgeFade = 1.0f - smoothstep(gPerFrame.ambient.halfExtent * 0.72f, gPerFrame.ambient.halfExtent, maxAxis);
        alpha = p.startAlpha * fadeIn * fadeOut * edgeFade;
    } else if ((p.flags & kParticleFlagFadeOut) != 0u) {
        // 発生直後の微小フェードインでポップを抑え、終盤へ滑らかにフェードアウト
        float32_t softIn = (p.shapeType == kShapeSmokeCloud) ? saturate(t * 10.0f) : 1.0f;
        alpha = p.startAlpha * softIn * (1.0f - t);
    }
    p.color.a = alpha;

    gParticles[particleIndex] = p;

    // 10. 2バケット Indirect Draw コンパクトリストへ振り分け
    if ((p.flags & kParticleFlagAdditive) != 0u) {
        uint32_t addIndex;
        InterlockedAdd(gIndirectArgs[5], 1u, addIndex);
        if (addIndex < kMaxParticles) {
            gAliveListAdd[addIndex] = particleIndex;
        }
    } else {
        uint32_t alphaIndex;
        InterlockedAdd(gIndirectArgs[1], 1u, alphaIndex);
        if (alphaIndex < kMaxParticles) {
            gAliveListAlpha[alphaIndex] = particleIndex;
        }
    }
}
