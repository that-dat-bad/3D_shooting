#pragma once
#include <memory>
#include <string>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/io/json.hpp"
#include "MissionManager.h"

// ImGui forward declaration
struct ImDrawList;

/// @brief ミッションエディタのビジュアルマップ表示（War Thunder風）
class MissionMapView {
public:
	MissionMapView() = default;
	~MissionMapView() = default;

	void Initialize();

	/// @brief 2Dマップを描画し、インタラクション（ドラッグ等）を処理する
	/// @param missionData 現在編集中のミッションデータ
	/// @param playerPos 現在のプレイヤー座標
	void Draw(MissionData& missionData, const MyMath::Vector3& playerPos);

	// 選択状態の取得・設定用
	int GetSelectedEnemyIndex() const { return selectedEnemyIndex_; }
	int GetSelectedGroundEnemyIndex() const { return selectedGroundEnemyIndex_; }
	int GetSelectedWaypointIndex() const { return selectedWaypointIndex_; }
	int GetSelectedWaypointPathIndex() const { return selectedWaypointPathIndex_; }
	int GetSelectedTriggerIndex() const { return selectedTriggerIndex_; }

	void SetSelection(int enemyIdx, int groundIdx, int wpPathIdx, int wpIdx, int triggerIdx) {
		selectedEnemyIndex_ = enemyIdx;
		selectedGroundEnemyIndex_ = groundIdx;
		selectedWaypointPathIndex_ = wpPathIdx;
		selectedWaypointIndex_ = wpIdx;
		selectedTriggerIndex_ = triggerIdx;
	}

private:
	// カメラ（マップ表示のオフセットとズーム）
	Vector2 cameraPos_{ 0.0f, 0.0f }; // マップの中心座標 (ワールドX, ワールドZ)
	float zoom_ = 1.0f;                       // 1ピクセルあたりのワールド距離の逆数 (1.0 = 1m/px)

	// ドラッグ・パン操作の状態
	bool isDraggingMap_ = false;
	Vector2 dragStartMapPos_{};

	// ユニットドラッグ操作の状態
	enum class DragTarget {
		None,
		Player,
		Enemy,
		GroundEnemy,
		Waypoint,
		TriggerArea
	};
	DragTarget currentDragTarget_ = DragTarget::None;
	int dragIndex1_ = -1; // メインインデックス (enemy, path, trigger 等)
	int dragIndex2_ = -1; // サブインデックス (waypoint 等)

	// 選択状態
	int selectedEnemyIndex_ = -1;
	int selectedGroundEnemyIndex_ = -1;
	int selectedWaypointPathIndex_ = -1;
	int selectedWaypointIndex_ = -1;
	int selectedTriggerIndex_ = -1;

	// ヘルパー：ワールド座標(x,z)をスクリーン座標(x,y)へ変換
	Vector2 WorldToScreen(const MyMath::Vector3& worldPos, const Vector2& canvasSize, const Vector2& canvasPos) const;
	// ヘルパー：スクリーン座標(x,y)をワールド座標(x,z)へ変換
	MyMath::Vector3 ScreenToWorld(const Vector2& screenPos, const Vector2& canvasSize, const Vector2& canvasPos) const;
};
