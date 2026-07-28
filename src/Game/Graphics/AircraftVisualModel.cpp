#include "AircraftVisualModel.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include <numbers>

void AircraftVisualModel::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
	object3dCommon_ = object3dCommon;
	camera_ = camera;
}

void AircraftVisualModel::SetModelForPart(DamagePart part, const std::string& modelFilePath, const std::string& targetNodeName) {
	Model* model = ModelManager::GetInstance()->FindModel(modelFilePath);
	if (!model) {
		ModelManager::GetInstance()->LoadModel(modelFilePath);
		model = ModelManager::GetInstance()->FindModel(modelFilePath);
	}
	SetModelForPart(part, model, targetNodeName);
}

void AircraftVisualModel::SetModelForPart(DamagePart part, Model* model, const std::string& targetNodeName) {
	auto& node = partNodes_[part];
	if (!node.object) {
		node.object = std::make_unique<Object3d>();
		node.object->Initialize(object3dCommon_);
		if (camera_) {
			node.object->SetCamera(camera_);
		}
	}
	node.object->SetModel(model);
	node.object->SetTargetNodeName(targetNodeName);
	if (node.object->GetModel()) {
		node.object->GetModel()->SetEnvironmentCoefficient(0.0f); // 金属反射をOFF
	}
}

static bool HasNode(const Model::Node& node, const std::string& name) {
	if (node.name == name) return true;
	for (const auto& child : node.children) {
		if (HasNode(child, name)) return true;
	}
	return false;
}

void AircraftVisualModel::SetupFromSingleModel(const std::string& modelFilePath) {
	Model* model = ModelManager::GetInstance()->FindModel(modelFilePath);
	if (!model) {
		ModelManager::GetInstance()->LoadModel(modelFilePath);
		model = ModelManager::GetInstance()->FindModel(modelFilePath);
	}
	SetupFromSingleModel(model);
}

void AircraftVisualModel::SetupFromSingleModel(Model* model) {
	if (!model) return;

	// 部位と対応する一般的なノード名のマッピング
	std::unordered_map<DamagePart, std::string> nameMap = {
		{DamagePart::Fuse, "Fuse"},
		{DamagePart::Engine1, "Engine1"},
		{DamagePart::Propeller1, "Propeller1"},
		{DamagePart::Wing_L, "Wing_L"},
		{DamagePart::Wing_R, "Wing_R"},
		{DamagePart::Wing1_L, "Wing1_L"},
		{DamagePart::Wing1_R, "Wing1_R"},
		{DamagePart::Wing2_L, "Wing2_L"},
		{DamagePart::Wing2_R, "Wing2_R"},
		{DamagePart::Tail, "Tail"},
		{DamagePart::Rudder, "Rudder"},
		{DamagePart::Elevator0, "Elevator_L"},
		{DamagePart::Elevator1, "Elevator_R"}
	};

	const auto& rootNode = model->GetModelData().rootNode;

	for (const auto& [part, nodeName] : nameMap) {
		// そのノードがモデル内に存在する場合のみ、部位として登録する
		if (HasNode(rootNode, nodeName)) {
			SetModelForPart(part, model, nodeName);
		}
	}
}

void AircraftVisualModel::SetPartLocalTransform(DamagePart part, const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	auto& node = partNodes_[part];
	node.localTransform.scale = scale;
	node.localTransform.rotate = rotate;
	node.localTransform.translate = translate;
}

void AircraftVisualModel::SetPartVisible(DamagePart part, bool visible) {
	auto it = partNodes_.find(part);
	if (it != partNodes_.end()) {
		it->second.isVisible = visible;
	}
}

bool AircraftVisualModel::IsPartVisible(DamagePart part) const {
	auto it = partNodes_.find(part);
	if (it != partNodes_.end()) {
		return it->second.isVisible;
	}
	return false;
}

void AircraftVisualModel::Update(const Matrix4x4& parentWorldMatrix, float deltaTime) {
	// プロペラ回転角の更新
	if (propellerRpm_ > 0.001f) {
		float revPerSec = propellerRpm_ / 60.0f;
		propellerAngle_ += revPerSec * 2.0f * std::numbers::pi_v<float> * deltaTime;
		if (propellerAngle_ > 2.0f * std::numbers::pi_v<float>) {
			propellerAngle_ -= 2.0f * std::numbers::pi_v<float>;
		}
	}

	Matrix4x4 baseRotateMatrix = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, baseRotation_, { 0.0f, 0.0f, 0.0f });
	Matrix4x4 adjustedParentMatrix = Multiply(baseRotateMatrix, parentWorldMatrix);

	for (auto& [part, node] : partNodes_) {
		if (!node.object || !node.isVisible) {
			continue;
		}

		// ローカル回転の計算（プロペラなどの自転も追加）
		Vector3 localRot = node.localTransform.rotate;
		if (part == DamagePart::Propeller1) {
			localRot.z += propellerAngle_; // Roll軸周りに回転
		}

		Matrix4x4 localMatrix = MakeAffineMatrix(node.localTransform.scale, localRot, node.localTransform.translate);
		node.currentWorldMatrix = Multiply(localMatrix, adjustedParentMatrix);

		node.object->UpdateWithWorldMatrix(node.currentWorldMatrix);
	}
}

void AircraftVisualModel::Draw() {
	for (auto& [part, node] : partNodes_) {
		if (node.object && node.isVisible) {
			node.object->Draw();
		}
	}
}

Matrix4x4 AircraftVisualModel::GetPartWorldMatrix(DamagePart part) const {
	auto it = partNodes_.find(part);
	if (it != partNodes_.end()) {
		return it->second.currentWorldMatrix;
	}
	return Identity4x4();
}

Vector3 AircraftVisualModel::GetPartWorldPosition(DamagePart part) const {
	Matrix4x4 world = GetPartWorldMatrix(part);
	return { world.m[3][0], world.m[3][1], world.m[3][2] };
}
