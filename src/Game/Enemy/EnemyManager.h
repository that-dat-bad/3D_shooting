#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../../engine/base/Math/MyMath.h"
#include "Enemy.h"
#include "GroundEnemy.h"

class Camera;

/// @brief 敵の配置データ（空中目標）
struct EnemySpawnData {
	MyMath::Vector3 position;
	std::string modelPath;
	float health;
	AIType aiType = AIType::ChaseAttack;
	std::string waypointPathName = ""; // Phase 2: ウェイポイントパス名
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
};

