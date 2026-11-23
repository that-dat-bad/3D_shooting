#include "Particle.hlsli"

struct VSInput
{
    float32_t4 position : POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

VSOutput main(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    // StructuredBufferから、現在のインスタンスに対応するデータを取得
    ParticleForGPU particle = gParticleData[instanceId];

    // 座標変換
    output.svpos = mul(input.position, particle.WVP);
    
    // UV座標はそのまま
    output.texcoord = input.texcoord;
    
    // 色情報をピクセルシェーダーへ渡す
    output.color = particle.color;

    return output;
}