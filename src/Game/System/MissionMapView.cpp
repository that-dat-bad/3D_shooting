#include "MissionMapView.h"
#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif
#include <cmath>

void MissionMapView::Initialize() {
	cameraPos_ = { 0.0f, 0.0f };
	zoom_ = 0.5f; // 初期ズーム
}

Vector2 MissionMapView::WorldToScreen(const MyMath::Vector3& worldPos, const Vector2& canvasSize, const Vector2& canvasPos) const {
	Vector2 center = { canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f };
	float screenX = center.x + (worldPos.x - cameraPos_.x) * zoom_;
	float screenY = center.y + (worldPos.z - cameraPos_.y) * zoom_; // ワールドZをスクリーンYにマッピング
	return { screenX, screenY };
}

MyMath::Vector3 MissionMapView::ScreenToWorld(const Vector2& screenPos, const Vector2& canvasSize, const Vector2& canvasPos) const {
	Vector2 center = { canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f };
	float worldX = (screenPos.x - center.x) / zoom_ + cameraPos_.x;
	float worldZ = (screenPos.y - center.y) / zoom_ + cameraPos_.y;
	return { worldX, 0.0f, worldZ }; // Y(高度)は0として返す
}

void MissionMapView::Draw(MissionData& missionData, const MyMath::Vector3& playerPos) {
#ifdef USE_IMGUI
	ImGui::Begin("Tactical Map Editor (WT Style)");

	// ツールバー
	if (ImGui::Button("Reset Camera")) {
		cameraPos_ = { playerPos.x, playerPos.z };
		zoom_ = 0.5f;
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Path")) {
		WaypointPath newPath;
		newPath.name = "Path_" + std::to_string(missionData.waypointPaths.size());
		missionData.waypointPaths.push_back(newPath);
		selectedWaypointPathIndex_ = static_cast<int>(missionData.waypointPaths.size()) - 1;
		selectedWaypointIndex_ = -1;
	}
	ImGui::SameLine();
	if (selectedWaypointPathIndex_ >= 0 && selectedWaypointPathIndex_ < (int)missionData.waypointPaths.size()) {
		if (ImGui::Button("Add Waypoint")) {
			Waypoint wp;
			MyMath::Vector3 centerWorld = ScreenToWorld({ImGui::GetWindowPos().x + ImGui::GetWindowSize().x*0.5f, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y*0.5f}, 
				{ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y}, 
				{ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y});
			wp.position = { centerWorld.x, 200.0f, centerWorld.z }; // 高度200m
			missionData.waypointPaths[selectedWaypointPathIndex_].points.push_back(wp);
			selectedWaypointIndex_ = static_cast<int>(missionData.waypointPaths[selectedWaypointPathIndex_].points.size()) - 1;
		}
		ImGui::SameLine();
		ImGui::Checkbox("Loop Path", &missionData.waypointPaths[selectedWaypointPathIndex_].isLoop);
		ImGui::SameLine();
	}
	ImGui::Text("Zoom: %.2f", zoom_);

	ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
	ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
	if (canvas_sz.x < 100.0f) canvas_sz.x = 100.0f;
	if (canvas_sz.y < 100.0f) canvas_sz.y = 100.0f;
	ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(30, 40, 50, 255));
	draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

	ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool is_hovered = ImGui::IsItemHovered();
	const bool is_active = ImGui::IsItemActive();
	const ImVec2 origin(canvas_p0.x, canvas_p0.y);
	const ImVec2 mouse_pos_in_canvas(ImGui::GetIO().MousePos.x - origin.x, ImGui::GetIO().MousePos.y - origin.y);
	
	Vector2 canvasSizeVec = { canvas_sz.x, canvas_sz.y };
	Vector2 canvasPosVec = { canvas_p0.x, canvas_p0.y };

	// ズーム処理
	if (is_hovered && ImGui::GetIO().MouseWheel != 0.0f) {
		Vector2 mouseScreen = { ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y };
		MyMath::Vector3 worldBeforeZoom = ScreenToWorld(mouseScreen, canvasSizeVec, canvasPosVec);
		zoom_ *= std::pow(1.2f, ImGui::GetIO().MouseWheel);
		if (zoom_ < 0.01f) zoom_ = 0.01f;
		if (zoom_ > 10.0f) zoom_ = 10.0f;
		
		MyMath::Vector3 worldAfterZoom = ScreenToWorld(mouseScreen, canvasSizeVec, canvasPosVec);
		cameraPos_.x -= (worldAfterZoom.x - worldBeforeZoom.x);
		cameraPos_.y -= (worldAfterZoom.z - worldBeforeZoom.z);
	}

	// パン操作 (右ドラッグ)
	if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
		cameraPos_.x -= ImGui::GetIO().MouseDelta.x / zoom_;
		cameraPos_.y -= ImGui::GetIO().MouseDelta.y / zoom_;
	}

	// グリッド描画 (1000m間隔)
	float gridSize = 1000.0f * zoom_;
	Vector2 center = { canvasPosVec.x + canvasSizeVec.x * 0.5f, canvasPosVec.y + canvasSizeVec.y * 0.5f };
	float startX = std::fmod(center.x - cameraPos_.x * zoom_, gridSize);
	float startY = std::fmod(center.y - cameraPos_.y * zoom_, gridSize);
	
	draw_list->PushClipRect(canvas_p0, canvas_p1, true);

	for (float x = startX; x < canvas_sz.x; x += gridSize) {
		draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p1.y), IM_COL32(200, 200, 200, 40));
	}
	for (float y = startY; y < canvas_sz.y; y += gridSize) {
		draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p1.x, canvas_p0.y + y), IM_COL32(200, 200, 200, 40));
	}

	// ドラッグ操作（左ドラッグ）
	bool isLeftDragging = is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
	if (!isLeftDragging) {
		currentDragTarget_ = DragTarget::None;
	}

	// 描画 ＆ クリック判定関数
	auto HandleNode = [&](const MyMath::Vector3& worldPos, ImU32 color, float radius, DragTarget targetType, int index1, int index2) -> MyMath::Vector3 {
		Vector2 sc = WorldToScreen(worldPos, canvasSizeVec, canvasPosVec);
		ImVec2 screenPos(sc.x, sc.y);
		
		// 画面外カリング
		if (sc.x < canvas_p0.x - radius || sc.x > canvas_p1.x + radius ||
			sc.y < canvas_p0.y - radius || sc.y > canvas_p1.y + radius) {
			return worldPos; 
		}

		// 選択中ハイライト
		bool isSelected = false;
		if (targetType == DragTarget::Enemy && selectedEnemyIndex_ == index1) isSelected = true;
		if (targetType == DragTarget::GroundEnemy && selectedGroundEnemyIndex_ == index1) isSelected = true;
		if (targetType == DragTarget::Waypoint && selectedWaypointPathIndex_ == index1 && selectedWaypointIndex_ == index2) isSelected = true;
		if (targetType == DragTarget::TriggerArea && selectedTriggerIndex_ == index1) isSelected = true;

		if (isSelected) {
			draw_list->AddCircleFilled(screenPos, radius + 3.0f, IM_COL32(255, 255, 255, 100));
		}

		draw_list->AddCircleFilled(screenPos, radius, color);
		draw_list->AddCircle(screenPos, radius + 1.0f, IM_COL32(0, 0, 0, 255));

		// クリック＆ドラッグ判定
		if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			float distSq = (ImGui::GetIO().MousePos.x - sc.x) * (ImGui::GetIO().MousePos.x - sc.x) +
						   (ImGui::GetIO().MousePos.y - sc.y) * (ImGui::GetIO().MousePos.y - sc.y);
			if (distSq <= radius * radius) {
				currentDragTarget_ = targetType;
				dragIndex1_ = index1;
				dragIndex2_ = index2;
				
				// 選択の更新
				if (targetType == DragTarget::Enemy) SetSelection(index1, -1, -1, -1, -1);
				if (targetType == DragTarget::GroundEnemy) SetSelection(-1, index1, -1, -1, -1);
				if (targetType == DragTarget::Waypoint) SetSelection(-1, -1, index1, index2, -1);
				if (targetType == DragTarget::TriggerArea) SetSelection(-1, -1, -1, -1, index1);
			}
		}

		if (isLeftDragging && currentDragTarget_ == targetType && dragIndex1_ == index1 && dragIndex2_ == index2) {
			Vector2 mouseScreen = { ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y };
			MyMath::Vector3 newWorldPos = ScreenToWorld(mouseScreen, canvasSizeVec, canvasPosVec);
			return { newWorldPos.x, worldPos.y, newWorldPos.z }; // Y(高度)は維持
		}

		return worldPos;
	};

	// --- トリガーエリアの描画 ---
	for (int i = 0; i < (int)missionData.triggers.size(); ++i) {
		auto& trigger = missionData.triggers[i];
		if (trigger.condition == TriggerCondition::OnAreaEnter || trigger.condition == TriggerCondition::OnAreaLeave) {
			Vector2 sc = WorldToScreen(trigger.areaCenter, canvasSizeVec, canvasPosVec);
			float screenRadius = trigger.areaRadius * zoom_;
			draw_list->AddCircleFilled(ImVec2(sc.x, sc.y), screenRadius, IM_COL32(255, 165, 0, 50));
			draw_list->AddCircle(ImVec2(sc.x, sc.y), screenRadius, IM_COL32(255, 165, 0, 200));
			
			// 中心点を動かせるようにする
			trigger.areaCenter = HandleNode(trigger.areaCenter, IM_COL32(255, 165, 0, 255), 6.0f, DragTarget::TriggerArea, i, 0);
		}
	}

	// --- ウェイポイント経路の描画 ---
	for (int i = 0; i < (int)missionData.waypointPaths.size(); ++i) {
		auto& path = missionData.waypointPaths[i];
		for (int j = 0; j < (int)path.points.size(); ++j) {
			auto& wp = path.points[j];
			
			// 線を引く
			if (j > 0) {
				Vector2 sc1 = WorldToScreen(path.points[j-1].position, canvasSizeVec, canvasPosVec);
				Vector2 sc2 = WorldToScreen(wp.position, canvasSizeVec, canvasPosVec);
				draw_list->AddLine(ImVec2(sc1.x, sc1.y), ImVec2(sc2.x, sc2.y), IM_COL32(255, 255, 255, 150), 2.0f);
			}

			// ウェイポイントノード
			wp.position = HandleNode(wp.position, IM_COL32(200, 200, 255, 255), 5.0f, DragTarget::Waypoint, i, j);
		}
		// ループの場合、最後から最初へ
		if (path.isLoop && path.points.size() > 1) {
			Vector2 sc1 = WorldToScreen(path.points.back().position, canvasSizeVec, canvasPosVec);
			Vector2 sc2 = WorldToScreen(path.points.front().position, canvasSizeVec, canvasPosVec);
			draw_list->AddLine(ImVec2(sc1.x, sc1.y), ImVec2(sc2.x, sc2.y), IM_COL32(255, 255, 255, 100), 2.0f);
		}
	}

	// --- 地上目標の描画 ---
	for (int i = 0; i < (int)missionData.groundEnemies.size(); ++i) {
		auto& g = missionData.groundEnemies[i];
		ImU32 color = IM_COL32(200, 100, 50, 255); // 赤茶
		g.position = HandleNode(g.position, color, 8.0f, DragTarget::GroundEnemy, i, 0);
	}

	// --- 空中敵の描画 ---
	for (int i = 0; i < (int)missionData.enemies.size(); ++i) {
		auto& e = missionData.enemies[i];
		ImU32 color = IM_COL32(255, 50, 50, 255); // 赤
		e.position = HandleNode(e.position, color, 8.0f, DragTarget::Enemy, i, 0);
		
		// FollowWaypointなら線を引く
		if (e.aiType == AIType::FollowWaypoint && !e.waypointPathName.empty()) {
			for (const auto& path : missionData.waypointPaths) {
				if (path.name == e.waypointPathName && !path.points.empty()) {
					Vector2 sc1 = WorldToScreen(e.position, canvasSizeVec, canvasPosVec);
					Vector2 sc2 = WorldToScreen(path.points[0].position, canvasSizeVec, canvasPosVec);
					draw_list->AddLine(ImVec2(sc1.x, sc1.y), ImVec2(sc2.x, sc2.y), IM_COL32(255, 100, 100, 150), 1.0f);
					break;
				}
			}
		}
	}

	// --- プレイヤーの描画 ---
	Vector2 playerSc = WorldToScreen(playerPos, canvasSizeVec, canvasPosVec);
	draw_list->AddTriangleFilled(
		ImVec2(playerSc.x, playerSc.y - 10.0f),
		ImVec2(playerSc.x - 8.0f, playerSc.y + 8.0f),
		ImVec2(playerSc.x + 8.0f, playerSc.y + 8.0f),
		IM_COL32(50, 150, 255, 255)
	);
	draw_list->AddText(ImVec2(playerSc.x + 10.0f, playerSc.y), IM_COL32(255,255,255,255), "Player");

	draw_list->PopClipRect();

	// 背景クリックで選択解除
	if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && currentDragTarget_ == DragTarget::None) {
		SetSelection(-1, -1, -1, -1, -1);
	}

	ImGui::End();
#endif
}
