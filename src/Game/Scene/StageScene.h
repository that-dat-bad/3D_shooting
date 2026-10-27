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
#include "Enemy/EnemyManager.h"
#include "Bullet/BulletManager.h"
#include "../Graphics/AircraftVisualModel.h"
#include "../Graphics/DebrisManager.h"
#include "../../engine/Physics/CollisionSystem.h"
#include "../../engine/Physics/CollisionConfig.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../engine/Graphics/UI/UIText.h"

#include "CombatBootSequence.h"
#include "../System/MissionManager.h"
#include "../System/MissionMapView.h"

/// @brief ゲーム本編のステージシーン
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
	bool isAssetsInitialized_ = false; ///< アセット（3Dモデル・スカイボックス・バッファ）初回生成完了フラグ
	void InitializeAssets();           ///< 初回アセット生成
	void ResetGameplayState();         ///< ゲームプレイ状態リセット（リトライ・再出撃用高速処理）

	// フライトモデル（物理）
	FlightModel flightModel_;

	// フライトインストラクター（自動水平復帰）
	FlightInstructor flightInstructor_;

	// 描画用 3Dオブジェクト（機体の見た目 - レガシー）
	std::unique_ptr<Object3d> aircraftObject_ = nullptr;

	// 部位破壊対応マルチパーツビジュアルモデル
	AircraftVisualModel aircraftVisualModel_;

	// パーツ破壊イベントチェック
	void CheckPartDestructionEvents();

	// 地面テクスチャ
	uint32_t groundTextureIndex_ = 0;

	// 地面グリッド描画
	void DrawGround();

	// --- 出撃イントロ（AC6メインシステム戦闘モード起動シーケンス & カメラワーク）---
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
	GunPod gunpod_;
	EnemyManager enemyManager_;
	BulletManager bulletManager_;
	bool enableEnemies_ = false; ///< 敵出現フラグ（falseで敵スポーン無効）

	// --- 統合当たり判定 ---
	CollisionSystem collisionSystem_;

	/// @brief プレイヤーの当たり判定ボディ（内部クラス）
	class PlayerCollisionBody : public ICollisionBody3D {
	public:
		StageScene* scene_ = nullptr;
		SphereCollider GetSphereCollider() const override {
			return { scene_->flightModel_.GetPosition(), scene_->playerCollisionRadius_ };
		}
		uint32_t GetCollisionAttribute() const override { return CollisionAttribute::kPlayer; }
		uint32_t GetCollisionMask() const override { return CollisionMask::kPlayer; }
		void OnCollision(ICollisionBody3D* other) override;
		bool IsCollisionActive() const override { return !scene_->isMissionFailed_ && !scene_->isMissionCleared_; }
	};
	PlayerCollisionBody playerBody_;

	// 追従マズルフラッシュ描画用タイマー
	float muzzleFlashTimer_ = 0.0f;
	
	// マズルフラッシュの間引き（スキップ）制御用カウンター
	int muzzleFlashCount_ = 0;
	
	// マズルフラッシュのランダム化パラメータ
	float muzzleFlashRandomScale_ = 1.0f;
	float muzzleFlashRandomRoll_ = 0.0f;
	float muzzleFlashRandomAlpha_ = 1.0f;

	// ============================
	// ゲームオーバー / クリア演出シーケンス（項目1〜7）
	// ============================
	enum class StageEndSequence {
		None,
		// ゲームオーバー演出
		GameOver_Critical,      ///< 項目4: HUD赤反転、CRITICAL DAMAGE 点滅
		GameOver_SignalLost,    ///< 項目1: 3Dフリーズ、強烈ノイズ/砂嵐、SIGNAL LOST 表示
		GameOver_CrtShutdown,   ///< 項目2: ブラウン管電源切断演出（中央水平線収束・プツッ消滅）
		// ステージクリア演出
		Clear_HudShutdown,      ///< 項目5: HUD順次OFFLINE消去、MISSION ACCOMPLISHED タイピング
		Clear_AutopilotReturn,  ///< 項目6: オートパイロット移行、自機急上昇・急加速離脱
		Clear_TacticalMapZoom   ///< 項目7: 垂直急ズームアウト、戦術マップ、SECTOR CLEARED スタンプ
	};
	StageEndSequence endSequence_ = StageEndSequence::None;
	float endSeqTimer_ = 0.0f;

	// クリア演出用パラメータ（飛行機離脱機動用）
	MyMath::Quaternion clearStartOrientation_{ 0.0f, 0.0f, 0.0f, 1.0f };
	MyMath::Vector3 clearStartForward_{ 0.0f, 0.0f, 1.0f };
	MyMath::Vector3 clearHorizontalDir_{ 0.0f, 0.0f, 1.0f };
	float clearStartSpeed_ = 100.0f;

	void TriggerGameOver();
	void TriggerGameClear();
	void UpdateEndSequence(float dt);
	void DrawEndSequenceUI();

	// ゲーム状態
	bool isMissionCleared_ = false;
	bool isMissionFailed_ = false;
	bool isGameOver_ = false;
	float gameOverTimer_ = 0.0f;
	static constexpr float kGameOverDuration = 1.10f;
	float timeScale_ = 1.0f;

	// ミッション制限時間（MissionData.timeLimitから設定）
	float remainingTime_ = 300.0f;
	float missionElapsedTime_ = 0.0f;  ///< ミッション経過時間（トリガー評価用）
	UIText timeText_;

	// 演出用スプライト
	std::unique_ptr<Sprite> seqOverlaySprite_;
	std::unique_ptr<Sprite> seqNoiseSprite_;

	// 戦闘HUD（残弾数・照準・タイマー）
	StageHUD hud_;
	bool isFiringThisFrame_ = false;

	// プレイヤーのステータス
	float playerMaxHP_ = 100.0f;
	float playerHP_ = 100.0f;
	float playerCollisionRadius_ = 2.0f; // プレイヤーの当たり判定の半径

	// スロットル入力状態（メンバー変数化して初期化時にリセットできるようにする）
	float throttle_ = 0.0f;

	// 翼端エフェクト用の前フレーム位置保存
	MyMath::Vector3 lastLeftWingTip_{};
	MyMath::Vector3 lastRightWingTip_{};
	bool hasLastWingTips_ = false;

	// 経過時間（エフェクトアニメーション用）
	float totalTime_ = 0.0f;

	// ナイトビジョン状態
	bool isNvdActive_ = false;

	// 環境管理（太陽・昼夜サイクルなど）
	EnvironmentManager environmentManager_;

	// ============================
	// ポストエフェクト統合制御
	// ============================
	void UpdatePostEffects(float deltaTime);

	bool enableAutoPostEffects_ = true;     // ゲーム状況連動の自動ポストエフェクト
	float damageFlashTimer_ = 0.0f;         // 被弾フラッシュ制御タイマー
	int manualSelectedEffect_ = 0;          // 手動・ImGuiテスト用エフェクトインデックス
	ActivePostEffect manualEffectParams_{}; // 手動・ImGuiテスト用パラメータ

	// 機体設定（CFG）
	AircraftConfig aircraftConfig_;
	bool isAircraftTuningOpen_ = true;
	int selectedAircraftConfigIndex_ = 0;
	char tempAircraftConfigName_[128] = "mig21.cfg";
	void DrawAircraftTuningEditor();
	void DrawMissionObjectivesHUD();
	void DrawMissionMessages();

	// ============================
	// 分割ヘルパー関数群 (Gameplay / Rendering / Combat)
	// ============================
	void UpdateFlightControls(float gameDeltaTime);
	void UpdateCombatFiring(float gameDeltaTime);
	void UpdateFlightAndEffects(float gameDeltaTime);
	void UpdateCombatAndMission(float gameDeltaTime);
	void UpdateCamera(float gameDeltaTime);
	void UpdateDebugAndStatusUI(float gameDeltaTime);
	void DrawVaporConeEffect(Camera* cam);
	void DrawMuzzleFlashEffect(Camera* cam);

	// ============================
	// ポーズメニュー（タイトル画面完全同期・黒背景仕様）
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

	// ポーズからタイトル復帰時のCRTシャットダウン演出（プツッ暗転）
	bool isPauseAborting_ = false;
	float pauseAbortTimer_ = 0.0f;
	static constexpr float kPauseAbortDuration = 0.38f;
	void DrawPauseCRTShutdown(float progress);
};

