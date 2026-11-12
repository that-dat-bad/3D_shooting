struct PixelShaderInput {
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(PixelShaderInput input) {
    PixelShaderOutput output;
    
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = textureColor * input.color;
    
    // alphaが0なら破棄（不要な場合もありますが、アルファテスト用）
    if (output.color.a == 0.0f) {
        discard;
    }
    
    return output;
}
