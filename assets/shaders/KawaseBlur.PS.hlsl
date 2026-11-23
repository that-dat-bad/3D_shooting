#include "FullScreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer PostEffectParams : register(b0)
{
    int32_t kernelSize;
    float intensity; // Used as offset distance in Kawase blur
    float dirX;
    float dirY;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    uint32_t width, height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp((float32_t)width), rcp((float32_t)height));

    // Calculate sample offset based on intensity
    float2 dUV = uvStepSize * intensity;

    float32_t4 color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Kawase blur samples 4 points around the center using bilinear filtering
    color += gTexture.Sample(gSampler, input.texcoord + float2(-dUV.x,  dUV.y));
    color += gTexture.Sample(gSampler, input.texcoord + float2( dUV.x,  dUV.y));
    color += gTexture.Sample(gSampler, input.texcoord + float2( dUV.x, -dUV.y));
    color += gTexture.Sample(gSampler, input.texcoord + float2(-dUV.x, -dUV.y));
    
    PixelShaderOutput output;
    output.color = color * 0.25f;
    return output;
}
