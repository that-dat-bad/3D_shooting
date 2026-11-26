#include "EnemyManager.h"
#include <algorithm>
#include "../Environment/TerrainChunkManager.h"
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
	watchtowersGrounded_ = false;

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
		enemy->SetTerrainChunkManager(terrain_);
		enemy->Initialize(
			data.position, data.modelPath, data.health,
			cachedAirframe_, cachedEngine_, cachedGunpod_,
			playerFlightModel_, bulletManager_,
			data.aiType,
			data.behaviorTreeJson,
			data.colliderModelPath
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
	size_t watchtowerCounter = 0;
	for (const auto& g : groundEnemies_) {
		if (g && g->GetAIType() == GroundAIType::Watchtower) {
			++watchtowerCounter;
		}
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
		if (data.aiType == GroundAIType::Watchtower) {
			groundEnemy->SetWatchtowerYaw(static_cast<float>(watchtowerCounter) * 0.785398f);
			++watchtowerCounter;
		}
		if (const WaypointPath* path = FindWaypointPathByName(data.waypointPathName)) {
			groundEnemy->SetWaypointPath(*path);
		}
		groundEnemies_.push_back(std::move(groundEnemy));
	}
	GroundWatchtowersToTerrain();
}

void EnemyManager::GroundWatchtowersToTerrain() {
	if (!terrain_ || !terrain_->IsLoaded()) {
		return;
	}
	int lastWatchtowerIdx = -1;
	for (int i = 0; i < static_cast<int>(groundEnemies_.size()); ++i) {
		if (groundEnemies_[static_cast<size_t>(i)] &&
		    groundEnemies_[static_cast<size_t>(i)]->GetAIType() == GroundAIType::Watchtower) {
			lastWatchtowerIdx = i;
		}
	}
	for (int i = 0; i < static_cast<int>(groundEnemies_.size()); ++i) {
		auto& g = groundEnemies_[static_cast<size_t>(i)];
		if (!g || g->GetAIType() != GroundAIType::Watchtower) {
			continue;
		}
		MyMath::Vector3 pos = g->GetPosition();
		const float groundY = terrain_->GetTerrainHeightAt(pos.x, pos.z, -485.0f);
		pos.y = (i == lastWatchtowerIdx) ? (std::max)(groundY, -472.0f) : groundY;
		g->SetPosition(pos);
	}
	watchtowersGrounded_ = true;
}

void EnemyManager::IgniteLastWatchtower() {
	GroundWatchtowersToTerrain();
	for (auto it = groundEnemies_.rbegin(); it != groundEnemies_.rend(); ++it) {
		if (*it && (*it)->GetAIType() == GroundAIType::Watchtower) {
			(*it)->Ignite();
			return;
		}
	}
}

bool EnemyManager::GetLastHitMarkerTriggered() {
	bool anyHit = false;
	for (auto& enemy : enemies_) {
		if (enemy && enemy->ConsumeHitMarkerTriggered()) {
			anyHit = true;
		}
	}
	return anyHit;
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
	if (!watchtowersGrounded_ && terrain_ && terrain_->IsLoaded()) {
		GroundWatchtowersToTerrain();
	}

	Enemy* activeWestern = nullptr;
	for (auto& enemy : enemies_) {
		if (enemy && enemy->GetAIType() == AIType::CanyonWesternFighter && enemy->IsAlive()) {
			activeWestern = enemy.get();
			break;
		}
	}

	for (auto& enemy : enemies_) {
		if (!enemy) { continue; }
		enemy->SetTerrainChunkManager(terrain_);
		if (enemy->GetAIType() == AIType::AllyWingman) {
			enemy->SetTargetEnemy(activeWestern);
		}
		if (enemy->IsRenderActive()) {
			enemy->Update(deltaTime);
		}
	}
	for (auto& groundEnemy : groundEnemies_) {
		if (groundEnemy) {
			groundEnemy->Update(deltaTime);
		}
	}
}

void EnemyManager::Draw(Camera* camera) {
	// 空中敵・僚機・墜落中機の描画
	for (auto& enemy : enemies_) {
		if (enemy && enemy->IsRenderActive()) {
			enemy->Draw();
		}
	}

	// 地上目標・監視塔の描画
	Camera* targetCam = camera ? camera : CameraManager::GetInstance()->GetActiveCamera();
	for (auto& groundEnemy : groundEnemies_) {
		if (groundEnemy) {
			groundEnemy->Draw(targetCam);
		}
	}
}

int EnemyManager::GetAliveAirCount() const {
	int count = EnemyManagerDefaults::kZeroCount;
	for (const auto& enemy : enemies_) {
		if (enemy && !enemy->IsAlly() && enemy->IsAlive()) {
			++count;
		}
	}
	return count;
}

int EnemyManager::GetAliveGroundCount() const {
	int count = EnemyManagerDefaults::kZeroCount;
	for (const auto& groundEnemy : groundEnemies_) {
		if (groundEnemy && groundEnemy->IsHostile() && groundEnemy->IsAlive()) {
			++count;
		}
	}
	return count;
}

int EnemyManager::GetAliveCount() const {
	return GetAliveAirCount() + GetAliveGroundCount();
}

int EnemyManager::GetTotalCount() const {
	int total = EnemyManagerDefaults::kZeroCount;
	for (const auto& enemy : enemies_) {
		if (enemy && !enemy->IsAlly()) {
			++total;
		}
	}
	for (const auto& groundEnemy : groundEnemies_) {
		if (groundEnemy && groundEnemy->IsHostile()) {
			++total;
		}
	}
	return total;
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
