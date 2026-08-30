#include "GroundEnemy.h"
#include <cmath>
#include <algorithm>
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "../../engine/Graphics/Camera/Camera.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../FlightModel/FlightModel.h"
#include "../Bullet/Bullet.h"
#include "../../engine/base/logger.h"

namespace {
	constexpr float kPI = 3.14159265358979323846f;
	constexpr float kDeg2Rad = kPI / 180.0f;
	constexpr float kRad2Deg = 180.0f / kPI;

	// 角度を -PI 〜 +PI の範囲に正規化
	float NormalizeAngle(float angle) {
		while (angle > kPI) angle -= 2.0f * kPI;
		while (angle < -kPI) angle += 2.0f * kPI;
		return angle;
	}

	// クランプ関数
	float Clamp(float v, float minV, float maxV) {
		return (std::max)(minV, (std::min)(v, maxV));
	}
}

const char* GroundEnemy::GetAITypeString() const {
	switch (aiType_) {
	case GroundAIType::Turret: return "AAA Turret";
	case GroundAIType::Structure: return "Structure";
	case GroundAIType::PatrolVehicle: return "Patrol Vehicle";
	default: return "Unknown";
	}
}

void GroundEnemy::Initialize(
	const MyMath::Vector3& position,
	GroundAIType aiType,
	const GroundEnemyParam& param,
	FlightModel* playerFlightModel,
	BulletManager* bulletManager
) {
	position_ = position;
	initialPosition_ = position;
	aiType_ = aiType;
	param_ = param;
	health_ = param.maxHealth;
	maxHealth_ = param.maxHealth;
	isAlive_ = true;

	playerFlightModel_ = playerFlightModel;
	bulletManager_ = bulletManager;

	turretYaw_ = 0.0f;
	turretPitch_ = 0.35f; // 初期仰角約20度
	targetYaw_ = 0.0f;
	targetPitch_ = 0.35f;
	isTargetInSight_ = false;

	fireTimer_ = 0.0f;
	currentBurstShot_ = 0;
	cooldownTimer_ = 0.5f; // 最初の射撃まで少し待つ
	barrelToggle_ = 0;
	muzzleFlashTimer_ = 0.0f;

	moveDirection_ = 1.0f;
	currentPatrolOffset_ = 0.0f;
	animTime_ = 0.0f;
	smokeEmitTimer_ = 0.0f;
}

void GroundEnemy::UpdateAI(float deltaTime) {
	if (!playerFlightModel_) return;

	// 1. 車両のパトロール移動 (PatrolVehicle)
	if (aiType_ == GroundAIType::PatrolVehicle) {
		float moveStep = param_.moveSpeed * deltaTime * moveDirection_;
		currentPatrolOffset_ += moveStep;

		if (std::abs(currentPatrolOffset_) >= param_.patrolDistance * 0.5f) {
			moveDirection_ *= -1.0f;
			currentPatrolOffset_ = Clamp(currentPatrolOffset_, -param_.patrolDistance * 0.5f, param_.patrolDistance * 0.5f);
		}
		// X軸方向にパトロール
		position_.x = initialPosition_.x + currentPatrolOffset_;
	}

	// 構造物（Structure）は攻撃しない
	if (aiType_ == GroundAIType::Structure) {
		isTargetInSight_ = false;
		return;
	}

	// 2. プレイヤー機との距離・偏差射撃（リード予測）の計算
	MyMath::Vector3 playerPos = playerFlightModel_->GetPosition();
	MyMath::Vector3 playerVel = playerFlightModel_->GetVelocity();
	MyMath::Vector3 gunBasePos = { position_.x, position_.y + 2.5f, position_.z };

	MyMath::Vector3 toPlayer = MyMath::Subtract(playerPos, gunBasePos);
	float distance = MyMath::Length(toPlayer);

	// 射程範囲内か
	if (distance > param_.fireRange || distance < 10.0f) {
		isTargetInSight_ = false;
		// 待機時のゆっくりとした旋回
		targetYaw_ = turretYaw_ + 0.1f * deltaTime;
		targetPitch_ = 0.35f;
	} else {
		// 偏差射撃計算
		float bulletSpeed = param_.bulletSpeed > 10.0f ? param_.bulletSpeed : 600.0f;
		
		// 1次予測
		float t1 = distance / bulletSpeed;
		MyMath::Vector3 futurePos = MyMath::Add(playerPos, MyMath::Multiply(t1, playerVel));

		// 2次予測（より高精度な予測未来位置）
		float dist2 = MyMath::Length(MyMath::Subtract(futurePos, gunBasePos));
		float t2 = dist2 / bulletSpeed;
		futurePos = MyMath::Add(playerPos, MyMath::Multiply(t2, playerVel));

		MyMath::Vector3 aimVec = MyMath::Subtract(futurePos, gunBasePos);
		float horizDist = std::sqrt(aimVec.x * aimVec.x + aimVec.z * aimVec.z);

		// 目標ヨー角・ピッチ角（ラジアン）
		targetYaw_ = std::atan2(aimVec.x, aimVec.z);
		targetPitch_ = std::atan2(aimVec.y, horizDist);

		// 仰角制限
		float minPitch = param_.minElevationDeg * kDeg2Rad;
		float maxPitch = param_.maxElevationDeg * kDeg2Rad;
		targetPitch_ = Clamp(targetPitch_, minPitch, maxPitch);

		isTargetInSight_ = true;
	}

	// 3. 砲塔の滑らかな旋回補間
	float turnSpeedRad = param_.turnSpeedDeg * kDeg2Rad;
	float maxTurnStep = turnSpeedRad * deltaTime;

	// ヨーの回転（最短回り）
	float yawDiff = NormalizeAngle(targetYaw_ - turretYaw_);
	turretYaw_ += Clamp(yawDiff, -maxTurnStep, maxTurnStep);
	turretYaw_ = NormalizeAngle(turretYaw_);

	// ピッチの回転
	float pitchDiff = targetPitch_ - turretPitch_;
	turretPitch_ += Clamp(pitchDiff, -maxTurnStep, maxTurnStep);

	// 4. 対空射撃制御 (バースト連射 & クールダウン)
	if (cooldownTimer_ > 0.0f) {
		cooldownTimer_ -= deltaTime;
	} else if (isTargetInSight_) {
		// 砲塔が目標方向（未来位置）と概ね一致しているかチェック（約10度以内）
		bool isAimed = (std::abs(yawDiff) < 0.18f) && (std::abs(pitchDiff) < 0.18f);

		if (isAimed) {
			fireTimer_ -= deltaTime;
			if (fireTimer_ <= 0.0f) {
				Fire();
				fireTimer_ = param_.fireInterval;
				currentBurstShot_++;

				if (currentBurstShot_ >= param_.burstCount) {
					currentBurstShot_ = 0;
					cooldownTimer_ = param_.burstCooldown;
				}
			}
		}
	}
}

void GroundEnemy::Fire() {
	if (!bulletManager_) return;

	// 砲身の方向ベクトルを計算
	float cosP = std::cos(turretPitch_);
	float sinP = std::sin(turretPitch_);
	float cosY = std::cos(turretYaw_);
	float sinY = std::sin(turretYaw_);

	MyMath::Vector3 forward = {
		sinY * cosP,
		sinP,
		cosY * cosP
	};

	MyMath::Vector3 right = {
		cosY,
		0.0f,
		-sinY
	};

	// 左右連装砲身のオフセット
	float barrelOffsetAmount = (barrelToggle_ == 0) ? 0.7f : -0.7f;
	barrelToggle_ = 1 - barrelToggle_;

	MyMath::Vector3 gunPivot = { position_.x, position_.y + 2.5f, position_.z };
	MyMath::Vector3 muzzlePos = MyMath::Add(
		MyMath::Add(gunPivot, MyMath::Multiply(barrelOffsetAmount, right)),
		MyMath::Multiply(4.0f, forward)
	);

	// 弾丸初速度
	MyMath::Vector3 muzzleVelocity = MyMath::Multiply(param_.bulletSpeed, forward);

	// 敵対空弾丸を生成
	bulletManager_->SpawnBullet(muzzlePos, muzzleVelocity, param_.bulletDamage, true);

	// マズルフラッシュ
	muzzleFlashPos_ = muzzlePos;
	muzzleFlashDir_ = forward;
	muzzleFlashTimer_ = 0.08f;
	EffectManager::GetInstance()->EmitMuzzleFlash(muzzlePos, forward);
}

void GroundEnemy::Update(float deltaTime) {
	animTime_ += deltaTime;

	if (!isAlive_) {
		// 撃破後の残骸から黒煙を定期的に放出
		smokeEmitTimer_ += deltaTime;
		if (smokeEmitTimer_ >= 0.25f) {
			smokeEmitTimer_ = 0.0f;
			MyMath::Vector3 smokePos = {
				position_.x + (std::sin(animTime_ * 5.0f) * 1.5f),
				position_.y + 2.0f,
				position_.z + (std::cos(animTime_ * 5.0f) * 1.5f)
			};
			ParticleManager::GetInstance()->Emit("ExplosionSmoke", smokePos, 2);
		}
		return;
	}

	UpdateAI(deltaTime);

	if (muzzleFlashTimer_ > 0.0f) {
		muzzleFlashTimer_ -= deltaTime;
	}
}

void GroundEnemy::TakeDamage(float damage) {
	if (!isAlive_) return;

	health_ -= damage;
	if (health_ <= 0.0f) {
		health_ = 0.0f;
		isAlive_ = false;
		// 大爆発エフェクト発生
		MyMath::Vector3 explodePos = { position_.x, position_.y + 2.0f, position_.z };
		EffectManager::GetInstance()->EmitDestroyEffect(explodePos);
		logger::Log("[GroundEnemy] Destroyed at (" + std::to_string(position_.x) + ", " + std::to_string(position_.z) + ")\n");
	}
}

void GroundEnemy::OnCollision(ICollisionBody3D* other) {
	if (!isAlive_) return;

	// プレイヤーの機銃弾との衝突
	if (other->GetCollisionAttribute() & CollisionAttribute::kPlayerBullet) {
		auto* bullet = dynamic_cast<Bullet*>(other);
		float dmg = bullet ? bullet->GetDamage() : 10.0f;
		MyMath::Vector3 bulletPos = bullet ? bullet->GetPosition() : position_;

		EffectManager::GetInstance()->EmitHitEffect(bulletPos);
		TakeDamage(dmg);
	}
	// プレイヤー機との衝突（突入）
	else if (other->GetCollisionAttribute() & CollisionAttribute::kPlayer) {
		TakeDamage(maxHealth_);
	}
}

void GroundEnemy::Draw(Camera* camera) {
	if (!camera) return;

	if (!isAlive_) {
		DrawDestroyed(camera);
		return;
	}

	switch (aiType_) {
	case GroundAIType::Turret:
		DrawTurret(camera);
		break;
	case GroundAIType::Structure:
		DrawStructure(camera);
		break;
	case GroundAIType::PatrolVehicle:
		DrawVehicle(camera);
		break;
	}
}

void GroundEnemy::DrawTurret(Camera* camera) {
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim) return;

	uint32_t whiteTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");

	// 1. 固定台座 (Base) - 六角形/八角形風の強固なスチールベース
	{
		MyMath::Vector3 scale = { 6.0f, 1.2f, 6.0f };
		MyMath::Vector3 rot = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 pos = { position_.x, position_.y + 0.6f, position_.z };
		MyMath::Vector4 color = { 0.18f, 0.20f, 0.22f, 1.0f }; // ダークスチール
		prim->DrawCylinder(scale, rot, pos, color, whiteTex, camera, BlendMode::kNormal);

		// 装甲リムリング
		MyMath::Vector3 rimScale = { 6.8f, 0.3f, 6.8f };
		MyMath::Vector3 rimPos = { position_.x, position_.y + 0.15f, position_.z };
		MyMath::Vector4 rimColor = { 0.12f, 0.13f, 0.15f, 1.0f };
		prim->DrawCylinder(rimScale, rot, rimPos, rimColor, whiteTex, camera, BlendMode::kNormal);
	}

	// 2. 旋回砲台リング & マウント (Turret Ring)
	{
		MyMath::Vector3 mountScale = { 4.0f, 0.8f, 4.0f };
		MyMath::Vector3 mountRot = { 0.0f, turretYaw_, 0.0f };
		MyMath::Vector3 mountPos = { position_.x, position_.y + 1.6f, position_.z };
		MyMath::Vector4 mountColor = { 0.28f, 0.31f, 0.35f, 1.0f }; // ミリタリーグレー
		prim->DrawCylinder(mountScale, mountRot, mountPos, mountColor, whiteTex, camera, BlendMode::kNormal);
	}

	// 3. 砲塔メインキャビン (Turret Body)
	{
		MyMath::Vector3 bodyScale = { 3.0f, 1.8f, 3.4f };
		MyMath::Vector3 bodyRot = { 0.0f, turretYaw_, 0.0f };
		MyMath::Vector3 bodyPos = { position_.x, position_.y + 2.5f, position_.z };
		MyMath::Vector4 bodyColor = { 0.22f, 0.25f, 0.28f, 1.0f };
		prim->DrawCylinder(bodyScale, bodyRot, bodyPos, bodyColor, whiteTex, camera, BlendMode::kNormal);

		// レーダー/光学照準ドーム
		MyMath::Vector3 domeScale = { 1.2f, 1.0f, 1.2f };
		MyMath::Vector3 domePos = {
			position_.x - std::sin(turretYaw_) * 0.8f,
			position_.y + 3.8f,
			position_.z - std::cos(turretYaw_) * 0.8f
		};
		MyMath::Vector4 domeColor = { 0.15f, 0.45f, 0.35f, 1.0f }; // レーダードーム（ダークグリーン）
		prim->DrawCylinder(domeScale, bodyRot, domePos, domeColor, whiteTex, camera, BlendMode::kNormal);
	}

	// 4. 連装砲身 (Twin Barrels) - ヨーとピッチに連動
	{
		float cosP = std::cos(turretPitch_);
		float sinP = std::sin(turretPitch_);
		float cosY = std::cos(turretYaw_);
		float sinY = std::sin(turretYaw_);

		MyMath::Vector3 forward = { sinY * cosP, sinP, cosY * cosP };
		MyMath::Vector3 right = { cosY, 0.0f, -sinY };

		MyMath::Vector3 pivot = { position_.x, position_.y + 2.5f, position_.z };

		// 左右2本の砲身
		float offsets[2] = { -0.7f, 0.7f };
		for (int i = 0; i < 2; ++i) {
			MyMath::Vector3 barrelBase = MyMath::Add(pivot, MyMath::Multiply(offsets[i], right));
			MyMath::Vector3 barrelCenter = MyMath::Add(barrelBase, MyMath::Multiply(2.0f, forward));

			// 砲身（細長いシリンダー: Y軸長さをZ方向へ回転）
			MyMath::Vector3 bScale = { 0.28f, 4.0f, 0.28f };
			// ピッチ（X回転）とヨー（Y回転）の合成回転
			MyMath::Vector3 bRot = {
				-(kPI * 0.5f - turretPitch_),
				turretYaw_,
				0.0f
			};
			MyMath::Vector4 barrelColor = { 0.12f, 0.12f, 0.14f, 1.0f }; // ガンメタルブラック
			prim->DrawCylinder(bScale, bRot, barrelCenter, barrelColor, whiteTex, camera, BlendMode::kNormal);

			// マズルブレーキ（砲口）
			MyMath::Vector3 muzzlePos = MyMath::Add(barrelBase, MyMath::Multiply(4.0f, forward));
			MyMath::Vector3 mScale = { 0.45f, 0.8f, 0.45f };
			MyMath::Vector4 muzzleColor = { 0.25f, 0.25f, 0.28f, 1.0f };
			prim->DrawCylinder(mScale, bRot, muzzlePos, muzzleColor, whiteTex, camera, BlendMode::kNormal);
		}
	}
}

void GroundEnemy::DrawStructure(Camera* camera) {
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim) return;

	uint32_t whiteTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");

	// 1. 大型建築物ベース（司令所・通信センター）
	{
		MyMath::Vector3 baseScale = { 16.0f, 5.0f, 16.0f };
		MyMath::Vector3 baseRot = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 basePos = { position_.x, position_.y + 2.5f, position_.z };
		MyMath::Vector4 baseColor = { 0.25f, 0.28f, 0.32f, 1.0f };
		prim->DrawCylinder(baseScale, baseRot, basePos, baseColor, whiteTex, camera, BlendMode::kNormal);

		// 上部第2階層
		MyMath::Vector3 topScale = { 10.0f, 3.5f, 10.0f };
		MyMath::Vector3 topPos = { position_.x, position_.y + 6.0f, position_.z };
		MyMath::Vector4 topColor = { 0.20f, 0.22f, 0.25f, 1.0f };
		prim->DrawCylinder(topScale, baseRot, topPos, topColor, whiteTex, camera, BlendMode::kNormal);
	}

	// 2. 回転する巨大レーダーアンテナ
	{
		float radarRotY = animTime_ * 1.5f;
		MyMath::Vector3 dishScale = { 6.0f, 1.2f, 2.0f };
		MyMath::Vector3 dishRot = { 0.3f, radarRotY, 0.0f };
		MyMath::Vector3 dishPos = { position_.x, position_.y + 10.0f, position_.z };
		MyMath::Vector4 dishColor = { 0.85f, 0.85f, 0.90f, 1.0f }; // ホワイトレーダー
		prim->DrawCylinder(dishScale, dishRot, dishPos, dishColor, whiteTex, camera, BlendMode::kNormal);

		// アンテナ支柱
		MyMath::Vector3 mastScale = { 0.8f, 3.0f, 0.8f };
		MyMath::Vector3 mastRot = { 0.0f, 0.0f, 0.0f };
		MyMath::Vector3 mastPos = { position_.x, position_.y + 8.5f, position_.z };
		MyMath::Vector4 mastColor = { 0.15f, 0.15f, 0.15f, 1.0f };
		prim->DrawCylinder(mastScale, mastRot, mastPos, mastColor, whiteTex, camera, BlendMode::kNormal);
	}

	// 3. 隣接燃料サイロ（2基）
	{
		MyMath::Vector3 silo1Scale = { 5.0f, 9.0f, 5.0f };
		MyMath::Vector3 silo1Pos = { position_.x + 12.0f, position_.y + 4.5f, position_.z - 4.0f };
		MyMath::Vector4 siloColor = { 0.40f, 0.42f, 0.45f, 1.0f };
		prim->DrawCylinder(silo1Scale, { 0,0,0 }, silo1Pos, siloColor, whiteTex, camera, BlendMode::kNormal);

		MyMath::Vector3 silo2Pos = { position_.x + 12.0f, position_.y + 4.5f, position_.z + 4.0f };
		prim->DrawCylinder(silo1Scale, { 0,0,0 }, silo2Pos, siloColor, whiteTex, camera, BlendMode::kNormal);
	}
}

void GroundEnemy::DrawVehicle(Camera* camera) {
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim) return;

	uint32_t whiteTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");

	// 進行方向に応じた車体向き
	float bodyYaw = (moveDirection_ > 0.0f) ? (kPI * 0.5f) : (-kPI * 0.5f);

	// 1. 車体（装甲車シャーシ）
	{
		MyMath::Vector3 hullScale = { 4.5f, 1.6f, 8.0f };
		MyMath::Vector3 hullRot = { 0.0f, bodyYaw, 0.0f };
		MyMath::Vector3 hullPos = { position_.x, position_.y + 1.2f, position_.z };
		MyMath::Vector4 hullColor = { 0.22f, 0.28f, 0.18f, 1.0f }; // カモフラージュオリーブ
		prim->DrawCylinder(hullScale, hullRot, hullPos, hullColor, whiteTex, camera, BlendMode::kNormal);
	}

	// 2. 装輪 / クローラー足回り
	{
		MyMath::Vector3 trackScale = { 1.0f, 1.2f, 8.2f };
		MyMath::Vector3 trackRot = { 0.0f, bodyYaw, 0.0f };
		MyMath::Vector4 trackColor = { 0.10f, 0.10f, 0.10f, 1.0f };

		// 左右トラック
		prim->DrawCylinder(trackScale, trackRot, { position_.x, position_.y + 0.6f, position_.z + 2.0f }, trackColor, whiteTex, camera, BlendMode::kNormal);
		prim->DrawCylinder(trackScale, trackRot, { position_.x, position_.y + 0.6f, position_.z - 2.0f }, trackColor, whiteTex, camera, BlendMode::kNormal);
	}

	// 3. 車載対空砲塔（Turret）
	{
		MyMath::Vector3 mountScale = { 2.6f, 1.2f, 2.6f };
		MyMath::Vector3 mountRot = { 0.0f, turretYaw_, 0.0f };
		MyMath::Vector3 mountPos = { position_.x, position_.y + 2.4f, position_.z };
		MyMath::Vector4 mountColor = { 0.18f, 0.22f, 0.16f, 1.0f };
		prim->DrawCylinder(mountScale, mountRot, mountPos, mountColor, whiteTex, camera, BlendMode::kNormal);

		// 連装砲身
		float cosP = std::cos(turretPitch_);
		float sinP = std::sin(turretPitch_);
		float cosY = std::cos(turretYaw_);
		float sinY = std::sin(turretYaw_);

		MyMath::Vector3 forward = { sinY * cosP, sinP, cosY * cosP };
		MyMath::Vector3 right = { cosY, 0.0f, -sinY };
		MyMath::Vector3 pivot = { position_.x, position_.y + 2.5f, position_.z };

		float offsets[2] = { -0.5f, 0.5f };
		for (int i = 0; i < 2; ++i) {
			MyMath::Vector3 barrelBase = MyMath::Add(pivot, MyMath::Multiply(offsets[i], right));
			MyMath::Vector3 barrelCenter = MyMath::Add(barrelBase, MyMath::Multiply(1.6f, forward));

			MyMath::Vector3 bScale = { 0.22f, 3.2f, 0.22f };
			MyMath::Vector3 bRot = { -(kPI * 0.5f - turretPitch_), turretYaw_, 0.0f };
			MyMath::Vector4 barrelColor = { 0.10f, 0.10f, 0.12f, 1.0f };
			prim->DrawCylinder(bScale, bRot, barrelCenter, barrelColor, whiteTex, camera, BlendMode::kNormal);
		}
	}
}

void GroundEnemy::DrawDestroyed(Camera* camera) {
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim) return;

	uint32_t whiteTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("assets/textures/white1x1.png");

	// 破壊された黒焦げの残骸
	MyMath::Vector3 wreckScale = { 5.5f, 1.2f, 5.5f };
	MyMath::Vector3 wreckRot = { 0.1f, 0.2f, 0.05f };
	MyMath::Vector3 wreckPos = { position_.x, position_.y + 0.6f, position_.z };
	MyMath::Vector4 wreckColor = { 0.06f, 0.06f, 0.07f, 1.0f }; // 完全な黒焦げ
	prim->DrawCylinder(wreckScale, wreckRot, wreckPos, wreckColor, whiteTex, camera, BlendMode::kNormal);

	// 傾いた砲身の破片
	MyMath::Vector3 barrelScale = { 0.35f, 2.5f, 0.35f };
	MyMath::Vector3 barrelRot = { 0.8f, 0.5f, 0.3f };
	MyMath::Vector3 barrelPos = { position_.x + 1.2f, position_.y + 0.8f, position_.z + 1.0f };
	prim->DrawCylinder(barrelScale, barrelRot, barrelPos, wreckColor, whiteTex, camera, BlendMode::kNormal);
}
