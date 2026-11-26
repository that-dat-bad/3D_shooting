#pragma once
#include <cstdint>
#include <vector>
#include "../../engine/base/Math/MyMath.h"

/// <summary>
/// 航空機カウンターメジャー（チャフ＆フレア）展開・燃焼管理システム
/// （光学FlareCore・オレンジ照り返し煙・距離補間白煙リボン・繊細スパーク・ウェイク連動金属チャフ）
/// </summary>
class CountermeasureSystem {
public:
	struct AircraftEmitterState {
		MyMath::Vector3 position = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 velocity = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 forward  = { 0.0f, 0.0f, 1.0f };
		MyMath::Vector3 right    = { 1.0f, 0.0f, 0.0f };
		MyMath::Vector3 up       = { 0.0f, 1.0f, 0.0f };
		float floorY = 0.0f;
	};

	void Reset(int maxAmmo = 60);

	/// <summary>
	/// 1斉射（4ペア連続放出）の展開を要求する
	/// </summary>
	bool TriggerSalvo(bool ignoreAmmo = false);

	/// <summary>
	/// 毎フレーム更新（斉射タイマー進行・燃焼中フレアの軌跡＆煙幕＆火花生成）
	/// </summary>
	void Update(float dt, const AircraftEmitterState& aircraft);

	void Replenish() { currentAmmo_ = maxAmmo_; }
	void ClearActiveDecoys() { activeFlares_.clear(); pendingPairs_ = 0; }

	int GetRemainingAmmo() const { return currentAmmo_; }
	int GetMaxAmmo() const { return maxAmmo_; }
	bool IsSalvoActive() const { return pendingPairs_ > 0; }
	size_t GetActiveFlareCount() const { return activeFlares_.size(); }

private:
	struct FlareDecoy {
		MyMath::Vector3 position{};
		MyMath::Vector3 prevPosition{};
		MyMath::Vector3 velocity{};
		float age = 0.0f;
		float lifeTime = 2.8f;
		float emitAccum = 0.0f;
		float floorY = 0.0f;
		uint32_t seed = 0u;
	};

	void EjectPair(const AircraftEmitterState& aircraft, int pairIndex);
	void EmitIgnitionFlash(const MyMath::Vector3& pos, const MyMath::Vector3& vel);
	void EmitChaffCloud(const MyMath::Vector3& pos, const MyMath::Vector3& vel, const MyMath::Vector3& ejectDir);
	void EmitFlareTrailSegment(const FlareDecoy& flare, float dt);

	std::vector<FlareDecoy> activeFlares_;
	int maxAmmo_ = 60;
	int currentAmmo_ = 60;
	int pendingPairs_ = 0;
	int emittedPairIndex_ = 0;
	float salvoIntervalTimer_ = 0.0f;
	float cooldownTimer_ = 0.0f;
	bool salvoIgnoreAmmo_ = false;
	uint32_t decoySerial_ = 1u;
};
