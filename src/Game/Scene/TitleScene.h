#pragma once
#include "IScene.h"
#include <vector>
#include <memory>
#include <string>
#include "../../engine/Graphics/UI/UIPanel.h"
#include "../../engine/Graphics/UI/UIText.h"
#include "../../engine/Graphics/UI/UIButton.h"
#include "../../engine/Graphics/UI/UISelectionManager.h"
#include "../../engine/Graphics/Model/Skybox.h"
#include "../../engine/Graphics/Camera/Camera.h"
#include "../Graphics/AircraftVisualModel.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"

/// @brief タイトルシーン。ゲームタイトルとメニューボタンを表示する。
class TitleScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawUI() override;
	void Finalize() override;

private:
	// --- UI要素 ---
	UIPanel backgroundPanel_;		///< 背景オーバーレイ
	UIText titleText_;				///< ゲームタイトル「戦雷」
	UIText subtitleText_;			///< サブタイトル
	UIButton startButton_;			///< スタートボタン
	UIButton editorButton_;         ///< エディタボタン
	UIButton settingsButton_;		///< 設定ボタン
	UIButton exitButton_;			///< 終了ボタン

	UISelectionManager selectionManager_;

	// アニメーション
	float animTimer_ = 0.0f;		///< テキストアニメーション用タイマー

	// --- モード管理 ---
	enum class TitleState {
		DroneView,
		Menu
	};
	TitleState state_ = TitleState::DroneView;
	float stateTimer_ = 0.0f;
	float cutTimer_ = 0.0f;
	int currentLocationIndex_ = 0;
	bool lockLocation_ = false;          ///< デバッグ用：カット切替を停止
	static constexpr float kCutDuration = 6.0f; ///< 1カットの長さ(秒)

	// --- UI追加要素 ---
	UIText pressSpaceText_;
	UIPanel scanlineOverlay_;

	// ============================
	// 3D背景ロケーション（ハンガー / 空母 / 墜落森）
	// ============================
	enum class TitleLocation : int {
		Hangar,         ///< 格納庫（出撃前・静）
		CarrierDeck,    ///< 空母飛行甲板（発艦・緊迫）
		CrashedForest,  ///< 墜落済みの森（哀愁）
		Count
	};

	/// @brief ロケーションごとのライティング設定
	struct LocationLighting {
		int32_t lightType = 1; ///< bit: 1=Directional, 2=Point, 4=Spot

		MyMath::Vector4 dirColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		MyMath::Vector3 dirDirection = { 0.0f, -1.0f, 0.0f };
		float dirIntensity = 1.0f;

		MyMath::Vector4 pointColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		MyMath::Vector3 pointOffset = { 0.0f, 2.0f, 0.0f }; ///< ロケーション原点からの相対位置
		float pointIntensity = 0.0f;
		float pointRadius = 15.0f;
		float pointDecay = 1.0f;
		float pointFlicker = 0.0f; ///< 0で無効。炎のゆらぎ量(0〜1)

		MyMath::Vector4 spotColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		MyMath::Vector3 spotOffset = { 0.0f, 12.0f, 0.0f }; ///< ロケーション原点からの相対位置
		MyMath::Vector3 spotDirection = { 0.0f, -1.0f, 0.0f };
		float spotIntensity = 0.0f;
		float spotDistance = 30.0f;
		float spotDecay = 1.0f;
		float spotAngleDeg = 35.0f;
		float spotFalloffStartDeg = 20.0f;
	};

	/// @brief ロケーションごとのカメラワーク設定
	struct LocationCamera {
		float distance = 10.0f;
		float distanceSway = 1.5f;
		float height = 1.5f;
		float heightSway = 0.5f;
		float lookAtHeight = 0.5f;  ///< 注視点の機体からの高さ
		float orbitSpeed = 0.1f;    ///< rad/sec
		float shake = 0.002f;
	};

	/// @brief ロケーションごとのポストエフェクト色味
	struct LocationGrade {
		MyMath::Vector3 tint = { 1.0f, 1.0f, 1.0f };
		float tintIntensity = 0.0f;
		float vignette = 0.0f;
	};

	struct LocationData {
		TitleLocation id = TitleLocation::Hangar;
		const char* name = "";
		MyMath::Vector3 origin = { 0.0f, 0.0f, 0.0f }; ///< セットの原点（床面 Y=0 がここに来る）

		// 背景セットモデル（未制作ならnullptrのまま、描画スキップ）
		std::string envModelPath;
		std::unique_ptr<Object3d> envObject;
		MyMath::Vector3 envScale = { 1.0f, 1.0f, 1.0f };
		MyMath::Vector3 envRotation = { 0.0f, 0.0f, 0.0f };

		// 機体
		MyMath::Vector3 aircraftOffset = { 0.0f, 0.0f, 0.0f }; ///< origin からの相対
		MyMath::Vector3 aircraftRotation = { 0.0f, 0.0f, 0.0f };
		float propellerRpm = 0.0f;
		std::vector<DamagePart> hiddenParts;   ///< 部位破壊で非表示にするパーツ
		std::unique_ptr<AircraftVisualModel> visualModel;

		// 千切れたパーツ（墜落森用）：hiddenParts の逆だけを表示した別インスタンス
		std::unique_ptr<AircraftVisualModel> debrisModel;
		MyMath::Vector3 debrisOffset = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 debrisRotation = { 0.0f, 0.0f, 0.0f };

		// 環境
		uint32_t skyboxTexIndex = 0;
		MyMath::Vector4 skyboxColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool hasOcean = false;
		float oceanHeight = 0.0f; ///< ワールドY

		LocationLighting lighting;
		LocationCamera camera;
		LocationGrade grade;
	};
	std::vector<LocationData> locations_;

	/// @brief ロケーション共通の生成処理
	void SetupLocation(LocationData& loc, Camera* camera);
	/// @brief 現在ロケーションのライトを Object3dCommon に反映
	void ApplyLocationLighting(const LocationData& loc);
	/// @brief 現在ロケーションの周回カメラを更新
	void UpdateLocationCamera(const LocationData& loc, float dt, bool menuMode);
	/// @brief ロケーション切替
	void ChangeLocation(int index);
	LocationData& CurrentLocation() { return locations_[currentLocationIndex_]; }

	std::unique_ptr<Object3d> oceanObject_ = nullptr;
	std::unique_ptr<Skybox> skybox_ = nullptr;
	float cameraTheta_ = 0.0f;
};