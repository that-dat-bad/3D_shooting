#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../../engine/base/Math/MyMath.h"
#include "Enemy.h"
#include "GroundEnemy.h"

namespace EnemyManagerDefaults {
	inline constexpr int kZeroCount = 0;
}

class Camera;

// 敵の配置データ（空中目標）
struct EnemySpawnData {
	MyMath::Vector3 position;
	std::string modelPath;
	float health;
	AIType aiType = AIType::ChaseAttack;
	std::string waypointPathName = "";
	std::string behaviorTreeJson = "";
};

// 地上敵の配置データ
struct GroundEnemySpawnData {
	MyMath::Vector3 position{ 0.0f, 0.0f, 0.0f };
	GroundAIType aiType = GroundAIType::Turret;
	float health = 60.0f;
	GroundEnemyParam param{};
	std::string waypointPathName = "";
};

// 敵の生成・管理クラス
class EnemyManager {
public:
	EnemyManager() = default;
	~EnemyManager() = default;

	// 初期化
	void Initialize(
		const std::vector<EnemySpawnData>& spawnList,
		const AirframeData& airframeData,
		const EngineData& engineData,
		const GunPodData& gunpodData,
		FlightModel* playerFlightModel,
		BulletManager* bulletManager,
		const std::vector<GroundEnemySpawnData>& groundSpawnList = {}
	);

	// 更新
	void Update(float deltaTime);

	// 描画
	void Draw(Camera* camera = nullptr);

	// ウェイポイントパス登録
	void SetWaypointPaths(const std::vector<WaypointPath>& paths);

	// 空中敵の追加スポーン
	void SpawnAirEnemies(const std::vector<EnemySpawnData>& spawnList);

	// 地上敵の追加スポーン
	void SpawnGroundEnemies(const std::vector<GroundEnemySpawnData>& groundSpawnList);

	// 撃破判定
	bool IsUnitDestroyedByIndex(int unitIndex) const;

	// === ステータス照会 ===

	int GetAliveCount() const;
	int GetTotalCount() const;
	int GetDestroyedCount() const;
	int GetAliveAirCount() const;
	int GetAliveGroundCount() const;
	bool IsAllDestroyed() const;

	// 生存敵の取得
	std::vector<Enemy*> GetAliveEnemies();
	std::vector<GroundEnemy*> GetAliveGroundEnemies();

	const std::vector<std::unique_ptr<GroundEnemy>>& GetGroundEnemies() const { return groundEnemies_; }
	std::vector<std::unique_ptr<GroundEnemy>>& GetGroundEnemies() { return groundEnemies_; }

private:
	std::vector<std::unique_ptr<Enemy>> enemies_;
	std::vector<std::unique_ptr<GroundEnemy>> groundEnemies_;
	std::vector<WaypointPath> waypointPaths_;
	AirframeData cachedAirframe_{};
	EngineData cachedEngine_{};
	GunPodData cachedGunpod_{};
	FlightModel* playerFlightModel_ = nullptr;
	BulletManager* bulletManager_ = nullptr;
	bool hasCachedInitContext_ = false;

	const WaypointPath* FindWaypointPathByName(const std::string& pathName) const;
};

