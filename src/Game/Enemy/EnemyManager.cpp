#include "EnemyManager.h"
#include "../../engine/Graphics/Camera/CameraManager.h"

void EnemyManager::SetWaypointPaths(const std::vector<WaypointPath>& paths) {
	waypointPaths_ = paths;
}

const WaypointPath* EnemyManager::FindWaypointPathByName(const std::string& pathName) const {
	if (pathName.empty()) {
		return nullptr;
	}
	for (const auto& path : waypointPaths_) {
		if (path.name == pathName) {
			return &path;
		}
	}
	return nullptr;
}

void EnemyManager::Initialize(
	const std::vector<EnemySpawnData>& spawnList,
	const AirframeData& airframeData,
	const EngineData& engineData,
	const GunPodData& gunpodData,
	FlightModel* playerFlightModel,
	BulletManager* bulletManager,
	const std::vector<GroundEnemySpawnData>& groundSpawnList
) {
	cachedAirframe_ = airframeData;
	cachedEngine_ = engineData;
	cachedGunpod_ = gunpodData;
	playerFlightModel_ = playerFlightModel;
	bulletManager_ = bulletManager;
	hasCachedInitContext_ = true;

	enemies_.clear();
	enemies_.reserve(spawnList.size());
	SpawnAirEnemies(spawnList);

	groundEnemies_.clear();
	groundEnemies_.reserve(groundSpawnList.size());
	SpawnGroundEnemies(groundSpawnList);
}

void EnemyManager::SpawnAirEnemies(const std::vector<EnemySpawnData>& spawnList) {
	if (!hasCachedInitContext_ || spawnList.empty()) {
		return;
	}
	for (const auto& data : spawnList) {
		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize(
			data.position, data.modelPath, data.health,
			cachedAirframe_, cachedEngine_, cachedGunpod_,
			playerFlightModel_, bulletManager_,
			data.aiType
		);
		if (const WaypointPath* path = FindWaypointPathByName(data.waypointPathName)) {
			enemy->SetWaypointPath(*path);
		}
		enemies_.push_back(std::move(enemy));
	}
}

void EnemyManager::SpawnGroundEnemies(const std::vector<GroundEnemySpawnData>& groundSpawnList) {
	if (!hasCachedInitContext_ || groundSpawnList.empty()) {
		return;
	}
	for (const auto& data : groundSpawnList) {
		auto groundEnemy = std::make_unique<GroundEnemy>();
		groundEnemy->Initialize(
			data.position,
			data.aiType,
			data.param,
			playerFlightModel_,
			bulletManager_
		);
		if (const WaypointPath* path = FindWaypointPathByName(data.waypointPathName)) {
			groundEnemy->SetWaypointPath(*path);
		}
		groundEnemies_.push_back(std::move(groundEnemy));
	}
}

bool EnemyManager::IsUnitDestroyedByIndex(int unitIndex) const {
	if (unitIndex < EnemyManagerDefaults::kZeroCount) {
		return false;
	}
	const int airCount = static_cast<int>(enemies_.size());
	if (unitIndex < airCount) {
		return !enemies_[static_cast<size_t>(unitIndex)]->IsAlive();
	}
	const int groundIdx = unitIndex - airCount;
	const int groundCount = static_cast<int>(groundEnemies_.size());
	if (groundIdx >= EnemyManagerDefaults::kZeroCount && groundIdx < groundCount) {
		return !groundEnemies_[static_cast<size_t>(groundIdx)]->IsAlive();
	}
	return false;
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
	int count = EnemyManagerDefaults::kZeroCount;
	for (const auto& enemy : enemies_) {
		if (enemy->IsAlive()) {
			++count;
		}
	}
	return count;
}

int EnemyManager::GetAliveGroundCount() const {
	int count = EnemyManagerDefaults::kZeroCount;
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
	return GetAliveCount() == EnemyManagerDefaults::kZeroCount &&
	       GetTotalCount() > EnemyManagerDefaults::kZeroCount;
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
