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
    uint32_t padding;
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
};

struct PerFrame {
    float32_t time;
    float32_t deltaTime;
    uint32_t requestCount;
    uint32_t totalEmitCount;
};

struct PerView {
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
};

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

static const uint32_t kMaxParticles = 65536u;
static const uint32_t kMaxEmitRequests = 64u;
static const uint32_t kParticleFlagFadeOut = 1u;
