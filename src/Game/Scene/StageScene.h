#pragma once
#include "IScene.h"
#include <memory>
#include "../../engine/base/Math/Vector2.h"
#include "StageHUD.h"
#include "../Environment/EnvironmentManager.h"
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/Graphics/Model/Skybox.h"
#include "../Camera/PlayerCamera.h"
#include "FlightModel/AircraftConfig.h"
#include "FlightModel/FlightModel.h"
#include "FlightModel/FlightInstructor.h"
#include "FlightModel/MouseAimController.h"
#include "FlightModel/Payload/Gunpod.h"
#include "FlightModel/Payload/MissileLauncher.h"
#include "Enemy/EnemyManager.h"
#include "Bullet/BulletManager.h"
#include "Bullet/MissileManager.h"
#include "../Graphics/AircraftVisualModel.h"
#include "../Graphics/DebrisManager.h"
#include "../../engine/Physics/CollisionSystem.h"
#include "../../engine/Physics/CollisionConfig.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../engine/Graphics/UI/UIText.h"

#include "CombatBootSequence.h"
#include "../System/MissionManager.h"
#include "../System/MissionMapView.h"
#include "../UI/RadioCommunicator.h"
#include "../UI/InfoUI.h"
#include "../Graphics/Afterburner.h"
#include "../Effect/CountermeasureSystem.h"
#include "../Effect/AeroVaporSystem.h"
#include "../Effect/AfterburnerParticleEmitter.h"
#include "../Effect/AmbientWeatherSystem.h"
#include "../Effect/GunCombatEffectEmitter.h"

namespace StageRadioDefaults {
	inline constexpr int kInitialKillCount = 0;
	inline constexpr int kAlternateModulo = 2;
	inline constexpr int kEvenRemainder = 0;
	inline constexpr float kCriticalHpRatioThreshold = 0.35f;
	inline constexpr size_t kImGuiSpeakerBufferSize = 128;
	inline constexpr size_t kImGuiDialogueBufferSize = 512;
	inline constexpr float kImGuiMinHoldSeconds = 0.5f;
	inline constexpr float kImGuiMaxHoldSeconds = 12.0f;

	inline constexpr const char* kAwacsName = "AWACS SKY EYE";
	inline constexpr const char* kWingmanName = "Joker 2 Coster";
	inline constexpr const char* kEnemyAceName = "STRIGON 1";

	inline constexpr const char* kSortieDialogue = "全機、作戦空域へ突入。自由交戦を許可する。";
	inline constexpr const char* kKillAwacsDialogue = "敵機の撃墜を確認。引き続き周囲を警戒せよ。";
	inline constexpr const char* kKillWingmanDialogue = "命中確認！ナイスショットだ！";
	inline constexpr const char* kCriticalDamageDialogue = "機体損傷増大！無理はするな、回避機動をとれ！";
	inline constexpr const char* kPresetEnemyDialogue = "来い、見せてみろ……その翼の力を！";
	inline constexpr const char* kImGuiDefaultDialogue = "今はやめだ";
	inline constexpr float kZeroTimeLimit = 0.0f;
	inline constexpr float kTimeWarningThresholdSeconds = 30.0f;
	inline constexpr float kDegToRad = 3.14159265358979f / 180.0f;
	inline constexpr float kHalfAngleFactor = 0.5f;
	inline constexpr float kReachAreaRingDepthScale = 3.5f;
	inline constexpr float kReachAreaRingSpinSpeed = 0.45f;
	inline constexpr const char* kRingTexPath = "assets/textures/circle2.png";
	inline constexpr MyMath::Vector4 kReachAreaRingColor = { 0.42f, 0.82f, 0.45f, 0.88f };
}

class TerrainChunkManager;

// ステージシーン
class StageScene : public IScene {
public:
	void Initialize() override;
	void PrewarmAssets() override;
	void Update() override;
	void Draw() override;
	void DrawUI() override;
	void Finalize() override;
	void Restart();

private:
	bool isAssetsInitialized_ = false; // アセット生成完了フラグ
	void InitializeAssets();           // 初回アセット生成
	void ResetGameplayState();         // ゲームプレイ状態リセット

	// フライトモデル
	FlightModel flightModel_;

	// フライトインストラクター
	FlightInstructor flightInstructor_;

	// 描画用 3Dオブジェクト
	std::unique_ptr<Object3d> aircraftObject_ = nullptr;

	// 部位破壊対応マルチパーツビジュアルモデル
	AircraftVisualModel aircraftVisualModel_;

	// 自機アフターバーナーエフェクト
	std::unique_ptr<Afterburner> afterburner_ = nullptr;

	// パーツ破壊イベントチェック
	void CheckPartDestructionEvents();

	// 地面テクスチャ
	uint32_t groundTextureIndex_ = 0;

	// 地面グリッド描画
	void DrawGround();

	// --- 出撃イントロ---
	float sortieIntroTimer_ = 0.0f;
	float sortieIntroDuration_ = CombatBootSequence::GetDuration();
	CombatBootSequence combatBootSequence_;

	std::unique_ptr<Skybox> skybox_ = nullptr;

	// ============================
	// プレイヤーカメラ
	// ============================
	PlayerCamera playerCamera_;

	// ============================
	// マウスエイム操縦
	// ============================
	MouseAimController mouseAimController_;
	bool mouseAimEnabled_ = true;    // マウスエイムモード ON/OFF

	// ============================
	// 戦闘システム
	// ============================
	enum class WeaponMode {
		Gun = 0,
		Missile = 1
	};
	WeaponMode activeWeaponMode_ = WeaponMode::Gun;
	float weaponSwitchCooldown_ = 0.0f;

	GunPod gunpod_;
	MissileLauncher* missileLauncher_ = nullptr; // 所有権は flightModel_ が保持
	EnemyManager enemyManager_;
	BulletManager bulletManager_;
	MissileManager missileManager_;
	bool enableEnemies_ = true; // 敵出現フラグ

	// --- 統合当たり判定 ---
	CollisionSystem collisionSystem_;

	// プレイヤーの当たり判定ボディ
	class PlayerCollisionBody : public ICollisionBody3D {
	public:
		StageScene* scene_ = nullptr;
		SphereCollider GetSphereCollider() const override {
			const auto& mc = scene_->flightModel_.GetDamageModel().GetMeshCollider();
			const float radius = mc.IsValid() ? (mc.GetBoundingRadius() + 2.5f) : scene_->playerCollisionRadius_;
			return { scene_->flightModel_.GetPosition(), radius };
		}
		uint32_t GetCollisionAttribute() const override { return CollisionAttribute::kPlayer; }
		uint32_t GetCollisionMask() const override { return CollisionMask::kPlayer; }
		bool RefineCollision(const ICollisionBody3D* other) const override;
		void OnCollision(ICollisionBody3D* other) override;
		bool IsCollisionActive() const override { return !scene_->isMissionFailed_ && !scene_->isMissionCleared_; }
	};
	PlayerCollisionBody playerBody_;

	// 追従マズルフラッシュ描画用タイマー
	float muzzleFlashTimer_ = 0.0f;
	MyMath::Vector3 lastMuzzleLocalOffset_{ 0.0f, 0.0f, 8.0f };
	
	// マズルフラッシュの間引き制御用カウンター
	int muzzleFlashCount_ = 0;
	
	// マズルフラッシュのランダム化パラメータ
	float muzzleFlashRandomScale_ = 1.0f;
	float muzzleFlashRandomRoll_ = 0.0f;
	float muzzleFlashRandomAlpha_ = 1.0f;

	// ============================
	// ゲームオーバー / クリア演出シーケンス
	// ============================
	enum class StageEndSequence {
		None,
		// ゲームオーバー演出
		GameOver_Critical,      // HUD赤反転、警告点滅
		GameOver_SignalLost,    // シグナルロスト演出
		GameOver_CrtShutdown,   // CRT電源切断演出
		// 時間切れ離脱演出
		TimeOver_Abort,         // 離脱指示
		TimeOver_Withdraw,      // 急上昇加速離脱
		// ステージクリア演出
		Clear_HudShutdown,      // HUD順次消去
		Clear_AutopilotReturn,  // オートパイロット離脱
		Clear_TacticalMapZoom   // 戦術マップ演出
	};
	StageEndSequence endSequence_ = StageEndSequence::None;
	float endSeqTimer_ = 0.0f;

	// クリア・離脱演出用パラメータ
	MyMath::Quaternion clearStartOrientation_{ 0.0f, 0.0f, 0.0f, 1.0f };
	MyMath::Vector3 clearStartForward_{ 0.0f, 0.0f, 1.0f };
	MyMath::Vector3 clearHorizontalDir_{ 0.0f, 0.0f, 1.0f };
	float clearStartSpeed_ = 100.0f;

	void TriggerGameOver();
	void TriggerTimeOver();
	void TriggerGameClear();
	void UpdateEndSequence(float dt);
	void DrawEndSequenceUI();

	// ゲーム状態
	bool isFreeFlightMode_ = false;
	bool isMissionCleared_ = false;
	bool isMissionFailed_ = false;
	bool isGameOver_ = false;
	float gameOverTimer_ = 0.0f;
	static constexpr float kGameOverDuration = 1.10f;
	float timeScale_ = 1.0f;

	// エースコンバット風 撃墜・ベイルアウト演出用パラメータ
	MyMath::Vector3 gameOverVelocity_{ 0.0f, 0.0f, 0.0f };
	MyMath::Vector3 gameOverTumbleRate_{ 0.0f, 0.0f, 0.0f };
	float gameOverBurnTimer_ = 0.0f;
	bool isBailout_ = false;

	// ミッション制限時間
	float remainingTime_ = 300.0f;
	float missionElapsedTime_ = 0.0f;  // ミッション経過時間
	UIText timeText_;

	// 演出用スプライト
	std::unique_ptr<Sprite> seqOverlaySprite_;
	std::unique_ptr<Sprite> seqNoiseSprite_;

	// 戦闘HUD
	StageHUD hud_;
	RadioCommunicator radioCommunicator_;
	InfoUI infoUI_;
	bool hasSentSortieRadio_ = false;
	bool hasSentDamageWarningRadio_ = false;
	int lastRadioKillCount_ = StageRadioDefaults::kInitialKillCount;
	void UpdateRadioCommunications(float gameDeltaTime);
	bool isFiringThisFrame_ = false;
	bool clearFadeOutActive_ = false;
	float clearFadeOutTimer_ = 0.0f;
	float hitMarkerTimer_ = 0.0f;
	bool hasPrevAimDir_ = false;
	MyMath::Vector3 prevAimDir_{};

	// プレイヤーのステータス
	float playerMaxHP_ = 100.0f;
	float playerHP_ = 100.0f;
	float playerCollisionRadius_ = 2.0f; // プレイヤーの当たり判定の半径
	bool wasInSortieIntro_ = false;

	// スロットル入力状態
	float throttle_ = 0.0f;
	float wepDetentTimer_ = 0.0f;
	bool holdWForWep_ = false;

	// 経過時間
	float totalTime_ = 0.0f;

	// ナイトビジョン状態
	bool isNvdActive_ = false;

	// 環境管理
	EnvironmentManager environmentManager_;

	// GPUパーティクル連携エフェクト群（単一責務モジュール委譲）
	CountermeasureSystem countermeasures_;
	AeroVaporSystem aeroVapor_;
	AfterburnerParticleEmitter afterburnerParticles_;
	AmbientWeatherSystem ambientWeather_;

	// 地形チャンク管理
	TerrainChunkManager* terrainChunkManager_ = nullptr;

	// ============================
	// ポストエフェクト統合制御
	// ============================
	void UpdatePostEffects(float deltaTime);

	bool enableAutoPostEffects_ = true;     // ゲーム状況連動の自動ポストエフェクト
	float damageFlashTimer_ = 0.0f;         // 被弾フラッシュ制御タイマー
	int manualSelectedEffect_ = 0;          // 手動・ImGuiテスト用エフェクトインデックス
	ActivePostEffect manualEffectParams_{}; // 手動・ImGuiテスト用パラメータ

	// 機体設定
	AircraftConfig aircraftConfig_;
	uint64_t appliedAircraftConfigRevision_ = 0;
	std::string loadedVisualModelPath_;
	bool isAircraftTuningOpen_ = true;
	int selectedAircraftConfigIndex_ = 0;
	char tempAircraftConfigName_[128] = "mig21.cfg";
	void DrawAircraftTuningEditor();
	void DrawMissionObjectivesHUD();
	void DrawMissionMessages();
	void ApplyMissionLiveSyncUpdate();
	void DrawMissionReachAreaRings(Camera* cam);

	// ============================
	// 分割ヘルパー関数群
	// ============================
	void UpdateFlightControls(float gameDeltaTime);
	void UpdateCombatFiring(float gameDeltaTime);
	void UpdateFlightAndEffects(float gameDeltaTime);
	void UpdateCombatAndMission(float gameDeltaTime);
	void UpdateCamera(float gameDeltaTime);
	void UpdateDebugAndStatusUI(float gameDeltaTime);
	void DrawMuzzleFlashEffect(Camera* cam);

	// ============================
	// ポーズメニュー
	// ============================
	bool isPaused_ = false;
	int pauseMenuIndex_ = 0;
	float pauseAnimTimer_ = 0.0f;
	Vector2 customCursorPos_ = { 640.0f, 360.0f };
	float cursorRippleTimer_ = 0.0f;
	Vector2 cursorRipplePos_ = { 0.0f, 0.0f };

	// ポーズウィンドウ位置 & ドラッグ状態
	Vector2 pauseWinPos_ = { 390.0f, 145.0f };
	bool pauseIsDragging_ = false;
	Vector2 pauseDragOffset_ = { 0.0f, 0.0f };

	std::vector<std::unique_ptr<Sprite>> pauseSpritePool_;
	size_t pauseSpritePoolIndex_ = 0;
	std::vector<std::unique_ptr<Sprite>> pauseCircleSpritePool_;
	size_t pauseCircleSpritePoolIndex_ = 0;

	Sprite* GetPauseSprite();
	Sprite* GetPauseCircleSprite();
	void ResetPauseSpritePool() {
		pauseSpritePoolIndex_ = 0;
		pauseCircleSpritePoolIndex_ = 0;
	}
	void DrawPauseSolidRect(const Vector2& pos, const Vector2& size, const MyMath::Vector4& color, BlendMode blend = BlendMode::kNormal);
	void DrawPauseLine(const Vector2& p1, const Vector2& p2, float thickness, const MyMath::Vector4& color, BlendMode blend = BlendMode::kNormal);
	void DrawPauseGlowOval(const Vector2& center, const Vector2& size, const MyMath::Vector4& color, BlendMode blend = BlendMode::kAdd);
	void DrawPauseBorder(const Vector2& pos, const Vector2& size, const MyMath::Vector4& color, float thickness = 1.5f);
	void DrawPausePanel(const Vector2& pos, const Vector2& size, const MyMath::Vector4& bgCol, const MyMath::Vector4& borderCol, float thickness = 1.5f);
	void DrawPauseCornerBrackets(const Vector2& pos, const Vector2& size, float bracketLen, float thickness, const MyMath::Vector4& color);
	void DrawPauseCustomCursor();

	bool IsGameplayActive() const;
	void SetPaused(bool paused);
	void UpdatePauseMenu();
	void DrawPauseMenuUI();
	void ExecutePauseAction(int actionIndex);

	// ポーズからタイトル復帰時のCRTシャットダウン演出
	bool isPauseAborting_ = false;
	float pauseAbortTimer_ = 0.0f;
	static constexpr float kPauseAbortDuration = 0.38f;
	void DrawPauseCRTShutdown(float progress);
};

