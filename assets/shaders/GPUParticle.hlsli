// ============================================================================
// GPUParticle.hlsli
// GPU駆動パーティクルシステム共通構造体・ノイズ・黒体放射・ウェイク場ライブラリ
// ============================================================================

struct ParticleCS {
    float32_t3 translate;
    float32_t rotation;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float32_t3 acceleration;
    float32_t rotationSpeed;
    float32_t4 color;
    float32_t startScale;
    float32_t endScale;
    float32_t scaleEasing;
    float32_t startAlpha;
    float32_t spawnY;
    float32_t maxRiseHeight;
    uint32_t flags;
    uint32_t shapeType;
    float32_t drag;
    float32_t curlStrength;
    float32_t curlFrequency;
    float32_t stretchFactor;
    float32_t temperature;
    float32_t coolingRate;
    float32_t floorY;
    float32_t restitution;
    float32_t emissiveIntensity;
    float32_t softDistance;
    float32_t randomSeed;
    float32_t wakeSensitivity;
};

struct GpuEmitRequest {
    float32_t3 origin;
    float32_t randomPositionRange;
    float32_t3 minVelocity;
    float32_t minLifeTime;
    float32_t3 maxVelocity;
    float32_t maxLifeTime;
    float32_t3 acceleration;
    float32_t maxRiseHeight;
    float32_t4 minColor;
    float32_t4 maxColor;
    float32_t minScale;
    float32_t maxScale;
    float32_t endScale;
    float32_t scaleEasing;
    float32_t minRotation;
    float32_t maxRotation;
    float32_t minRotationSpeed;
    float32_t maxRotationSpeed;
    uint32_t startOffset;
    uint32_t count;
    uint32_t flags;
    uint32_t seedSalt;
    uint32_t shapeType;
    float32_t drag;
    float32_t curlStrength;
    float32_t curlFrequency;
    float32_t stretchFactor;
    float32_t minTemperature;
    float32_t maxTemperature;
    float32_t coolingRate;
    float32_t floorY;
    float32_t restitution;
    float32_t emissiveIntensity;
    float32_t softDistance;
    float32_t3 emitAxis;
    float32_t radialSpeedMin;
    float32_t radialSpeedMax;
    float32_t ringRadius;
    float32_t wakeSensitivity;
    float32_t padding0;
};

struct GpuWakeField {
    float32_t3 position;
    float32_t halfWingspan;
    float32_t3 forward;
    float32_t vortexStrength;
    float32_t3 right;
    float32_t exhaustThrust;
    float32_t3 velocity;
    uint32_t active;
};

struct GpuAmbientVolume {
    float32_t3 center;
    float32_t halfExtent;
    float32_t3 windVelocity;
    uint32_t active;
};

struct PerFrame {
    float32_t time;
    float32_t deltaTime;
    uint32_t requestCount;
    uint32_t totalEmitCount;
    GpuAmbientVolume ambient;
    GpuWakeField wakeFields[4];
    float32_t defaultFloorY;
    uint32_t activeWakeCount;
    float32_t globalCurlTimeScale;
    float32_t pad0;
};

struct PerView {
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
    float32_t3 cameraPosition;
    float32_t nearClip;
    float32_t farClip;
    float32_t time;
    uint32_t hasDepthTexture;
    float32_t pad0;
};

static const uint32_t kMaxParticles = 65536u;
static const uint32_t kMaxEmitRequests = 512u;
static const uint32_t kMaxWakeFields = 4u;

static const uint32_t kParticleFlagFadeOut          = 1u << 0;
static const uint32_t kParticleFlagAdditive         = 1u << 1;
static const uint32_t kParticleFlagCurlNoise        = 1u << 2;
static const uint32_t kParticleFlagBlackbody        = 1u << 3;
static const uint32_t kParticleFlagStretched        = 1u << 4;
static const uint32_t kParticleFlagFloorBounce      = 1u << 5;
static const uint32_t kParticleFlagWakeInteract     = 1u << 6;
static const uint32_t kParticleFlagCameraWrap       = 1u << 7;
static const uint32_t kParticleFlagSoftParticle     = 1u << 8;
static const uint32_t kParticleFlagSphereEmit       = 1u << 9;
static const uint32_t kParticleFlagRingEmit         = 1u << 10;
static const uint32_t kParticleFlagWorldUnitsPerSec = 1u << 11;

static const uint32_t kShapeSoftCircle  = 0u;
static const uint32_t kShapeSmokeCloud  = 1u;
static const uint32_t kShapeSparkStreak = 2u;
static const uint32_t kShapeShockRing   = 3u;
static const uint32_t kShapeSnowCrystal = 4u;
static const uint32_t kShapeChaffFoil   = 5u;
static const uint32_t kShapeVaporMist   = 6u;
static const uint32_t kShapeFlareCore   = 7u;

// ============================================================================
// PCG 疑似乱数ジェネレータ
// ============================================================================
uint PcgHash(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float32_t Rand01(inout uint state) {
    state = PcgHash(state);
    return (float32_t)(state & 0x00FFFFFFu) / 16777216.0f;
}

float32_t3 Rand3(inout uint state) {
    return float32_t3(Rand01(state), Rand01(state), Rand01(state));
}

float32_t3 RandUnitSphere(inout uint state) {
    float32_t z = Rand01(state) * 2.0f - 1.0f;
    float32_t phi = Rand01(state) * 6.2831853f;
    float32_t r = sqrt(max(0.0f, 1.0f - z * z));
    return float32_t3(r * cos(phi), z, r * sin(phi));
}

// ============================================================================
// 3D ハッシュ・スムースバリューノイズ & 発散なし 3D カールノイズ
// ============================================================================
float32_t Hash31(float32_t3 p) {
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float32_t ValueNoise3D(float32_t3 p) {
    float32_t3 i = floor(p);
    float32_t3 f = frac(p);
    // クインティック補間（C2連続でカール微分が滑らか）
    float32_t3 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    float32_t n000 = Hash31(i + float32_t3(0.0f, 0.0f, 0.0f));
    float32_t n100 = Hash31(i + float32_t3(1.0f, 0.0f, 0.0f));
    float32_t n010 = Hash31(i + float32_t3(0.0f, 1.0f, 0.0f));
    float32_t n110 = Hash31(i + float32_t3(1.0f, 1.0f, 0.0f));
    float32_t n001 = Hash31(i + float32_t3(0.0f, 0.0f, 1.0f));
    float32_t n101 = Hash31(i + float32_t3(1.0f, 0.0f, 1.0f));
    float32_t n011 = Hash31(i + float32_t3(0.0f, 1.0f, 1.0f));
    float32_t n111 = Hash31(i + float32_t3(1.0f, 1.0f, 1.0f));

    float32_t nx00 = lerp(n000, n100, u.x);
    float32_t nx10 = lerp(n010, n110, u.x);
    float32_t nx01 = lerp(n001, n101, u.x);
    float32_t nx11 = lerp(n011, n111, u.x);

    float32_t nxy0 = lerp(nx00, nx10, u.y);
    float32_t nxy1 = lerp(nx01, nx11, u.y);

    return lerp(nxy0, nxy1, u.z) * 2.0f - 1.0f;
}

float32_t3 PotentialField3D(float32_t3 p) {
    // 2オクターブのベクトルポテンシャル場（乱流の大小渦を表現）
    float32_t3 p1 = p;
    float32_t3 p2 = p * 2.07f + float32_t3(13.7f, -9.2f, 21.4f);

    float32_t3 o1 = float32_t3(
        ValueNoise3D(p1),
        ValueNoise3D(p1 + float32_t3(31.4f, 17.2f, -11.8f)),
        ValueNoise3D(p1 + float32_t3(-19.5f, 43.1f, 27.6f))
    );
    float32_t3 o2 = float32_t3(
        ValueNoise3D(p2),
        ValueNoise3D(p2 + float32_t3(31.4f, 17.2f, -11.8f)),
        ValueNoise3D(p2 + float32_t3(-19.5f, 43.1f, 27.6f))
    );
    return o1 + o2 * 0.45f;
}

// 発散ゼロの3Dカールノイズベクトル場（∇ × Ψ）
float32_t3 ComputeCurlNoise3D(float32_t3 pos, float32_t timeOffset) {
    float32_t3 p = pos + float32_t3(0.0f, -timeOffset * 0.45f, timeOffset * 0.25f);
    const float32_t eps = 0.08f;
    const float32_t inv2Eps = 1.0f / (2.0f * eps);

    float32_t3 dx = (PotentialField3D(p + float32_t3(eps, 0.0f, 0.0f)) - PotentialField3D(p - float32_t3(eps, 0.0f, 0.0f))) * inv2Eps;
    float32_t3 dy = (PotentialField3D(p + float32_t3(0.0f, eps, 0.0f)) - PotentialField3D(p - float32_t3(0.0f, eps, 0.0f))) * inv2Eps;
    float32_t3 dz = (PotentialField3D(p + float32_t3(0.0f, 0.0f, eps)) - PotentialField3D(p - float32_t3(0.0f, 0.0f, eps))) * inv2Eps;

    return float32_t3(
        dy.z - dz.y,
        dz.x - dx.z,
        dx.y - dy.x
    );
}

// ============================================================================
// 物理ベース黒体放射カラーランプ（温度 0.0〜1.2 → 深紅→橙→黄→白熱→青白プラズマ）
// ============================================================================
float32_t3 BlackbodyColorRamp(float32_t temp) {
    float32_t t = clamp(temp, 0.0f, 1.35f);

    float32_t3 cAsh    = float32_t3(0.10f, 0.05f, 0.04f); // 炭化・冷えた残り火
    float32_t3 cRed    = float32_t3(0.95f, 0.12f, 0.02f); // ~1100K 深紅
    float32_t3 cOrange = float32_t3(1.00f, 0.42f, 0.05f); // ~2000K 爆炎オレンジ
    float32_t3 cYellow = float32_t3(1.00f, 0.85f, 0.35f); // ~3800K 高温イエロー
    float32_t3 cWhite  = float32_t3(1.00f, 0.98f, 0.92f); // ~6500K 白熱
    float32_t3 cPlasma = float32_t3(0.82f, 0.92f, 1.00f); // ~11000K 青白閃光

    if (t < 0.18f) {
        return lerp(cAsh, cRed, saturate(t / 0.18f));
    } else if (t < 0.42f) {
        return lerp(cRed, cOrange, saturate((t - 0.18f) / 0.24f));
    } else if (t < 0.68f) {
        return lerp(cOrange, cYellow, saturate((t - 0.42f) / 0.26f));
    } else if (t < 0.95f) {
        return lerp(cYellow, cWhite, saturate((t - 0.68f) / 0.27f));
    } else {
        return lerp(cWhite, cPlasma, saturate((t - 0.95f) / 0.40f));
    }
}

// ============================================================================
// 航空機ウェイクベクトル場（ランブ＝オゼーン翼端渦 ＋ ジェット排気ブラスト）
// ============================================================================
float32_t3 EvaluateAircraftWakeForce(float32_t3 worldPos, GpuWakeField wake) {
    if (wake.active == 0u) {
        return float32_t3(0.0f, 0.0f, 0.0f);
    }

    float32_t3 fwd = normalize(wake.forward);
    float32_t3 right = normalize(wake.right);
    float32_t3 up = normalize(cross(fwd, right));

    float32_t3 rel = worldPos - wake.position;
    float32_t along = dot(rel, fwd);      // 機首方向が正、後流が負
    float32_t lateral = dot(rel, right);  // 右翼方向が正
    float32_t vertical = dot(rel, up);    // 機体上方向が正

    // 影響範囲外の高速カリング
    if (along > 22.0f || along < -130.0f) {
        return float32_t3(0.0f, 0.0f, 0.0f);
    }
    if (abs(lateral) > 48.0f || abs(vertical) > 42.0f) {
        return float32_t3(0.0f, 0.0f, 0.0f);
    }

    float32_t3 totalAccel = float32_t3(0.0f, 0.0f, 0.0f);

    // 1. 機首バウウェーブ（機体通過時の押しのけ気流）
    if (along > -6.0f && along < 14.0f) {
        float32_t3 radialVec = right * lateral + up * vertical;
        float32_t r2 = dot(radialVec, radialVec) + 2.5f;
        float32_t bowEnvelope = exp(-r2 * 0.045f) * saturate(1.0f - abs(along - 3.0f) / 11.0f);
        float32_t rLen = max(0.25f, sqrt(r2));
        totalAccel += (radialVec / rLen) * (bowEnvelope * 38.0f);
    }

    // 2. 左右翼端トレイル渦（Lamb-Oseen 反回転ペア渦）
    if (along < 4.0f && wake.vortexStrength > 0.01f) {
        float32_t distBehind = max(0.0f, -along);
        // 後方に流れるほど渦コア半径がわずかに広がり、強度が減衰
        float32_t coreRadius = 2.6f + distBehind * 0.045f;
        float32_t coreR2 = coreRadius * coreRadius;
        float32_t downstreamFade = exp(-distBehind * 0.014f) * saturate((4.0f - along) * 0.25f);

        // 左翼端渦（-halfWingspan）: 下面高圧→翼端外側へ巻き上がり、翼端内側はダウンウォッシュ
        {
            float32_t dx = lateral + wake.halfWingspan;
            float32_t dy = vertical;
            float32_t r2 = dx * dx + dy * dy + 0.15f;
            float32_t tangential = wake.vortexStrength * downstreamFade * (1.0f - exp(-r2 / coreR2)) / max(1.2f, sqrt(r2));
            // 翼端外側(dx<0)は上昇(+up)、内側(dx>0)はダウンウォッシュ(-up)、上面(dy>0)は内側(+right)
            float32_t3 swirlDir = normalize(right * (dy) + up * (-dx) + float32_t3(1e-5f, 0.0f, 0.0f));
            totalAccel += swirlDir * tangential;
        }

        // 右翼端渦（+halfWingspan）: 対称の逆回転
        {
            float32_t dx = lateral - wake.halfWingspan;
            float32_t dy = vertical;
            float32_t r2 = dx * dx + dy * dy + 0.15f;
            float32_t tangential = wake.vortexStrength * downstreamFade * (1.0f - exp(-r2 / coreR2)) / max(1.2f, sqrt(r2));
            // 翼端外側(dx>0)は上昇(+up)、内側(dx<0)はダウンウォッシュ(-up)、上面(dy>0)は内側(-right)
            float32_t3 swirlDir = normalize(right * (-dy) + up * (dx) + float32_t3(1e-5f, 0.0f, 0.0f));
            totalAccel += swirlDir * tangential;
        }
    }

    // 3. ジェット排気ブラストコーン（ノズル後方の強烈な吹き飛ばし）
    if (along < -2.0f && along > -75.0f && wake.exhaustThrust > 0.01f) {
        float32_t distBehind = -along - 2.0f;
        float32_t coneRadius = 2.4f + distBehind * 0.14f;
        float32_t r2 = lateral * lateral + vertical * vertical;
        float32_t radialMask = exp(-r2 / max(1.0f, coneRadius * coneRadius));
        float32_t axialMask = exp(-distBehind * 0.035f);
        float32_t blast = wake.exhaustThrust * radialMask * axialMask;

        float32_t3 radialOut = right * lateral + up * vertical;
        float32_t radialLen = length(radialOut);
        float32_t3 expandDir = (radialLen > 0.05f) ? (radialOut / radialLen) : up;

        totalAccel += (-fwd * 1.0f + expandDir * 0.38f) * blast;
    }

    return totalAccel;
}
