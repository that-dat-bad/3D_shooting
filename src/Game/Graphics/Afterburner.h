#pragma once
#include <string>
#include <memory>
#include <vector>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/System/BlendMode.h"
#include "../../engine/Graphics/Camera/Camera.h"

/// @brief ジェット戦闘機アフターバーナー（A/B: Afterburner）エフェクトクラス
///        左右ツインエンジン対応、超音速ショックダイヤモンド（マッハディスク）、
///        多層インナーコア・アウタープルーム・ノズルグローを高品位レンダリング
class Afterburner {
public:
	Afterburner();
	~Afterburner() = default;

	/// @brief 初期化（必要なテクスチャのロード・設定）
	void Initialize();

	/// @brief 毎フレームの更新
	/// @param dt デルタタイム (秒)
	/// @param intensity アフターバーナー強度 (0.0f〜1.0f)
	void Update(float dt, float intensity = 1.0f);

	/// @brief 描画（機体位置とオイラー角回転から描画）
	/// @param aircraftPosition 機体ワールド座標
	/// @param aircraftRotation 機体オイラー角 (Pitch, Yaw, Roll [rad])
	/// @param camera 描画カメラ
	void Draw(const MyMath::Vector3& aircraftPosition, const MyMath::Vector3& aircraftRotation, Camera* camera);

	/// @brief 描画（機体のワールド行列から描画）
	/// @param aircraftWorldMatrix 機体のワールド行列
	/// @param camera 描画カメラ
	void Draw(const MyMath::Matrix4x4& aircraftWorldMatrix, Camera* camera);

	// ========================================================
	// パラメータカスタマイズ
	// ========================================================

	/// @brief 双発エンジンフラグ
	void SetTwinEngine(bool isTwin) { isTwinEngine_ = isTwin; }
	bool IsTwinEngine() const { return isTwinEngine_; }

	/// @brief 左右エンジンノズルのローカルオフセット設定
	void SetNozzleOffsets(const MyMath::Vector3& leftNozzle, const MyMath::Vector3& rightNozzle) {
		leftNozzleOffset_ = leftNozzle;
		rightNozzleOffset_ = rightNozzle;
	}

	/// @brief 単発ノズル時のローカルオフセット設定
	void SetSingleNozzleOffset(const MyMath::Vector3& offset) {
		singleNozzleOffset_ = offset;
	}

	/// @brief ノズル噴射角オフセット設定・取得 (Pitch, Yaw, Roll [rad])
	void SetNozzleRotation(const MyMath::Vector3& rotation) { nozzleRotation_ = rotation; }
	const MyMath::Vector3& GetNozzleRotation() const { return nozzleRotation_; }

	/// @brief 基準の炎の長さ設定 (m)
	void SetFlameLength(float length) { baseFlameLength_ = length; }
	float GetFlameLength() const { return baseFlameLength_; }

	/// @brief 基準のノズル半径設定 (m)
	void SetFlameRadius(float radius) { baseFlameRadius_ = radius; }
	float GetFlameRadius() const { return baseFlameRadius_; }

	/// @brief ショックダイヤモンド（マッハディスク）数
	void SetShockDiamondCount(int count) { shockDiamondCount_ = count; }
	int GetShockDiamondCount() const { return shockDiamondCount_; }

	/// @brief 単発ノズル時のローカルオフセット取得
	const MyMath::Vector3& GetSingleNozzleOffset() const { return singleNozzleOffset_; }

	/// @brief 左右エンジンノズルのローカルオフセット取得
	const MyMath::Vector3& GetLeftNozzleOffset() const { return leftNozzleOffset_; }
	const MyMath::Vector3& GetRightNozzleOffset() const { return rightNozzleOffset_; }

	/// @brief 炎のベースカラー設定・取得（RGB加算色）
	void SetFlameColor(const MyMath::Vector3& color) { baseFlameColor_ = color; }
	const MyMath::Vector3& GetFlameColor() const { return baseFlameColor_; }

	/// @brief インナーコア・ショックダイヤモンド色設定・取得
	void SetCoreColor(const MyMath::Vector3& color) { coreColor_ = color; }
	const MyMath::Vector3& GetCoreColor() const { return coreColor_; }

	/// @brief アフターバーナー強度設定・取得 (0.0f〜1.0f)
	void SetIntensity(float intensity) { currentIntensity_ = intensity; }
	float GetIntensity() const { return currentIntensity_; }

	/// @brief 設定をJSONファイルに保存
	bool SaveConfig(const std::string& filePath) const;

	/// @brief 設定をJSONファイルから読み込み
	bool LoadConfig(const std::string& filePath);

private:
	/// @brief 1基のノズルに対するアフターバーナー描画
	void DrawSingleNozzle(const MyMath::Matrix4x4& nozzleWorldMatrix, Camera* camera);

	// ノズル設定（m21_wg の双発ジェットノズル実寸値）
	bool isTwinEngine_ = true;
	MyMath::Vector3 leftNozzleOffset_  = { -0.407f, 0.011f, 7.55f };
	MyMath::Vector3 rightNozzleOffset_ = {  0.396f, 0.011f, 7.55f };
	MyMath::Vector3 singleNozzleOffset_ = { 0.0f, 0.0f, 7.55f };
	MyMath::Vector3 nozzleRotation_ = { 0.0f, 0.0f, 0.0f }; ///< ノズル噴射角 (Pitch, Yaw, Roll [rad])

	// 形状パラメータ
	float baseFlameLength_ = 5.2f;    ///< アフターバーナー炎の長さ
	float baseFlameRadius_ = 0.28f;   ///< ノズル出口半径
	int shockDiamondCount_ = 5;       ///< マッハディスクの節の数
	MyMath::Vector3 baseFlameColor_ = { 0.20f, 0.65f, 1.0f }; ///< 外炎（コバルトブルー〜エレクトリックシアン）
	MyMath::Vector3 coreColor_ = { 0.88f, 0.95f, 1.0f };      ///< 内炎・マッハディスク（白熱青白）

	// 状態・アニメーション
	float animTimer_ = 0.0f;
	float currentIntensity_ = 1.0f;
	float flicker_ = 1.0f;

	// テクスチャインデックス
	uint32_t flameTexIndex_ = 0;
	uint32_t shockDiamondTexIndex_ = 0;
	uint32_t whiteTexIndex_ = 0;
};
