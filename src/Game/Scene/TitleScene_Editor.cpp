#include "TitleSceneInternal.h"

#ifdef USE_IMGUI
void TitleScene::DrawAfterburnerEditor() {
	if (!afterburner_) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(480, 600), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("AFTERBURNER ASSET EDITOR", &showAfterburnerEditor_)) {
		ImGui::End();
		return;
	}

	LocationData& tunnelLoc = locations_[static_cast<int>(TitleLocation::Tunnel)];

	ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.55f, 1.0f), "/// AFTERBURNER & PROPULSION ASSET EDITOR ///");
	ImGui::TextDisabled("Real-time nozzle offset, flame geometry, shock diamonds, color & JSON pipeline");
	ImGui::Separator();

	// ------------------------------------------------------------
	// 1. エンジン構成 (Engine Mode)
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("1. Engine Configuration (Single / Twin)", ImGuiTreeNodeFlags_DefaultOpen)) {
		bool isTwin = afterburner_->IsTwinEngine();
		if (ImGui::Checkbox("Twin Engine Mode", &isTwin)) {
			afterburner_->SetTwinEngine(isTwin);
		}

		if (isTwin) {
			MyMath::Vector3 left = afterburner_->GetLeftNozzleOffset();
			MyMath::Vector3 right = afterburner_->GetRightNozzleOffset();
			bool changed = false;
			if (ImGui::DragFloat3("Left Nozzle Offset (X,Y,Z)", &left.x, 0.005f)) { changed = true; }
			if (ImGui::DragFloat3("Right Nozzle Offset (X,Y,Z)", &right.x, 0.005f)) { changed = true; }
			if (changed) {
				afterburner_->SetNozzleOffsets(left, right);
			}
		} else {
			MyMath::Vector3 single = afterburner_->GetSingleNozzleOffset();
			if (ImGui::DragFloat3("Single Nozzle Offset (X,Y,Z)", &single.x, 0.005f)) {
				afterburner_->SetSingleNozzleOffset(single);
			}
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 0.9f), "Mig-21 standard single nozzle: (0.000, 0.011, 14.950)");
		}

		// ノズル噴射角（向き・オイラー角）
		MyMath::Vector3 nRot = afterburner_->GetNozzleRotation();
		MyMath::Vector3 nRotDeg = {
			nRot.x * 180.0f / 3.14159265f,
			nRot.y * 180.0f / 3.14159265f,
			nRot.z * 180.0f / 3.14159265f
		};
		if (ImGui::DragFloat3("Nozzle Angle (Pitch, Yaw, Roll deg)", &nRotDeg.x, 0.5f, -180.0f, 180.0f, "%.1f deg")) {
			afterburner_->SetNozzleRotation({
				nRotDeg.x * 3.14159265f / 180.0f,
				nRotDeg.y * 3.14159265f / 180.0f,
				nRotDeg.z * 3.14159265f / 180.0f
			});
		}

		if (ImGui::Button("Flip Flame Direction (Yaw 180 deg)")) {
			MyMath::Vector3 r = afterburner_->GetNozzleRotation();
			r.y += 3.14159265f;
			if (r.y > 3.14159265f) { r.y -= 6.2831853f; }
			afterburner_->SetNozzleRotation(r);
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Angle (0, 0, 0)")) {
			afterburner_->SetNozzleRotation({ 0.0f, 0.0f, 0.0f });
		}
	}

	// ------------------------------------------------------------
	// 2. 炎のジオメトリ (Flame Geometry)
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("2. Flame Geometry & Shock Diamonds", ImGuiTreeNodeFlags_DefaultOpen)) {
		float radius = afterburner_->GetFlameRadius();
		if (ImGui::SliderFloat("Flame Radius (m)", &radius, 0.05f, 1.5f, "%.3f m")) {
			afterburner_->SetFlameRadius(radius);
		}

		float length = afterburner_->GetFlameLength();
		if (ImGui::SliderFloat("Flame Length (m)", &length, 0.5f, 20.0f, "%.2f m")) {
			afterburner_->SetFlameLength(length);
		}

		int diamonds = afterburner_->GetShockDiamondCount();
		if (ImGui::SliderInt("Shock Diamonds (Mach Disks)", &diamonds, 0, 10)) {
			afterburner_->SetShockDiamondCount(diamonds);
		}
	}

	// ------------------------------------------------------------
	// 3. カラー & 発光強度 (Color & Intensity)
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("3. Color & Glow Intensity", ImGuiTreeNodeFlags_DefaultOpen)) {
		MyMath::Vector3 flameColor = afterburner_->GetFlameColor();
		if (ImGui::ColorEdit3("Outer Flame Color", &flameColor.x)) {
			afterburner_->SetFlameColor(flameColor);
		}

		MyMath::Vector3 coreColor = afterburner_->GetCoreColor();
		if (ImGui::ColorEdit3("Shock Diamond / Core Color", &coreColor.x)) {
			afterburner_->SetCoreColor(coreColor);
		}

		float intensity = afterburner_->GetIntensity();
		if (ImGui::SliderFloat("AB Throttle / Intensity", &intensity, 0.0f, 3.0f, "%.2f")) {
			afterburner_->SetIntensity(intensity);
		}
	}

	// ------------------------------------------------------------
	// 4. 機体の向き・アライメント調整 (Aircraft Orientation)
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("4. Aircraft Orientation & Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Flight Motion / Sway Animation", &enableTunnelMotion_);
		ImGui::DragFloat3("Aircraft Rotation", &tunnelLoc.aircraftRotation.x, 0.01f);

		if (ImGui::Button("Flip 180 deg (Reverse)")) {
			tunnelLoc.aircraftRotation.y += 3.14159265f;
			if (tunnelLoc.aircraftRotation.y > 6.2831853f) {
				tunnelLoc.aircraftRotation.y -= 6.2831853f;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Align Forward (+Z)")) {
			tunnelLoc.aircraftRotation = { 0.0f, 3.14159265f, 0.0f };
			if (tunnelLoc.visualModel) {
				tunnelLoc.visualModel->SetBaseRotation({ 0.0f, 0.0f, 0.0f });
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Mig-21 Preset")) {
			afterburner_->SetTwinEngine(false);
			afterburner_->SetSingleNozzleOffset({ 0.0f, 0.011f, 14.95f });
			afterburner_->SetNozzleRotation({ 0.0f, 0.0f, 0.0f });
			afterburner_->SetFlameRadius(0.38f);
			afterburner_->SetFlameLength(6.2f);
			afterburner_->SetShockDiamondCount(5);
			afterburner_->SetFlameColor({ 0.28f, 0.65f, 1.0f });
			afterburner_->SetCoreColor({ 0.88f, 0.95f, 1.0f });
			afterburner_->SetIntensity(1.35f);
		}
	}

	// ------------------------------------------------------------
	// 5. JSON 保存 & 読み込み (Asset Pipeline)
	// ------------------------------------------------------------
	if (ImGui::CollapsingHeader("5. Asset Pipeline (JSON Save/Load)", ImGuiTreeNodeFlags_DefaultOpen)) {
		char pathBuf[256];
		strncpy_s(pathBuf, afterburnerConfigPath_.c_str(), sizeof(pathBuf) - 1);
		if (ImGui::InputText("JSON Path", pathBuf, sizeof(pathBuf))) {
			afterburnerConfigPath_ = pathBuf;
		}

		if (ImGui::Button("Save Config")) {
			if (afterburner_->SaveConfig(afterburnerConfigPath_)) {
				abEditorStatusMsg_ = "SUCCESS: Saved to " + afterburnerConfigPath_;
				abEditorStatusTimer_ = 4.0f;
			} else {
				abEditorStatusMsg_ = "ERROR: Failed to save to " + afterburnerConfigPath_;
				abEditorStatusTimer_ = 5.0f;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Load Config")) {
			if (afterburner_->LoadConfig(afterburnerConfigPath_)) {
				abEditorStatusMsg_ = "SUCCESS: Loaded from " + afterburnerConfigPath_;
				abEditorStatusTimer_ = 4.0f;
			} else {
				abEditorStatusMsg_ = "ERROR: Failed to load from " + afterburnerConfigPath_;
				abEditorStatusTimer_ = 5.0f;
			}
		}

		if (abEditorStatusTimer_ > 0.0f) {
			abEditorStatusTimer_ -= 1.0f / 60.0f;
			if (abEditorStatusMsg_.find("SUCCESS") != std::string::npos) {
				ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", abEditorStatusMsg_.c_str());
			} else {
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", abEditorStatusMsg_.c_str());
			}
		}
	}

	ImGui::End();
}
#endif
