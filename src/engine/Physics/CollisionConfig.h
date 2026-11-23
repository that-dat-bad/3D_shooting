#pragma once
#include <cstdint>

// 衝突判定用の属性ビット定数
namespace CollisionAttribute {
	constexpr uint32_t kNone         = 0;
	constexpr uint32_t kPlayer       = 1 << 0;
	constexpr uint32_t kEnemy        = 1 << 1;
	constexpr uint32_t kPlayerBullet = 1 << 2;
	constexpr uint32_t kEnemyBullet  = 1 << 3;
	constexpr uint32_t kGround       = 1 << 4;
	constexpr uint32_t kUI           = 1 << 5;
	constexpr uint32_t kMap          = 1 << 6;
	constexpr uint32_t kPlayerMissile = 1 << 7;
	// 必要に応じて追加
}

// 衝突対象のマスク定数
namespace CollisionMask {
	constexpr uint32_t kPlayer       = CollisionAttribute::kEnemyBullet | CollisionAttribute::kEnemy;
	constexpr uint32_t kEnemy        = CollisionAttribute::kPlayerBullet | CollisionAttribute::kPlayerMissile | CollisionAttribute::kPlayer;
	constexpr uint32_t kPlayerBullet = CollisionAttribute::kEnemy;
	constexpr uint32_t kPlayerMissile = CollisionAttribute::kEnemy;
	constexpr uint32_t kEnemyBullet  = CollisionAttribute::kPlayer;
	constexpr uint32_t kNone         = 0;
	constexpr uint32_t kAll          = 0xFFFFFFFF;
}
