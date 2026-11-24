#pragma once
#ifdef USE_IMGUI

#include "IScene.h"
#include <memory>
#include <string>
#include <vector>
#include "../FlightModel/AircraftConfig.h"
#include "../FlightModel/DamageModel.h"
#include "../FlightModel/Payload/MissileLauncher.h"
#include "../Graphics/AircraftVisualModel.h"
#include "../Graphics/Afterburner.h"
#include "../Environment/EnvironmentManager.h"
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Model/Skybox.h"

class Camera;

// 機体専用エディタシーン（ハンガー／ターンテーブル型ワークベンチ）
class AircraftEditorScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawUI() override;
	void Finalize() override;

private:
	// ファイル・アセット管理
	void RefreshFileLists();
	void LoadAircraftCfg(const std::string& path);
	void ReloadVisualModel(const std::string& modelPath);
	void RebuildPreviewMeshCollider();
	void ApplyMaterialFromConfig();
	bool SaveCurrentAircraft();
	void SetStatus(const std::string& msg, bool isError = false);

	// カメラ・プレビュー行列
	void UpdateOrbitCamera();
	void UpdateMeshRaycastTest();
	void SetCameraPreset(float yaw, float pitch, float distance, const MyMath::Vector3& target = { 0.0f, 1.5f, 0.0f });
	MyMath::Matrix4x4 ComputeAircraftWorldMatrix() const;

	// 3Dギズモ・ガイド描画
	void DrawFloorGrid(Camera* camera);
	void DrawWireBox(const MyMath::Vector3& center, const MyMath::Vector3& halfSize, const MyMath::Matrix4x4& worldMat, const MyMath::Vector4& color, Camera* camera);
	void DrawCrossMarker(const MyMath::Vector3& localPos, float radius, const MyMath::Matrix4x4& worldMat, const MyMath::Vector4& color, Camera* camera);
	void DrawMeshColliderGizmo(Camera* camera, const MyMath::Matrix4x4& aircraftWorld);
	void DrawAircraftGizmos(Camera* camera, const MyMath::Matrix4x4& aircraftWorld, const MyMath::Matrix4x4& visualWorld);

	// ImGui UI群（AircraftEditorScene_UI.cpp）
	void DrawEditorWindows();
	void DrawMainInspectorWindow();
	void DrawGeneralTab();
	void DrawAirframeTab();
	void DrawEngineAndAfterburnerTab();
	void DrawArmamentTab();
	void DrawPartsAndDamageTab();
	void DrawTelemetryWindow();
	void DrawViewportBarWindow();

	// 機体モデル＆データ
	AircraftConfig aircraftConfig_;
	AircraftVisualModel visualModel_;
	std::unique_ptr<Afterburner> afterburner_;
	std::unique_ptr<Skybox> skybox_;
	EnvironmentManager environmentManager_;
	DamageModel previewDamageModel_;
	MissileLauncherData previewMissileData_;
	uint32_t whiteTexIndex_ = 0;
	bool isInitializedOnce_ = false;

	// ファイル一覧・ステータス
	std::vector<std::string> availableCfgFiles_;
	std::vector<std::string> availableModelFiles_;
	int selectedCfgIndex_ = 0;
	char saveAsNameBuf_[128] = "mig21.cfg";
	std::string loadedModelPath_;
	std::string statusMessage_;
	float statusTimer_ = 0.0f;
	bool statusIsError_ = false;

	// オービットカメラ＆プレビュー状態
	MyMath::Vector3 orbitTarget_ = { 0.0f, 1.5f, 0.0f };
	float orbitYaw_ = -2.45f;
	float orbitPitch_ = 0.25f;
	float orbitDistance_ = 19.0f;
	bool autoTurntable_ = false;
	float turntableSpeed_ = 0.35f;
	float turntableAngle_ = 0.0f;
	MyMath::Vector3 previewAttitudeDeg_ = { 0.0f, 0.0f, 0.0f };
	float previewThrottle_ = 1.15f;
	bool previewFlaps_ = false;

	// 表示オプション
	bool isSkyboxVisible_ = true;
	bool showGrid_ = true;
	bool showAxes_ = true;
	bool showCoGAndCP_ = true;
	bool showMeshCollider_ = true;
	bool enableMeshRaycastTest_ = true;
	bool showSubColliders_ = false;
	bool showPylons_ = true;
	bool showNozzleGizmo_ = true;
	int highlightedColliderIndex_ = -1;
	int highlightedMeshPartIndex_ = -1;
	MeshHitResult lastRaycastHit_{};
	bool hasRaycastSample_ = false;
	float timeOfDay_ = 16.5f;
};

#endif // USE_IMGUI
