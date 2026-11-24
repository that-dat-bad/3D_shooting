#include "GPUParticle.hlsli"

struct PixelShaderInput {
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
    float32_t4 extraParams : TEXCOORD1; // x: lifeRatio, y: emissiveIntensity, z: softDistance, w: randomSeed
    float32_t2 motionData  : TEXCOORD2; // x: rotation, y: speed
    nointerpolation uint32_t2 metaData : TEXCOORD3; // x: shapeType, y: flags
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gDepthTexture : register(t1);
ConstantBuffer<PerView> gPerView : register(b0);
SamplerState gSampler : register(s0);

// 2D ハッシュ・プロシージャル雲ノイズ（煙雲シェイプ用）
float32_t Hash21(float32_t2 p) {
    p = frac(p * float32_t2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float32_t ValueNoise2D(float32_t2 p) {
    float32_t2 i = floor(p);
    float32_t2 f = frac(p);
    float32_t2 u = f * f * (3.0f - 2.0f * f);

    float32_t a = Hash21(i);
    float32_t b = Hash21(i + float32_t2(1.0f, 0.0f));
    float32_t c = Hash21(i + float32_t2(0.0f, 1.0f));
    float32_t d = Hash21(i + float32_t2(1.0f, 1.0f));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float32_t Fbm2D(float32_t2 p) {
    float32_t v = 0.0f;
    float32_t amp = 0.55f;
    for (int i = 0; i < 3; ++i) {
        v += amp * ValueNoise2D(p);
        p = p * 2.05f + float32_t2(7.3f, 11.9f);
        amp *= 0.5f;
    }
    return v;
}

float32_t LinearizeDepth(float32_t depth) {
    float32_t n = max(0.01f, gPerView.nearClip);
    float32_t f = max(n + 1.0f, gPerView.farClip);
    return (n * f) / max(1e-5f, (f - depth * (f - n)));
}

PixelShaderOutput main(PixelShaderInput input) {
    PixelShaderOutput output;

    float32_t2 uvCentered = input.texcoord * 2.0f - 1.0f;
    float32_t r = length(uvCentered);
    uint32_t shapeType = input.metaData.x;
    uint32_t flags = input.metaData.y;

    float32_t lifeRatio = input.extraParams.x;
    float32_t emissive = input.extraParams.y;
    float32_t softDist = input.extraParams.z;
    float32_t seed = input.extraParams.w;

    float32_t3 rgb = input.color.rgb;
    float32_t alpha = input.color.a;

    if (shapeType == kShapeSoftCircle) {
        // 既存互換＋滑らかな円形グロウ
        float32_t4 texCol = gTexture.Sample(gSampler, input.texcoord);
        rgb *= texCol.rgb;
        alpha *= texCol.a;
    } else if (shapeType == kShapeSmokeCloud) {
        // プロシージャル多層FBMボリューム煙雲（擬似法線ライティング付き）
        if (r > 1.0f) {
            discard;
        }
        float32_t2 noiseUv = uvCentered * 2.1f + float32_t2(seed * 23.7f, seed * 41.3f - lifeRatio * 0.65f);
        float32_t n = Fbm2D(noiseUv);
        float32_t radialMask = saturate(1.0f - r * r);
        float32_t billow = smoothstep(0.18f, 0.78f, radialMask * (0.65f + 0.65f * n));

        // 上方からの太陽光による擬似ボリューム陰影（上部が明るく下部が濃い）
        float32_t topLight = saturate(0.76f - uvCentered.y * 0.28f + (n - 0.5f) * 0.25f);
        rgb *= topLight;
        alpha *= billow;
    } else if (shapeType == kShapeSparkStreak) {
        // 高輝度ホットコア＋滑らかな火花ストリーク
        float32_t core = exp(-(uvCentered.x * uvCentered.x * 9.5f + uvCentered.y * uvCentered.y * 2.6f));
        float32_t halo = exp(-(uvCentered.x * uvCentered.x * 2.8f + uvCentered.y * uvCentered.y * 1.15f)) * 0.45f;
        float32_t intensity = saturate(core + halo);
        // 中心部ほど高温オレンジ〜ゴールドに発光（白飛びを抑えて鮮やかな火色を保持）
        rgb = lerp(rgb, float32_t3(1.0f, 0.88f, 0.65f), core * 0.30f);
        alpha *= intensity;
    } else if (shapeType == kShapeShockRing) {
        // 衝撃波・マッハコーンリング
        if (r > 1.0f) {
            discard;
        }
        float32_t ringCenter = 0.72f;
        float32_t ringWidth = 0.18f;
        float32_t band = exp(-pow((r - ringCenter) / ringWidth, 2.0f));
        float32_t innerGlow = smoothstep(0.0f, ringCenter, r) * (1.0f - smoothstep(ringCenter, 1.0f, r)) * 0.28f;
        float32_t shimmer = 0.85f + 0.15f * sin(atan2(uvCentered.y, uvCentered.x) * 6.0f + gPerView.time * 12.0f + seed * 6.28f);
        float32_t mask = saturate((band + innerGlow) * shimmer);
        alpha *= mask;
    } else if (shapeType == kShapeSnowCrystal) {
        // きらめく極地氷晶・雪の結晶
        if (r > 1.0f) {
            discard;
        }
        float32_t softCore = exp(-r * r * 5.2f);
        // 6方向スター型グリント
        float32_t angle = atan2(uvCentered.y, uvCentered.x);
        float32_t star = pow(abs(cos(angle * 3.0f)), 6.0f) * exp(-r * 2.8f) * 0.65f;
        float32_t twinkle = 0.70f + 0.30f * sin(gPerView.time * 7.5f + seed * 43.0f);
        float32_t mask = saturate((softCore + star) * twinkle);
        rgb = lerp(rgb, float32_t3(1.0f, 1.0f, 1.0f), softCore * 0.6f);
        alpha *= mask;
    } else if (shapeType == kShapeChaffFoil) {
        // 3Dタンブリング反射を擬似表現する金属チャフ片
        float32_t tumble = cos(gPerView.time * (9.0f + seed * 10.0f) + seed * 62.8f);
        float32_t halfWidth = 0.22f + 0.58f * abs(tumble);
        float32_t halfHeight = 0.82f;
        if (abs(uvCentered.x) > halfWidth || abs(uvCentered.y) > halfHeight) {
            discard;
        }
        float32_t edgeSoft = smoothstep(halfWidth, halfWidth * 0.65f, abs(uvCentered.x)) *
                             smoothstep(halfHeight, halfHeight * 0.72f, abs(uvCentered.y));
        // 平面が太陽角を捉えた瞬間に強烈な金属グリントを発光
        float32_t glint = pow(abs(tumble), 8.0f);
        rgb = lerp(rgb * 0.55f, float32_t3(1.0f, 0.96f, 0.88f) * 2.2f, glint);
        alpha *= edgeSoft * (0.45f + 0.55f * glint);
    } else if (shapeType == kShapeVaporMist) {
        // 輪郭の出ない薄い水蒸気凝縮ベール（異方性気流FBM＋柔らかなガウス減衰）
        if (r > 1.0f) {
            discard;
        }
        float32_t gauss = exp(-r * r * 3.1f) * saturate(1.0f - r * r);
        float32_t2 mistUv = float32_t2(uvCentered.x * 1.8f, uvCentered.y * 3.4f) +
                            float32_t2(seed * 19.3f, seed * 37.1f - lifeRatio * 1.4f);
        float32_t wispy = lerp(0.55f, 1.15f, Fbm2D(mistUv));
        rgb *= saturate(0.90f - uvCentered.y * 0.14f);
        alpha *= saturate(gauss * wispy);
    } else if (shapeType == kShapeFlareCore) {
        // マグネシウムフレアの光学ピンポイント白熱芯＋オレンジコロナ＋スターバースト光条
        if (r > 1.0f) {
            discard;
        }
        float32_t flicker = 0.88f + 0.12f * sin(gPerView.time * 34.0f + seed * 62.8f);
        float32_t pinpoint = exp(-r * r * 52.0f);
        float32_t corona = exp(-r * r * 9.5f) * 0.85f;
        float32_t halo = exp(-r * r * 2.4f) * 0.34f * saturate(1.0f - r);

        float32_t starCross = exp(-abs(uvCentered.y) * 28.0f) * exp(-abs(uvCentered.x) * 3.6f) +
                              exp(-abs(uvCentered.x) * 28.0f) * exp(-abs(uvCentered.y) * 3.6f);
        float32_t starGlare = starCross * 0.38f * saturate(1.0f - r);

        float32_t3 deepOrange = rgb * float32_t3(1.0f, 0.58f, 0.25f);
        float32_t3 hotCore = float32_t3(1.0f, 0.93f, 0.76f);
        float32_t weight = saturate((pinpoint + corona + halo + starGlare) * flicker);
        rgb = lerp(deepOrange, rgb, saturate(corona + starGlare));
        rgb = lerp(rgb, hotCore, saturate(pinpoint * 0.85f));
        alpha *= weight;
    }

    // 深度バッファ参照ソフトパーティクル（地形・機体との硬い境界線を解消）
    if ((flags & kParticleFlagSoftParticle) != 0u && gPerView.hasDepthTexture != 0u) {
        float32_t bgDepth = gDepthTexture.Load(int3(input.position.xy, 0));
        if (bgDepth < 0.99999f) {
            float32_t linearBg = LinearizeDepth(bgDepth);
            float32_t linearPart = LinearizeDepth(input.position.z);
            float32_t depthDiff = max(0.0f, linearBg - linearPart);
            float32_t softFade = saturate(depthDiff / max(0.1f, softDist));
            alpha *= softFade;
        }
    }

    if (alpha <= 0.002f) {
        discard;
    }

    output.color = float32_t4(rgb * emissive, saturate(alpha));
    return output;
}
