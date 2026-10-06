#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include "../FlightModel/DamageModel.h"
#include "../../engine/Graphics/Model/Object3d.h"

class Object3dCommon;
class Camera;

/// @brief 機体のビジュアルモデル（マルチパーツ描画・部位非表示・プロペラ回転制御）
class AircraftVisualModel {
public:
	AircraftVisualModel() = default;
	~AircraftVisualModel() = default;

	/// @brief 初期化
	void Initialize(Object3dCommon* object3dCommon, Camera* camera);

	/// @brief パーツモデルの設定
	void SetModelForPart(DamagePart part, const std::string& modelFilePath, const std::string& targetNodeName = "");
	void SetModelForPart(DamagePart part, Model* model, const std::string& targetNodeName = "");
	void SetModelForPart(DamagePart part, Model* model, const std::vector<std::string>& targetNodeNames);

	/// @brief 単一のモデルファイルから標準的なノード名で一括セットアップ
	void SetupFromSingleModel(const std::string& modelFilePath);
	void SetupFromSingleModel(Model* model);

	/// @brief パーツのローカルオフセット設定
	void SetPartLocalTransform(DamagePart part, const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	/// @brief パーツの表示状態制御
	void SetPartVisible(DamagePart part, bool visible);
	bool IsPartVisible(DamagePart part) const;

	/// @brief プロペラ回転速度の設定 (rad/sec)
	void SetPropellerRpm(float rpm) { propellerRpm_ = rpm; }

	/// @brief モデル全体の向き補正（ベース回転）の設定 (rad)
	void SetBaseRotation(const Vector3& rotate) { baseRotation_ = rotate; }
	Vector3 GetBaseRotation() const { return baseRotation_; }

	/// @brief 更新（親ワールド行列からの階層合成）
	void Update(const Matrix4x4& parentWorldMatrix, float deltaTime);

	/// @brief 描画
	void Draw();

	/// @brief 指定パーツの最新ワールド行列を取得（破片スポーン用）
	Matrix4x4 GetPartWorldMatrix(DamagePart part) const;

	/// @brief 指定パーツの最新ワールド座標を取得
	Vector3 GetPartWorldPosition(DamagePart part) const;

	/// @brief 全パーツのマテリアルパラメータ（光沢度、スペキュラ強度、環境反射係数）を設定
	void SetMaterialProperties(float shininess, float specularIntensity, float envCoefficient);

	/// @brief 全パーツのベースカラー乗算色を設定（焦げ跡・ダメージ変色用）
	void SetMaterialColor(const Vector4& color);

private:
	struct PartNode {
		std::unique_ptr<Object3d> object = nullptr;
		Transform localTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
		Matrix4x4 currentWorldMatrix = Identity4x4();
		bool isVisible = true;
	};

	Object3dCommon* object3dCommon_ = nullptr;
	Camera* camera_ = nullptr;
	std::unordered_map<DamagePart, PartNode> partNodes_;

	Vector3 baseRotation_ = { 0.0f, static_cast<float>(M_PI), 0.0f }; // デフォルトは前後逆補正(180度回転)
	float propellerAngle_ = 0.0f;
	float propellerRpm_ = 2000.0f; // デフォルトRPM
};
