#pragma once
#include <cstdint>
#include "../../base/Math/MyMath.h"

/// <summary>
/// GPU駆動パーティクルシステム共通定数・フラグ・構造体定義
/// （HLSL: assets/shaders/GPUParticle.hlsli とバイナリレイアウト完全一致）
/// </summary>
namespace GPUParticleConstants {
	inline constexpr uint32_t kMaxParticles = 65536u;
	inline constexpr uint32_t kMaxEmitRequests = 512u;
	inline constexpr uint32_t kMaxWakeFields = 4u;
	inline constexpr uint32_t kInitThreadGroupSize = 256u;
	inline constexpr uint32_t kEmitThreadGroupSize = 64u;
	inline constexpr uint32_t kUpdateThreadGroupSize = 256u;
	inline constexpr size_t kPerViewCbAlignment = 256u;
	inline constexpr size_t kPerFrameCbAlignment = 512u;
}

/// <summary>
/// GPUパーティクル挙動フラグ（ビットマスク）
/// </summary>
namespace GPUParticleFlags {
	inline constexpr uint32_t kNone             = 0u;
	inline constexpr uint32_t kFadeOut          = 1u << 0;  // 寿命に応じてアルファフェードアウト（既存互換）
	inline constexpr uint32_t kAdditive         = 1u << 1;  // 加算HDRバケットへ振り分け（未設定時は通常アルファバケット）
	inline constexpr uint32_t kCurlNoise        = 1u << 2;  // 3Dカールノイズ乱流フィールドを適用
	inline constexpr uint32_t kBlackbody        = 1u << 3;  // 黒体放射カラーランプによる温度冷却発色
	inline constexpr uint32_t kStretched        = 1u << 4;  // GPU速度方向ストレッチビルボード
	inline constexpr uint32_t kFloorBounce      = 1u << 5;  // 地面・地形高さでのバウンド＆摩擦
	inline constexpr uint32_t kWakeInteract     = 1u << 6;  // 航空機翼端渦＆ジェット排気ウェイク場と相互作用
	inline constexpr uint32_t kCameraWrap       = 1u << 7;  // カメラ追従ラップアラウンド空間（環境吹雪・霧用）
	inline constexpr uint32_t kSoftParticle     = 1u << 8;  // 深度バッファ参照ソフトパーティクルフェード
	inline constexpr uint32_t kSphereEmit       = 1u << 9;  // 球体分布＋放射速度エミッション
	inline constexpr uint32_t kRingEmit         = 1u << 10; // 法線軸に垂直なリング／衝撃波エミッション
	inline constexpr uint32_t kWorldUnitsPerSec = 1u << 11; // SI単位系（m/s, m/s^2）で物理積分
}

/// <summary>
/// GPUピクセルシェーダでのプロシージャル形状タイプ
/// </summary>
enum class GPUParticleShape : uint32_t {
	SoftCircle  = 0u, // テクスチャ／ソフト円形グロウ（既存互換）
	SmokeCloud  = 1u, // プロシージャル多層FBMボリューム煙雲
	SparkStreak = 2u, // 高輝度コア＋テーパー火花ストリーク
	ShockRing   = 3u, // 衝撃波・マッハコーンリング
	SnowCrystal = 4u, // きらめく氷晶・雪の結晶
	ChaffFoil   = 5u, // 回転しながら太陽光を反射するチャフ金属片
	VaporMist   = 6u, // 輪郭の出ない極薄の水蒸気凝縮ベール
	FlareCore   = 7u, // ピンポイント白熱芯＋オレンジコロナ＋光学スターバースト
};

/// <summary>
/// GPU上の1パーティクルデータ（160 bytes / 16バイト整列）
/// </summary>
struct ParticleCS {
	MyMath::Vector3 translate;
	float rotation;
	MyMath::Vector3 scale;
	float lifeTime;
	MyMath::Vector3 velocity;
	float currentTime;
	MyMath::Vector3 acceleration;
	float rotationSpeed;
	MyMath::Vector4 color;
	float startScale;
	float endScale;
	float scaleEasing;
	float startAlpha;
	float spawnY;
	float maxRiseHeight;
	uint32_t flags;
	uint32_t shapeType;
	float drag;
	float curlStrength;
	float curlFrequency;
	float stretchFactor;
	float temperature;
	float coolingRate;
	float floorY;
	float restitution;
	float emissiveIntensity;
	float softDistance;
	float randomSeed;
	float wakeSensitivity;
};
static_assert(sizeof(ParticleCS) == 160, "ParticleCS size must match HLSL (160 bytes)");

/// <summary>
/// GPUエミッションリクエスト（224 bytes / 16バイト整列）
/// </summary>
struct GpuEmitRequest {
	MyMath::Vector3 origin;
	float randomPositionRange;
	MyMath::Vector3 minVelocity;
	float minLifeTime;
	MyMath::Vector3 maxVelocity;
	float maxLifeTime;
	MyMath::Vector3 acceleration;
	float maxRiseHeight;
	MyMath::Vector4 minColor;
	MyMath::Vector4 maxColor;
	float minScale;
	float maxScale;
	float endScale;
	float scaleEasing;
	float minRotation;
	float maxRotation;
	float minRotationSpeed;
	float maxRotationSpeed;
	uint32_t startOffset;
	uint32_t count;
	uint32_t flags;
	uint32_t seedSalt;
	uint32_t shapeType;
	float drag;
	float curlStrength;
	float curlFrequency;
	float stretchFactor;
	float minTemperature;
	float maxTemperature;
	float coolingRate;
	float floorY;
	float restitution;
	float emissiveIntensity;
	float softDistance;
	MyMath::Vector3 emitAxis;
	float radialSpeedMin;
	float radialSpeedMax;
	float ringRadius;
	float wakeSensitivity;
	float padding0;
};
static_assert(sizeof(GpuEmitRequest) == 224, "GpuEmitRequest size must match HLSL (224 bytes)");

/// <summary>
/// 航空機ウェイク（翼端渦＆ジェット排気）ベクトル場（64 bytes）
/// </summary>
struct GpuWakeField {
	MyMath::Vector3 position = { 0.0f, 0.0f, 0.0f };
	float halfWingspan = 6.0f;
	MyMath::Vector3 forward = { 0.0f, 0.0f, 1.0f };
	float vortexStrength = 0.0f;
	MyMath::Vector3 right = { 1.0f, 0.0f, 0.0f };
	float exhaustThrust = 0.0f;
	MyMath::Vector3 velocity = { 0.0f, 0.0f, 0.0f };
	uint32_t active = 0u;
};
static_assert(sizeof(GpuWakeField) == 64, "GpuWakeField size must match HLSL (64 bytes)");

/// <summary>
/// カメラ追従環境パーティクル空間（32 bytes）
/// </summary>
struct GpuAmbientVolume {
	MyMath::Vector3 center = { 0.0f, 0.0f, 0.0f };
	float halfExtent = 90.0f;
	MyMath::Vector3 windVelocity = { -4.0f, -6.0f, -2.0f };
	uint32_t active = 0u;
};
static_assert(sizeof(GpuAmbientVolume) == 32, "GpuAmbientVolume size must match HLSL (32 bytes)");

/// <summary>
/// ゲーム側からリッチGPUパーティクルを発生させるための高レベル記述子
/// （デフォルトで SI単位系 m/s・ソフトパーティクル有効）
/// </summary>
struct GpuParticleEmitDesc {
	MyMath::Vector3 origin = { 0.0f, 0.0f, 0.0f };
	float randomPositionRange = 0.0f;
	MyMath::Vector3 minVelocity = { 0.0f, 0.0f, 0.0f };
	MyMath::Vector3 maxVelocity = { 0.0f, 0.0f, 0.0f };
	MyMath::Vector3 acceleration = { 0.0f, 0.0f, 0.0f };
	MyMath::Vector4 minColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	MyMath::Vector4 maxColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float minLifeTime = 1.0f;
	float maxLifeTime = 2.0f;
	float minScale = 1.0f;
	float maxScale = 1.0f;
	float endScale = 0.0f;
	float scaleEasing = 1.0f;
	float minRotation = 0.0f;
	float maxRotation = 6.2831853f;
	float minRotationSpeed = 0.0f;
	float maxRotationSpeed = 0.0f;
	float maxRiseHeight = 0.0f;
	uint32_t count = 1u;
	uint32_t flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kSoftParticle | GPUParticleFlags::kWorldUnitsPerSec;
	GPUParticleShape shape = GPUParticleShape::SoftCircle;
	float drag = 0.0f;
	float curlStrength = 0.0f;
	float curlFrequency = 0.12f;
	float stretchFactor = 0.08f;
	float minTemperature = 0.0f;
	float maxTemperature = 0.0f;
	float coolingRate = 0.0f;
	float floorY = 0.0f;
	bool useCustomFloorY = false;
	float restitution = 0.45f;
	float emissiveIntensity = 1.0f;
	float softDistance = 3.5f;
	MyMath::Vector3 emitAxis = { 0.0f, 1.0f, 0.0f };
	float radialSpeedMin = 0.0f;
	float radialSpeedMax = 0.0f;
	float ringRadius = 0.0f;
	float wakeSensitivity = 1.0f;
};
