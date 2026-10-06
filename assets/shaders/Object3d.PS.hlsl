

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET;
};

// マテリアル情報を格納する定数バッファ
struct Material
{
    float32_t4 color;
    int enableLighting;
    float shininess;
    float environmentCoefficient;
    float specularIntensity; // 反射強度
    float32_t3 emissiveColor; // 自発光色
    float emissiveIntensity;  // 自発光強度
    matrix uvTransform;
};

// 平行光源の情報を格納する定数バッファ
struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

// ライティングモデルを選択するための定数バッファ
struct LightingSettings
{
    int shadingModel;  // 0: Lambert, 1: Half-Lambert
    int specularModel; // 0: None, 1: Phong, 2: Blinn-Phong
    int lightType;     // 0: Directional, 1: Point, 2: Both
    float padding;
    float32_t3 cameraPosition;
    float emissiveIntensityScale; // メッシュ自発光の全体強度スケール
};

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    float decay;
    float32_t2 padding; 
};

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float32_t3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
    float padding;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<LightingSettings> gLightingSettings : register(b3);
ConstantBuffer<PointLight> gPointLight : register(b4);
ConstantBuffer<SpotLight> gSpotLight : register(b5);

Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvTexture : register(t1); // 環境マップ用テクスチャキューブ
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

    float32_t4 texColor = float32_t4(1.0, 1.0, 1.0, 1.0);
    
    // Texture Sampling & Alpha Test
    float32_t4 uv = float32_t4(input.texcoord, 0.0f, 1.0f);
    uv = mul(uv, gMaterial.uvTransform);
    texColor = gTexture.Sample(gSampler, uv.xy);
    if (texColor.a < 0.5f) {
        discard;
    }

    // ライティングが有効な場合
    if (gMaterial.enableLighting != 0)
    {
        float32_t3 N = normalize(input.normal);
        float32_t3 toEye = normalize(gLightingSettings.cameraPosition - input.worldPosition);

        // 金属的な反射色を計算（ベースカラーを80%染める）
        float32_t3 specColor = lerp(float32_t3(1.0f, 1.0f, 1.0f), texColor.rgb, 0.8f) * gMaterial.specularIntensity;

        float32_t3 totalDiffuse = float32_t3(0.0f, 0.0f, 0.0f);
        float32_t3 totalSpecular = float32_t3(0.0f, 0.0f, 0.0f);

        // --- Directional Light Calculation ---
        if (gLightingSettings.lightType & 1)
        {
            float32_t3 dirLightL = normalize(-gDirectionalLight.direction); 
            float dirNdotL = dot(N, dirLightL);
            float dirCos = saturate(dirNdotL);

            // Half-Lambert
            if (gLightingSettings.shadingModel == 1)
            {
                dirCos = dirNdotL * 0.5f + 0.5f;
                dirCos = dirCos * dirCos;
            }

            float32_t3 dirDiffuse = gMaterial.color.rgb * texColor.rgb * gDirectionalLight.color.rgb * dirCos * gDirectionalLight.intensity;
            float32_t3 dirSpecular = float32_t3(0.0f, 0.0f, 0.0f);

            if (gLightingSettings.specularModel > 0)
            {
                 if (dirNdotL > 0.0f || gLightingSettings.shadingModel == 1) 
                 {
                     float specularPow = 0.0f;
                     if (gLightingSettings.specularModel == 1) // Phong
                     {
                         float32_t3 reflectLight = reflect(gDirectionalLight.direction, N);
                         float RdotE = dot(reflectLight, toEye);
                         specularPow = pow(saturate(RdotE), gMaterial.shininess);
                     }
                     else if (gLightingSettings.specularModel == 2) // Blinn-Phong
                     {
                         float32_t3 halfVector = normalize(dirLightL + toEye);
                         float NDotH = dot(N, halfVector);
                         specularPow = pow(saturate(NDotH), gMaterial.shininess);
                     }
                      dirSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * specColor;
                 }
            }
            totalDiffuse += dirDiffuse;
            totalSpecular += dirSpecular;
        }

        // --- Point Light Calculation ---
        if (gLightingSettings.lightType & 2)
        {
            float32_t3 pointLightL = normalize(gPointLight.position - input.worldPosition);
            float pointLightDistance = length(gPointLight.position - input.worldPosition);
            
            // Attenuation
            float pointLightFactor = pow(saturate(1.0f - pointLightDistance / gPointLight.radius), gPointLight.decay);
            
            float pointNdotL = dot(N, pointLightL);
            float pointCos = saturate(pointNdotL);
            
            if (gLightingSettings.shadingModel == 1)
            {
                pointCos = pointNdotL * 0.5f + 0.5f;
                pointCos = pointCos * pointCos;
            }

            float32_t3 pointDiffuse = gMaterial.color.rgb * texColor.rgb * gPointLight.color.rgb * pointCos * gPointLight.intensity * pointLightFactor;
            float32_t3 pointSpecular = float32_t3(0.0f, 0.0f, 0.0f);

            if (gLightingSettings.specularModel > 0)
            {
                 if (pointNdotL > 0.0f || gLightingSettings.shadingModel == 1) 
                 {
                     float specularPow = 0.0f;
                     if (gLightingSettings.specularModel == 1) // Phong
                     {
                         float32_t3 incident = -pointLightL;
                         float32_t3 reflectLight = reflect(incident, N);
                         float RdotE = dot(reflectLight, toEye);
                         specularPow = pow(saturate(RdotE), gMaterial.shininess);
                     }
                     else if (gLightingSettings.specularModel == 2) // Blinn-Phong
                     {
                         float32_t3 halfVector = normalize(pointLightL + toEye);
                         float NDotH = dot(N, halfVector);
                         specularPow = pow(saturate(NDotH), gMaterial.shininess);
                     }
                      pointSpecular = gPointLight.color.rgb * gPointLight.intensity * pointLightFactor * specularPow * specColor;
                 }
            }
            totalDiffuse += pointDiffuse;
            totalSpecular += pointSpecular;
        }

        // --- Spot Light Calculation ---
        if (gLightingSettings.lightType & 4)
        {
             float32_t3 spotDir = normalize(gSpotLight.direction);
             float32_t3 L = normalize(gSpotLight.position - input.worldPosition);
             float distance = length(gSpotLight.position - input.worldPosition);
              
             float distAttenuation = pow(saturate(1.0f - distance / gSpotLight.distance), gSpotLight.decay);
              
             float32_t3 directionOnSurface = normalize(input.worldPosition - gSpotLight.position);
             float cosAngle = dot(directionOnSurface, spotDir);
             
             float falloffFactor = 0.0f;
             float cosDiff = gSpotLight.cosFalloffStart - gSpotLight.cosAngle;
             // Avoid divide by zero
             if (abs(cosDiff) > 0.0001f) {
                 falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / cosDiff);
             } else if (cosAngle >= gSpotLight.cosAngle) {
                 falloffFactor = 1.0f; // Simplified handling
             }
              
             float totalData = distAttenuation * falloffFactor * gSpotLight.intensity;
             
             float NdotL = dot(N, L);
             float cos = saturate(NdotL);
              
             if (gLightingSettings.shadingModel == 1)
             {
                 cos = NdotL * 0.5f + 0.5f;
                 cos = cos * cos;
             }
 
             float32_t3 spotDiffuse = gMaterial.color.rgb * texColor.rgb * gSpotLight.color.rgb * cos * totalData;
             float32_t3 spotSpecular = float32_t3(0.0f, 0.0f, 0.0f);
 
             if (gLightingSettings.specularModel > 0)
             {
                  if (NdotL > 0.0f || gLightingSettings.shadingModel == 1) 
                  {
                      float specularPow = 0.0f;
                      if (gLightingSettings.specularModel == 1) // Phong
                      {
                          float32_t3 incident = -L;
                          float32_t3 reflectLight = reflect(incident, N);
                          float RdotE = dot(reflectLight, toEye);
                          specularPow = pow(saturate(RdotE), gMaterial.shininess);
                      }
                      else if (gLightingSettings.specularModel == 2) // Blinn-Phong
                      {
                          float32_t3 halfVector = normalize(L + toEye);
                          float NDotH = dot(N, halfVector);
                          specularPow = pow(saturate(NDotH), gMaterial.shininess);
                      }
                       spotSpecular = gSpotLight.color.rgb * totalData * specularPow * specColor;
                  }
             }
             totalDiffuse += spotDiffuse;
             totalSpecular += spotSpecular;
        }

        // 環境マップ (Environment Mapping) の計算
        float32_t3 envColor = float32_t3(0.0f, 0.0f, 0.0f);
        if (gMaterial.environmentCoefficient > 0.0f)
        {
            float32_t3 reflectVector = reflect(-toEye, N); // 視線からサーフェスへのベクトルの反射
            float32_t3 baseEnv = gEnvTexture.Sample(gSampler, reflectVector).rgb;
            
            // 環境反射光もベースカラーで染める (70% tint)
            float32_t3 envTint = lerp(float32_t3(1.0f, 1.0f, 1.0f), texColor.rgb, 0.7f);
            envColor = baseEnv * envTint * gMaterial.environmentCoefficient * gMaterial.specularIntensity;

            // 暗い場所（影）で光って見える現象をごまかすハック
            // 主光源（平行光源）の当たり具合を利用して、光が当たりにくい面の反射を暗くする
            if (gLightingSettings.lightType & 1)
            {
                float32_t3 dirLightL = normalize(-gDirectionalLight.direction);
                float NdotL = dot(N, dirLightL);
                
                // 完全に光の裏側であっても少しは反射させるため、0.2程度の下限を設ける
                float shadowFactor = saturate(NdotL * 0.5f + 0.5f);
                shadowFactor = max(shadowFactor, 0.2f); 
                
                envColor *= shadowFactor;
            }
        }

        // 自発光（メッシュ自体の発光）
        float32_t3 emissive = gMaterial.emissiveColor * gMaterial.emissiveIntensity * gLightingSettings.emissiveIntensityScale;

        // Combine
        output.color.rgb = totalDiffuse + totalSpecular + envColor + emissive;
        output.color.a = gMaterial.color.a * texColor.a;
    }
    else
    {
        // ライティング無効時
        float32_t3 emissive = gMaterial.emissiveColor * gMaterial.emissiveIntensity * gLightingSettings.emissiveIntensityScale;
        output.color.rgb = (texColor.rgb * gMaterial.color.rgb) + emissive;
        output.color.a = gMaterial.color.a * texColor.a;
    }

    return output;
}
