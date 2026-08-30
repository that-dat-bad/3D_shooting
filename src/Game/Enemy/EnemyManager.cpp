#include "EnemyManager.h"
#include "../../engine/Graphics/Camera/CameraManager.h"

void EnemyManager::Initialize(
	const std::vector<EnemySpawnData>& spawnList,
	const AirframeData& airframeData,
	const EngineData& engineData,
	const GunPodData& gunpodData,
	FlightModel* playerFlightModel,
	BulletManager* bulletManager,
	const std::vector<GroundEnemySpawnData>& groundSpawnList
) {
	enemies_.clear();
	enemies_.reserve(spawnList.size());

	for (const auto& data : spawnList) {
		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize(
			data.position, data.modelPath, data.health,
			airframeData, engineData, gunpodData,
			playerFlightModel, bulletManager,
			data.aiType
		);
		enemies_.push_back(std::move(enemy));
	}

	groundEnemies_.clear();
	groundEnemies_.reserve(groundSpawnList.size());

	for (const auto& data : groundSpawnList) {
		auto groundEnemy = std::make_unique<GroundEnemy>();
		groundEnemy->Initialize(
			data.position,
			data.aiType,
			data.param,
			playerFlightModel,
			bulletManager
		);
		groundEnemies_.push_back(std::move(groundEnemy));
	}
}

void EnemyManager::Update(float deltaTime) {
	for (auto& enemy : enemies_) {
		enemy->Update(deltaTime);
	}
	for (auto& groundEnemy : groundEnemies_) {
		groundEnemy->Update(deltaTime);
	}
}

void EnemyManager::Draw(Camera* camera) {
	// 空中敵の描画
	for (auto& enemy : enemies_) {
		enemy->Draw();
	}

	// 地上敵の描画
	Camera* targetCam = camera ? camera : CameraManager::GetInstance()->GetActiveCamera();
	for (auto& groundEnemy : groundEnemies_) {
		groundEnemy->Draw(targetCam);
	}
}

int EnemyManager::GetAliveAirCount() const {
	int count = 0;
	for (const auto& enemy : enemies_) {
		if (enemy->IsAlive()) {
			++count;
		}
	}
	return count;
}

int EnemyManager::GetAliveGroundCount() const {
	int count = 0;
	for (const auto& groundEnemy : groundEnemies_) {
		if (groundEnemy->IsAlive()) {
			++count;
		}
	}
	return count;
}

int EnemyManager::GetAliveCount() const {
	return GetAliveAirCount() + GetAliveGroundCount();
}

int EnemyManager::GetTotalCount() const {
	return static_cast<int>(enemies_.size() + groundEnemies_.size());
}

int EnemyManager::GetDestroyedCount() const {
	return GetTotalCount() - GetAliveCount();
}

bool EnemyManager::IsAllDestroyed() const {
	return GetAliveCount() == 0 && GetTotalCount() > 0;
}

std::vector<Enemy*> EnemyManager::GetAliveEnemies() {
	std::vector<Enemy*> alive;
	for (auto& enemy : enemies_) {
		if (enemy->IsAlive()) {
			alive.push_back(enemy.get());
		}
	}
	return alive;
}

std::vector<GroundEnemy*> EnemyManager::GetAliveGroundEnemies() {
	std::vector<GroundEnemy*> alive;
	for (auto& groundEnemy : groundEnemies_) {
		if (groundEnemy->IsAlive()) {
			alive.push_back(groundEnemy.get());
		}
	}
	return alive;
}

