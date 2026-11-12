struct ParticleCS {
    float32_t3 translate;
    float32_t padding1; // 16-byte alignment
    float32_t3 scale;
    float32_t lifeTime; // 16-byte alignment
    float32_t3 velocity;
    float32_t currentTime; // 16-byte alignment
    float32_t4 color;
};

struct EmitterSphere {
    float32_t3 translate;
    float32_t radius;
    uint32_t count;
    float32_t frequency;
    float32_t frequencyTime;
    uint32_t emit;
};

struct PerFrame {
    float32_t time;
    float32_t deltaTime;
};

struct PerView {
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
};

// 乱数生成用
float rand2dTo1d(float2 value, float2 dotDir = float2(12.9898, 78.233)) {
    float2 smallValue = sin(value);
    float random = dot(smallValue, dotDir);
    random = frac(sin(random) * 143758.5453);
    return random;
}

float32_t3 rand3dTo3d(float32_t3 value) {
    return float32_t3(
        rand2dTo1d(value.xy, float2(12.989, 78.233)),
        rand2dTo1d(value.yz, float2(39.346, 11.135)),
        rand2dTo1d(value.zx, float2(73.156, 52.235))
    );
}

class RandomGenerator {
    float32_t3 seed;
    float32_t3 Generate3d() {
        seed = rand3dTo3d(seed);
        return seed;
    }
    float32_t Generate1d() {
        float32_t result = rand2dTo1d(seed.xy);
        seed.x = result;
        return result;
    }
};

static const uint32_t kMaxParticles = 1024;
