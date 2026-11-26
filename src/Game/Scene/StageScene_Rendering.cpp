#include "StageSceneInternal.h"

using namespace MyMath;

// ===========================================================
// 地面描画
// ===========================================================
void StageScene::DrawGround() {
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (!camera) { return; }

	Vector3 aircraftPos = flightModel_.GetPosition();

	// --- 地面タイル ---
	const float tileSize = 160.0f;
	const int tileRange = 14;

	// 機体位置をタイルサイズでスナップ
	float snapX = std::floor(aircraftPos.x / tileSize) * tileSize;
	float snapZ = std::floor(aircraftPos.z / tileSize) * tileSize;

	// タイルを敷き詰めて描画
	for (int ix = -tileRange; ix <= tileRange; ix++) {
		for (int iz = -tileRange; iz <= tileRange; iz++) {
			float cx = snapX + ix * tileSize;
			float cz = snapZ + iz * tileSize;

			// チェッカーボードパターンで色を交互に変える
			int gridX = static_cast<int>(std::floor(cx / tileSize));
			int gridZ = static_cast<int>(std::floor(cz / tileSize));
			bool isDark = ((gridX + gridZ) % 2 != 0);

			Vector4 color = isDark
				? Vector4{ 0.18f, 0.26f, 0.18f, 1.0f }   // 落ち着いた軍用ダークオリーブグリーン
				: Vector4{ 0.24f, 0.35f, 0.22f, 1.0f };   // 自然なミリタリーグリーン

			Vector3 scale = { tileSize, 1.0f, tileSize };
			Vector3 rotate = { 0.0f, 0.0f, 0.0f };
			Vector3 translate = { cx + tileSize * 0.5f, 0.0f, cz + tileSize * 0.5f };

			PrimitiveModel::GetInstance()->DrawPlane(
				scale, rotate, translate, color,
				groundTextureIndex_, camera
			);
		}
	}

	// グリッドライン
	const float lineExtent = tileRange * tileSize;
	const Vector4 lineCol = { 0.18f, 0.55f, 0.32f, 0.70f };

	for (int i = -tileRange; i <= tileRange; i++) {
		float x = snapX + i * tileSize;
		PrimitiveModel::GetInstance()->DrawLine3D(
			{ x, 0.15f, snapZ - lineExtent },
			{ x, 0.15f, snapZ + lineExtent },
			lineCol, camera
		);
	}

	for (int i = -tileRange; i <= tileRange; i++) {
		float z = snapZ + i * tileSize;
		PrimitiveModel::GetInstance()->DrawLine3D(
			{ snapX - lineExtent, 0.15f, z },
			{ snapX + lineExtent, 0.15f, z },
			lineCol, camera
		);
	}

	// 自機の地面投影シャドウ
	if (!isGameOver_ && aircraftPos.y > 0.0f && aircraftPos.y < 800.0f) {
		float altFade = std::clamp(1.0f - aircraftPos.y / 800.0f, 0.0f, 1.0f);
		float shadowScale = 14.0f + aircraftPos.y * 0.02f;
		uint32_t softShadowTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");
		Vector3 aircraftRot = PlayerCamera::QuaternionToEuler(flightModel_.GetOrientation());
		PrimitiveModel::GetInstance()->DrawPlane(
			{ shadowScale * 0.65f, 1.0f, shadowScale },
			{ 0.0f, aircraftRot.y, 0.0f },
			{ aircraftPos.x, 0.25f, aircraftPos.z },
			{ 0.0f, 0.0f, 0.0f, 0.55f * altFade },
			softShadowTex, camera
		);
	}
}

void StageScene::DrawMissionReachAreaRings(Camera* cam) {
	if (!cam) {
		return;
	}
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim) {
		return;
	}

	const auto& objectives = MissionManager::GetInstance().GetCurrentMission().objectives;
	const uint32_t ringTex = TextureManager::GetInstance()->GetTextureIndexByFilePath(StageRadioDefaults::kRingTexPath);
	const float spinAngle = totalTime_ * StageRadioDefaults::kReachAreaRingSpinSpeed;

	auto drawDashedStreamer = [&](const Vector3& fromPos, const Vector3& toPos, const Vector4& col) {
		constexpr int kDashCount = 14;
		const float phase = std::fmod(totalTime_ * 1.6f, 1.0f);
		for (int i = 0; i < kDashCount; ++i) {
			float t0 = (static_cast<float>(i) + phase) / static_cast<float>(kDashCount);
			if (t0 >= 1.0f) {
				t0 -= 1.0f;
			}
			const float t1 = (std::min)(1.0f, t0 + (0.45f / static_cast<float>(kDashCount)));
			if (t1 <= t0) {
				continue;
			}
			const Vector3 p0 = Lerp(fromPos, toPos, t0);
			const Vector3 p1 = Lerp(fromPos, toPos, t1);
			prim->DrawLine3D(p0, p1, col, cam);
		}
	};

	const MissionObjective* prevGate = nullptr;
	for (size_t i = 0; i < objectives.size(); ++i) {
		const auto& obj = objectives[i];
		if (obj.type != ObjectiveType::ReachArea) {
			continue;
		}

		if (obj.enableGatePassCheck) {
			if (obj.status == ObjectiveStatus::Completed) {
				prevGate = &obj;
				if (obj.passFlashTimer > 0.0f) {
					const float t = std::clamp(1.0f - (obj.passFlashTimer / 0.55f), 0.0f, 1.0f);
					const float flashRadius = 52.0f * (1.0f + t * 0.85f);
					const Vector4 flashCol = { 1.0f, 1.0f, 1.0f, 0.90f * (1.0f - t) };
					prim->DrawRing(
						{ flashRadius, flashRadius, 1.0f },
						{ 0.0f, 0.0f, spinAngle },
						obj.areaCenter,
						flashCol,
						ringTex,
						cam,
						BlendMode::kAdd);
				}
				continue;
			}
			if (obj.status != ObjectiveStatus::Active) {
				continue;
			}

			const float pulse = 0.85f + 0.15f * std::sin(totalTime_ * 4.5f);
			constexpr float kGateRadius = 52.0f;
			constexpr float kTickLength = 16.0f;
			const Vector4 mainCol = { 0.28f, 1.00f, 0.58f, 0.92f * pulse };
			const Vector4 glowCol = { 0.28f, 1.00f, 0.58f, 0.38f * pulse };
			const Vector4 tickCol = { 0.35f, 1.00f, 0.68f, 0.95f * pulse };

			prim->DrawRing(
				{ kGateRadius, kGateRadius, 1.0f },
				{ 0.0f, 0.0f, spinAngle },
				obj.areaCenter,
				mainCol,
				ringTex,
				cam,
				BlendMode::kAdd);
			prim->DrawRing(
				{ kGateRadius * 1.25f, kGateRadius * 1.25f, 1.0f },
				{ 0.0f, 0.0f, -spinAngle * 0.6f },
				obj.areaCenter,
				glowCol,
				ringTex,
				cam,
				BlendMode::kAdd);

			const Vector3& c = obj.areaCenter;
			prim->DrawLine3D({ c.x + kGateRadius, c.y, c.z }, { c.x + kGateRadius + kTickLength, c.y, c.z }, tickCol, cam);
			prim->DrawLine3D({ c.x - kGateRadius, c.y, c.z }, { c.x - kGateRadius - kTickLength, c.y, c.z }, tickCol, cam);
			prim->DrawLine3D({ c.x, c.y + kGateRadius, c.z }, { c.x, c.y + kGateRadius + kTickLength, c.z }, tickCol, cam);
			prim->DrawLine3D({ c.x, c.y - kGateRadius, c.z }, { c.x, c.y - kGateRadius - kTickLength, c.z }, tickCol, cam);

			const Vector3 streamerStart = prevGate
				? prevGate->areaCenter
				: Add(flightModel_.GetPosition(), Multiply(40.0f, flightModel_.GetForwardDirection()));
			drawDashedStreamer(streamerStart, c, { 0.30f, 1.00f, 0.65f, 0.75f });

			for (size_t j = i + 1; j < objectives.size(); ++j) {
				const auto& nextObj = objectives[j];
				if (nextObj.type == ObjectiveType::ReachArea && nextObj.enableGatePassCheck) {
					prim->DrawRing(
						{ kGateRadius, kGateRadius, 1.0f },
						{ 0.0f, 0.0f, spinAngle * 0.7f },
						nextObj.areaCenter,
						{ 0.22f, 0.85f, 0.72f, 0.55f },
						ringTex,
						cam,
						BlendMode::kAdd);
					drawDashedStreamer(c, nextObj.areaCenter, { 0.22f, 0.85f, 0.72f, 0.35f });
					break;
				}
			}
			continue;
		}

		if (obj.status != ObjectiveStatus::Active) {
			continue;
		}

		if (obj.enableBoundaryLineCheck) {
			if (obj.boundaryCrossed) {
				continue;
			}
			const Vector3& c = obj.areaCenter;
			prim->DrawRing(
				{ 64.0f, 64.0f, 1.0f },
				{ 0.0f, 0.0f, spinAngle },
				c,
				{ 0.28f, 0.92f, 1.00f, 0.90f },
				ringTex,
				cam,
				BlendMode::kAdd);
			prim->DrawLine3D(
				{ c.x - 65.0f, c.y, c.z },
				{ c.x + 65.0f, c.y, c.z },
				{ 0.28f, 0.92f, 1.00f, 0.85f },
				cam);
			continue;
		}

		if (obj.startDelaySeconds > 0.0f) {
			continue;
		}

		prim->DrawRing(
			{ obj.areaRadius, obj.areaRadius, StageRadioDefaults::kReachAreaRingDepthScale },
			{ StageRadioDefaults::kZeroTimeLimit, StageRadioDefaults::kZeroTimeLimit, spinAngle },
			obj.areaCenter,
			StageRadioDefaults::kReachAreaRingColor,
			ringTex,
			cam,
			BlendMode::kAdd);
	}
}

// ============================================================
// ミッション目標HUD表示
// ============================================================
void StageScene::DrawMissionObjectivesHUD() {
	auto& mm = MissionManager::GetInstance();
	const auto& objectives = mm.GetCurrentMission().objectives;

	if (objectives.empty()) return;
	for (const auto& obj : objectives) {
		if (!obj.guideTitle.empty()) {
			return;
		}
	}

	// 画面右上に目標リストを表示
	float startX = WinApp::kClientWidth - 350.0f;
	float startY = 60.0f;

#ifdef USE_IMGUI
	ImGui::SetNextWindowPos(ImVec2(startX, startY), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.5f);

	ImGui::Begin("Mission Objectives", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs);

	ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "MISSION OBJECTIVES");
	ImGui::Separator();

	for (const auto& obj : objectives) {
		if (obj.isHidden && obj.status != ObjectiveStatus::Completed) continue;

		ImVec4 color;
		const char* statusIcon;
		switch (obj.status) {
		case ObjectiveStatus::Completed: color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); statusIcon = "[OK]"; break;
		case ObjectiveStatus::Failed:    color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); statusIcon = "[X]"; break;
		case ObjectiveStatus::Inactive:  continue; // 非表示
		default:                         color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); statusIcon = "[ ]"; break;
		}

		std::string prefix = obj.isPrimary ? "" : "  ";
		ImGui::TextColored(color, "%s%s %s", prefix.c_str(), statusIcon, obj.name.c_str());
	}

	ImGui::End();
#endif
}

// ============================================================
// Phase 1: トリガーメッセージのHUD表示
// ============================================================
void StageScene::DrawMissionMessages() {
	auto& mm = MissionManager::GetInstance();
	const auto& messages = mm.GetActiveMessages();

	if (messages.empty()) return;

#ifdef USE_IMGUI
	// 画面中央上部にメッセージを表示
	float centerX = WinApp::kClientWidth * 0.5f;
	float msgY = 80.0f;

	ImGui::SetNextWindowPos(ImVec2(centerX - 250.0f, msgY), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.7f);

	ImGui::Begin("MissionMessages", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs);

	for (const auto& msg : messages) {
		// フェードアウト効果
		float alpha = (std::min)(msg.remainingTime, 1.0f);
		ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.5f, alpha), "%s", msg.text.c_str());
	}

	ImGui::End();
#endif
}

// ============================================================
// 追従マズルフラッシュの描画
// ============================================================
void StageScene::DrawMuzzleFlashEffect(Camera* cam) {
	if (muzzleFlashTimer_ <= 0.0f) {
		return;
	}

	Vector3 aircraftPos = flightModel_.GetPosition();
	Vector3 right = flightModel_.GetRightDirection();
	Vector3 up = flightModel_.GetUpDirection();
	Vector3 forward = flightModel_.GetForwardDirection();
	Vector3 muzzlePos = Add(
		aircraftPos,
		Add(
			Add(Multiply(lastMuzzleLocalOffset_.x, right), Multiply(lastMuzzleLocalOffset_.y, up)),
			Multiply(lastMuzzleLocalOffset_.z, forward)
		)
	);
	
	float t = 1.0f - (muzzleFlashTimer_ / 0.08f); // 0.0 -> 1.0
	if (t < 0.0f) { t = 0.0f; }
	if (t > 1.0f) { t = 1.0f; }
	
	// 機体の回転を取得
	Vector3 aircraftRot = PlayerCamera::QuaternionToEuler(flightModel_.GetOrientation());
	
	float randomAlpha = muzzleFlashRandomAlpha_ * (1.0f - t);
	
	// 1. Flash
	uint32_t flashTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");
	Vector4 flashColor = { 1.0f, 0.7f, 0.2f, randomAlpha }; // 燃えるようなオレンジイエロー
	
	float flashLength = 4.0f * muzzleFlashRandomScale_ * (1.0f - t);
	float flashWidth = 1.0f * muzzleFlashRandomScale_ * (1.0f - t);
	Vector3 flashScale = { flashWidth, flashLength, 1.0f };
	Vector3 flashPos = Add(muzzlePos, Multiply(flashLength * 0.5f, forward));
	
	for (int i = 0; i < 2; i++) {
		Vector3 rot = aircraftRot;
		rot.x -= 3.14159265f / 2.0f;
		rot.z += muzzleFlashRandomRoll_ + (i * 3.14159f / 2.0f);
		PrimitiveModel::GetInstance()->DrawPlane(flashScale, rot, flashPos, flashColor, flashTex, cam, BlendMode::kAdd);
	}

	// 2. Petals
	uint32_t petalTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");
	Vector4 petalColor = { 1.0f, 0.4f, 0.05f, randomAlpha }; // 濃いオレンジ色
	
	float petalLength = 3.0f * muzzleFlashRandomScale_ * (1.0f - t);
	float petalWidth = 0.5f * muzzleFlashRandomScale_ * (1.0f - t);
	Vector3 petalScale = { petalWidth, petalLength, 1.0f };
	
	for (int i = 0; i < 4; i++) {
		Vector3 rot = aircraftRot;
		rot.z += muzzleFlashRandomRoll_ + (i * 3.14159f / 4.0f);
		PrimitiveModel::GetInstance()->DrawPlane(petalScale, rot, muzzlePos, petalColor, petalTex, cam, BlendMode::kAdd);
	}
}
