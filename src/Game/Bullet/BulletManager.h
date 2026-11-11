#pragma once
#include <vector>
#include <cstdint>
#include "Bullet.h"

class Camera;

// 弾丸マネージャー
class BulletManager {
public:
	BulletManager() = default;
	~BulletManager() = default;

	// 初期化
	// maxBullets : 最大弾数
	void Initialize(uint32_t maxBullets = 512);

	// 更新
	void Update(float dt);

	// 描画
	void Draw(Camera* camera);

	// 弾丸生成
	// position : 発射位置
	// velocity : 初速度ベクトル
	// damage : ダメージ
	// isEnemyBullet : 敵弾フラグ
	void SpawnBullet(const MyMath::Vector3& position, const MyMath::Vector3& velocity, float damage, bool isEnemyBullet = false);

	// 弾丸リスト取得
	std::vector<Bullet>& GetBullets() { return bullets_; }

	// 有効弾数取得
	uint32_t GetActiveBulletCount() const;

private:
	void DrawTracerSegment(
		const MyMath::Vector3& p1,
		const MyMath::Vector3& p2,
		int segmentIndex,
		Camera* camera,
		uint32_t texIndex
	);

	std::vector<Bullet> bullets_;
	uint32_t maxBullets_ = 512;
};
