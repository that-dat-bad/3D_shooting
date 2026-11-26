#include "VirtualFlybyJet.h"

#ifdef USE_IMGUI

#include "AeroVaporSystem.h"
#include "../Graphics/AeroVaporMeshRenderer.h"
#include "AmbientWeatherSystem.h"
#include "CountermeasureSystem.h"
#include "../../engine/Graphics/Camera/Camera.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Graphics/Particle/GPUParticleTypes.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

void VirtualFlybyJet::Initialize(Camera* camera) {
	jetObject_.reset();
	enabled_ = false;
	flightMode_ = FlightMode::BankedOrbit;
	angle_ = 0.0f;
	passT_ = 0.0f;
	autoFlareTimer_ = 0.0f;
	passedCenter_ = false;

	// TitleScene等から直接F9起動した場合でも確実にMiG-21 3Dモデルを読み込む（既ロード時は即時返却）
	if (ModelManager* mm = ModelManager::GetInstance()) {
		mm->LoadModel("Resources/models/m21.gltf");
		Model* jetModel = mm->FindModel("Resources/models/m21.gltf");
		if (jetModel && Object3dCommon::GetInstance()) {
			AeroVaporMeshRenderer::GetInstance()->RebuildWingMeshFromModel(jetModel, { 0.0f, 3.14159265f, 0.0f });
			jetObject_ = std::make_unique<Object3d>();
			jetObject_->Initialize(Object3dCommon::GetInstance());
			jetObject_->SetCamera(camera);
			jetObject_->SetModel(jetModel);
		}
	}
}

void VirtualFlybyJet::Finalize() {
	jetObject_.reset();
	enabled_ = false;
}

void VirtualFlybyJet::Update(
	float dt,
	const Vector3& origin,
	AmbientWeatherSystem& weather,
	AeroVaporSystem& aeroVapor,
	CountermeasureSystem& countermeasures) {

	if (!enabled_) {
		if (GPUParticleManager* gpu = GPUParticleManager::GetInstance(); gpu && gpu->IsInitialized()) {
			gpu->ClearWakeFields();
		}
		return;
	}

	if (flightMode_ == FlightMode::BankedOrbit) {
		const float angularSpeed = speed_ / (std::max)(8.0f, orbitRadius_);
		angle_ += angularSpeed * dt;
		if (angle_ > 6.2831853f) {
			angle_ -= 6.2831853f;
		}

		const float c = std::cos(angle_);
		const float s = std::sin(angle_);
		pos_ = {
			origin.x + c * orbitRadius_,
			origin.y + altitude_ + std::sin(angle_ * 2.0f) * 1.6f,
			origin.z + s * orbitRadius_
		};
		forward_ = Normalize({ -s, 0.0f, c });
		const Vector3 inward = Normalize({ -c, 0.0f, -s });
		const Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
		const float bankRad = 1.08f; // 62度バンク旋回姿勢
		up_ = Normalize(Add(Multiply(std::cos(bankRad), worldUp), Multiply(std::sin(bankRad), inward)));
		right_ = Normalize(Cross(up_, forward_));
		vel_ = Multiply(speed_, forward_);
		currentG_ = 6.2f;
	} else {
		// 超音速ローパス通過モード（-Z -> +Z へ一直線に駆け抜ける）
		passT_ += dt * 0.42f;
		if (passT_ >= 1.0f) {
			passT_ -= 1.0f;
			passedCenter_ = false;
		}
		const float zCoord = -95.0f + passT_ * 190.0f;
		pos_ = { origin.x, origin.y + altitude_, origin.z + zCoord };
		forward_ = { 0.0f, 0.0f, 1.0f };
		right_ = { 1.0f, 0.0f, 0.0f };
		up_ = { 0.0f, 1.0f, 0.0f };
		vel_ = { 0.0f, 0.0f, 345.0f };
		currentG_ = 1.5f;

		if (!passedCenter_ && zCoord >= -2.0f) {
			passedCenter_ = true;
			aeroVapor.TriggerSonicBoom(pos_, forward_, vel_, 1.0f);
			if (autoFlares_) {
				countermeasures.TriggerSalvo(true);
			}
		}
	}

	// ウェイク場（翼端渦＆アフターバーナー排気ブラスト）をGPUへ送信
	AmbientWeatherSystem::WakeSource wake{};
	wake.position = pos_;
	wake.velocity = vel_;
	wake.forward = forward_;
	wake.right = right_;
	wake.halfWingspan = 4.50f;
	wake.currentG = currentG_;
	wake.throttle = 1.1f;
	wake.afterburnerIntensity = 1.0f;
	wake.active = true;
	weather.SubmitWakeSource(0u, wake);

	// 翼面ベイパー＆遷音速ベイパーの連続発生（3D多層シェル＋後縁千切れGPUパーティクル）
	AeroVaporSystem::AircraftAeroState aero{};
	aero.position = pos_;
	aero.velocity = vel_;
	aero.forward = forward_;
	aero.right = right_;
	aero.up = up_;
	aero.speed = Length(vel_);
	aero.currentG = currentG_;
	aero.aoaRad = (flightMode_ == FlightMode::BankedOrbit) ? 0.24f : 0.04f;
	aero.halfWingspan = 4.50f;

	const float wingInt = (wingVaporEnabled_ && flightMode_ == FlightMode::BankedOrbit) ? 0.85f : 0.0f;
	const float coneInt = (wingVaporEnabled_ && flightMode_ == FlightMode::SupersonicPass) ? 0.90f : 0.0f;
	aeroVapor.UpdateVisualStateExternal(dt, aero, wingInt, coneInt);

	if (wingVaporEnabled_) {
		if (flightMode_ == FlightMode::BankedOrbit) {
			aeroVapor.EmitHighGWingVaporStep(aero, 0.85f);
		} else {
			aeroVapor.EmitTransonicConeStep(aero, 0.90f);
		}
	}

	EmitAfterburnerPlume();

	// 自動フレア展開（オービット中）
	if (autoFlares_ && flightMode_ == FlightMode::BankedOrbit) {
		autoFlareTimer_ += dt;
		if (autoFlareTimer_ >= 2.2f) {
			autoFlareTimer_ = 0.0f;
			countermeasures.TriggerSalvo(true);
		}
	}

	UpdateVisualTransform();
}

void VirtualFlybyJet::EmitAfterburnerPlume() const {
	GPUParticleManager* gpu = GPUParticleManager::GetInstance();
	if (!gpu || !gpu->IsInitialized()) {
		return;
	}

	const Vector3 nozzlePos = Add(pos_, Multiply(-6.8f, forward_));
	GpuParticleEmitDesc ab{};
	ab.origin = nozzlePos;
	ab.randomPositionRange = 0.22f;
	ab.minVelocity = Multiply(-48.0f, forward_);
	ab.maxVelocity = Multiply(-28.0f, forward_);
	ab.minLifeTime = 0.06f;
	ab.maxLifeTime = 0.12f;
	ab.minScale = 0.75f;
	ab.maxScale = 1.25f;
	ab.endScale = 0.12f;
	ab.scaleEasing = 0.5f;
	ab.minColor = { 0.45f, 0.75f, 1.0f, 0.85f };
	ab.maxColor = { 1.00f, 0.85f, 0.45f, 1.00f };
	ab.count = 8u;
	ab.flags = GPUParticleFlags::kFadeOut | GPUParticleFlags::kAdditive |
	           GPUParticleFlags::kBlackbody | GPUParticleFlags::kStretched |
	           GPUParticleFlags::kWorldUnitsPerSec;
	ab.shape = GPUParticleShape::SparkStreak;
	ab.stretchFactor = 0.06f;
	ab.minTemperature = 0.95f;
	ab.maxTemperature = 1.25f;
	ab.coolingRate = 4.2f;
	ab.emissiveIntensity = 2.1f;
	gpu->EnqueueRichEmit(ab);
}

void VirtualFlybyJet::UpdateVisualTransform() {
	if (!jetObject_) {
		return;
	}
	// m21.gltf は -Z 前方なので Y軸180度補正
	Matrix4x4 rotMat = Identity4x4();
	rotMat.m[0][0] = right_.x;   rotMat.m[0][1] = right_.y;   rotMat.m[0][2] = right_.z;
	rotMat.m[1][0] = up_.x;      rotMat.m[1][1] = up_.y;      rotMat.m[1][2] = up_.z;
	rotMat.m[2][0] = forward_.x; rotMat.m[2][1] = forward_.y; rotMat.m[2][2] = forward_.z;
	rotMat.m[3][0] = pos_.x;     rotMat.m[3][1] = pos_.y;     rotMat.m[3][2] = pos_.z;

	Matrix4x4 baseFlip = MakeRotateYMatrix(3.14159265f);
	jetObject_->UpdateWithWorldMatrix(Multiply(baseFlip, rotMat));
}

void VirtualFlybyJet::Draw(Camera* camera) {
	if (!enabled_ || !camera) {
		return;
	}

	if (jetObject_) {
		jetObject_->SetCamera(camera);
		Object3dCommon::GetInstance()->SetupCommonState();
		jetObject_->Draw();
	} else if (PrimitiveModel* prim = PrimitiveModel::GetInstance()) {
		const Vector3 nose = Add(pos_, Multiply(6.0f, forward_));
		const Vector3 tail = Add(pos_, Multiply(-6.0f, forward_));
		const Vector3 tipL = Add(Add(pos_, Multiply(-5.8f, right_)), Multiply(-2.0f, forward_));
		const Vector3 tipR = Add(Add(pos_, Multiply( 5.8f, right_)), Multiply(-2.0f, forward_));
		const Vector4 col = { 0.4f, 0.95f, 1.0f, 0.95f };
		prim->DrawLine3D(nose, tail, col, camera);
		prim->DrawLine3D(nose, tipL, col, camera);
		prim->DrawLine3D(nose, tipR, col, camera);
		prim->DrawLine3D(tipL, tail, col, camera);
		prim->DrawLine3D(tipR, tail, col, camera);
	}
}

#endif // USE_IMGUI
