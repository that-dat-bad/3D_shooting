#include "BulletManager.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/Camera/Camera.h"
#include "../../engine/Graphics/System/BlendMode.h"

using namespace MyMath;

void BulletManager::Initialize(uint32_t maxBullets) {
	maxBullets_ = maxBullets;
	bullets_.resize(maxBullets_);
	// 全弾丸を非活性状態で初期化（Bulletのデフォルトコンストラクタで isAlive_ = false）
}

void BulletManager::Update(float dt) {
	for (auto& bullet : bullets_) {
		bullet.Update(dt);
	}
}

void BulletManager::Draw(Camera* camera) {
	if (!camera) { return; }

	// 丸いパーティクル用テクスチャ
	uint32_t texIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/circle2.png");

	for (const auto& bullet : bullets_) {
		if (!bullet.IsAlive()) { continue; }

		const Vector3* history = bullet.GetHistory();
		int count = bullet.GetHistoryCount();
		Vector3 currentPos = bullet.GetPosition();

		// 現在の位置から history[0] を繋ぐセグメントを描画
		DrawTracerSegment(currentPos, history[0], 0, camera, texIndex);

		// 履歴を辿って、連続する2点間を繋ぐセグメントを描画
		for (int i = 0; i < count - 1; ++i) {
			DrawTracerSegment(history[i], history[i + 1], i + 1, camera, texIndex);
		}
	}
}

void BulletManager::DrawTracerSegment(const Vector3& p1, const Vector3& p2, int segmentIndex, Camera* camera, uint32_t texIndex) {
	Vector3 toP1 = Subtract(p1, p2);
	float length = Length(toP1);
	if (length < 0.01f) { return; }

	Vector3 pos = Multiply(0.5f, Add(p1, p2));
	Vector3 dir = Normalize(toP1);

	Vector3 rotate = { 0.0f, 0.0f, 0.0f };
	rotate.y = std::atan2(dir.x, dir.z);
	rotate.x = std::asin(-dir.y);

	// セグメントインデックスに応じた減衰（先端ほど太く明るく、後ろに行くほど細くフェード）
	float widthFactor = 1.0f;
	float alphaFactor = 1.0f;
	if (segmentIndex == 0) {
		widthFactor = 1.0f;
		alphaFactor = 1.0f;
	} else if (segmentIndex == 1) {
		widthFactor = 0.7f;
		alphaFactor = 0.6f;
	} else if (segmentIndex == 2) {
		widthFactor = 0.4f;
		alphaFactor = 0.3f;
	} else {
		widthFactor = 0.2f;
		alphaFactor = 0.1f;
	}

	float thickness = 0.15f * widthFactor;
	// シリンダーの本来の長さは 2.0（-1.0〜1.0）なので、スケール長は length * 0.5f
	Vector3 scale = { thickness, thickness, length * 0.5f };
	Vector4 color = { 1.0f, 0.3f, 0.05f, 1.0f * alphaFactor }; // 黄オレンジ系の発光色

	PrimitiveModel::GetInstance()->DrawCylinder(
		scale, rotate, pos, color,
		texIndex, camera, BlendMode::kAdd
	);
}

void BulletManager::SpawnBullet(const Vector3& position, const Vector3& velocity, float damage, bool isEnemyBullet) {
	for (auto& bullet : bullets_) {
		if (!bullet.IsAlive()) {
			bullet.Spawn(position, velocity, damage, isEnemyBullet);
			break;
		}
	}
}

uint32_t BulletManager::GetActiveBulletCount() const {
	uint32_t count = 0;
	for (const auto& bullet : bullets_) {
		if (bullet.IsAlive()) {
			++count;
		}
	}
	return count;
}
