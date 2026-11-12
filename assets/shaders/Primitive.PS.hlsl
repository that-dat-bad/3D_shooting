// Primitive.PS.hlsl
// ライティング計算を省いた軽量なエフェクト専用ピクセルシェーダー

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET;
};

// マテリアル情報を格納する定数バッファ
struct Material
{
    float32_t4 color;
    int enableLighting; // Primitiveでは無視します（常にUnlit）
    float shininess;
    float32_t2 padding;
    matrix uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 頂点シェーダーからの入力
struct PixelInput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : WORLDPOSITION0;
};

PixelShaderOutput main(PixelInput input)
{
    PixelShaderOutput output;

    // UV変換
    float32_t4 uv = float32_t4(input.texcoord, 0.0f, 1.0f);
    uv = mul(uv, gMaterial.uvTransform);
    
    // テクスチャサンプリング
    float32_t4 texColor = gTexture.Sample(gSampler, uv.xy);
    
    // アルファテスト (半透明の滑らかなグラデーションがパキッと途切れるのを防ぐため、閾値を下げる)
    if (texColor.a < 0.001f) {
        discard;
    }

    // テクスチャの色 × マテリアルの色（エフェクトなのでシンプルに乗算するだけ）
    output.color = texColor * gMaterial.color;

    return output;
}
