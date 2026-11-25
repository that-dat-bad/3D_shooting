#include "ParticleDemoController.h"
#include "../Enemy/EnemyManager.h"
#include "../Enemy/Enemy.h"
#include "../Enemy/GroundEnemy.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace MyMath;

namespace {
	constexpr float kSafeMinAltitude = 145.0f;
	constexpr float kSafeMaxAltitude = 620.0f;
	constexpr float kDemoFinaleTime = 24.5f;
	constexpr float kGunAlignmentDot = 0.962f;
	constexpr float kGunMaxRange = 980.0f;
}

void ParticleDemoController::Reset(bool enabled) {
	enabled_ = enabled;
	elapsed_ = 0.0f;
	missileShotCooldown_ = 0.0f;
	flareStageTriggered_ = 0;
	finaleDeathRequested_ = false;
	finaleTriggered_ = false;
	hasActiveTarget_ = false;
	activeTargetPos_ = {};
	prevDestroyedCount_ = 0;
	wasEnemyAlive_.clear();
	explosionHoldTimer_ = 0.0f;
	explosionHoldPos_ = {};
	flareFocusTimer_ = 0.0f;
	hasFocusedFlare_ = false;
	output_ = {};
}

bool ParticleDemoController::ConsumeFinaleDeathRequested() {
	if (!finaleDeathRequested_) {
		return false;
	}
	finaleDeathRequested_ = false;
	return true;
}

ParticleDemoController::TargetCandidate ParticleDemoController::FindBestTarget(
	const Vector3& pos,
	const Vector3& forward,
	const EnemyManager* enemyManager,
	bool preferAir)
{
	TargetCandidate best{};
	if (!enemyManager) {
		return best;
	}

	float bestScore = (std::numeric_limits<float>::max)();
	auto evaluatePoint = [&](const Vector3& targetPos) {
		const Vector3 toTarget = Subtract(targetPos, pos);
		const float dist = Length(toTarget);
		if (dist < 1.0f) {
			return;
		}
		const Vector3 dir = Multiply(1.0f / dist, toTarget);
		const float dot = Dot(forward, dir);
		// 前方（正対）かつ射程距離が十分にあるターゲットを最優先し、通り過ぎ直後の反転旋回を防ぐ
		const float behindPenalty = (dot < 0.25f) ? 3500.0f : 0.0f;
		const float tooClosePenalty = (dist < 260.0f) ? (260.0f - dist) * 10.0f : 0.0f;
		const float score = dist + behindPenalty + tooClosePenalty + (1.0f - dot) * 900.0f;
		if (score < bestScore) {
			bestScore = score;
			best.found = true;
			best.position = targetPos;
			best.distance = dist;
			best.forwardDot = dot;
		}
	};

	if (preferAir) {
		for (const auto& enemy : enemyManager->GetEnemies()) {
			if (!enemy || !enemy->IsAlive() || enemy->IsAlly()) {
				continue;
			}
			evaluatePoint(enemy->GetPosition());
		}
		if (best.found) {
			return best;
		}
	}

	for (const auto& groundEnemy : enemyManager->GetGroundEnemies()) {
		if (!groundEnemy || !groundEnemy->IsAlive()) {
			continue;
		}
		evaluatePoint(groundEnemy->GetPosition());
	}
	return best;
}

Vector3 ParticleDemoController::ApplyAltitudeSafetyAndBank(
	const Vector3& pos,
	const Vector3& rawDir,
	float bankLateralOffset,
	float minPitchBias)
{
	Vector3 dir = rawDir;
	if (Length(dir) < 0.001f) {
		dir = { 0.0f, 0.0f, 1.0f };
	} else {
		dir = Normalize(dir);
	}

	// 横方向のスイープオフセット（高G旋回＆翼端ベイパー誘発用）
	const Vector3 worldUp{ 0.0f, 1.0f, 0.0f };
	Vector3 right = Cross(worldUp, dir);
	if (Length(right) > 0.001f) {
		right = Normalize(right);
		dir = Add(dir, Multiply(bankLateralOffset, right));
	}

	// 最低高度を下回らないよう安全ピッチを維持
	if (pos.y < kSafeMinAltitude) {
		const float pullStrength = std::clamp((kSafeMinAltitude - pos.y) / 60.0f, 0.15f, 0.65f);
		dir.y = (std::max)(dir.y, pullStrength);
	} else if (pos.y > kSafeMaxAltitude) {
		dir.y = (std::min)(dir.y, -0.18f);
	}
	if (minPitchBias > -0.9f) {
		dir.y = (std::max)(dir.y, minPitchBias);
	}

	return Normalize(dir);
}

void ParticleDemoController::Update(const DemoContext& ctx) {
	output_ = {};
	if (!enabled_ || finaleTriggered_) {
		return;
	}

	elapsed_ += ctx.dt;
	missileShotCooldown_ = (std::max)(0.0f, missileShotCooldown_ - ctx.dt);
	explosionHoldTimer_ = (std::max)(0.0f, explosionHoldTimer_ - ctx.dt);
	flareFocusTimer_ = (std::max)(0.0f, flareFocusTimer_ - ctx.dt);

	// 最終タイミングで自機撃墜フィナーレへ移行（高度確保済みの状態）
	if (elapsed_ >= kDemoFinaleTime) {
		finaleTriggered_ = true;
		finaleDeathRequested_ = true;
		output_.desiredDirection = ctx.forward;
		output_.throttle = ctx.wepThrottleLimit;
		return;
	}

	const TargetCandidate airTarget = FindBestTarget(ctx.position, ctx.forward, ctx.enemyManager, true);
	if (ctx.enemyManager) {
		const auto& enemies = ctx.enemyManager->GetEnemies();
		if (wasEnemyAlive_.size() != enemies.size()) {
			wasEnemyAlive_.resize(enemies.size(), false);
			for (size_t i = 0; i < enemies.size(); ++i) {
				wasEnemyAlive_[i] = (enemies[i] && enemies[i]->IsAlive());
			}
		}

		const int curDestroyed = ctx.enemyManager->GetDestroyedCount();
		if (curDestroyed > prevDestroyedCount_) {
			for (size_t i = 0; i < enemies.size(); ++i) {
				const bool aliveNow = (enemies[i] && enemies[i]->IsAlive());
				if (wasEnemyAlive_[i] && !aliveNow) {
					wasEnemyAlive_[i] = false;
					// 2つ目の気球を撃破したあとに1.5秒間、爆散地点をカメラ注視する
					if (curDestroyed >= 2) {
						explosionHoldPos_ = enemies[i]->GetPosition();
						explosionHoldTimer_ = 1.5f;
					}
				}
			}
			prevDestroyedCount_ = curDestroyed;
		}
	}

	if (explosionHoldTimer_ > 0.0f) {
		hasActiveTarget_ = true;
		activeTargetPos_ = explosionHoldPos_;
	} else {
		hasActiveTarget_ = airTarget.found;
		activeTargetPos_ = airTarget.position;
	}
	const Vector3 fallbackCruiseDir = Normalize(Vector3{
		std::sin(elapsed_ * 0.35f) * 0.65f,
		0.08f,
		std::cos(elapsed_ * 0.35f) * 0.65f
	});

	Vector3 rawAimDir = fallbackCruiseDir;
	if (airTarget.found) {
		rawAimDir = Normalize(Subtract(airTarget.position, ctx.position));
	}

	// 全編アフターバーナー／高出力でベイパー・衝撃波・排気炎を見せる
	// 15.8〜16.35s だけ一時的にドライ出力へ落とし、Phase 4突入時にWEP再点火スパークを見せる
	const bool briefThrottleDip = (elapsed_ >= 15.8f && elapsed_ < 16.35f);
	if (elapsed_ < 2.5f) {
		output_.throttle = ctx.normalThrottleLimit;
	} else if (briefThrottleDip) {
		output_.throttle = ctx.normalThrottleLimit * 0.85f;
	} else {
		output_.throttle = ctx.wepThrottleLimit;
	}
	float bankOffset = 0.0f;
	float minPitchBias = -0.35f;

	if (elapsed_ < 5.2f) {
		// Phase 1: 第1目標へ直進しつつ機銃掃射（薬莢・マズルフラッシュ・命中爆散）
		output_.useMissileMode = false;
		if (airTarget.found && airTarget.forwardDot >= kGunAlignmentDot && airTarget.distance <= kGunMaxRange) {
			output_.fireGun = true;
		}
	} else if (elapsed_ < 11.0f) {
		// Phase 2: 旋回せず前方目標へ真っ直ぐロック＆ミサイル発射（距離十分で確実に命中させる）
		output_.useMissileMode = (ctx.remainingMissiles > 0);
		bankOffset = 0.0f;
		if (!airTarget.found || airTarget.forwardDot < 0.25f) {
			rawAimDir = fallbackCruiseDir;
		}

		if (ctx.missileLocked && ctx.missileReadyToFire && missileShotCooldown_ <= 0.0f &&
			airTarget.found && airTarget.distance >= 320.0f && airTarget.forwardDot >= 0.85f) {
			output_.fireMissile = true;
			missileShotCooldown_ = 2.6f;
		}

		if (flareStageTriggered_ == 0 && elapsed_ >= 7.8f) {
			output_.triggerFlare = true;
			flareStageTriggered_ = 1;
		}
	} else if (elapsed_ < 16.5f) {
		// Phase 3: ミサイル命中後の高G・高AoA旋回（翼端ボルテックス・主翼ベイパー・フレア）
		output_.useMissileMode = false;
		rawAimDir = fallbackCruiseDir;
		bankOffset = 0.65f * std::sin((elapsed_ - 11.0f) * 1.9f);
		minPitchBias = 0.08f;

		if (flareStageTriggered_ == 1 && elapsed_ >= 12.2f) {
			output_.triggerFlare = true;
			flareStageTriggered_ = 2;
		} else if (flareStageTriggered_ == 2 && elapsed_ >= 15.0f) {
			output_.triggerFlare = true;
			flareStageTriggered_ = 3;
		}
	} else if (elapsed_ < 21.5f) {
		// Phase 4: マッハ突破高速パス（ベイパーコーン・ソニックブーム）+ 機銃掃射＆フレア
		output_.useMissileMode = false;
		rawAimDir = fallbackCruiseDir;
		bankOffset = 0.22f * std::sin((elapsed_ - 16.5f) * 1.3f);
		minPitchBias = 0.04f;
		output_.fireGun = (elapsed_ >= 17.5f && elapsed_ < 19.6f);

		if (flareStageTriggered_ == 3 && elapsed_ >= 18.2f) {
			output_.triggerFlare = true;
			flareStageTriggered_ = 4;
		}
	} else {
		// Phase 5: フィナーレ前の高AoA急上昇ブレイク（失速ベイパー・翼端渦・最後の射撃＆フレア）
		output_.useMissileMode = false;
		const float breakT = elapsed_ - 21.5f;
		bankOffset = 0.95f * std::cos(breakT * 1.8f);
		minPitchBias = 0.42f;
		rawAimDir = ctx.forward;
		output_.fireGun = (std::fmod(breakT, 0.35f) < 0.20f);
		if (flareStageTriggered_ == 4 && elapsed_ >= 22.1f) {
			output_.triggerFlare = true;
			flareStageTriggered_ = 5;
		}
	}

	if (output_.triggerFlare && !hasFocusedFlare_ && elapsed_ >= 11.0f) {
		flareFocusTimer_ = 2.4f;
		hasFocusedFlare_ = true;
	}

	output_.desiredDirection = ApplyAltitudeSafetyAndBank(
		ctx.position,
		rawAimDir,
		bankOffset,
		minPitchBias);
}
