#include "CollisionSystem.h"

void CollisionSystem::UpdateAll() {
	// 3D判定を先に実行
	manager3d_.CheckAllCollisions();
	// 2D判定を実行
	manager2d_.CheckAllCollisions();
}

void CollisionSystem::ClearAll() {
	manager3d_.Clear();
	manager2d_.Clear();
}
