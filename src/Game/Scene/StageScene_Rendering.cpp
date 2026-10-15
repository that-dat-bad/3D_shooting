#include "StageSceneInternal.h"

using namespace MyMath;

// ===========================================================
// 地面描画（タイル式グリッド ＆ シミュレータ空間Rez構築演出：項目7）
// ===========================================================
void StageScene::DrawGround() {
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (!camera) { return; }

	Vector3 aircraftPos = flightModel_.GetPosition();

	// --- 地面タイル（項目7: 4.5km四方の広大な大地） ---
	const float tileSize = 160.0f;
	const int tileRange = 14;

	// 機体位置をタイルサイズでスナップ（タイルの中心座標に丸める）
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

	// --- グリッドライン（160m間隔） ---
	// 仮想戦闘空間のタクティカルネオングリッド
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
}

// ============================================================
// ミッション目標HUD表示
// ============================================================
void StageScene::DrawMissionObjectivesHUD() {
	auto& mm = MissionManager::GetInstance();
	const auto& objectives = mm.GetCurrentMission().objectives;

	if (objectives.empty()) return;

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
// ベイパーコーン（プラントル・グロワート・シンギュラリティ）エフェクトの描画
// ============================================================
void StageScene::DrawVaporConeEffect(Camera* cam) {
	float speed = flightModel_.GetSpeed();
	float speedKmH = speed * 3.6f; // m/s から km/h に変換
	if (speedKmH < 1000.0f || speedKmH > 1200.0f) {
		return;
	}

	// 1000km/hで0.0、1100km/hで最大(1.0)、1200km/hで0.0になるフェードイン・アウト係数
	float speedFactor = 0.0f;
	if (speedKmH < 1100.0f) {
		speedFactor = (speedKmH - 1000.0f) / 100.0f;
	} else {
		speedFactor = (1200.0f - speedKmH) / 100.0f;
	}

	// 経過時間を用いたゆらぎの計算
	float t = totalTime_ * 40.0f; // 高速で揺らす

	// 基本位置・姿勢
	Vector3 aircraftPos = flightModel_.GetPosition();
	Vector3 forward = flightModel_.GetForwardDirection();
	MyMath::Quaternion aircraftQ = flightModel_.GetOrientation();

	// クォータニオンの乗算ヘルパー
	auto qMul = [](const MyMath::Quaternion& a, const MyMath::Quaternion& b) -> MyMath::Quaternion {
		return {
			a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
			a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
			a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
			a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
		};
	};

	// 軸と角度からクォータニオンを作成するヘルパー
	auto makeRotation = [](const MyMath::Vector3& axis, float angle) -> MyMath::Quaternion {
		float halfAngle = angle * 0.5f;
		float sinHalf = sinf(halfAngle);
		Vector3 normAxis = Normalize(axis);
		return {
			normAxis.x * sinHalf,
			normAxis.y * sinHalf,
			normAxis.z * sinHalf,
			cosf(halfAngle)
		};
	};

	// 描画用のテクスチャ取得
	uint32_t coneTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");

	// -------------------------------------------------------------
	// コーン1 (アウターシェル: 大きくて薄い)
	// -------------------------------------------------------------
	{
		float scaleNoise = 1.0f + 0.08f * sinf(t * 1.3f);
		float lengthNoise = 1.0f + 0.12f * cosf(t * 0.9f);
		float offsetNoise = 0.15f * sinf(t * 1.7f);

		float radius = 5.0f * scaleNoise;
		float length = 4.0f * lengthNoise;
		Vector3 scale = { radius, length, radius };

		Vector3 pos = Add(aircraftPos, Multiply(-0.8f + offsetNoise, forward));

		MyMath::Quaternion localPitch = makeRotation({1.0f, 0.0f, 0.0f}, 3.14159265f / 2.0f);
		MyMath::Quaternion localYaw = makeRotation({0.0f, 1.0f, 0.0f}, 0.03f * sinf(t * 0.8f));
		MyMath::Quaternion localRoll = makeRotation({0.0f, 0.0f, 1.0f}, t * 0.2f);
		MyMath::Quaternion localRot = qMul(localRoll, qMul(localYaw, localPitch));
		MyMath::Quaternion finalRot = qMul(aircraftQ, localRot);

		float alpha = 0.22f * speedFactor * (0.8f + 0.2f * sinf(t * 2.1f));
		Vector4 color = { 0.92f, 0.96f, 1.0f, alpha };

		PrimitiveModel::GetInstance()->DrawCone(scale, finalRot, pos, color, coneTex, cam, BlendMode::kNormal);
	}

	// -------------------------------------------------------------
	// コーン2 (インナーシェル: 小さくて濃い)
	// -------------------------------------------------------------
	{
		float scaleNoise = 1.0f + 0.06f * sinf(t * 1.5f + 1.0f);
		float lengthNoise = 1.0f + 0.10f * cosf(t * 1.1f + 0.5f);
		float offsetNoise = 0.10f * sinf(t * 2.0f + 2.0f);

		float radius = 3.5f * scaleNoise;
		float length = 5.0f * lengthNoise;
		Vector3 scale = { radius, length, radius };

		Vector3 pos = Add(aircraftPos, Multiply(-0.4f + offsetNoise, forward));

		MyMath::Quaternion localPitch = makeRotation({1.0f, 0.0f, 0.0f}, 3.14159265f / 2.0f);
		MyMath::Quaternion localYaw = makeRotation({0.0f, 1.0f, 0.0f}, 0.02f * sinf(t * 1.2f + 0.3f));
		MyMath::Quaternion localRoll = makeRotation({0.0f, 0.0f, 1.0f}, -t * 0.15f);
		MyMath::Quaternion localRot = qMul(localRoll, qMul(localYaw, localPitch));
		MyMath::Quaternion finalRot = qMul(aircraftQ, localRot);

		float alpha = 0.28f * speedFactor * (0.85f + 0.15f * cosf(t * 1.9f));
		Vector4 color = { 0.95f, 0.98f, 1.0f, alpha };

		PrimitiveModel::GetInstance()->DrawCone(scale, finalRot, pos, color, coneTex, cam, BlendMode::kNormal);
	}
}

// ============================================================
// 追従マズルフラッシュ（板ポリ・リング）の描画
// ============================================================
void StageScene::DrawMuzzleFlashEffect(Camera* cam) {
	if (muzzleFlashTimer_ <= 0.0f) {
		return;
	}

	Vector3 aircraftPos = flightModel_.GetPosition();
	Vector3 forward = flightModel_.GetForwardDirection();
	Vector3 muzzlePos = Add(aircraftPos, Multiply(8.0f, forward));
	
	float t = 1.0f - (muzzleFlashTimer_ / 0.08f); // 0.0(開始) -> 1.0(終了)
	if (t < 0.0f) { t = 0.0f; }
	if (t > 1.0f) { t = 1.0f; }
	
	// 機体の回転を取得（オイラー角）
	Vector3 aircraftRot = PlayerCamera::QuaternionToEuler(flightModel_.GetOrientation());
	
	float randomAlpha = muzzleFlashRandomAlpha_ * (1.0f - t);
	
	// 1. Flash (中心の閃光)
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

	// 2. Petals (星型の広がり)
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
