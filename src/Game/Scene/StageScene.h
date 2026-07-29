#pragma once
#include "IScene.h"
#include <memory>
#include "../Environment/EnvironmentManager.h"
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/Graphics/Model/Skybox.h"
#include "../Camera/PlayerCamera.h"
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

#include "../System/MissionManager.h"

/// @brief ゲーム本編のステージシーン
class StageScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void Finalize() override;
	void Restart();

private:
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
		bool IsCollisionActive() const override { return !scene_->isMissionFailed_; }
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

	void TriggerGameOver();

	// ゲーム状態
	bool isMissionCleared_ = false;
	bool isMissionFailed_ = false;
	bool isGameOver_ = false;
	float gameOverTimer_ = 0.0f;
	static constexpr float kGameOverDuration = 2.0f;
	float timeScale_ = 1.0f;

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

	// ミッションエディタ用
	bool isMissionEditorOpen_ = true;
	char tempMissionName_[128] = "";
	char tempMissionDesc_[256] = "";
	char tempSaveFileName_[128] = "";
	int selectedMissionIndex_ = 0;
	int selectedEnemyIndex_ = -1;
	void DrawMissionEditor();
};

