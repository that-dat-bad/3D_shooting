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

/// @brief 敵の配置データ（空中目標）
struct EnemySpawnData {
	MyMath::Vector3 position;
	std::string modelPath;
	float health;
	AIType aiType = AIType::ChaseAttack;
	std::string waypointPathName = ""; // Phase 2: ウェイポイントパス名
	std::string behaviorTreeJson = ""; // ビヘイビアツリー定義 (Blender Live Sync / JSON)
};

/// @brief 地上敵の配置データ
struct GroundEnemySpawnData {
	MyMath::Vector3 position{ 0.0f, 0.0f, 0.0f };
	GroundAIType aiType = GroundAIType::Turret;
	float health = 60.0f;
	GroundEnemyParam param{};
	std::string waypointPathName = ""; // Phase 2: ウェイポイントパス名
};

/// @brief 敵の生成・管理クラス
class EnemyManager {
public:
	EnemyManager() = default;
	~EnemyManager() = default;

	/// @brief 配置データから敵（空中＋地上）を一括生成
	void Initialize(
		const std::vector<EnemySpawnData>& spawnList,
		const AirframeData& airframeData,
		const EngineData& engineData,
		const GunPodData& gunpodData,
		FlightModel* playerFlightModel,
		BulletManager* bulletManager,
		const std::vector<GroundEnemySpawnData>& groundSpawnList = {}
	);

	/// @brief 全敵の更新
	void Update(float deltaTime);

	/// @brief 全敵の描画
	void Draw(Camera* camera = nullptr);

	/// @brief ミッション内のウェイポイントパス一覧を登録（FollowWaypoint / PatrolVehicle 解決用）
	void SetWaypointPaths(const std::vector<WaypointPath>& paths);

	/// @brief トリガー等から実行中に追加の空中敵をスポーンする
	void SpawnAirEnemies(const std::vector<EnemySpawnData>& spawnList);

	/// @brief トリガー等から実行中に追加の地上敵をスポーンする
	void SpawnGroundEnemies(const std::vector<GroundEnemySpawnData>& groundSpawnList);

	/// @brief 通しインデックス（0..air-1: 空中敵, air..total-1: 地上敵）のユニットが撃破済みか判定
	bool IsUnitDestroyedByIndex(int unitIndex) const;

	// === ステータス照会 ===

	/// @brief 生存している敵（空中＋地上）の総数
	int GetAliveCount() const;

	/// @brief 総敵数（空中＋地上）
	int GetTotalCount() const;

	/// @brief 撃破した敵数（空中＋地上）
	int GetDestroyedCount() const;

	/// @brief 空中敵の生存数
	int GetAliveAirCount() const;
	/// @brief 地上敵の生存数
	int GetAliveGroundCount() const;

	/// @brief 全敵が撃破されたか
	bool IsAllDestroyed() const;

	/// @brief 生存中の空中敵へのポインタリストを取得（衝突判定用）
	std::vector<Enemy*> GetAliveEnemies();

	/// @brief 生存中の地上敵へのポインタリストを取得（衝突判定用）
	std::vector<GroundEnemy*> GetAliveGroundEnemies();

	/// @brief 全地上敵への参照を取得（エディタ用）
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

