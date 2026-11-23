#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Missile.h"
#include "../../engine/base/Math/MyMath.h"

class Camera;
class Enemy;
class EnemyManager;
class GroundEnemy;
class Object3d;
class TerrainChunkManager;

// IRシーカーの状態
enum class MissileSeekerState : int {
	Off = 0,
	WarmingUp = 1,
	Searching = 2,
	Locking = 3,
	Locked = 4
};

// HUD・UI連携用シーカー状態スナップショット
struct MissileSeekerStatus {
	MissileSeekerState state = MissileSeekerState::Off;
	float warmupProgress = 0.0f;
	float lockProgress = 0.0f;
	float outerGimbalAngleDeg = 12.0f;
	float innerAcqAngleDeg = 4.5f;
	MyMath::Vector3 boresightWorldPoint{};
	MyMath::Vector3 seekerTrackWorldPoint{};
	Enemy* lockedAirTarget = nullptr;
	GroundEnemy* lockedGroundTarget = nullptr;
	float targetDistance = 0.0f;
	bool hasTarget = false;
};

// ミサイル・IRシーカー統合管理クラス
class MissileManager {
public:
	MissileManager() = default;
	~MissileManager();

	// 初期化（最大同時飛翔数）
	void Initialize(uint32_t maxActiveMissiles = 16);

	// 飛翔中ミサイルを全消去
	void Clear();

	// シーカー状態をリセット
	void ResetSeeker();

	// 3Dモデル差し替え用パス設定（空文字列で仮プリミティブ描画へフォールバック）
	void SetModelPath(const std::string& modelPath);
	const std::string& GetModelPath() const { return customModelPath_; }

	// IRシーカー更新
	void UpdateSeeker(
		float dt,
		bool seekerEnabled,
		int remainingMissiles,
		const MyMath::Vector3& aircraftPos,
		const MyMath::Vector3& aircraftForward,
		EnemyManager& enemyManager
	);

	// ロック中のターゲットに向けてミサイルを発射
	bool LaunchLockedMissile(
		const MyMath::Vector3& spawnWorldPos,
		const MyMath::Vector3& aircraftVelocity,
		const MyMath::Vector3& aircraftForward,
		const MissileSpec& spec = MissileSpec{}
	);

	// 全飛翔中ミサイルの更新と近接信管・爆風処理
	void Update(float dt, EnemyManager& enemyManager, TerrainChunkManager* terrain);

	// 飛翔中ミサイル描画
	void Draw(Camera* camera);

	// 自機翼下パイロン懸架中の未発射ミサイル描画
	void DrawMountedMissile(
		const MyMath::Vector3& worldPos,
		const MyMath::Vector3& right,
		const MyMath::Vector3& up,
		const MyMath::Vector3& forward,
		Camera* camera
	);

	// アクセッサ
	std::vector<Missile>& GetMissiles() { return missiles_; }
	const std::vector<Missile>& GetMissiles() const { return missiles_; }
	uint32_t GetActiveMissileCount() const;
	const MissileSeekerStatus& GetSeekerStatus() const { return seekerStatus_; }
	bool IsLocked() const { return seekerStatus_.state == MissileSeekerState::Locked; }

private:
	void ValidateCurrentTarget(
		const MyMath::Vector3& aircraftPos,
		const MyMath::Vector3& aircraftForward,
		float& outEffectiveRange,
		float& outAspectFactor
	);

	void AcquireBestTarget(
		const MyMath::Vector3& aircraftPos,
		const MyMath::Vector3& aircraftForward,
		EnemyManager& enemyManager
	);

	void CheckProximityFuseAndBlast(Missile& missile, EnemyManager& enemyManager);

	void DrawMissileVisual(
		const MyMath::Vector3& worldPos,
		const MyMath::Vector3& right,
		const MyMath::Vector3& up,
		const MyMath::Vector3& forward,
		bool isBurning,
		Camera* camera
	);

	std::vector<Missile> missiles_;
	uint32_t maxActiveMissiles_ = 16;

	MissileSeekerStatus seekerStatus_{};
	float warmupTimer_ = 0.0f;
	float aspectLockSpeedMultiplier_ = 1.0f;

	std::string customModelPath_;
	std::vector<std::unique_ptr<Object3d>> customModelPool_;
	size_t customModelPoolIndex_ = 0;
};
