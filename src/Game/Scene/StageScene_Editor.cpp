#include "StageSceneInternal.h"

#ifdef USE_IMGUI
void StageScene::DrawAircraftTuningEditor() {
	if (ImGui::Begin("Aircraft Tuning (CFG)", &isAircraftTuningOpen_)) {
		// 1. CFGファイル管理
		if (ImGui::CollapsingHeader("CFG File Management", ImGuiTreeNodeFlags_DefaultOpen)) {
			std::vector<std::string> cfgFiles = AircraftConfig::GetAvailableAircraftList("Resources/aircraft");
			std::vector<std::string> fileNames;
			std::vector<const char*> fileNamePtrs;
			for (const auto& f : cfgFiles) {
				std::filesystem::path p(f);
				fileNames.push_back(p.filename().string());
			}
			for (const auto& fn : fileNames) {
				fileNamePtrs.push_back(fn.c_str());
			}

			if (ImGui::Combo("Select Aircraft CFG", &selectedAircraftConfigIndex_, fileNamePtrs.data(), static_cast<int>(fileNamePtrs.size()))) {
			}

			if (ImGui::Button("Load Selected CFG")) {
				if (selectedAircraftConfigIndex_ >= 0 && selectedAircraftConfigIndex_ < cfgFiles.size()) {
					aircraftConfig_.LoadFromCfg(cfgFiles[selectedAircraftConfigIndex_]);
					strcpy_s(tempAircraftConfigName_, sizeof(tempAircraftConfigName_), std::filesystem::path(cfgFiles[selectedAircraftConfigIndex_]).filename().string().c_str());
					Restart();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Save (Overwrite)")) {
				std::string savePath = aircraftConfig_.GetCurrentFilePath();
				if (savePath.empty()) {
					savePath = "Resources/aircraft/" + std::string(tempAircraftConfigName_);
				}
				aircraftConfig_.SaveToCfg(savePath);
			}

			ImGui::Separator();
			ImGui::InputText("New CFG Name", tempAircraftConfigName_, sizeof(tempAircraftConfigName_));
			if (ImGui::Button("Save As New CFG")) {
				std::string newPath = "Resources/aircraft/" + std::string(tempAircraftConfigName_);
				if (newPath.find(".cfg") == std::string::npos) {
					newPath += ".cfg";
				}
				aircraftConfig_.SaveToCfg(newPath);
				aircraftConfig_.SetCurrentFilePath(newPath);
			}
		}

		auto& general = aircraftConfig_.GetGeneral();
		auto& airframe = aircraftConfig_.GetAirframe();
		auto& engine = aircraftConfig_.GetEngine();
		auto& gunpod = aircraftConfig_.GetGunpod();

		// 2. [General & Visual]
		if (ImGui::CollapsingHeader("General & Visual", ImGuiTreeNodeFlags_DefaultOpen)) {
			static char nameBuf[128];
			strcpy_s(nameBuf, sizeof(nameBuf), general.name.c_str());
			if (ImGui::InputText("Aircraft Name", nameBuf, sizeof(nameBuf))) {
				general.name = nameBuf;
			}

			static char modelBuf[256];
			strcpy_s(modelBuf, sizeof(modelBuf), general.modelPath.c_str());
			if (ImGui::InputText("Model Path", modelBuf, sizeof(modelBuf))) {
				general.modelPath = modelBuf;
			}

			ImGui::SliderFloat("Env Reflection", &general.environmentCoefficient, 0.0f, 1.0f);
			ImGui::SliderFloat("Specular Intensity", &general.specularIntensity, 0.0f, 5.0f);
			ImGui::SliderFloat("Shininess", &general.shininess, 1.0f, 200.0f);
		}

		// 3. [Airframe]
		if (ImGui::CollapsingHeader("Airframe & Aerodynamics", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat("Empty Mass (kg)", &airframe.emptyFrameMass, 50.0f, 500.0f, 30000.0f);
			ImGui::DragFloat("Max Fuel (kg)", &airframe.maxInternalFuel, 10.0f, 0.0f, 10000.0f);
			ImGui::SliderFloat("Base Drag (Cd0)", &airframe.baseDrag, 0.001f, 0.2f, "%.4f");
			ImGui::SliderFloat("Lift Coeff (CL0)", &airframe.liftCoefficient, 0.0f, 1.5f);
			ImGui::DragFloat("Wing Area (m^2)", &airframe.wingArea, 0.5f, 5.0f, 200.0f);
			ImGui::DragFloat("Max Health (HP)", &airframe.maxHealth, 5.0f, 10.0f, 2000.0f);

			ImGui::Separator();
			ImGui::Text("--- Lift & Stall Characteristics ---");
			ImGui::SliderFloat("Critical AoA (rad)", &airframe.criticalAoA, 0.1f, 1.0f);
			ImGui::SliderFloat("Max Lift Coeff (CLmax)", &airframe.maxLiftCoefficient, 0.5f, 3.5f);
			ImGui::SliderFloat("Stall Lift Coeff", &airframe.stallLiftCoefficient, 0.05f, 1.0f);

			ImGui::Separator();
			ImGui::Text("--- Induced Drag & G Limits ---");
			ImGui::SliderFloat("Aspect Ratio", &airframe.aspectRatio, 1.0f, 20.0f);
			ImGui::SliderFloat("Oswald Efficiency", &airframe.oswaldEfficiency, 0.3f, 1.0f);
			ImGui::SliderFloat("Positive G Limit", &airframe.positiveGLimit, 2.0f, 20.0f);
			ImGui::SliderFloat("Negative G Limit", &airframe.negativeGLimit, -10.0f, 0.0f);

			ImGui::Separator();
			ImGui::Text("--- Flaps & Airbrake ---");
			ImGui::SliderFloat("Flap Lift Bonus", &airframe.flapLiftBonus, 0.0f, 2.0f);
			ImGui::SliderFloat("Flap Drag Bonus", &airframe.flapDragBonus, 0.0f, 0.5f);
			ImGui::DragFloat("Flap Max Speed (m/s)", &airframe.flapMaxSpeed, 1.0f, 30.0f, 300.0f);
			ImGui::SliderFloat("Flap Deploy Speed", &airframe.flapDeploySpeed, 0.5f, 10.0f);

			ImGui::SliderFloat("Airbrake Drag Bonus", &airframe.airBrakeDragBonus, 0.0f, 1.0f);
			ImGui::SliderFloat("Airbrake Deploy Speed", &airframe.airBrakeDeploySpeed, 0.5f, 10.0f);
		}

		// 4. [Engine]
		if (ImGui::CollapsingHeader("Engine & Propulsion", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat("Engine Mass (kg)", &engine.mass, 10.0f, 50.0f, 5000.0f);
			ImGui::DragFloat("Base Thrust (N)", &engine.baseThrust, 1000.0f, 1000.0f, 500000.0f, "%.0f N");
			ImGui::SliderFloat("Normal Throttle Limit", &engine.normalThrottleLimit, 0.5f, 1.0f);
			ImGui::SliderFloat("WEP Throttle Limit", &engine.wepThrottleLimit, 1.0f, 2.0f);
			ImGui::SliderFloat("Spool Speed", &engine.physicalSpoolSpeed, 0.05f, 5.0f);
			ImGui::SliderFloat("Fuel Flow Rate (kg/s)", &engine.baseFuelFlowRate, 0.1f, 20.0f);
			ImGui::SliderFloat("Altitude Thrust Factor", &engine.altitudeThrottleFactor, 0.0f, 0.0002f, "%.6f");
		}

		// 5. [Gunpod]
		if (ImGui::CollapsingHeader("Gunpod & Armament", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat("Gunpod Mass (kg)", &gunpod.baseMass, 5.0f, 10.0f, 500.0f);
			ImGui::SliderFloat("Gunpod Drag (Cd)", &gunpod.drag, 0.0f, 0.05f, "%.4f");
			ImGui::SliderFloat("Ammo Weight (kg/rd)", &gunpod.ammoWeight, 0.01f, 1.0f);
			ImGui::DragInt("Max Ammo", &gunpod.maxAmmo, 10, 50, 5000);
			ImGui::SliderFloat("Fire Rate (rds/s)", &gunpod.fireRate, 5.0f, 100.0f);
		}

		ImGui::Separator();

		// 即座に適用
		if (ImGui::Button("Apply to Current Aircraft", ImVec2(-1, 35))) {
			Vector3 curPos = flightModel_.GetPosition();
			Vector3 curVel = flightModel_.GetVelocity();
			Quaternion curOri = flightModel_.GetOrientation();

			flightModel_.Initialize(airframe, engine);
			flightModel_.SetPosition(curPos);
			flightModel_.SetVelocity(curVel);
			flightModel_.SetOrientation(curOri);

			gunpod_.Initialize(gunpod);
		}

		if (ImGui::Button("Save & Restart Stage", ImVec2(-1, 35))) {
			std::string savePath = aircraftConfig_.GetCurrentFilePath();
			if (savePath.empty()) {
				savePath = "Resources/aircraft/mig21.cfg";
			}
			aircraftConfig_.SaveToCfg(savePath);
			Restart();
		}
	}
	ImGui::End();
}
#else
void StageScene::DrawAircraftTuningEditor() {}
#endif
