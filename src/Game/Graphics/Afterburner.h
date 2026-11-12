#pragma once
#include <string>
#include <memory>
#include <vector>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/System/BlendMode.h"
#include "../../engine/Graphics/Camera/Camera.h"

// アフターバーナー演出クラス
class Afterburner {
public:
	Afterburner();
	~Afterburner() = default;

	// 初期化
	void Initialize();

	// 更新
	// dt: デルタタイム
	// intensity: 強度
	void Update(float dt, float intensity = 1.0f);

	// 描画
	// aircraftPosition: 機体座標
	// aircraftRotation: 機体回転
	// camera: カメラ
	void Draw(const MyMath::Vector3& aircraftPosition, const MyMath::Vector3& aircraftRotation, Camera* camera);

	// 描画（行列指定）
	// aircraftWorldMatrix: ワールド行列
	// camera: カメラ
	void Draw(const MyMath::Matrix4x4& aircraftWorldMatrix, Camera* camera);

	// ========================================================
	// パラメータカスタマイズ
	// ========================================================

	void SetTwinEngine(bool isTwin) { isTwinEngine_ = isTwin; }
	bool IsTwinEngine() const { return isTwinEngine_; }

	// ノズルオフセット設定
	void SetNozzleOffsets(const MyMath::Vector3& leftNozzle, const MyMath::Vector3& rightNozzle) {
		leftNozzleOffset_ = leftNozzle;
		rightNozzleOffset_ = rightNozzle;
	}

	void SetSingleNozzleOffset(const MyMath::Vector3& offset) {
		singleNozzleOffset_ = offset;
	}

	void SetNozzleRotation(const MyMath::Vector3& rotation) { nozzleRotation_ = rotation; }
	const MyMath::Vector3& GetNozzleRotation() const { return nozzleRotation_; }

	void SetFlameLength(float length) { baseFlameLength_ = length; }
	float GetFlameLength() const { return baseFlameLength_; }

	void SetFlameRadius(float radius) { baseFlameRadius_ = radius; }
	float GetFlameRadius() const { return baseFlameRadius_; }

	void SetShockDiamondCount(int count) { shockDiamondCount_ = count; }
	int GetShockDiamondCount() const { return shockDiamondCount_; }

	const MyMath::Vector3& GetSingleNozzleOffset() const { return singleNozzleOffset_; }
	const MyMath::Vector3& GetLeftNozzleOffset() const { return leftNozzleOffset_; }
	const MyMath::Vector3& GetRightNozzleOffset() const { return rightNozzleOffset_; }

	void SetFlameColor(const MyMath::Vector3& color) { baseFlameColor_ = color; }
	const MyMath::Vector3& GetFlameColor() const { return baseFlameColor_; }

	void SetCoreColor(const MyMath::Vector3& color) { coreColor_ = color; }
	const MyMath::Vector3& GetCoreColor() const { return coreColor_; }

	void SetIntensity(float intensity) { currentIntensity_ = intensity; }
	float GetIntensity() const { return currentIntensity_; }

	// 設定保存
	// filePath: ファイルパス
	bool SaveConfig(const std::string& filePath) const;

	// 設定読み込み
	// filePath: ファイルパス
	bool LoadConfig(const std::string& filePath);

private:
	// ノズル個別描画
	void DrawSingleNozzle(const MyMath::Matrix4x4& nozzleWorldMatrix, Camera* camera);

	// ノズル設定（m21_wg の双発ジェットノズル実寸値）
	bool isTwinEngine_ = true;
	MyMath::Vector3 leftNozzleOffset_  = { -0.407f, 0.011f, 7.55f };
	MyMath::Vector3 rightNozzleOffset_ = {  0.396f, 0.011f, 7.55f };
	MyMath::Vector3 singleNozzleOffset_ = { 0.0f, 0.0f, 7.55f };
	MyMath::Vector3 nozzleRotation_ = { 0.0f, 0.0f, 0.0f }; // ノズル噴射角 (Pitch, Yaw, Roll [rad])

	// 形状パラメータ
	float baseFlameLength_ = 5.2f;    // アフターバーナー炎の長さ
	float baseFlameRadius_ = 0.28f;   // ノズル出口半径
	int shockDiamondCount_ = 5;       // マッハディスクの節の数
	MyMath::Vector3 baseFlameColor_ = { 0.20f, 0.65f, 1.0f }; // 外炎（コバルトブルー〜エレクトリックシアン）
	MyMath::Vector3 coreColor_ = { 0.88f, 0.95f, 1.0f };      // 内炎・マッハディスク（白熱青白）

	// 状態・アニメーション
	float animTimer_ = 0.0f;
	float currentIntensity_ = 1.0f;
	float flicker_ = 1.0f;

	// テクスチャインデックス
	uint32_t flameTexIndex_ = 0;
	uint32_t shockDiamondTexIndex_ = 0;
	uint32_t whiteTexIndex_ = 0;
};
