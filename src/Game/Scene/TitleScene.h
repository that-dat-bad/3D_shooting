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
#include "../Graphics/Afterburner.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"

/// @brief タイトルシーン
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
	UIText titleText_;				///< ゲームタイトル
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
		ConsoleBoot,
		Menu
	};
	TitleState state_ = TitleState::DroneView;
	float stateTimer_ = 0.0f;
	float cutTimer_ = 0.0f;
	float transitionTimer_ = 0.0f;      ///< カメラ切替時のノイズトランジションタイマー
	static constexpr float kTransitionDuration = 0.15f; ///< 0.15秒の一瞬の切り替え（テンポ向上）
	float consoleBootTimer_ = 0.0f;     ///< クリック後のハッカー風コンソール演出タイマー
	static constexpr float kConsoleBootDuration = 1.15f;
	float consoleExitTimer_ = 0.0f;     ///< コンソールからメニューへのスムーズなフェード移行タイマー
	static constexpr float kConsoleExitDuration = 0.28f;
	float menuEnterTimer_ = 0.0f;       ///< メニュー展開トランジションタイマー
	static constexpr float kMenuEnterDuration = 0.35f;
	bool isCursorShown_ = false;
	void SetCursorVisible(bool visible);

	int currentLocationIndex_ = 0;
	bool lockLocation_ = false;          ///< デバッグ用：カット切替を停止
	static constexpr float kCutDuration = 6.0f; ///< 1カットの長さ(秒)

	// --- UI追加要素 ---
	UIText pressSpaceText_;
	std::unique_ptr<Sprite> pressSpaceBannerBg_;               ///< 操作案内の半透明ダーク帯（機体との被り防止）
	std::vector<std::unique_ptr<Sprite>> pressSpaceBorders_;   ///< 操作案内帯のサイバー枠線
	std::unique_ptr<Sprite> consoleBgSprite_;                  ///< コンソールブート時の完全漆黒背景スプライト
	UIPanel scanlineOverlay_;
	UIPanel menuCardPanel_;                         ///< メニュー中央のコンソール端末ウィンドウ
	UIPanel menuHeaderPanel_;                       ///< コンソール端末上部ヘッダーバー
	std::unique_ptr<Sprite> headerLampSprite_;      ///< ヘッダーの点滅LEDランプ
	std::unique_ptr<Sprite> titleLogoSprite_;       ///< DAWN ロゴスプライト（白文字 + MiG-21通過）
	std::vector<std::unique_ptr<Sprite>> menuBorderSprites_;   ///< メニューカード枠線・アクセント・走査線

	struct ButtonBorderGroup {
		std::vector<std::unique_ptr<Sprite>> outlines;   ///< 外周細線
		std::vector<std::unique_ptr<Sprite>> brackets;   ///< 四隅ブラケット
		std::unique_ptr<Sprite> indicator;               ///< 左端アクティブバー
	};
	std::vector<ButtonBorderGroup> buttonBorders_;             ///< ボタンごとのサイバー枠線
	std::vector<std::unique_ptr<Sprite>> glitchSprites_;       ///< カメラ切替グリッチ用スプライト

	void InitializeMenuCard(SpriteCommon* spriteCommon);
	void UpdateMenuConsoleVisuals();
	void DrawMenuCard();
	void DrawTransitionGlitch();
	void DrawConsoleBoot();

	// --- OSD (ドローンカメラビュー枠) ---
	std::vector<std::unique_ptr<Sprite>> osdSprites_;
	std::unique_ptr<Sprite> recDotSprite_;
	void InitializeOSD(SpriteCommon* spriteCommon);
	void DrawOSD();

	// ============================
	// 3D背景ロケーション
	// ============================
	enum class TitleLocation : int {
		Hangar,         ///< 格納庫（大梁トラス鉄骨構造）
		Tunnel,         ///< 地下トンネル基地
		CrashedForest,  ///< 墜落済みの森
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
		float bankTilt = 0.40f;     ///< ドローンの移動傾き係数（ロール角）
		bool isTunnelCamera = false; ///< トンネル内カメラ（壁突き抜け防止・正面構図）
	};

	/// @brief ロケーションごとのポストエフェクト色味
	struct LocationGrade {
		MyMath::Vector3 tint = { 1.0f, 1.0f, 1.0f };
		float tintIntensity = 0.0f;
		float vignette = 0.0f;
	};

	/// @brief 追加の背景セットパーツ（道路、誘導灯、回転灯など）
	struct ExtraEnvPart {
		std::string modelPath;
		std::unique_ptr<Object3d> object;
		MyMath::Vector3 offset = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 rotation = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 scale = { 1.0f, 1.0f, 1.0f };
		bool rotateY = false;          ///< 回転灯用Y軸自転フラグ
		float rotateSpeed = 0.0f;      ///< 自転速度 (rad/s)
		float currentAngle = 0.0f;
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
		std::vector<ExtraEnvPart> extraEnvParts; ///< 道路、誘導灯、回転灯などの追加パーツ

		// 機体
		std::string aircraftModelPath = "Resources/models/m21_wg.gltf";
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

	std::unique_ptr<Skybox> skybox_ = nullptr;
	float cameraTheta_ = 0.0f;

	// 誘導灯の自発光パラメータ
	MyMath::Vector3 guideEmissiveColor_ = { 0.2f, 1.8f, 2.8f }; // 鮮やかなエレクトリックシアン
	float guideEmissiveIntensity_ = 2.5f;
	uint32_t whiteTexIndex_ = 0;
	uint32_t softShadowTexIndex_ = 0;
	uint32_t crashScorchTexIndex_ = 0;
	uint32_t tunnelWallTexIndex_ = 0;

	// 墜落現場用パーティクルタイマー
	float crashSmokeTimer_ = 0.0f;
	float crashSparkTimer_ = 0.0f;

	// 高品位アフターバーナーエフェクト
	std::unique_ptr<Afterburner> afterburner_ = nullptr;
	MyMath::Matrix4x4 currentAircraftMatrix_ = MyMath::Identity4x4(); ///< 機体の最新動的ワールド行列（A/Bと完全同期）
	bool enableTunnelMotion_ = true;                                  ///< トンネル内浮遊・バンク動揺アニメ有効フラグ

#ifdef USE_IMGUI
	bool showAfterburnerEditor_ = true;
	std::string afterburnerConfigPath_ = "assets/configs/afterburner_m21.json";
	std::string abEditorStatusMsg_ = "";
	float abEditorStatusTimer_ = 0.0f;
	void DrawAfterburnerEditor();
#endif
};