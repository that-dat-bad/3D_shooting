#pragma once
#include <memory>
#include <vector>
#include <string>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Sprite/Sprite.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Audio/AudioManager.h"

using namespace MyMath;

class DirectXCommon;
class AircraftVisualModel;

/// @brief 戦闘モード起動スキャン定数バッファ構造体 (HLSL register b3 と同一アライメント)
struct CombatBootScanSettings {
	float scanProgress = 0.0f;      // 0.0: 未起動 -> 1.0: スキャン完了
	float scanBandWidth = 3.5f;     // スキャン波形帯の幅
	float scanMaxRadius = 13.0f;    // 機体中心からの最大走査半径
	float scanWaveFreq = 3.5f;      // 波形干渉縞の周波数
	Vector4 scanColor = { 0.15f, 1.0f, 0.55f, 1.0f }; // エメラルドグリーン強烈発光
	float scanTime = 0.0f;          // 脈動パルス用タイマー
	Vector3 centerOffset = { 0.0f, 0.0f, 0.0f }; // スキャン起点（中心オフセット）
};

/// @brief AC6（アーマード・コア6）スタイルの「メインシステム、戦闘モード起動...」演出シーケンス管理クラス
class CombatBootSequence {
public:
	CombatBootSequence() = default;
	~CombatBootSequence();

	/// @brief 初期化
	/// @param spriteCommon SpriteCommonポインタ
	/// @param dxCommon DirectXCommonポインタ (nullptrの場合はシングルトン利用)
	void Initialize(SpriteCommon* spriteCommon, DirectXCommon* dxCommon = nullptr);

	/// @brief 自機戦闘モード起動スキャン描画開始（案1: GSマテリアライズパイプライン適用）
	/// @param visualModel 自機ビジュアルモデル
	/// @param scanProgress スキャン進行度（0.0f: 完全不可視 〜 1.0f: 完全実体化）
	void BeginAircraftScanDraw(AircraftVisualModel& visualModel, float scanProgress);

	/// @brief 自機戦闘モード起動スキャン描画終了（通常パイプラインに復元）
	/// @param visualModel 自機ビジュアルモデル
	void EndAircraftScanDraw(AircraftVisualModel& visualModel);

	/// @brief 演出開始（音声再生とタイマーリセット）
	void Start();

	/// @brief リトライ用高速ブートモード設定（0.9秒で即座に戦闘復帰）
	void SetQuickMode(bool quick) { isQuickMode_ = quick; }
	bool IsQuickMode() const { return isQuickMode_; }

	/// @brief 毎フレーム更新
	/// @param dt 経過時間
	void Update(float dt);

	/// @brief 背景の黒フェード描画（3Dシーンの直後に描画）
	void DrawBackgroundFade();

	/// @brief AC6風ブートUI描画（最前面に描画）
	void DrawBootUI();

	/// @brief 演出が完了したか
	bool IsFinished() const { return timer_ >= (isQuickMode_ ? kQuickDuration : kDuration); }

	/// @brief 演出の総時間（秒）
	float GetCurrentDuration() const { return isQuickMode_ ? kQuickDuration : kDuration; }
	static constexpr float GetDuration() { return kDuration; }

	/// @brief 現在の演出タイマー取得
	float GetTimer() const { return timer_; }

	/// @brief ブリーフィング中、SPACEキー入力待ち状態か
	bool IsWaitingForSpace() const { return isWaitingForSpace_; }

	/// @brief カーソル位置設定（ブリーフィング時のボタンホバー判定用）
	void SetCursorPos(const Vector2& pos) { cursorPos_ = pos; }

	/// @brief 黒フェードの現在のアルファ値（0.0f〜1.0f）
	float GetFadeAlpha() const { return fadeAlpha_; }

	/// @brief 演出終了処理
	void Finalize();

private:
	// --- ブートUI描画サブルーチン ---
	void DrawQuickBootUI(float cx, float cy);
	void DrawLoadingDiagnostics(float cx, float cy);
	void DrawCinematicLetterbox(float sw, float sh);
	void DrawScanningLaser(float sw, float cx, float cy);
	void DrawTacticalBanner(float cx, float cy, float bannerW, float overallAlpha);
	void DrawTacticalReticle(float cx, float cy, float easeExpand, float overallAlpha);
	void DrawBootText(float cx, float cy, float topY, float overallAlpha);
	void DrawTelemetryLogs(float cx, float cy, float bannerW, float overallAlpha);
	void DrawPilotPrompt(float cx, float overallAlpha);

	// --- 2D描画ヘルパー ---
	void DrawLine(const Vector2& p1, const Vector2& p2, float thickness, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);
	void DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& anchor = { 0.0f, 0.0f }, BlendMode blendMode = BlendMode::kNormal);
	void DrawTacticalBracket(const Vector2& center, const Vector2& size, float bracketLen, float thickness, const Vector4& color);
	void DrawCircleRing(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// スプライト群
	std::unique_ptr<Sprite> fadeSprite_;    ///< 全画面黒フェード用
	std::unique_ptr<Sprite> rectSprite_;    ///< ライン・矩形描画用
	std::unique_ptr<Sprite> ringSprite_;    ///< 円形リング描画用

	// スプライトプール（複数Rect/Line同時描画時のCBV競合解消）
	std::vector<std::unique_ptr<Sprite>> bootSpritePool_;
	size_t bootSpritePoolIndex_ = 0;
	Sprite* GetBootSprite();

	std::vector<std::unique_ptr<Sprite>> bootCirclePool_;
	size_t bootCirclePoolIndex_ = 0;
	Sprite* GetBootCircleSprite();

	// サウンドデータ
	SoundData voiceData_{};
	bool voicePlayed_ = false;

	// タイマー管理
	float timer_ = 0.0f;
	float briefingTimer_ = 0.0f;                  ///< ブリーフィング中のアニメーションタイマー
	bool isWaitingForSpace_ = false;              ///< SPACEキーでの出撃待機中フラグ
	Vector2 cursorPos_ = { -999.0f, -999.0f };    ///< ブリーフィング中マウス座標
	static constexpr float kDuration = 5.20f;      ///< 演出の総時間（秒: 作戦ブリーフィング＋戦闘起動＋操縦権移行）
	static constexpr float kQuickDuration = 0.90f; ///< リトライ時高速演出の総時間（秒）
	bool isQuickMode_ = false;                    ///< リトライ用高速ブートフラグ
	float fadeAlpha_ = 1.0f;

	// スキャンパイプライン（案1: 自機専用GSマテリアライズ）
	void InitializeScanPipeline(DirectXCommon* dxCommon);

	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> scanPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12Resource> scanSettingsResource_;
	CombatBootScanSettings* scanSettingsData_ = nullptr;
};
