#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gMaskTexture : register(t1);
SamplerState gSampler : register(s0);

cbuffer PostEffectParams : register(b0)
{
    int32_t kernelSize;   // unused for dissolve
    float threshold;      // dissolve threshold [0,1]
    float edgeWidth;      // edge glow width
    float padding;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float32_t4 sceneColor = gTexture.Sample(gSampler, input.texcoord);
    float32_t mask = gMaskTexture.Sample(gSampler, input.texcoord);

    // pixels below threshold → output black
    if (mask <= threshold)
    {
        output.color = float32_t4(0.0f, 0.0f, 0.0f, 1.0f);
        return output;
    }

    // edge glow: highlight pixels near the dissolve boundary
    float32_t edge = smoothstep(threshold, threshold + edgeWidth, mask);
    // bright edge color
    float32_t3 edgeColor = float32_t3(1.0f, 0.5f, 0.1f);
    output.color.rgb = lerp(edgeColor, sceneColor.rgb, edge);
    output.color.a = 1.0f;

    return output;
}