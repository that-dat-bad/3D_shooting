#pragma once
#include "GPUParticleTypes.h"
#include <fstream>
#include <string>

class ConfigFile;
struct EffectParticleLayer;

/// <summary>
/// .effect ファイルの GPU リッチ演出パラメータ
/// </summary>
struct EffectGpuRichParams {
	bool useRichGpu = false;
	GPUParticleShape shape = GPUParticleShape::SoftCircle;
	bool worldUnitsPerSec = true;

	bool useCurlNoise = false;
	float curlStrength = 0.0f;
	float curlFrequency = 0.12f;

	bool useBlackbody = false;
	float minTemperature = 0.45f;
	float maxTemperature = 0.65f;
	float coolingRate = 0.5f;

	bool useFloorBounce = false;
	float floorY = 0.0f;
	float restitution = 0.45f;

	bool useWakeInteract = false;
	float wakeSensitivity = 1.0f;

	bool useCameraWrap = false;

	bool useSoftParticle = true;
	float softDistance = 3.0f;

	bool useSphereEmit = false;
	bool useRingEmit = false;
	MyMath::Vector3 emitAxis = { 0.0f, 1.0f, 0.0f };
	float radialSpeedMin = 0.0f;
	float radialSpeedMax = 0.0f;
	float ringRadius = 0.0f;

	float drag = 0.0f;
	float emissiveIntensity = 1.0f;
};

/// <summary>
/// .effect レイヤーの GPU シリアライズ・Emit変換・ImGui編集ブリッジ
/// </summary>
namespace EffectGpuPresetBridge {
	/// <summary>
	/// 実行時の発生パラメータ上書き指定
	/// </summary>
	struct EmitOverride {
		float scale = 1.0f;
		float countMultiplier = 1.0f;
		float alphaMultiplier = 1.0f;
		bool overrideFloorY = false;
		float floorY = 0.0f;
		bool addVelocityOffset = false;
		MyMath::Vector3 velocityOffset = { 0.0f, 0.0f, 0.0f };
		bool overrideEmitAxis = false;
		MyMath::Vector3 emitAxis = { 0.0f, 1.0f, 0.0f };
		bool overrideOrigin = false;
		MyMath::Vector3 origin = { 0.0f, 0.0f, 0.0f };
		bool overrideRandomPositionRange = false;
		float randomPositionRange = 0.0f;
		bool overrideVelocityRange = false;
		MyMath::Vector3 minVelocity = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 maxVelocity = { 0.0f, 0.0f, 0.0f };
		bool overrideLifeTime = false;
		float minLifeTime = 1.0f;
		float maxLifeTime = 2.0f;
		bool overrideFlagsMask = false;
		uint32_t additionalFlags = 0u;
	};

	void LoadFromConfig(const ConfigFile& cfg, const std::string& sec, EffectGpuRichParams& out);
	void SaveToStream(std::ofstream& file, const EffectGpuRichParams& p);

	GpuParticleEmitDesc BuildEmitDesc(
		const EffectParticleLayer& layer,
		const MyMath::Vector3& baseOrigin,
		const EmitOverride& ov = {});

	bool EmitLayer(
		const EffectParticleLayer& layer,
		const MyMath::Vector3& baseOrigin,
		const EmitOverride& ov = {});

#ifdef USE_IMGUI
	void DrawImGuiControls(EffectParticleLayer& layer);
#endif
}
