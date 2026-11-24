#pragma once
#ifdef USE_IMGUI

#include <memory>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Model/Object3d.h"

class Camera;
class AeroVaporSystem;
class AmbientWeatherSystem;
class CountermeasureSystem;

/// <summary>
/// F9 ショーケース用の仮想フライバイ機（MiG-21）シミュレータ＆描画アクター
/// （高Gバンクオービット／超音速ローパス・ウェイク場送信・翼面ベイパー・アフターバーナー炎）
/// </summary>
class VirtualFlybyJet {
public:
	enum class FlightMode : int {
		BankedOrbit = 0,    // 高G旋回オービット（翼面ベイパー＆翼端渦が常に視界内）
		SupersonicPass = 1  // 超音速ローパス通過（通過時にソニックブーム＆フレア展開）
	};

	void Initialize(Camera* camera);
	void Finalize();

	void Update(
		float dt,
		const MyMath::Vector3& origin,
		AmbientWeatherSystem& weather,
		AeroVaporSystem& aeroVapor,
		CountermeasureSystem& countermeasures);

	void Draw(Camera* camera);

	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }
	bool& EnabledRef() { return enabled_; }

	void SetFlightMode(FlightMode mode) {
		if (flightMode_ != mode) {
			flightMode_ = mode;
			passedCenter_ = false;
		}
	}
	FlightMode GetFlightMode() const { return flightMode_; }

	void SetWingVaporEnabled(bool enabled) { wingVaporEnabled_ = enabled; }
	bool& WingVaporEnabledRef() { return wingVaporEnabled_; }
	bool& AutoFlaresRef() { return autoFlares_; }
	float& OrbitRadiusRef() { return orbitRadius_; }
	float& AltitudeRef() { return altitude_; }
	float& SpeedRef() { return speed_; }

	const MyMath::Vector3& GetPosition() const { return pos_; }
	const MyMath::Vector3& GetVelocity() const { return vel_; }
	const MyMath::Vector3& GetForward() const { return forward_; }
	const MyMath::Vector3& GetRight() const { return right_; }
	const MyMath::Vector3& GetUp() const { return up_; }

private:
	void EmitAfterburnerPlume() const;
	void UpdateVisualTransform();

	std::unique_ptr<Object3d> jetObject_;
	bool enabled_ = false;
	FlightMode flightMode_ = FlightMode::BankedOrbit;
	float angle_ = 0.0f;
	float passT_ = 0.0f;
	float orbitRadius_ = 28.0f;
	float altitude_ = 9.0f;
	float speed_ = 145.0f;
	bool wingVaporEnabled_ = true;
	bool autoFlares_ = false;
	float autoFlareTimer_ = 0.0f;
	bool passedCenter_ = false;

	MyMath::Vector3 pos_ = { 0.0f, 8.0f, 0.0f };
	MyMath::Vector3 vel_ = { 0.0f, 0.0f, 120.0f };
	MyMath::Vector3 forward_ = { 0.0f, 0.0f, 1.0f };
	MyMath::Vector3 right_ = { 1.0f, 0.0f, 0.0f };
	MyMath::Vector3 up_ = { 0.0f, 1.0f, 0.0f };
	float currentG_ = 5.5f;
};

#endif // USE_IMGUI
