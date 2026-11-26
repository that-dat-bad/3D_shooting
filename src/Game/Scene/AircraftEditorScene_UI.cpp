#include "AircraftEditorScene.h"

#ifdef USE_IMGUI

#include "../System/GameResult.h"
#include "../System/MissionManager.h"
#include "../../../external/imgui/imgui.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

using namespace MyMath;
namespace fs = std::filesystem;

namespace {
	constexpr float kPi = 3.14159265358979323846f;
	constexpr float kGravity = 9.81f;
	constexpr float kSeaLevelDensity = 1.225f;
	constexpr float kScaleHeight = 8500.0f;
	constexpr float kSpeedOfSoundSL = 340.3f;

	float ComputeCLForGraph(const AirframeData& af, float aoaRad, bool withFlaps) {
		float cl = 0.0f;
		const float crit = (std::max)(af.criticalAoA, 0.05f);
		if (aoaRad >= 0.0f) {
			if (aoaRad <= crit) {
				cl = af.liftCoefficient + (af.maxLiftCoefficient - af.liftCoefficient) * (aoaRad / crit);
			} else {
				const float over = std::clamp((aoaRad - crit) / crit, 0.0f, 1.0f);
				cl = af.maxLiftCoefficient - (af.maxLiftCoefficient - af.stallLiftCoefficient) * over;
			}
		} else {
			const float absAoA = -aoaRad;
			if (absAoA <= crit) {
				cl = af.liftCoefficient - (af.maxLiftCoefficient + af.liftCoefficient) * (absAoA / crit);
			} else {
				const float over = std::clamp((absAoA - crit) / crit, 0.0f, 1.0f);
				cl = -af.maxLiftCoefficient + (af.maxLiftCoefficient - af.stallLiftCoefficient) * over;
			}
		}
		if (withFlaps) {
			cl += af.flapLiftBonus;
		}
		return cl;
	}
}

void AircraftEditorScene::DrawEditorWindows() {
	DrawMainInspectorWindow();
	DrawViewportBarWindow();
	DrawTelemetryWindow();
}

bool AircraftEditorScene::SaveCurrentAircraft() {
	std::string savePath = aircraftConfig_.GetCurrentFilePath();
	if (savePath.empty()) {
		const std::string nameStr(saveAsNameBuf_);
		savePath = nameStr.empty()
			? std::string("Resources/aircraft/mig21.cfg")
			: ("Resources/aircraft/" + nameStr);
	}

	if (!aircraftConfig_.SaveToCfg(savePath)) {
		SetStatus("Failed to save CFG: " + savePath, true);
		return false;
	}
	aircraftConfig_.SetCurrentFilePath(savePath);
	AircraftConfig::SetActiveConfigPath(savePath);

	const std::string& abPath = aircraftConfig_.GetGeneral().afterburnerConfigPath;
	bool abOk = true;
	if (afterburner_ && !abPath.empty()) {
		abOk = afterburner_->SaveConfig(abPath);
	}

	const std::string cfgName = fs::path(savePath).filename().string();
	if (abOk) {
		SetStatus("Saved CFG & AB: " + cfgName, false);
	} else {
		SetStatus("Saved CFG (" + cfgName + "), but failed to save AB JSON: " + abPath, true);
	}
	return true;
}

void AircraftEditorScene::DrawMainInspectorWindow() {
	ImGui::SetNextWindowPos(ImVec2(16, 86), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(460, 620), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("Aircraft Editor [DEV]")) {
		ImGui::End();
		return;
	}

	// --- CFGファイル管理ツールバー ---
	if (ImGui::CollapsingHeader("CFG File & Test Flight", ImGuiTreeNodeFlags_DefaultOpen)) {
		std::vector<std::string> fileNames;
		std::vector<const char*> filePtrs;
		fileNames.reserve(availableCfgFiles_.size());
		filePtrs.reserve(availableCfgFiles_.size());
		for (const auto& f : availableCfgFiles_) {
			fileNames.push_back(fs::path(f).filename().string());
		}
		for (const auto& fn : fileNames) {
			filePtrs.push_back(fn.c_str());
		}

		ImGui::SetNextItemWidth(250.0f);
		if (!filePtrs.empty()) {
			ImGui::Combo("Aircraft CFG", &selectedCfgIndex_, filePtrs.data(), static_cast<int>(filePtrs.size()));
		} else {
			ImGui::TextDisabled("No .cfg files in Resources/aircraft");
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh")) {
			RefreshFileLists();
			SetStatus("Refreshed file lists.", false);
		}

		if (ImGui::Button("Load") && selectedCfgIndex_ >= 0 && selectedCfgIndex_ < static_cast<int>(availableCfgFiles_.size())) {
			LoadAircraftCfg(availableCfgFiles_[selectedCfgIndex_]);
		}
		ImGui::SameLine();
		if (ImGui::Button("Save (Overwrite)")) {
			std::string savePath = aircraftConfig_.GetCurrentFilePath();
			if (savePath.empty()) {
				savePath = "Resources/aircraft/" + std::string(saveAsNameBuf_);
			}
			if (aircraftConfig_.SaveToCfg(savePath)) {
				AircraftConfig::SetActiveConfigPath(savePath);
				SetStatus("Saved CFG: " + fs::path(savePath).filename().string(), false);
			} else {
				SetStatus("Failed to save CFG: " + savePath, true);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Default")) {
			std::string curPath = aircraftConfig_.GetCurrentFilePath();
			if (curPath.empty()) {
				curPath = "Resources/aircraft/mig21.cfg";
			}
			aircraftConfig_.CreateDefault(curPath);
			LoadAircraftCfg(curPath);
			SetStatus("Reset to default MiG-21 parameters.", false);
		}

		ImGui::SetNextItemWidth(250.0f);
		ImGui::InputText("Save As", saveAsNameBuf_, sizeof(saveAsNameBuf_));
		ImGui::SameLine();
		if (ImGui::Button("Save As New")) {
			std::string nameStr(saveAsNameBuf_);
			if (!nameStr.empty()) {
				std::string newPath = "Resources/aircraft/" + nameStr;
				if (newPath.find(".cfg") == std::string::npos) {
					newPath += ".cfg";
				}
				if (aircraftConfig_.SaveToCfg(newPath)) {
					aircraftConfig_.SetCurrentFilePath(newPath);
					AircraftConfig::SetActiveConfigPath(newPath);
					RefreshFileLists();
					SetStatus("Saved as: " + fs::path(newPath).filename().string(), false);
				} else {
					SetStatus("Failed to save: " + newPath, true);
				}
			}
		}

		const float availW = ImGui::GetContentRegionAvail().x;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float halfW = (availW - spacing) * 0.5f;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.42f, 0.72f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.52f, 0.85f, 1.0f));
		if (ImGui::Button("Save", ImVec2(halfW, 32.0f))) {
			SaveCurrentAircraft();
		}
		ImGui::PopStyleColor(2);

		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.55f, 0.28f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.68f, 0.35f, 1.0f));
		if (ImGui::Button("Save & Test Fly", ImVec2(-1.0f, 32.0f))) {
			if (SaveCurrentAircraft()) {
				MissionManager::GetInstance().SetCurrentFilePath("Resources/missions/free_flight.json");
				GameResult::GetInstance().isRetry = true;
				IScene::SetSceneID(SCENE::STAGE);
			}
		}
		ImGui::PopStyleColor(2);

		if (statusTimer_ > 0.0f && !statusMessage_.empty()) {
			const ImVec4 col = statusIsError_ ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f) : ImVec4(0.30f, 1.0f, 0.55f, 1.0f);
			ImGui::TextColored(col, "%s", statusMessage_.c_str());
		}
	}

	ImGui::Separator();

	if (ImGui::BeginTabBar("AircraftEditorTabs")) {
		if (ImGui::BeginTabItem("General")) {
			DrawGeneralTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Airframe")) {
			DrawAirframeTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Engine & AB")) {
			DrawEngineAndAfterburnerTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Armament")) {
			DrawArmamentTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Parts & DM")) {
			DrawPartsAndDamageTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

void AircraftEditorScene::DrawGeneralTab() {
	auto& general = aircraftConfig_.GetGeneral();

	char nameBuf[128]{};
	strncpy_s(nameBuf, sizeof(nameBuf), general.name.c_str(), _TRUNCATE);
	if (ImGui::InputText("Aircraft Name", nameBuf, sizeof(nameBuf))) {
		general.name = nameBuf;
	}

	// モデル一覧コンボ
	if (!availableModelFiles_.empty()) {
		int modelIdx = -1;
		std::vector<const char*> modelPtrs;
		modelPtrs.reserve(availableModelFiles_.size());
		for (int i = 0; i < static_cast<int>(availableModelFiles_.size()); ++i) {
			modelPtrs.push_back(availableModelFiles_[i].c_str());
			if (availableModelFiles_[i] == general.modelPath) {
				modelIdx = i;
			}
		}
		if (ImGui::Combo("Model Preset", &modelIdx, modelPtrs.data(), static_cast<int>(modelPtrs.size()))) {
			if (modelIdx >= 0 && modelIdx < static_cast<int>(availableModelFiles_.size())) {
				general.modelPath = availableModelFiles_[modelIdx];
				ReloadVisualModel(general.modelPath);
			}
		}
	}

	char modelBuf[256]{};
	strncpy_s(modelBuf, sizeof(modelBuf), general.modelPath.c_str(), _TRUNCATE);
	if (ImGui::InputText("Model Path", modelBuf, sizeof(modelBuf))) {
		general.modelPath = modelBuf;
	}
	if (ImGui::Button("Reload 3D Model")) {
		ReloadVisualModel(general.modelPath);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Loaded: %s", loadedModelPath_.c_str());

	ImGui::Separator();
	ImGui::Text("Material & Reflection");
	bool matChanged = false;
	matChanged |= ImGui::SliderFloat("Env Reflection", &general.environmentCoefficient, 0.0f, 1.0f);
	matChanged |= ImGui::SliderFloat("Specular Intensity", &general.specularIntensity, 0.0f, 5.0f);
	matChanged |= ImGui::SliderFloat("Shininess", &general.shininess, 1.0f, 200.0f);
	if (matChanged) {
		ApplyMaterialFromConfig();
	}

	ImGui::Separator();
	ImGui::Text("Visual Base Rotation (Model Space -> FlightModel +Z)");
	Vector3 baseRotDeg = {
		visualModel_.GetBaseRotation().x * 180.0f / kPi,
		visualModel_.GetBaseRotation().y * 180.0f / kPi,
		visualModel_.GetBaseRotation().z * 180.0f / kPi
	};
	if (ImGui::DragFloat3("Base Rotation (deg)", &baseRotDeg.x, 1.0f, -180.0f, 180.0f, "%.1f")) {
		visualModel_.SetBaseRotation({
			baseRotDeg.x * kPi / 180.0f,
			baseRotDeg.y * kPi / 180.0f,
			baseRotDeg.z * kPi / 180.0f
		});
		RebuildPreviewMeshCollider();
	}
	if (ImGui::Button("180 deg Yaw (FlightModel +Z)")) {
		visualModel_.SetBaseRotation({ 0.0f, kPi, 0.0f });
		RebuildPreviewMeshCollider();
	}
	ImGui::SameLine();
	if (ImGui::Button("0 deg (Raw Model)")) {
		visualModel_.SetBaseRotation({ 0.0f, 0.0f, 0.0f });
		RebuildPreviewMeshCollider();
	}
}

void AircraftEditorScene::DrawAirframeTab() {
	auto& af = aircraftConfig_.GetAirframe();

	if (ImGui::CollapsingHeader("Mass & Basic Aerodynamics", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("Empty Mass (kg)", &af.emptyFrameMass, 50.0f, 500.0f, 30000.0f, "%.0f kg");
		ImGui::DragFloat("Max Internal Fuel (kg)", &af.maxInternalFuel, 10.0f, 0.0f, 15000.0f, "%.0f kg");
		ImGui::SliderFloat("Base Drag (Cd0)", &af.baseDrag, 0.001f, 0.2f, "%.4f");
		ImGui::SliderFloat("Lift Coeff (CL0)", &af.liftCoefficient, -0.5f, 1.5f, "%.3f");
		ImGui::DragFloat("Wing Area (m^2)", &af.wingArea, 0.5f, 5.0f, 200.0f, "%.1f m^2");
		ImGui::DragFloat("Max Health (HP)", &af.maxHealth, 5.0f, 10.0f, 2000.0f, "%.0f");
		ImGui::SliderFloat("Center of Gravity Z (m)", &af.centerOfGravityZ, -3.0f, 3.0f, "%.2f m");
	}

	if (ImGui::CollapsingHeader("Lift & Stall Characteristics", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Critical AoA (rad)", &af.criticalAoA, 0.1f, 1.0f, "%.3f rad");
		ImGui::SameLine();
		ImGui::TextDisabled("(%.1f deg)", af.criticalAoA * 180.0f / kPi);
		ImGui::SliderFloat("Max Lift Coeff (CLmax)", &af.maxLiftCoefficient, 0.5f, 3.5f, "%.3f");
		ImGui::SliderFloat("Stall Lift Coeff", &af.stallLiftCoefficient, 0.05f, 1.0f, "%.3f");
	}

	if (ImGui::CollapsingHeader("Induced Drag & G Limits", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Aspect Ratio", &af.aspectRatio, 1.0f, 20.0f, "%.2f");
		ImGui::SliderFloat("Oswald Efficiency", &af.oswaldEfficiency, 0.3f, 1.0f, "%.2f");
		ImGui::SliderFloat("Positive G Limit", &af.positiveGLimit, 2.0f, 20.0f, "+%.1f G");
		ImGui::SliderFloat("Negative G Limit", &af.negativeGLimit, -10.0f, 0.0f, "%.1f G");
	}

	if (ImGui::CollapsingHeader("Flaps & Airbrake", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Flap Lift Bonus", &af.flapLiftBonus, 0.0f, 2.0f, "%.3f");
		ImGui::SliderFloat("Flap Drag Bonus", &af.flapDragBonus, 0.0f, 0.5f, "%.3f");
		ImGui::DragFloat("Flap Max Speed (m/s)", &af.flapMaxSpeed, 1.0f, 30.0f, 300.0f, "%.1f m/s");
		ImGui::SameLine();
		ImGui::TextDisabled("(%.0f km/h)", af.flapMaxSpeed * 3.6f);
		ImGui::SliderFloat("Flap Deploy Speed (1/s)", &af.flapDeploySpeed, 0.5f, 10.0f);

		ImGui::SliderFloat("Airbrake Drag Bonus", &af.airBrakeDragBonus, 0.0f, 1.0f, "%.3f");
		ImGui::SliderFloat("Airbrake Deploy Speed (1/s)", &af.airBrakeDeploySpeed, 0.5f, 10.0f);
	}
}

void AircraftEditorScene::DrawEngineAndAfterburnerTab() {
	auto& eng = aircraftConfig_.GetEngine();
	auto& general = aircraftConfig_.GetGeneral();

	if (ImGui::CollapsingHeader("Engine Propulsion (CFG)", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("Engine Mass (kg)", &eng.mass, 10.0f, 50.0f, 5000.0f, "%.0f kg");
		ImGui::DragFloat("Base Thrust (N)", &eng.baseThrust, 1000.0f, 1000.0f, 500000.0f, "%.0f N");
		ImGui::SameLine();
		ImGui::TextDisabled("(%.1f kN)", eng.baseThrust * 0.001f);

		ImGui::SliderFloat("Normal Throttle Limit", &eng.normalThrottleLimit, 0.5f, 1.0f, "%.2f");
		ImGui::SliderFloat("WEP Throttle Limit", &eng.wepThrottleLimit, 1.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Physical Spool Speed", &eng.physicalSpoolSpeed, 0.05f, 5.0f, "%.2f /s");
		ImGui::SliderFloat("Fuel Flow Rate (kg/s)", &eng.baseFuelFlowRate, 0.1f, 20.0f, "%.2f kg/s");
		ImGui::SliderFloat("Alt Thrust Factor", &eng.altitudeThrottleFactor, 0.0f, 0.0002f, "%.6f");
	}

	if (afterburner_ && ImGui::CollapsingHeader("Afterburner & Nozzle (JSON)", ImGuiTreeNodeFlags_DefaultOpen)) {
		char abBuf[256]{};
		strncpy_s(abBuf, sizeof(abBuf), general.afterburnerConfigPath.c_str(), _TRUNCATE);
		if (ImGui::InputText("AB JSON Path", abBuf, sizeof(abBuf))) {
			general.afterburnerConfigPath = abBuf;
		}
		if (ImGui::Button("Load AB JSON")) {
			if (afterburner_->LoadConfig(general.afterburnerConfigPath)) {
				SetStatus("Loaded AB JSON: " + general.afterburnerConfigPath, false);
			} else {
				SetStatus("Failed to load AB JSON: " + general.afterburnerConfigPath, true);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Save AB JSON")) {
			if (afterburner_->SaveConfig(general.afterburnerConfigPath)) {
				SetStatus("Saved AB JSON: " + general.afterburnerConfigPath, false);
			} else {
				SetStatus("Failed to save AB JSON: " + general.afterburnerConfigPath, true);
			}
		}

		ImGui::Separator();

		bool isTwin = afterburner_->IsTwinEngine();
		if (ImGui::Checkbox("Twin Engine Mode", &isTwin)) {
			afterburner_->SetTwinEngine(isTwin);
		}
		if (isTwin) {
			Vector3 left = afterburner_->GetLeftNozzleOffset();
			Vector3 right = afterburner_->GetRightNozzleOffset();
			bool changed = false;
			changed |= ImGui::DragFloat3("Left Nozzle Offset", &left.x, 0.01f);
			changed |= ImGui::DragFloat3("Right Nozzle Offset", &right.x, 0.01f);
			if (changed) {
				afterburner_->SetNozzleOffsets(left, right);
			}
		} else {
			Vector3 single = afterburner_->GetSingleNozzleOffset();
			if (ImGui::DragFloat3("Single Nozzle Offset", &single.x, 0.01f)) {
				afterburner_->SetSingleNozzleOffset(single);
			}
		}

		Vector3 nRot = afterburner_->GetNozzleRotation();
		Vector3 nRotDeg = {
			nRot.x * 180.0f / kPi,
			nRot.y * 180.0f / kPi,
			nRot.z * 180.0f / kPi
		};
		if (ImGui::DragFloat3("Nozzle Angle (deg)", &nRotDeg.x, 0.5f, -180.0f, 180.0f, "%.1f deg")) {
			afterburner_->SetNozzleRotation({
				nRotDeg.x * kPi / 180.0f,
				nRotDeg.y * kPi / 180.0f,
				nRotDeg.z * kPi / 180.0f
			});
		}
		if (ImGui::Button("Flip Nozzle 180 deg")) {
			Vector3 r = afterburner_->GetNozzleRotation();
			r.y += kPi;
			if (r.y > kPi) { r.y -= 2.0f * kPi; }
			afterburner_->SetNozzleRotation(r);
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Nozzle Angle")) {
			afterburner_->SetNozzleRotation({ 0.0f, 0.0f, 0.0f });
		}

		float radius = afterburner_->GetFlameRadius();
		if (ImGui::SliderFloat("Flame Radius (m)", &radius, 0.05f, 1.5f, "%.3f m")) {
			afterburner_->SetFlameRadius(radius);
		}
		float length = afterburner_->GetFlameLength();
		if (ImGui::SliderFloat("Flame Length (m)", &length, 0.5f, 20.0f, "%.2f m")) {
			afterburner_->SetFlameLength(length);
		}
		int diamonds = afterburner_->GetShockDiamondCount();
		if (ImGui::SliderInt("Shock Diamonds", &diamonds, 0, 10)) {
			afterburner_->SetShockDiamondCount(diamonds);
		}

		Vector3 flameColor = afterburner_->GetFlameColor();
		if (ImGui::ColorEdit3("Outer Flame Color", &flameColor.x)) {
			afterburner_->SetFlameColor(flameColor);
		}
		Vector3 coreColor = afterburner_->GetCoreColor();
		if (ImGui::ColorEdit3("Core / Shock Color", &coreColor.x)) {
			afterburner_->SetCoreColor(coreColor);
		}
	}
}

void AircraftEditorScene::DrawArmamentTab() {
	auto& gun = aircraftConfig_.GetGunpod();

	if (ImGui::CollapsingHeader("Gunpod (CFG)", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("Pod Mass (kg)", &gun.baseMass, 5.0f, 10.0f, 500.0f, "%.1f kg");
		ImGui::SliderFloat("Pod Drag (Cd)", &gun.drag, 0.0f, 0.05f, "%.4f");
		ImGui::SliderFloat("Ammo Weight (kg/rd)", &gun.ammoWeight, 0.01f, 1.0f, "%.3f kg");
		ImGui::DragInt("Max Ammo (rds)", &gun.maxAmmo, 10, 50, 5000);
		ImGui::SliderFloat("Fire Rate (rds/s)", &gun.fireRate, 5.0f, 100.0f, "%.1f /s");

		const float totalAmmoMass = gun.ammoWeight * static_cast<float>(gun.maxAmmo);
		const float totalGunMass = gun.baseMass + totalAmmoMass;
		const float rpm = gun.fireRate * 60.0f;
		const float burstSec = (gun.fireRate > 0.01f) ? (static_cast<float>(gun.maxAmmo) / gun.fireRate) : 0.0f;
		ImGui::Separator();
		ImGui::Text("Total Ammo Mass: %.1f kg (Pod Loaded: %.1f kg)", totalAmmoMass, totalGunMass);
		ImGui::Text("Rate of Fire: %.0f RPM | Trigger Time: %.1f sec", rpm, burstSec);

		ImGui::Separator();
		if (gun.muzzleOffsets.empty()) {
			gun.muzzleOffsets.push_back({ 0.0f, 0.0f, 8.0f });
		}
		selectedMuzzleIndex_ = std::clamp(selectedMuzzleIndex_, 0, static_cast<int>(gun.muzzleOffsets.size()) - 1);

		ImGui::Text("Gun Muzzle Offsets (Local: +X=Right, +Y=Up, +Z=Fwd)");
		ImGui::Checkbox("Show Gun Muzzles in 3D", &showGunMuzzles_);

		const int muzzleCount = static_cast<int>(gun.muzzleOffsets.size());
		const bool canAdd = (muzzleCount < GunPodData::kMaxMuzzles);

		if (!canAdd) { ImGui::BeginDisabled(); }
		if (ImGui::Button("+ Add Muzzle")) {
			gun.muzzleOffsets.push_back(gun.muzzleOffsets[static_cast<size_t>(selectedMuzzleIndex_)]);
			selectedMuzzleIndex_ = static_cast<int>(gun.muzzleOffsets.size()) - 1;
		}
		ImGui::SameLine();
		if (ImGui::Button("+ Mirror L/R")) {
			Vector3 mirrored = gun.muzzleOffsets[static_cast<size_t>(selectedMuzzleIndex_)];
			mirrored.x = -mirrored.x;
			gun.muzzleOffsets.push_back(mirrored);
			selectedMuzzleIndex_ = static_cast<int>(gun.muzzleOffsets.size()) - 1;
		}
		if (!canAdd) { ImGui::EndDisabled(); }

		ImGui::SameLine();
		const bool canRemove = (gun.muzzleOffsets.size() > 1);
		if (!canRemove) { ImGui::BeginDisabled(); }
		if (ImGui::Button("Remove")) {
			gun.muzzleOffsets.erase(gun.muzzleOffsets.begin() + selectedMuzzleIndex_);
			selectedMuzzleIndex_ = std::clamp(selectedMuzzleIndex_, 0, static_cast<int>(gun.muzzleOffsets.size()) - 1);
		}
		if (!canRemove) { ImGui::EndDisabled(); }

		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			gun.muzzleOffsets[static_cast<size_t>(selectedMuzzleIndex_)] = { 0.0f, 0.0f, 8.0f };
		}

		if (hasRaycastSample_ && lastRaycastHit_.hit) {
			if (ImGui::Button("Snap Selected to Last L-Click Hit")) {
				gun.muzzleOffsets[static_cast<size_t>(selectedMuzzleIndex_)] = lastRaycastHit_.localPoint;
				showGunMuzzles_ = true;
			}
		}

		for (int i = 0; i < static_cast<int>(gun.muzzleOffsets.size()); ++i) {
			ImGui::PushID(2000 + i);
			bool isSelected = (selectedMuzzleIndex_ == i);
			char radioLabel[32];
			snprintf(radioLabel, sizeof(radioLabel), "##sel%d", i);
			if (ImGui::RadioButton(radioLabel, isSelected)) {
				selectedMuzzleIndex_ = i;
			}
			ImGui::SameLine();
			char dragLabel[32];
			snprintf(dragLabel, sizeof(dragLabel), "Muzzle %d", i + 1);
			if (ImGui::DragFloat3(dragLabel, &gun.muzzleOffsets[static_cast<size_t>(i)].x, 0.05f, -20.0f, 20.0f, "%.2f m")) {
				selectedMuzzleIndex_ = i;
			}
			if (ImGui::IsItemClicked()) {
				selectedMuzzleIndex_ = i;
			}
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Missile Pylons (Standard Layout)", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Show Pylon Gizmos in 3D", &showPylons_);
		ImGui::Text("Launcher Mass: %.1f kg | Missile: %.1f kg x %d | Cd: %.3f",
			previewMissileData_.baseMass,
			previewMissileData_.missileMass,
			previewMissileData_.maxMissiles,
			previewMissileData_.drag);

		if (ImGui::BeginTable("PylonTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
			ImGui::TableSetupColumn("Pylon");
			ImGui::TableSetupColumn("X (m)");
			ImGui::TableSetupColumn("Y (m)");
			ImGui::TableSetupColumn("Z (m)");
			ImGui::TableHeadersRow();
			const char* labels[4] = { "Left Inner", "Right Inner", "Left Outer", "Right Outer" };
			for (int i = 0; i < 4; ++i) {
				const auto& p = previewMissileData_.pylonLocalOffsets[i];
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::Text("%s", labels[i]);
				ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", p.x);
				ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", p.y);
				ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", p.z);
			}
			ImGui::EndTable();
		}
	}
}

void AircraftEditorScene::DrawPartsAndDamageTab() {
	if (ImGui::CollapsingHeader("Mesh Collider (Exact Triangles)", ImGuiTreeNodeFlags_DefaultOpen)) {
		const auto& mc = previewDamageModel_.GetMeshCollider();
		ImGui::Checkbox("Draw Mesh Wireframe in 3D", &showMeshCollider_);
		ImGui::SameLine();
		ImGui::Checkbox("L-Click Raycast Test", &enableMeshRaycastTest_);

		if (mc.IsValid()) {
			ImGui::Text("Triangles: %zu | Edges: %zu | Radius: %.2f m",
				mc.GetTotalTriangleCount(),
				mc.GetTotalEdgeCount(),
				mc.GetBoundingRadius());

			if (hasRaycastSample_) {
				if (lastRaycastHit_.hit) {
					ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.55f, 1.0f),
						"Raycast HIT: %s (Tri #%u) @ (%.2f, %.2f, %.2f)",
						DamageModel::GetPartName(lastRaycastHit_.part),
						lastRaycastHit_.triangleIndex,
						lastRaycastHit_.localPoint.x,
						lastRaycastHit_.localPoint.y,
						lastRaycastHit_.localPoint.z);
				} else {
					ImGui::TextDisabled("Raycast: Miss");
				}
			} else {
				ImGui::TextDisabled("Left-click the aircraft in the 3D viewport to test mesh hit.");
			}

			const auto& parts = mc.GetParts();
			if (ImGui::BeginTable("MeshColliderTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 170.0f))) {
				ImGui::TableSetupColumn("Part");
				ImGui::TableSetupColumn("Tris", ImGuiTableColumnFlags_WidthFixed, 48.0f);
				ImGui::TableSetupColumn("Edges", ImGuiTableColumnFlags_WidthFixed, 52.0f);
				ImGui::TableSetupColumn("Highlight", ImGuiTableColumnFlags_WidthFixed, 68.0f);
				ImGui::TableHeadersRow();

				for (int i = 0; i < static_cast<int>(parts.size()); ++i) {
					const auto& p = parts[i];
					ImGui::PushID(1000 + i);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", DamageModel::GetPartName(p.part));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%zu", p.triangles.size());
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%zu", p.wireEdges.size());
					ImGui::TableSetColumnIndex(3);
					const bool selected = (highlightedMeshPartIndex_ == i);
					if (ImGui::Selectable(selected ? "ON" : "Select", selected)) {
						highlightedMeshPartIndex_ = selected ? -1 : i;
						showMeshCollider_ = true;
					}
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
		} else {
			ImGui::TextDisabled("No mesh collider loaded.");
		}
	}

	if (ImGui::CollapsingHeader("Visual Model Parts", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::Button("Show All Parts")) {
			visualModel_.ResetAllPartsVisible();
		}
		ImGui::SameLine();
		if (ImGui::Button("Hide Wings Test")) {
			visualModel_.SetPartVisible(DamagePart::Wing_L, false);
			visualModel_.SetPartVisible(DamagePart::Wing1_L, false);
			visualModel_.SetPartVisible(DamagePart::Wing2_L, false);
			visualModel_.SetPartVisible(DamagePart::Wing_R, false);
			visualModel_.SetPartVisible(DamagePart::Wing1_R, false);
			visualModel_.SetPartVisible(DamagePart::Wing2_R, false);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("Active Nodes: %zu", visualModel_.GetPartCount());

		for (int i = 0; i < DamageModel::kPartCount; ++i) {
			const DamagePart part = static_cast<DamagePart>(i);
			if (!visualModel_.HasPart(part)) {
				continue;
			}
			bool visible = visualModel_.IsPartVisible(part);
			if (ImGui::Checkbox(DamageModel::GetPartName(part), &visible)) {
				visualModel_.SetPartVisible(part, visible);
			}
		}
	}

	if (ImGui::CollapsingHeader("Damage Sub-Colliders (Hitboxes)", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Draw Sub-Colliders in 3D", &showSubColliders_);
		const auto& cols = previewDamageModel_.GetSubColliders();
		if (ImGui::BeginTable("ColliderTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 200.0f))) {
			ImGui::TableSetupColumn("Part");
			ImGui::TableSetupColumn("Center");
			ImGui::TableSetupColumn("HalfSize");
			ImGui::TableSetupColumn("Highlight", ImGuiTableColumnFlags_WidthFixed, 68.0f);
			ImGui::TableHeadersRow();

			for (int i = 0; i < static_cast<int>(cols.size()); ++i) {
				const auto& col = cols[i];
				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%s", DamageModel::GetPartName(col.part));
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.1f, %.1f, %.1f", col.center.x, col.center.y, col.center.z);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.2f, %.2f, %.2f", col.halfSize.x, col.halfSize.y, col.halfSize.z);
				ImGui::TableSetColumnIndex(3);
				const bool selected = (highlightedColliderIndex_ == i);
				if (ImGui::Selectable(selected ? "ON" : "Select", selected)) {
					highlightedColliderIndex_ = selected ? -1 : i;
					showSubColliders_ = true;
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
}

void AircraftEditorScene::DrawTelemetryWindow() {
	ImGui::SetNextWindowPos(ImVec2(844, 86), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(420, 560), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("Aero & Performance Telemetry")) {
		ImGui::End();
		return;
	}

	const auto& af = aircraftConfig_.GetAirframe();
	const auto& eng = aircraftConfig_.GetEngine();
	const auto& gun = aircraftConfig_.GetGunpod();

	const float gunLoadedMass = gun.baseMass + gun.ammoWeight * static_cast<float>(gun.maxAmmo);
	const float missileLoadedMass = previewMissileData_.baseMass + previewMissileData_.missileMass * static_cast<float>(previewMissileData_.maxMissiles);
	const float totalMassKg = af.emptyFrameMass + eng.mass + af.maxInternalFuel + gunLoadedMass + missileLoadedMass;
	const float weightN = totalMassKg * kGravity;
	const float wingArea = (std::max)(af.wingArea, 0.1f);
	const float wingLoading = totalMassKg / wingArea;

	const float thrustNormal = eng.baseThrust * eng.normalThrottleLimit;
	const float thrustWep = eng.baseThrust * eng.wepThrottleLimit;
	const float twrNormal = (weightN > 0.0f) ? (thrustNormal / weightN) : 0.0f;
	const float twrWep = (weightN > 0.0f) ? (thrustWep / weightN) : 0.0f;

	const float clMaxClean = (std::max)(af.maxLiftCoefficient, 0.1f);
	const float clMaxFlaps = (std::max)(af.maxLiftCoefficient + af.flapLiftBonus, 0.1f);
	const float stallSpeedClean = std::sqrt((2.0f * weightN) / (kSeaLevelDensity * wingArea * clMaxClean));
	const float stallSpeedFlaps = std::sqrt((2.0f * weightN) / (kSeaLevelDensity * wingArea * clMaxFlaps));

	const float totalCd0 = (std::max)(af.baseDrag + gun.drag + previewMissileData_.drag, 0.002f);
	auto solveLevelTopSpeed = [&](float thrustN) -> float {
		// 寄生抵抗 + 水平飛行時誘導抵抗 (L = W) の近似解を2分法で求める
		const float ar = (std::max)(af.aspectRatio, 0.5f);
		const float e = (std::max)(af.oswaldEfficiency, 0.1f);
		float lo = 20.0f;
		float hi = 1200.0f;
		for (int iter = 0; iter < 32; ++iter) {
			const float mid = 0.5f * (lo + hi);
			const float q = 0.5f * kSeaLevelDensity * mid * mid;
			const float clLevel = weightN / (q * wingArea);
			const float cdTotal = totalCd0 + (clLevel * clLevel) / (kPi * ar * e);
			const float drag = q * wingArea * cdTotal;
			if (drag < thrustN) {
				lo = mid;
			} else {
				hi = mid;
			}
		}
		return 0.5f * (lo + hi);
	};

	const float topSpeedNormal = solveLevelTopSpeed(thrustNormal);
	const float topSpeedWep = solveLevelTopSpeed(thrustWep);

	const float fuelRateNormal = eng.baseFuelFlowRate * eng.normalThrottleLimit;
	const float fuelRateWep = eng.baseFuelFlowRate * eng.wepThrottleLimit;
	const float enduranceNormalMin = (fuelRateNormal > 0.001f) ? (af.maxInternalFuel / fuelRateNormal) / 60.0f : 0.0f;
	const float enduranceWepMin = (fuelRateWep > 0.001f) ? (af.maxInternalFuel / fuelRateWep) / 60.0f : 0.0f;

	if (ImGui::CollapsingHeader("Mass & Weight Breakdown", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Airframe + Engine : %.0f kg", af.emptyFrameMass + eng.mass);
		ImGui::Text("Internal Fuel     : %.0f kg", af.maxInternalFuel);
		ImGui::Text("Gunpod + Missiles : %.0f kg (Gun: %.0f + MSL: %.0f)", gunLoadedMass + missileLoadedMass, gunLoadedMass, missileLoadedMass);
		ImGui::TextColored(ImVec4(0.45f, 0.95f, 1.0f, 1.0f), "Total Takeoff Mass: %.0f kg", totalMassKg);
		ImGui::Text("Wing Loading      : %.1f kg/m^2", wingLoading);
	}

	if (ImGui::CollapsingHeader("Flight Performance (Sea Level Est.)", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Thrust-to-Weight  : %.2f (Normal) | %.2f (WEP)", twrNormal, twrWep);
		ImGui::Text("Stall Speed Clean : %.1f m/s (%.0f km/h)", stallSpeedClean, stallSpeedClean * 3.6f);
		ImGui::Text("Stall Speed Flaps : %.1f m/s (%.0f km/h)", stallSpeedFlaps, stallSpeedFlaps * 3.6f);
		ImGui::Text("Top Speed Normal  : %.0f km/h (Mach %.2f)", topSpeedNormal * 3.6f, topSpeedNormal / kSpeedOfSoundSL);
		ImGui::Text("Top Speed WEP     : %.0f km/h (Mach %.2f)", topSpeedWep * 3.6f, topSpeedWep / kSpeedOfSoundSL);
		ImGui::Text("Fuel Endurance    : %.1f min (Normal) | %.1f min (WEP)", enduranceNormalMin, enduranceWepMin);
	}

	if (ImGui::CollapsingHeader("Aerodynamic & Propulsion Curves", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Include Flap Lift Bonus in CL Curve", &previewFlaps_);

		constexpr int kAoASamples = 91; // -45 deg .. +45 deg
		std::array<float, kAoASamples> clCurve{};
		for (int i = 0; i < kAoASamples; ++i) {
			const float deg = -45.0f + static_cast<float>(i);
			clCurve[i] = ComputeCLForGraph(af, deg * (kPi / 180.0f), previewFlaps_);
		}
		ImGui::PlotLines("CL vs AoA (-45..+45 deg)", clCurve.data(), kAoASamples, 0, nullptr, -2.5f, 3.5f, ImVec2(-1.0f, 90.0f));

		constexpr int kAltSamples = 31; // 0 km .. 15 km (step 0.5 km)
		std::array<float, kAltSamples> thrustCurve{};
		float maxThrustKN = 1.0f;
		for (int i = 0; i < kAltSamples; ++i) {
			const float altM = static_cast<float>(i) * 500.0f;
			const float altFactor = (std::max)(1.0f - eng.altitudeThrottleFactor * altM, 0.1f);
			const float thrustKN = (eng.baseThrust * eng.normalThrottleLimit * altFactor) * 0.001f;
			thrustCurve[i] = thrustKN;
			maxThrustKN = (std::max)(maxThrustKN, thrustKN);
		}
		ImGui::PlotLines("Thrust (kN) vs Alt (0..15km)", thrustCurve.data(), kAltSamples, 0, nullptr, 0.0f, maxThrustKN * 1.1f, ImVec2(-1.0f, 80.0f));
	}

	ImGui::End();
}

void AircraftEditorScene::DrawViewportBarWindow() {
	ImGui::SetNextWindowPos(ImVec2(486, 86), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(348, 225), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("Viewport & Gizmos")) {
		ImGui::End();
		return;
	}

	ImGui::TextDisabled("Camera: Right-Drag Orbit | Mid-Drag Pan | Wheel Zoom | WASD+QE");
	if (ImGui::Button("3/4 Front")) { SetCameraPreset(-2.45f, 0.25f, 19.0f); }
	ImGui::SameLine();
	if (ImGui::Button("Side")) { SetCameraPreset(-kPi * 0.5f, 0.05f, 19.0f); }
	ImGui::SameLine();
	if (ImGui::Button("Top")) { SetCameraPreset(0.0f, 1.42f, 22.0f); }
	ImGui::SameLine();
	if (ImGui::Button("Rear")) { SetCameraPreset(0.0f, 0.15f, 18.0f); }
	ImGui::SameLine();
	if (ImGui::Button("Nozzle")) { SetCameraPreset(0.45f, 0.08f, 9.5f, { 0.0f, 1.5f, -5.0f }); }
	ImGui::SameLine();
	if (ImGui::Button("Reset (R)")) { SetCameraPreset(-2.45f, 0.25f, 19.0f); }

	ImGui::Separator();

	ImGui::Checkbox("Turntable (Space)", &autoTurntable_);
	ImGui::SameLine();
	ImGui::Checkbox("Skybox", &isSkyboxVisible_);
	ImGui::SameLine();
	ImGui::Checkbox("Grid", &showGrid_);
	ImGui::SameLine();
	ImGui::Checkbox("Axes", &showAxes_);

	ImGui::Checkbox("Mesh Collider", &showMeshCollider_);
	ImGui::SameLine();
	ImGui::Checkbox("L-Click Ray Test", &enableMeshRaycastTest_);
	ImGui::SameLine();
	ImGui::Checkbox("CoG / CP", &showCoGAndCP_);
	ImGui::SameLine();
	ImGui::Checkbox("Colliders", &showSubColliders_);

	ImGui::Checkbox("Pylons", &showPylons_);
	ImGui::SameLine();
	ImGui::Checkbox("Nozzle Axis", &showNozzleGizmo_);
	ImGui::SameLine();
	ImGui::Checkbox("Gun Muzzles", &showGunMuzzles_);

	const auto& eng = aircraftConfig_.GetEngine();
	const float normalLimit = eng.normalThrottleLimit;
	const float wepLimit = eng.wepThrottleLimit;
	const float maxThrottle = (std::max)(wepLimit, 1.1f);
	ImGui::SliderFloat("Preview Throttle", &previewThrottle_, 0.0f, maxThrottle, "%.2f");
	const bool isPreviewWep = (wepLimit > normalLimit + 0.005f) && (previewThrottle_ > normalLimit + 0.005f);
	if (isPreviewWep) {
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.20f, 1.0f), "[WEP / A/B ON] (%.0f%%)", previewThrottle_ * 100.0f);
	} else {
		ImGui::TextDisabled("[DRY / A/B OFF] (%.0f%%)", previewThrottle_ * 100.0f);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("0%")) { previewThrottle_ = 0.0f; }
	ImGui::SameLine();
	if (ImGui::SmallButton("100% Dry")) { previewThrottle_ = normalLimit; }
	ImGui::SameLine();
	if (ImGui::SmallButton("WEP")) { previewThrottle_ = wepLimit; }
	ImGui::SliderFloat("Time of Day", &timeOfDay_, 0.0f, 24.0f, "%.1f h");
	ImGui::DragFloat3("Attitude (P/Y/R deg)", &previewAttitudeDeg_.x, 1.0f, -180.0f, 180.0f, "%.0f deg");
	if (ImGui::Button("Zero Attitude")) {
		previewAttitudeDeg_ = { 0.0f, 0.0f, 0.0f };
		turntableAngle_ = 0.0f;
	}

	ImGui::End();
}

#endif // USE_IMGUI
