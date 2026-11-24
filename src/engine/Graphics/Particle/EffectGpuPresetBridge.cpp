#include "EffectGpuPresetBridge.h"
#include "EffectManager.h"
#include "GPUParticleManager.h"
#include "../../io/ConfigFile.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

#ifdef USE_IMGUI
#include "../../../../external/imgui/imgui.h"
#endif

using namespace MyMath;

namespace {
	GPUParticleShape ParseShapeType(const std::string& raw) {
		if (raw.empty()) {
			return GPUParticleShape::SoftCircle;
		}
		std::string lower = raw;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});

		if (lower == "softcircle" || lower == "0") { return GPUParticleShape::SoftCircle; }
		if (lower == "smokecloud" || lower == "1") { return GPUParticleShape::SmokeCloud; }
		if (lower == "sparkstreak" || lower == "2") { return GPUParticleShape::SparkStreak; }
		if (lower == "shockring" || lower == "3") { return GPUParticleShape::ShockRing; }
		if (lower == "snowcrystal" || lower == "4") { return GPUParticleShape::SnowCrystal; }
		if (lower == "chafffoil" || lower == "5") { return GPUParticleShape::ChaffFoil; }
		if (lower == "vapormist" || lower == "6") { return GPUParticleShape::VaporMist; }
		if (lower == "flarecore" || lower == "7") { return GPUParticleShape::FlareCore; }

		const int numeric = std::atoi(lower.c_str());
		const int clamped = std::clamp(numeric, 0, 7);
		return static_cast<GPUParticleShape>(clamped);
	}

	const char* ShapeTypeToString(GPUParticleShape shape) {
		switch (shape) {
		case GPUParticleShape::SoftCircle:  return "SoftCircle";
		case GPUParticleShape::SmokeCloud:  return "SmokeCloud";
		case GPUParticleShape::SparkStreak: return "SparkStreak";
		case GPUParticleShape::ShockRing:   return "ShockRing";
		case GPUParticleShape::SnowCrystal: return "SnowCrystal";
		case GPUParticleShape::ChaffFoil:   return "ChaffFoil";
		case GPUParticleShape::VaporMist:   return "VaporMist";
		case GPUParticleShape::FlareCore:   return "FlareCore";
		default:                            return "SoftCircle";
		}
	}
}

void EffectGpuPresetBridge::LoadFromConfig(const ConfigFile& cfg, const std::string& sec, EffectGpuRichParams& out) {
	out = EffectGpuRichParams{};
	out.useRichGpu = cfg.GetBool(sec, "useRichGpu", false);
	if (!out.useRichGpu) {
		return;
	}

	out.shape = ParseShapeType(cfg.GetString(sec, "shapeType", "SoftCircle"));
	out.worldUnitsPerSec = cfg.GetBool(sec, "worldUnitsPerSec", true);

	out.useCurlNoise = cfg.GetBool(sec, "useCurlNoise", false);
	out.curlStrength = cfg.GetFloat(sec, "curlStrength", 0.0f);
	out.curlFrequency = cfg.GetFloat(sec, "curlFrequency", 0.12f);

	out.useBlackbody = cfg.GetBool(sec, "useBlackbody", false);
	out.minTemperature = cfg.GetFloat(sec, "minTemperature", 0.45f);
	out.maxTemperature = cfg.GetFloat(sec, "maxTemperature", 0.65f);
	out.coolingRate = cfg.GetFloat(sec, "coolingRate", 0.5f);

	out.useFloorBounce = cfg.GetBool(sec, "useFloorBounce", false);
	out.floorY = cfg.GetFloat(sec, "floorY", 0.0f);
	out.restitution = cfg.GetFloat(sec, "restitution", 0.45f);

	out.useWakeInteract = cfg.GetBool(sec, "useWakeInteract", false);
	out.wakeSensitivity = cfg.GetFloat(sec, "wakeSensitivity", 1.0f);

	out.useCameraWrap = cfg.GetBool(sec, "useCameraWrap", false);

	out.useSoftParticle = cfg.GetBool(sec, "useSoftParticle", true);
	out.softDistance = cfg.GetFloat(sec, "softDistance", 3.0f);

	out.useSphereEmit = cfg.GetBool(sec, "useSphereEmit", false);
	out.useRingEmit = cfg.GetBool(sec, "useRingEmit", false);
	out.emitAxis.x = cfg.GetFloat(sec, "emitAxisX", 0.0f);
	out.emitAxis.y = cfg.GetFloat(sec, "emitAxisY", 1.0f);
	out.emitAxis.z = cfg.GetFloat(sec, "emitAxisZ", 0.0f);
	out.radialSpeedMin = cfg.GetFloat(sec, "radialSpeedMin", 0.0f);
	out.radialSpeedMax = cfg.GetFloat(sec, "radialSpeedMax", 0.0f);
	out.ringRadius = cfg.GetFloat(sec, "ringRadius", 0.0f);

	out.drag = cfg.GetFloat(sec, "drag", 0.0f);
	out.emissiveIntensity = cfg.GetFloat(sec, "emissiveIntensity", 1.0f);
}

void EffectGpuPresetBridge::SaveToStream(std::ofstream& file, const EffectGpuRichParams& p) {
	if (!p.useRichGpu) {
		return;
	}

	file << "useRichGpu = true\n";
	file << "shapeType = " << ShapeTypeToString(p.shape) << "\n";
	file << "worldUnitsPerSec = " << (p.worldUnitsPerSec ? "true" : "false") << "\n";
	file << "useCurlNoise = " << (p.useCurlNoise ? "true" : "false") << "\n";
	file << "curlStrength = " << p.curlStrength << "\n";
	file << "curlFrequency = " << p.curlFrequency << "\n";
	file << "useBlackbody = " << (p.useBlackbody ? "true" : "false") << "\n";
	file << "minTemperature = " << p.minTemperature << "\n";
	file << "maxTemperature = " << p.maxTemperature << "\n";
	file << "coolingRate = " << p.coolingRate << "\n";
	file << "useFloorBounce = " << (p.useFloorBounce ? "true" : "false") << "\n";
	file << "floorY = " << p.floorY << "\n";
	file << "restitution = " << p.restitution << "\n";
	file << "useWakeInteract = " << (p.useWakeInteract ? "true" : "false") << "\n";
	file << "wakeSensitivity = " << p.wakeSensitivity << "\n";
	file << "useCameraWrap = " << (p.useCameraWrap ? "true" : "false") << "\n";
	file << "useSoftParticle = " << (p.useSoftParticle ? "true" : "false") << "\n";
	file << "softDistance = " << p.softDistance << "\n";
	file << "useSphereEmit = " << (p.useSphereEmit ? "true" : "false") << "\n";
	file << "useRingEmit = " << (p.useRingEmit ? "true" : "false") << "\n";
	file << "emitAxisX = " << p.emitAxis.x << "\n";
	file << "emitAxisY = " << p.emitAxis.y << "\n";
	file << "emitAxisZ = " << p.emitAxis.z << "\n";
	file << "radialSpeedMin = " << p.radialSpeedMin << "\n";
	file << "radialSpeedMax = " << p.radialSpeedMax << "\n";
	file << "ringRadius = " << p.ringRadius << "\n";
	file << "drag = " << p.drag << "\n";
	file << "emissiveIntensity = " << p.emissiveIntensity << "\n";
}

GpuParticleEmitDesc EffectGpuPresetBridge::BuildEmitDesc(
	const EffectParticleLayer& layer,
	const Vector3& baseOrigin,
	const EmitOverride& ov) {

	const ParticleParameters& p = layer.params;
	const EffectGpuRichParams& r = layer.gpuRich;
	const float s = (std::max)(0.05f, ov.scale);
	const float cMult = (std::max)(0.0f, ov.countMultiplier);
	const float aMult = std::clamp(ov.alphaMultiplier, 0.0f, 2.0f);

	GpuParticleEmitDesc desc{};
	desc.origin = ov.overrideOrigin ? ov.origin : baseOrigin;
	desc.randomPositionRange = ov.overrideRandomPositionRange
		? ov.randomPositionRange
		: (p.randomPositionRange * s);

	if (ov.overrideVelocityRange) {
		desc.minVelocity = ov.minVelocity;
		desc.maxVelocity = ov.maxVelocity;
	} else {
		desc.minVelocity = Multiply(s, p.minVelocity);
		desc.maxVelocity = Multiply(s, p.maxVelocity);
		if (ov.addVelocityOffset) {
			desc.minVelocity = Add(desc.minVelocity, ov.velocityOffset);
			desc.maxVelocity = Add(desc.maxVelocity, ov.velocityOffset);
		}
	}

	desc.acceleration = p.acceleration;
	desc.minColor = {
		p.minColor.x,
		p.minColor.y,
		p.minColor.z,
		std::clamp(p.minColor.w * aMult, 0.0f, 1.0f)
	};
	desc.maxColor = {
		p.maxColor.x,
		p.maxColor.y,
		p.maxColor.z,
		std::clamp(p.maxColor.w * aMult, 0.0f, 1.0f)
	};

	desc.minLifeTime = ov.overrideLifeTime ? ov.minLifeTime : p.minLifeTime;
	desc.maxLifeTime = ov.overrideLifeTime ? ov.maxLifeTime : p.maxLifeTime;
	desc.minScale = p.minScale * s;
	desc.maxScale = p.maxScale * s;
	desc.endScale = p.endScale * s;
	desc.scaleEasing = p.scaleEasing;
	desc.minRotation = p.minRotation;
	desc.maxRotation = p.maxRotation;
	desc.minRotationSpeed = p.minRotationSpeed;
	desc.maxRotationSpeed = p.maxRotationSpeed;
	desc.maxRiseHeight = p.maxRiseHeight * s;

	const float rawCount = static_cast<float>(layer.emitCount) * cMult;
	desc.count = (layer.emitCount == 0u || cMult <= 0.0f)
		? 0u
		: static_cast<uint32_t>((std::max)(1.0f, std::round(rawCount)));

	uint32_t flags = GPUParticleFlags::kNone;
	if (p.fadeOut) { flags |= GPUParticleFlags::kFadeOut; }
	if (layer.blendMode == BlendMode::kAdd) { flags |= GPUParticleFlags::kAdditive; }
	if (p.isStretched) { flags |= GPUParticleFlags::kStretched; }
	if (r.worldUnitsPerSec) { flags |= GPUParticleFlags::kWorldUnitsPerSec; }
	if (r.useCurlNoise) { flags |= GPUParticleFlags::kCurlNoise; }
	if (r.useBlackbody) { flags |= GPUParticleFlags::kBlackbody; }
	if (r.useFloorBounce) { flags |= GPUParticleFlags::kFloorBounce; }
	if (r.useWakeInteract) { flags |= GPUParticleFlags::kWakeInteract; }
	if (r.useCameraWrap) { flags |= GPUParticleFlags::kCameraWrap; }
	if (r.useSoftParticle) { flags |= GPUParticleFlags::kSoftParticle; }
	if (r.useSphereEmit) { flags |= GPUParticleFlags::kSphereEmit; }
	if (r.useRingEmit) { flags |= GPUParticleFlags::kRingEmit; }
	if (ov.overrideFlagsMask) { flags |= ov.additionalFlags; }

	desc.flags = flags;
	desc.shape = r.shape;
	desc.drag = r.drag;
	desc.curlStrength = r.curlStrength * s;
	desc.curlFrequency = r.curlFrequency;
	desc.stretchFactor = p.stretchFactor;
	desc.minTemperature = r.minTemperature;
	desc.maxTemperature = r.maxTemperature;
	desc.coolingRate = r.coolingRate;
	desc.floorY = ov.overrideFloorY ? ov.floorY : r.floorY;
	desc.useCustomFloorY = ov.overrideFloorY || r.useFloorBounce;
	desc.restitution = r.restitution;
	desc.emissiveIntensity = r.emissiveIntensity;
	desc.softDistance = r.softDistance * s;
	desc.emitAxis = ov.overrideEmitAxis ? ov.emitAxis : r.emitAxis;
	desc.radialSpeedMin = r.radialSpeedMin * s;
	desc.radialSpeedMax = r.radialSpeedMax * s;
	desc.ringRadius = r.ringRadius * s;
	desc.wakeSensitivity = r.wakeSensitivity;

	return desc;
}

bool EffectGpuPresetBridge::EmitLayer(
	const EffectParticleLayer& layer,
	const Vector3& baseOrigin,
	const EmitOverride& ov) {

	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return false;
	}
	const GpuParticleEmitDesc desc = BuildEmitDesc(layer, baseOrigin, ov);
	if (desc.count == 0u) {
		return false;
	}
	gpu->EnqueueRichEmit(desc);
	return true;
}

#ifdef USE_IMGUI
void EffectGpuPresetBridge::DrawImGuiControls(EffectParticleLayer& layer) {
	EffectGpuRichParams& r = layer.gpuRich;

	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.35f, 0.90f, 1.0f, 1.0f), "--- GPU Rich Shader Params (.effect) ---");
	ImGui::Checkbox("Enable GPU Rich Features (2-Bucket / Curl / Blackbody)", &r.useRichGpu);
	if (!r.useRichGpu) {
		return;
	}

	const char* shapeItems[] = {
		"SoftCircle (Glow / Core)",
		"SmokeCloud (FBM Volume)",
		"SparkStreak (Hot Core)",
		"ShockRing (Mach Ring)",
		"SnowCrystal (Ice Glint)",
		"ChaffFoil (Metallic Foil)",
		"VaporMist (Thin Condensation)",
		"FlareCore (Optical Starburst)"
	};
	int shapeIdx = std::clamp(static_cast<int>(r.shape), 0, IM_ARRAYSIZE(shapeItems) - 1);
	if (ImGui::Combo("GPU Shape", &shapeIdx, shapeItems, IM_ARRAYSIZE(shapeItems))) {
		r.shape = static_cast<GPUParticleShape>(shapeIdx);
	}

	ImGui::Checkbox("SI Units (m/s, m/s^2)", &r.worldUnitsPerSec);
	ImGui::SameLine();
	ImGui::Checkbox("Soft Particle", &r.useSoftParticle);
	if (r.useSoftParticle) {
		ImGui::SliderFloat("Soft Distance (m)", &r.softDistance, 0.1f, 20.0f, "%.2f");
	}
	ImGui::SliderFloat("Emissive Intensity (HDR)", &r.emissiveIntensity, 0.1f, 5.0f, "%.2f");
	ImGui::SliderFloat("Air Drag", &r.drag, 0.0f, 10.0f, "%.2f");

	ImGui::Checkbox("3D Curl Noise", &r.useCurlNoise);
	if (r.useCurlNoise) {
		ImGui::SliderFloat("Curl Strength", &r.curlStrength, 0.0f, 40.0f, "%.2f");
		ImGui::SliderFloat("Curl Frequency", &r.curlFrequency, 0.01f, 1.0f, "%.3f");
	}

	ImGui::Checkbox("Blackbody Color Ramp", &r.useBlackbody);
	if (r.useBlackbody) {
		ImGui::DragFloatRange2("Temperature (0=Ash, 0.45=Orange, 1.0=White)",
			&r.minTemperature, &r.maxTemperature, 0.01f, 0.0f, 1.5f, "Min: %.2f", "Max: %.2f");
		ImGui::SliderFloat("Cooling Rate", &r.coolingRate, 0.0f, 6.0f, "%.2f");
	}

	ImGui::Checkbox("Floor Bounce", &r.useFloorBounce);
	if (r.useFloorBounce) {
		ImGui::DragFloat("Floor Y (m)", &r.floorY, 0.1f, -500.0f, 500.0f, "%.2f");
		ImGui::SliderFloat("Restitution", &r.restitution, 0.0f, 1.0f, "%.2f");
	}

	ImGui::Checkbox("Wake Interact", &r.useWakeInteract);
	if (r.useWakeInteract) {
		ImGui::SliderFloat("Wake Sensitivity", &r.wakeSensitivity, 0.0f, 3.0f, "%.2f");
	}
	ImGui::SameLine();
	ImGui::Checkbox("Camera Wrap", &r.useCameraWrap);

	int emitMode = r.useRingEmit ? 2 : (r.useSphereEmit ? 1 : 0);
	const char* emitModeItems[] = { "Box", "Sphere", "Ring" };
	if (ImGui::Combo("Emit Geometry", &emitMode, emitModeItems, IM_ARRAYSIZE(emitModeItems))) {
		r.useSphereEmit = (emitMode == 1);
		r.useRingEmit = (emitMode == 2);
	}
	if (r.useSphereEmit || r.useRingEmit) {
		ImGui::DragFloatRange2("Radial Speed (m/s)", &r.radialSpeedMin, &r.radialSpeedMax, 0.25f, 0.0f, 150.0f, "Min: %.1f", "Max: %.1f");
	}
	if (r.useRingEmit) {
		ImGui::DragFloat("Ring Radius (m)", &r.ringRadius, 0.1f, 0.0f, 40.0f, "%.2f");
		ImGui::DragFloat3("Emit Axis", &r.emitAxis.x, 0.05f, -1.0f, 1.0f, "%.2f");
	}
}
#endif
