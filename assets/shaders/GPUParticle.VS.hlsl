#include "GPUParticle.hlsli"

struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

struct VertexShaderOutput {
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
    float32_t4 extraParams : TEXCOORD1; // x: lifeRatio, y: emissiveIntensity, z: softDistance, w: randomSeed
    float32_t2 motionData  : TEXCOORD2; // x: rotation, y: speed
    nointerpolation uint32_t2 metaData : TEXCOORD3; // x: shapeType, y: flags
};

StructuredBuffer<ParticleCS> gParticles : register(t0);
StructuredBuffer<uint32_t> gAliveList : register(t1);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID) {
    VertexShaderOutput output;

    uint32_t particleIndex = gAliveList[instanceId];
    ParticleCS particle = gParticles[particleIndex];

    float32_t3 xAxis = gPerView.billboardMatrix[0].xyz;
    float32_t3 yAxis = gPerView.billboardMatrix[1].xyz;

    float32_t speed = length(particle.velocity);
    float32_t2 localPos = input.position.xy;

    if ((particle.flags & kParticleFlagStretched) != 0u && speed > 0.05f) {
        // GPU 速度方向ストレッチビルボード
        float32_t3 camToP = particle.translate - gPerView.cameraPosition;
        float32_t camDist = length(camToP);
        if (camDist > 0.001f) {
            float32_t3 viewDir = camToP / camDist;
            float32_t3 velDir = particle.velocity / speed;
            // 速度ベクトルを視線に垂直なスクリーン平面へ投影
            float32_t3 projVel = velDir - viewDir * dot(velDir, viewDir);
            float32_t projLen = length(projVel);
            if (projLen > 0.001f) {
                float32_t3 upAxis = projVel / projLen;
                float32_t3 rightAxis = normalize(cross(viewDir, upAxis));
                xAxis = rightAxis;
                yAxis = upAxis;
            }
        }

        // SI単位と旧フレーム単位の双方で自然な伸縮長に調整
        float32_t worldSpeed = ((particle.flags & kParticleFlagWorldUnitsPerSec) != 0u) ? speed : (speed * 60.0f);
        float32_t stretchLen = clamp(1.0f + worldSpeed * particle.stretchFactor, 1.0f, 32.0f);
        localPos.x *= particle.scale.x;
        localPos.y *= particle.scale.y * stretchLen;
    } else {
        float32_t c = cos(particle.rotation);
        float32_t s = sin(particle.rotation);
        float32_t2 rotatedLocal = float32_t2(
            localPos.x * c - localPos.y * s,
            localPos.x * s + localPos.y * c
        );
        localPos.x = rotatedLocal.x * particle.scale.x;
        localPos.y = rotatedLocal.y * particle.scale.y;
    }

    float32_t3 worldPos = particle.translate + xAxis * localPos.x + yAxis * localPos.y;

    output.position = mul(float32_t4(worldPos, 1.0f), gPerView.viewProjection);
    output.texcoord = input.texcoord;
    output.color = particle.color;

    float32_t lifeRatio = (particle.lifeTime > 0.0001f) ? saturate(particle.currentTime / particle.lifeTime) : 1.0f;
    output.extraParams = float32_t4(lifeRatio, particle.emissiveIntensity, particle.softDistance, particle.randomSeed);
    output.motionData = float32_t2(particle.rotation, speed);
    output.metaData = uint32_t2(particle.shapeType, particle.flags);

    return output;
}
