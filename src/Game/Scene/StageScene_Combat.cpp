#include "StageSceneInternal.h"
#include <cmath>

using namespace MyMath;

namespace {
	// 直交正規基底 (Right, Up, Forward) からクォータニオンを計算
	MyMath::Quaternion BasisToQuaternion(const MyMath::Vector3& r, const MyMath::Vector3& u, const MyMath::Vector3& f) {
		float tr = r.x + u.y + f.z;
		MyMath::Quaternion q{};
		if (tr > 0.0f) {
			float s = std::sqrt(tr + 1.0f) * 2.0f;
			q.w = 0.25f * s;
			q.x = (u.z - f.y) / s;
			q.y = (f.x - r.z) / s;
			q.z = (r.y - u.x) / s;
		} else if ((r.x > u.y) && (r.x > f.z)) {
			float s = std::sqrt(1.0f + r.x - u.y - f.z) * 2.0f;
			q.w = (u.z - f.y) / s;
			q.x = 0.25f * s;
			q.y = (r.y + u.x) / s;
			q.z = (f.x + r.z) / s;
		} else if (u.y > f.z) {
			float s = std::sqrt(1.0f + u.y - r.x - f.z) * 2.0f;
			q.w = (f.x - r.z) / s;
			q.x = (r.y + u.x) / s;
			q.y = 0.25f * s;
			q.z = (u.z + f.y) / s;
		} else {
			float s = std::sqrt(1.0f + f.z - r.x - u.y) * 2.0f;
			q.w = (r.y - u.x) / s;
			q.x = (f.x + r.z) / s;
			q.y = (u.z + f.y) / s;
			q.z = 0.25f * s;
		}
		float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
		if (len > 0.0001f) {
			float inv = 1.0f / len;
			q.x *= inv; q.y *= inv; q.z *= inv; q.w *= inv;
		}
		return q;
	}

	// 目標の前方ベクトルと基準上方向からクォータニオンを生成
	MyMath::Quaternion LookRotation(const MyMath::Vector3& forward, const MyMath::Vector3& desiredUp = { 0.0f, 1.0f, 0.0f }) {
		MyMath::Vector3 f = MyMath::Normalize(forward);
		MyMath::Vector3 upRef = desiredUp;
		if (std::fabs(MyMath::Dot(f, upRef)) > 0.99f) {
			upRef = { 0.0f, 0.0f, 1.0f };
		}
		// Right = desiredUp × forward
		MyMath::Vector3 r = MyMath::Normalize(MyMath::Cross(upRef, f));
		// Up = forward × Right
		MyMath::Vector3 u = MyMath::Cross(f, r);
		return BasisToQuaternion(r, u, f);
	}
}

void StageScene::TriggerGameOver() {
	if (isGameOver_ || isMissionCleared_) { return; }
	isGameOver_ = true;
	isMissionFailed_ = true;
	gameOverTimer_ = 0.0f;
	playerHP_ = 0.0f;
	timeScale_ = 0.35f; // 被弾スローモーション
	endSequence_ = StageEndSequence::GameOver_Critical;
	endSeqTimer_ = 0.0f;

	// 被弾・墜落ゲームオーバーを設定
	auto& res = GameResult::GetInstance();
	res.gameOverReason = GameOverReason::Destroyed;

	// 非同期処理による軽量化：ゲームオーバー演出中にバックグラウンドで次ステージデータを非同期プリロード！
	AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();

	// 項目4: テーマカラー赤反転
	hud_.SetWarningColorMode(true);

	EffectManager::GetInstance()->EmitDestroyEffect(flightModel_.GetPosition());

	// カメラシェイクによる被弾・撃墜の強烈な物理的衝撃（指摘1）
	if (CameraManager::GetInstance()->GetActiveCamera()) {
		CameraManager::GetInstance()->GetActiveCamera()->Shake(1.2f, 4.0f);
	}
}

void StageScene::TriggerTimeOver() {
	if (isGameOver_ || isMissionCleared_) { return; }
	isGameOver_ = true;
	isMissionFailed_ = true;
	gameOverTimer_ = 0.0f;
	timeScale_ = 1.0f;
	endSequence_ = StageEndSequence::TimeOver_Abort;
	endSeqTimer_ = 0.0f;

	// ゲームオーバーリザルトに時間切れ離脱要因を記録
	auto& res = GameResult::GetInstance();
	res.gameOverReason = GameOverReason::TimeOver;

	// 非同期プリロード開始
	AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();

	// 爆発・破壊はさせず、無敵化＆G効果無効化＆オートパイロット離脱に移行
	flightModel_.SetInvulnerable(true);
	flightModel_.SetGEffectsEnabled(false);
	flightModel_.ResetGEffects();
	flightModel_.SetAutopilot(true);

	// 離脱初期ベクトルを保持
	clearStartOrientation_ = flightModel_.GetOrientation();
	clearStartForward_ = flightModel_.GetForwardDirection();
	clearStartSpeed_ = (std::max)(flightModel_.GetSpeed(), 120.0f);

	Vector3 horiz = { clearStartForward_.x, 0.0f, clearStartForward_.z };
	if (Length(horiz) > 0.001f) {
		clearHorizontalDir_ = Normalize(horiz);
	} else {
		clearHorizontalDir_ = { 0.0f, 0.0f, 1.0f };
	}

	// HUDを警告色モードにしつつオフラインシャットダウン開始
	hud_.SetWarningColorMode(true);
	hud_.StartShutdown();
}

void StageScene::TriggerGameClear() {
	if (isMissionCleared_ || isGameOver_) { return; }
	isMissionCleared_ = true;
	endSequence_ = StageEndSequence::Clear_HudShutdown;
	endSeqTimer_ = 0.0f;

	// 自機を完全無敵化（受けるダメージ0）
	flightModel_.SetInvulnerable(true);

	// G効果（ブラックアウト・レッドアウト）を即座に無効化＆リセット
	flightModel_.SetGEffectsEnabled(false);
	flightModel_.ResetGEffects();

	// オートパイロット移行（物理シミュレーションを直接制御）
	flightModel_.SetAutopilot(true);

	// 離脱機動の初期状態を保持
	clearStartOrientation_ = flightModel_.GetOrientation();
	clearStartForward_ = flightModel_.GetForwardDirection();
	clearStartSpeed_ = (std::max)(flightModel_.GetSpeed(), 120.0f);

	Vector3 horiz = { clearStartForward_.x, 0.0f, clearStartForward_.z };
	if (Length(horiz) > 0.001f) {
		clearHorizontalDir_ = Normalize(horiz);
	} else {
		clearHorizontalDir_ = { 0.0f, 0.0f, 1.0f };
	}

	// 項目5: HUDの順次シャットダウン開始
	hud_.StartShutdown();
}

// ============================================================
// PlayerCollisionBody のコールバック実装
// ============================================================
void StageScene::PlayerCollisionBody::OnCollision(ICollisionBody3D* other) {
	if (!scene_ || scene_->isMissionFailed_ || scene_->isMissionCleared_) { return; }

	// EnemyBullet との衝突 → サブコライダー被弾処理
	if (other->GetCollisionAttribute() & CollisionAttribute::kEnemyBullet) {
		auto* bullet = dynamic_cast<Bullet*>(other);
		Vector3 bulletPos = bullet ? bullet->GetPosition() : scene_->flightModel_.GetPosition();
		float damage = bullet ? bullet->GetDamage() : 10.0f;

		HitResult res = scene_->flightModel_.ProcessBulletHit(bulletPos, damage);
		EffectManager::GetInstance()->EmitHitEffect(bulletPos);

		// カメラシェイク（被弾）
		if (CameraManager::GetInstance()->GetActiveCamera()) {
			CameraManager::GetInstance()->GetActiveCamera()->Shake(0.5f, 2.0f);
		}

		if (res.fireStarted) {
			EffectManager::GetInstance()->EmitDestroyEffect(bulletPos);
		}

		// 致命的ダメージでゲームオーバー
		if (scene_->flightModel_.GetAirframe().IsDestroyed()) {
			scene_->TriggerGameOver();
		}
	}
	// Enemy との衝突 → 胴体に大ダメージ
	else if (other->GetCollisionAttribute() & CollisionAttribute::kEnemy) {
		scene_->flightModel_.GetDamageModel().ProcessHit(DamagePart::Fuse, 50.0f);
		EffectManager::GetInstance()->EmitHitEffect(scene_->flightModel_.GetPosition());

		// カメラシェイク（激突）
		if (CameraManager::GetInstance()->GetActiveCamera()) {
			CameraManager::GetInstance()->GetActiveCamera()->Shake(0.8f, 5.0f);
		}

		if (scene_->flightModel_.GetAirframe().IsDestroyed()) {
			scene_->TriggerGameOver();
		}
	}
}

void StageScene::CheckPartDestructionEvents() {
	const auto& dm = flightModel_.GetDamageModel();

	for (int i = 0; i < DamageModel::kPartCount; ++i) {
		DamagePart part = static_cast<DamagePart>(i);

		if (dm.IsPartDestroyed(part)) {
			if (aircraftVisualModel_.IsPartVisible(part)) {
				// 1. ビジュアル側を非表示化
				aircraftVisualModel_.SetPartVisible(part, false);

				// 2. 切断破片（Debris）を生成しワールドへ放出して物理運動
				Matrix4x4 partWorldMatrix = aircraftVisualModel_.GetPartWorldMatrix(part);
				Vector3 baseVelocity = flightModel_.GetVelocity();
				Vector3 ejectForce = Multiply(6.0f, flightModel_.GetRightDirection());
				if (part == DamagePart::Wing_L || part == DamagePart::Wing1_L || part == DamagePart::Wing2_L) {
					ejectForce = Multiply(-6.0f, flightModel_.GetRightDirection());
				}

				Model* mainModel = ModelManager::GetInstance()->FindModel("Resources/models/m21.gltf");
				if (!mainModel) {
					mainModel = ModelManager::GetInstance()->FindModel("Resources/models/m21_wg.gltf");
				}
				static const std::unordered_map<DamagePart, std::string> kPartNodeNameMap = {
					{ DamagePart::Fuse, "Fuse" },
					{ DamagePart::Wing_L, "Wing_L" },
					{ DamagePart::Wing1_L, "Wing1_L" },
					{ DamagePart::Wing2_L, "Wing2_L" },
					{ DamagePart::Wing_R, "Wing_R" },
					{ DamagePart::Wing1_R, "Wing1_R" },
					{ DamagePart::Wing2_R, "Wing2_R" },
					{ DamagePart::Tail, "Tail" },
				};

				std::string nodeName = "";
				auto itMap = kPartNodeNameMap.find(part);
				if (itMap != kPartNodeNameMap.end()) {
					nodeName = itMap->second;
				}

				DebrisManager::GetInstance()->SpawnDebris(mainModel, partWorldMatrix, baseVelocity, ejectForce, nodeName);

				// 3. 切断面からの破片発煙エフェクト
				Vector3 partPos = aircraftVisualModel_.GetPartWorldPosition(part);
				if (EffectManager::GetInstance()) {
					EffectManager::GetInstance()->EmitDestroyEffect(partPos);
				}
			}
		}
	}

	// エンジン損傷時のプロペラ回転制御
	if (dm.IsPartDestroyed(DamagePart::Engine1) || dm.GetPartState(DamagePart::Engine1).HasFlag(PartStatus::Damaged)) {
		float powerFactor = dm.GetEnginePowerFactor();
		aircraftVisualModel_.SetPropellerRpm(2000.0f * powerFactor);
	}
}

// ============================================================
// ゲームオーバー / クリア演出シーケンス更新（項目1〜7）
// ============================================================
void StageScene::UpdateEndSequence(float dt) {
	if (endSequence_ == StageEndSequence::None) {
		timeScale_ = 1.0f;
		return;
	}

	endSeqTimer_ += dt;

	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->ClearActiveEffects();

	// --- ゲームオーバー系（被弾・撃墜のシステム破壊演出をシェーダーで画面全体に展開：指摘1, 5）---
	if (endSequence_ == StageEndSequence::GameOver_Critical) {
		// 0.0s - 0.25s: 撃墜瞬間の被弾スローモーション & 激しい画面破壊シェーダー
		timeScale_ = 0.20f;

		// 1. 赤色警告ティントストロボ点滅
		ActivePostEffect tint;
		tint.type = PostEffectType::kColorTint;
		tint.colorR = 1.0f;
		tint.colorG = (std::fmod(endSeqTimer_, 0.08f) < 0.04f) ? 0.15f : 0.40f;
		tint.colorB = 0.10f;
		postEffect->AddActiveEffect(tint);

		// 2. 衝撃レンズ歪み
		ActivePostEffect dist;
		dist.type = PostEffectType::kLensDistortion;
		dist.intensity = 0.25f;
		postEffect->AddActiveEffect(dist);

		// 3. 激しい色収差 (RGB分離)
		ActivePostEffect ca;
		ca.type = PostEffectType::kChromaticAberration;
		ca.intensity = 0.08f;
		postEffect->AddActiveEffect(ca);

		// 4. ランダム破壊ノイズ
		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.35f;
		postEffect->AddActiveEffect(noise);

		if (endSeqTimer_ >= 0.25f) {
			endSequence_ = StageEndSequence::GameOver_SignalLost;
		}
	} else if (endSequence_ == StageEndSequence::GameOver_SignalLost) {
		// 0.25s - 0.70s: 致命的通信途絶・フリーズ & 激しい走査線同期乱れ & SIGNAL LOST
		timeScale_ = 0.0f;

		// 1. 走査線の乱れ
		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.kernelSize = 0;
		scan.intensity = 0.45f;
		scan.dirX = 250.0f;
		scan.dirY = -30.0f;
		postEffect->AddActiveEffect(scan);

		// 2. デジタルグリッチノイズ
		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.45f;
		postEffect->AddActiveEffect(noise);

		// 3. 色収差
		ActivePostEffect ca;
		ca.type = PostEffectType::kChromaticAberration;
		ca.intensity = 0.06f;
		postEffect->AddActiveEffect(ca);

		// 4. 赤暗化ビネット
		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = 1.4f;
		postEffect->AddActiveEffect(vig);

		if (endSeqTimer_ >= 0.70f) {
			endSequence_ = StageEndSequence::GameOver_CrtShutdown;
		}
	} else if (endSequence_ == StageEndSequence::GameOver_CrtShutdown) {
		// 0.70s - 0.95s: ブラウン管テレビの電源遮断演出（水平収束・プツッ消滅）
		timeScale_ = 0.0f;

		ActivePostEffect dist;
		dist.type = PostEffectType::kLensDistortion;
		dist.intensity = 0.35f;
		postEffect->AddActiveEffect(dist);

		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.intensity = 0.5f;
		scan.dirX = 800.0f;
		scan.dirY = 5.0f;
		postEffect->AddActiveEffect(scan);

		if (endSeqTimer_ >= 0.95f) {
			postEffect->ClearActiveEffects();
			sceneID = SCENE::GAMEOVER;
			return;
		}
	}
	// --- 時間切れ離脱系（爆発なし・作戦離脱） ---
	else if (endSequence_ == StageEndSequence::TimeOver_Abort) {
		// 0.0s - 1.4s: 作戦時間切れ警報 ＆ 自機水平レベリング・離脱加速開始
		timeScale_ = 1.0f;

		// 翼のロールを水平に戻す目標姿勢
		Quaternion qLevel = LookRotation(clearHorizontalDir_, { 0.0f, 1.0f, 0.0f });
		float levelT = std::clamp(endSeqTimer_ / 0.9f, 0.0f, 1.0f);
		float smoothT = levelT * levelT * (3.0f - 2.0f * levelT);
		Quaternion currentOri = Slerp(clearStartOrientation_, qLevel, smoothT);
		flightModel_.SetOrientation(currentOri);

		// 巡航速度へ加速
		float speed = clearStartSpeed_ + smoothT * (180.0f - clearStartSpeed_);
		flightModel_.SetVelocity(Multiply(speed, flightModel_.GetForwardDirection()));

		// 走査線と警告ビネット（アンバー調）
		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.intensity = 0.20f;
		scan.dirX = 650.0f;
		scan.dirY = -2.0f;
		postEffect->AddActiveEffect(scan);

		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = 0.65f + 0.25f * std::sin(endSeqTimer_ * 6.28f);
		postEffect->AddActiveEffect(vig);

		if (endSeqTimer_ >= 1.4f) {
			endSequence_ = StageEndSequence::TimeOver_Withdraw;
		}
	} else if (endSequence_ == StageEndSequence::TimeOver_Withdraw) {
		// 1.4s - 3.4s: 機首を緩やかに上げてアフターバーナー離脱 ＆ 暗転フェード
		timeScale_ = 1.0f;

		Quaternion qLevel = LookRotation(clearHorizontalDir_, { 0.0f, 1.0f, 0.0f });
		constexpr float kClimbAngle = 26.0f * (3.14159265f / 180.0f);
		Vector3 climbForward = Normalize(Add(
			Multiply(std::cos(kClimbAngle), clearHorizontalDir_),
			{ 0.0f, std::sin(kClimbAngle), 0.0f }
		));
		Quaternion qClimb = LookRotation(climbForward, { 0.0f, 1.0f, 0.0f });

		float climbT = std::clamp((endSeqTimer_ - 1.4f) / 1.2f, 0.0f, 1.0f);
		float smoothClimb = climbT * climbT * (3.0f - 2.0f * climbT);
		Quaternion currentOri = Slerp(qLevel, qClimb, smoothClimb);
		flightModel_.SetOrientation(currentOri);

		// 急加速して戦域外へ離脱
		float speed = 180.0f + smoothClimb * 420.0f;
		flightModel_.SetVelocity(Multiply(speed, flightModel_.GetForwardDirection()));

		// 加速のラジアルブラー
		ActivePostEffect blur;
		blur.type = PostEffectType::kRadialBlur;
		blur.kernelSize = 6;
		blur.intensity = smoothClimb * 0.06f;
		postEffect->AddActiveEffect(blur);

		// 離脱完了に向けたフェード暗転
		float fadeT = std::clamp((endSeqTimer_ - 2.5f) / 0.9f, 0.0f, 1.0f);
		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = 0.70f + fadeT * 3.5f;
		postEffect->AddActiveEffect(vig);

		if (endSeqTimer_ >= 3.4f) {
			postEffect->ClearActiveEffects();
			sceneID = SCENE::GAMEOVER;
			return;
		}
	}
	// --- ステージクリア系 ---
	else if (endSequence_ == StageEndSequence::Clear_HudShutdown) {
		// 0.0s - 1.2s: 目標達成ファンファーレ & HUD順次OFFLINE消去
		// 自機は機動姿勢を整え（水平レベリング）、アフターバーナー点火（加速開始）
		timeScale_ = 1.0f;

		// 翼のロールを水平に戻す目標姿勢（水平巡航姿勢）
		Quaternion qLevel = LookRotation(clearHorizontalDir_, { 0.0f, 1.0f, 0.0f });

		// 最初の0.8秒でクリア突入時の姿勢から水平姿勢へ滑らかに復帰
		float levelT = std::clamp(endSeqTimer_ / 0.8f, 0.0f, 1.0f);
		float smoothT = levelT * levelT * (3.0f - 2.0f * levelT);
		Quaternion currentOri = Slerp(clearStartOrientation_, qLevel, smoothT);
		flightModel_.SetOrientation(currentOri);

		// 速度：クリア開始速度から巡航加速（約180m/sへ）
		float speed = clearStartSpeed_ + smoothT * (180.0f - clearStartSpeed_);
		Vector3 curFwd = flightModel_.GetForwardDirection();
		flightModel_.SetVelocity(Multiply(speed, curFwd));

		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = 2.5f;
		bloom.dirX = 1.2f;
		bloom.dirY = 0.7f;
		postEffect->AddActiveEffect(bloom);

		if (endSeqTimer_ >= 1.2f) {
			endSequence_ = StageEndSequence::Clear_AutopilotReturn;
		}
	} else if (endSequence_ == StageEndSequence::Clear_AutopilotReturn) {
		// 1.2s - 2.5s: オートパイロット帰還時の超音速ズームクライム急上昇・急加速離脱機動
		timeScale_ = 1.0f;

		// 水平巡航姿勢と、目標上昇姿勢（ピッチ角 約38度ピッチアップ）
		Quaternion qLevel = LookRotation(clearHorizontalDir_, { 0.0f, 1.0f, 0.0f });

		// ピッチアップ上昇角: 約38度（約0.663ラジアン）
		constexpr float kClimbAngle = 38.0f * (3.14159265f / 180.0f);
		Vector3 climbForward = Normalize(Add(
			Multiply(std::cos(kClimbAngle), clearHorizontalDir_),
			{ 0.0f, std::sin(kClimbAngle), 0.0f }
		));
		Quaternion qClimb = LookRotation(climbForward, { 0.0f, 1.0f, 0.0f });

		// 1.3秒間かけて滑らかに機首を持ち上げ（ピッチアップ）、進行方向と機体姿勢を完全に一致させる
		float climbT = std::clamp((endSeqTimer_ - 1.2f) / 1.3f, 0.0f, 1.0f);
		float smoothClimb = climbT * climbT * (3.0f - 2.0f * climbT);
		Quaternion currentOri = Slerp(qLevel, qClimb, smoothClimb);
		flightModel_.SetOrientation(currentOri);

		// 速度：機首を上げながらアフターバーナー全開で超音速（約660m/s = マッハ2）へ急加速！
		float speed = 180.0f + smoothClimb * 480.0f;
		Vector3 curFwd = flightModel_.GetForwardDirection();
		flightModel_.SetVelocity(Multiply(speed, curFwd));

		// 1. ラジアルブラー（スピード感・ワープ感の画面全体演出）
		ActivePostEffect blur;
		blur.type = PostEffectType::kRadialBlur;
		blur.kernelSize = 8;
		blur.intensity = smoothClimb * 0.08f;
		postEffect->AddActiveEffect(blur);

		// 2. 色収差（光速突入の色ズレ）
		ActivePostEffect ca;
		ca.type = PostEffectType::kChromaticAberration;
		ca.intensity = smoothClimb * 0.04f;
		postEffect->AddActiveEffect(ca);

		if (endSeqTimer_ >= 2.5f) {
			endSequence_ = StageEndSequence::Clear_TacticalMapZoom;
		}
	} else if (endSequence_ == StageEndSequence::Clear_TacticalMapZoom) {
		// 2.5s - 3.8s: 戦術マップ表示（走査線 + ビネット）＆ ターミナル転送接続
		timeScale_ = 1.0f;

		// 機体は最高速のまま空へ離脱を継続
		constexpr float kClimbAngle = 38.0f * (3.14159265f / 180.0f);
		Vector3 climbForward = Normalize(Add(
			Multiply(std::cos(kClimbAngle), clearHorizontalDir_),
			{ 0.0f, std::sin(kClimbAngle), 0.0f }
		));
		Quaternion qClimb = LookRotation(climbForward, { 0.0f, 1.0f, 0.0f });
		flightModel_.SetOrientation(qClimb);
		flightModel_.SetVelocity(Multiply(660.0f, flightModel_.GetForwardDirection()));

		ActivePostEffect scan;
		scan.type = PostEffectType::kScanLine;
		scan.intensity = 0.25f;
		scan.dirX = 500.0f;
		scan.dirY = -3.0f;
		postEffect->AddActiveEffect(scan);

		ActivePostEffect vig;
		vig.type = PostEffectType::kVignette;
		vig.intensity = 1.1f;
		postEffect->AddActiveEffect(vig);

		if (endSeqTimer_ >= 3.8f) {
			postEffect->ClearActiveEffects();
			// クリアリザルトへ記録を保存して遷移
			auto& res = GameResult::GetInstance();
			res.missionCleared = true;
			res.clearTimeSeconds = missionElapsedTime_;
			res.playerHPRemaining = playerHP_;
			res.playerMaxHP = playerMaxHP_;
			sceneID = SCENE::CLEAR;
			return;
		}
	}
}

// ============================================================
// ゲームオーバー / クリア演出UI描画（項目1〜7）
// ============================================================
void StageScene::DrawEndSequenceUI() {
	if (endSequence_ == StageEndSequence::None) {
		return;
	}

	float screenW = static_cast<float>(WinApp::kClientWidth);
	float screenH = static_cast<float>(WinApp::kClientHeight);

	// 軽量描画ヘルパーラムダ
	auto DrawSolidRect = [this](const Vector2& pos, const Vector2& size, const Vector4& color, BlendMode blend = BlendMode::kNormal) {
		if (!seqOverlaySprite_) return;
		seqOverlaySprite_->SetBlendMode(blend);
		seqOverlaySprite_->SetPosition(pos);
		seqOverlaySprite_->SetAnchorPoint({ 0.0f, 0.0f });
		seqOverlaySprite_->SetSize(size);
		seqOverlaySprite_->SetRotation(0.0f);
		seqOverlaySprite_->SetColor(color);
		seqOverlaySprite_->Update();
		seqOverlaySprite_->Draw();
	};

	auto DrawHollowBorder = [&DrawSolidRect](const Vector2& pos, const Vector2& size, const Vector4& color, float thickness) {
		DrawSolidRect(pos, { size.x, thickness }, color);
		DrawSolidRect({ pos.x, pos.y + size.y - thickness }, { size.x, thickness }, color);
		DrawSolidRect(pos, { thickness, size.y }, color);
		DrawSolidRect({ pos.x + size.x - thickness, pos.y }, { thickness, size.y }, color);
	};

	// ------------------------------------------------------------
	// 項目4: 被弾・撃墜時のテーマカラー反転 & CRITICAL DAMAGE 激しい点滅
	// ------------------------------------------------------------
	if (endSequence_ == StageEndSequence::GameOver_Critical) {
		// 0.08s 周期の激しい赤色警告フラッシュ
		float blinkPhase = std::fmod(endSeqTimer_, 0.10f);
		bool isBlinkOn = (blinkPhase < 0.06f);

		if (isBlinkOn) {
			float bannerW = 680.0f;
			float bannerH = 70.0f;
			float bannerX = (screenW - bannerW) * 0.5f;
			float bannerY = 120.0f;

			// 赤色半透明バナー背景
			DrawSolidRect({ bannerX, bannerY }, { bannerW, bannerH }, { 0.50f, 0.02f, 0.02f, 0.85f });
			DrawSolidRect({ bannerX, bannerY }, { bannerW, 2.0f }, { 1.0f, 0.25f, 0.20f, 0.95f });
			DrawSolidRect({ bannerX, bannerY + bannerH - 2.0f }, { bannerW, 2.0f }, { 1.0f, 0.25f, 0.20f, 0.95f });

			TextRenderer::GetInstance()->Print(
				"HackGen",
				"! CRITICAL DAMAGE !",
				screenW * 0.5f,
				bannerY + 10.0f,
				34.0f,
				{ 1.0f, 0.35f, 0.30f, 1.0f },
				{ 0.5f, 0.0f }
			);

			TextRenderer::GetInstance()->Print(
				"HackGen",
				"HULL INTEGRITY COMPROMISED // EJECTION RECOMMENDED",
				screenW * 0.5f,
				bannerY + 44.0f,
				14.0f,
				{ 1.0f, 0.85f, 0.85f, 0.90f },
				{ 0.5f, 0.0f }
			);
		}
	}

	// ------------------------------------------------------------
	// 項目1: 通信途絶（シグナルロスト）フリーズ & 軽量グリッチ帯 & テキスト
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::GameOver_SignalLost) {
		// 画面水平ジッター（致命的エラーによる同期ズレ）
		float jitterX = std::sin(endSeqTimer_ * 110.0f) * 8.0f;
		if (std::fmod(endSeqTimer_, 0.08f) < 0.03f) {
			jitterX += ((std::rand() % 20) - 10) * 1.5f;
		}

		// 赤黒ノイズベール
		DrawSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.20f, 0.02f, 0.02f, 0.40f });

		// 走査線インターレースグリッチ（数本が激しく上下を走る）
		float glitchOffset1 = std::fmod(endSeqTimer_ * 950.0f, screenH);
		DrawSolidRect({ 0.0f, glitchOffset1 }, { screenW, 20.0f }, { 0.95f, 0.95f, 1.0f, 0.25f }, BlendMode::kAdd);
		float glitchOffset2 = std::fmod(endSeqTimer_ * 1400.0f + 240.0f, screenH);
		DrawSolidRect({ 0.0f, glitchOffset2 }, { screenW, 40.0f }, { 1.0f, 0.15f, 0.15f, 0.35f }, BlendMode::kAdd);
		float glitchOffset3 = std::fmod(endSeqTimer_ * 600.0f + 480.0f, screenH);
		DrawSolidRect({ 0.0f, glitchOffset3 }, { screenW, 8.0f }, { 0.2f, 0.8f, 1.0f, 0.30f }, BlendMode::kAdd);

		// 中央警告カード（ジッター振動）
		float cardW = 760.0f;
		float cardH = 155.0f;
		float cardX = (screenW - cardW) * 0.5f + jitterX;
		float cardY = (screenH - cardH) * 0.5f;

		// カード背景と赤枠
		DrawSolidRect({ cardX, cardY }, { cardW, cardH }, { 0.08f, 0.01f, 0.01f, 0.94f });
		DrawHollowBorder({ cardX, cardY }, { cardW, cardH }, { 1.0f, 0.25f, 0.20f, 0.95f }, 2.5f);
		DrawSolidRect({ cardX, cardY }, { cardW, 4.0f }, { 1.0f, 0.40f, 0.40f, 1.0f });

		// 点滅＆RGB色ズレする「SIGNAL LOST」
		float signalPulse = 0.7f + 0.3f * std::sin(endSeqTimer_ * 30.0f);
		
		// グリッチ文字化（周期的に一瞬化ける）
		const char* signalLostTitle = (std::fmod(endSeqTimer_, 0.12f) < 0.03f) ? "S!GN#L L0ST // ERR" : "SIGNAL LOST";

		// RGBスプリット（シアン影＋赤影で致命的画面乱れ感を演出）
		TextRenderer::GetInstance()->Print(
			"HackGen",
			signalLostTitle,
			screenW * 0.5f + jitterX + 3.0f,
			cardY + 20.0f,
			46.0f,
			{ 0.10f, 0.85f, 1.0f, signalPulse * 0.6f },
			{ 0.5f, 0.0f }
		);
		TextRenderer::GetInstance()->Print(
			"HackGen",
			signalLostTitle,
			screenW * 0.5f + jitterX - 3.0f,
			cardY + 20.0f,
			46.0f,
			{ 1.0f, 0.15f, 0.15f, signalPulse * 0.8f },
			{ 0.5f, 0.0f }
		);
		TextRenderer::GetInstance()->Print(
			"HackGen",
			signalLostTitle,
			screenW * 0.5f + jitterX,
			cardY + 20.0f,
			46.0f,
			{ 1.0f, 0.95f, 0.95f, signalPulse },
			{ 0.5f, 0.0f }
		);

		// 「TELEMETRY DISCONNECTED (遠隔測定切断)」
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"TELEMETRY DISCONNECTED (遠隔測定切断)",
			screenW * 0.5f + jitterX,
			cardY + 76.0f,
			22.0f,
			{ 1.0f, 0.85f, 0.85f, 0.95f },
			{ 0.5f, 0.0f }
		);

		TextRenderer::GetInstance()->Print(
			"HackGen",
			"CARRIER FREQUENCY TERMINATED // SYSTEM FAULT 0x000000FA",
			screenW * 0.5f + jitterX,
			cardY + 110.0f,
			14.0f,
			{ 0.95f, 0.40f, 0.40f, 0.85f },
			{ 0.5f, 0.0f }
		);
	}

	// ------------------------------------------------------------
	// 項目2: システム強制シャットダウン（ブラウン管テレビのプツッ暗転: 0.25秒で鋭く閉鎖）
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::GameOver_CrtShutdown) {
		float crtT = std::clamp((endSeqTimer_ - 0.70f) / 0.25f, 0.0f, 1.0f);
		float centerY = screenH * 0.5f;
		float centerX = screenW * 0.5f;

		if (crtT < 0.60f) {
			// Phase 1 (0.00〜0.60): 上下から黒マスクが中央水平線に向かって急激に収束！
			float p = crtT / 0.60f;
			float collapse = 1.0f - std::pow(1.0f - p, 3.0f);
			float halfH = (screenH * 0.5f) * (1.0f - collapse);
			if (halfH < 2.0f) halfH = 2.0f;

			DrawSolidRect({ 0.0f, 0.0f }, { screenW, centerY - halfH }, { 0.0f, 0.0f, 0.0f, 1.0f });
			DrawSolidRect({ 0.0f, centerY + halfH }, { screenW, screenH - (centerY + halfH) }, { 0.0f, 0.0f, 0.0f, 1.0f });

			// 中央水平発光線
			float lineAlpha = 0.9f + 0.1f * std::sin(endSeqTimer_ * 60.0f);
			DrawSolidRect({ 0.0f, centerY - 2.0f }, { screenW, 4.0f }, { 0.90f, 0.95f, 1.0f, lineAlpha }, BlendMode::kAdd);
		} else if (crtT < 0.88f) {
			// Phase 2 (0.60〜0.88): 水平線が左右から急速に収縮！
			float p = (crtT - 0.60f) / 0.28f;
			float shrink = 1.0f - p;
			float halfW = (screenW * 0.5f) * shrink;

			DrawSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.0f, 0.0f, 0.0f, 1.0f });
			DrawSolidRect({ centerX - halfW, centerY - 2.5f }, { halfW * 2.0f, 5.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, BlendMode::kAdd);
		} else {
			// Phase 3 (0.88〜1.00): 「プツッ」と極小の光点が光って暗転完了（即時次シーンへ）
			float p = (crtT - 0.88f) / 0.12f;
			DrawSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.0f, 0.0f, 0.0f, 1.0f });

			if (p < 0.50f) {
				float popAlpha = 1.0f - (p / 0.50f);
				DrawSolidRect({ centerX - 4.0f, centerY - 4.0f }, { 8.0f, 8.0f }, { 1.0f, 1.0f, 1.0f, popAlpha }, BlendMode::kAdd);
			}
		}
	}
	// ------------------------------------------------------------
	// 時間切れ離脱演出: TIME OVER & ミッションから離脱します.....
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::TimeOver_Abort || endSequence_ == StageEndSequence::TimeOver_Withdraw) {
		float fadeAlpha = std::clamp(endSeqTimer_ / 0.30f, 0.0f, 1.0f);

		// 点滅ドット計算 (1〜5個)
		int dotCount = 1 + static_cast<int>(endSeqTimer_ * 3.5f) % 5;
		std::string dots = std::string(dotCount, '.');

		// 画面上部〜中央の警告バナー
		float cardW = 760.0f;
		float cardH = 150.0f;
		float cardX = (screenW - cardW) * 0.5f;
		float cardY = 140.0f;

		// 半透明ダークアンバー背景
		DrawSolidRect({ cardX, cardY }, { cardW, cardH }, { 0.04f, 0.02f, 0.01f, 0.90f * fadeAlpha });
		DrawHollowBorder({ cardX, cardY }, { cardW, cardH }, { 1.0f, 0.70f, 0.15f, 0.95f * fadeAlpha }, 2.0f);
		DrawSolidRect({ cardX, cardY }, { cardW, 4.0f }, { 1.0f, 0.85f, 0.30f, 1.0f * fadeAlpha });
		DrawSolidRect({ cardX, cardY + cardH - 2.0f }, { cardW, 2.0f }, { 1.0f, 0.70f, 0.15f, 0.75f * fadeAlpha });

		// 大見出し「Time Over」
		float blink = (std::fmod(endSeqTimer_, 0.5f) < 0.38f) ? 1.0f : 0.65f;
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"Time Over",
			screenW * 0.5f,
			cardY + 16.0f,
			42.0f,
			{ 1.0f, 0.75f, 0.18f, fadeAlpha * blink },
			{ 0.5f, 0.0f }
		);

		// サブメッセージ「ミッションから離脱します.....」
		std::string msgStr = "ミッションから離脱します" + dots;
		TextRenderer::GetInstance()->Print(
			"HackGen",
			msgStr.c_str(),
			screenW * 0.5f,
			cardY + 70.0f,
			22.0f,
			{ 1.0f, 1.0f, 1.0f, fadeAlpha },
			{ 0.5f, 0.0f }
		);

		// ステータスフッター
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"[ OPERATION TIME EXPIRED // AUTOPILOT RTB ENGAGED ]",
			screenW * 0.5f,
			cardY + 114.0f,
			13.0f,
			{ 1.0f, 0.75f, 0.30f, 0.85f * fadeAlpha },
			{ 0.5f, 0.0f }
		);
	}
	// ------------------------------------------------------------
	// ステージクリア時の巨大ファンファーレUI（大きく「OPERATION CLEARED」演出）
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::Clear_HudShutdown) {
		float bannerW = 880.0f;
		float bannerH = 120.0f;
		float bannerX = (screenW - bannerW) * 0.5f;
		float bannerY = screenH * 0.5f - 60.0f;

		// 発光バックグラウンド
		float pulseAlpha = 0.88f + 0.12f * std::sin(endSeqTimer_ * 15.0f);
		DrawSolidRect({ bannerX, bannerY }, { bannerW, bannerH }, { 0.02f, 0.12f, 0.06f, 0.92f });
		DrawHollowBorder({ bannerX, bannerY }, { bannerW, bannerH }, { 0.30f, 1.0f, 0.60f, pulseAlpha }, 3.0f);
		DrawSolidRect({ bannerX, bannerY }, { bannerW, 4.0f }, { 0.60f, 1.0f, 0.80f, 1.0f });

		// 巨大な "OPERATION CLEARED" スタンプ
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"// OPERATION CLEARED //",
			screenW * 0.5f,
			bannerY + 14.0f,
			48.0f,
			{ 0.35f, 1.0f, 0.60f, 1.0f },
			{ 0.5f, 0.0f }
		);

		TextRenderer::GetInstance()->Print(
			"HackGen",
			"ALL PRIMARY OBJECTIVES ACHIEVED // AIRSPACE SECURED (作戦空域完全制圧)",
			screenW * 0.5f,
			bannerY + 74.0f,
			20.0f,
			{ 0.80f, 1.0f, 0.90f, 0.95f },
			{ 0.5f, 0.0f }
		);
	}

	// ------------------------------------------------------------
	// 項目6: 帰還シーケンス（オートパイロット）
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::Clear_AutopilotReturn) {
		float bannerW = 760.0f;
		float bannerH = 80.0f;
		float bannerX = (screenW - bannerW) * 0.5f;
		float bannerY = screenH * 0.5f + 80.0f;

		DrawSolidRect({ bannerX, bannerY }, { bannerW, bannerH }, { 0.02f, 0.08f, 0.04f, 0.85f });
		DrawHollowBorder({ bannerX, bannerY }, { bannerW, bannerH }, { 0.25f, 0.95f, 0.55f, 0.90f }, 1.5f);

		TextRenderer::GetInstance()->Print(
			"HackGen",
			"ENTERING AUTOPILOT MODE... RETURNING TO BASE",
			screenW * 0.5f,
			bannerY + 14.0f,
			24.0f,
			{ 0.40f, 1.0f, 0.70f, 1.0f },
			{ 0.5f, 0.0f }
		);

		TextRenderer::GetInstance()->Print(
			"HackGen",
			"(自動操縦移行... 帰還中)",
			screenW * 0.5f,
			bannerY + 46.0f,
			18.0f,
			{ 0.80f, 0.95f, 0.85f, 0.90f },
			{ 0.5f, 0.0f }
		);
	}

	// ------------------------------------------------------------
	// 項目10, 11: タクティカルマップ移行（デジタル急ズームワイプ＆稼働する動的戦術マップ）
	// ------------------------------------------------------------
	else if (endSequence_ == StageEndSequence::Clear_TacticalMapZoom) {
		float mapTimer = endSeqTimer_ - 2.8f; // 0.0s 〜 1.7s

		const float gridSpacing = 64.0f;
		float gridScrollX = std::fmod(endSeqTimer_ * 32.0f, gridSpacing);
		float gridScrollY = std::fmod(endSeqTimer_ * 20.0f, gridSpacing);
		Vector4 gridLineCol = { 0.15f, 0.45f, 0.30f, 0.22f };

		for (float gx = -gridSpacing + gridScrollX; gx < screenW + gridSpacing; gx += gridSpacing) {
			DrawSolidRect({ gx, 0.0f }, { 1.0f, screenH }, gridLineCol);
		}
		for (float gy = -gridSpacing + gridScrollY; gy < screenH + gridSpacing; gy += gridSpacing) {
			DrawSolidRect({ 0.0f, gy }, { screenW, 1.0f }, gridLineCol);
		}

		// 戦術マップ走査ライングリッド風オーバーレイ
		float scanPulse = 0.18f + 0.06f * std::sin(endSeqTimer_ * 12.0f);
		DrawSolidRect({ 0.0f, 0.0f }, { screenW, screenH }, { 0.01f, 0.08f, 0.04f, scanPulse });

		// 四隅の戦術座標ブラケット
		float bracketLen = 50.0f;
		float margin = 40.0f;
		Vector4 tactCol = { 0.25f, 0.90f, 0.55f, 0.85f };
		DrawSolidRect({ margin, margin }, { bracketLen, 2.0f }, tactCol);
		DrawSolidRect({ margin, margin }, { 2.0f, bracketLen }, tactCol);
		DrawSolidRect({ screenW - margin - bracketLen, margin }, { bracketLen, 2.0f }, tactCol);
		DrawSolidRect({ screenW - margin, margin }, { 2.0f, bracketLen }, tactCol);
		DrawSolidRect({ margin, screenH - margin }, { bracketLen, 2.0f }, tactCol);
		DrawSolidRect({ margin, screenH - margin - bracketLen }, { 2.0f, bracketLen }, tactCol);
		DrawSolidRect({ screenW - margin - bracketLen, screenH - margin }, { bracketLen, 2.0f }, tactCol);
		DrawSolidRect({ screenW - margin, screenH - margin - bracketLen }, { 2.0f, bracketLen }, tactCol);

		// リアルタイムGPS・衛星テレメトリ情報
		int latNoise = static_cast<int>(std::abs(std::sin(endSeqTimer_ * 14.0f)) * 99.0f);
		int lonNoise = static_cast<int>(std::abs(std::cos(endSeqTimer_ * 18.0f)) * 99.0f);
		char coordBuf[128];
		snprintf(coordBuf, sizeof(coordBuf), "SAT_ORBIT: 420.4KM | LAT: 35.68%02d N | LON: 139.76%02d E | RATE: 10.4 Gbps", latNoise, lonNoise);
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"TACTICAL MAP // SATELLITE ORBIT VIEW [GRID: 44-ALPHA]",
			margin + 10.0f,
			margin + 8.0f,
			16.0f,
			tactCol
		);
		TextRenderer::GetInstance()->Print(
			"HackGen",
			coordBuf,
			margin + 10.0f,
			margin + 28.0f,
			12.0f,
			{ 0.35f, 0.85f, 0.60f, 0.75f }
		);

		// 自機アイコン（画面中央）と円形走査レーダー波
		float pX = screenW * 0.5f;
		float pY = screenH * 0.5f;

		// レーダー走査パルス波
		float pulseCycle = std::fmod(endSeqTimer_ * 1.5f, 1.0f);
		float ringRad = 20.0f + pulseCycle * 140.0f;
		float ringAlpha = (1.0f - pulseCycle) * 0.70f;
		DrawHollowBorder({ pX - ringRad, pY - ringRad }, { ringRad * 2.0f, ringRad * 2.0f }, { 0.20f, 0.95f, 0.55f, ringAlpha }, 1.5f);

		// 自機アイコンシンボル
		DrawSolidRect({ pX - 12.0f, pY - 1.0f }, { 24.0f, 2.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
		DrawSolidRect({ pX - 1.0f, pY - 12.0f }, { 2.0f, 24.0f }, { 0.40f, 1.0f, 0.70f, 0.95f });
		DrawHollowBorder({ pX - 8.0f, pY - 8.0f }, { 16.0f, 16.0f }, { 0.40f, 1.0f, 0.70f, 0.95f }, 1.5f);

		// 自機コールサイン
		TextRenderer::GetInstance()->Print(
			"HackGen",
			"▲ PLAYER [MiG-21] (RTB)",
			pX + 16.0f,
			pY - 14.0f,
			12.5f,
			{ 0.40f, 1.0f, 0.70f, 0.90f }
		);

		// 項目10: 移行直後（0.0s〜0.26s）の鋭利な垂直急ズーム＋走査デジタルワイプ
		if (mapTimer < 0.26f) {
			float wipeProg = mapTimer / 0.26f;
			float bandH = screenH * 0.25f;
			float bandY = wipeProg * (screenH + bandH) - bandH;
			DrawSolidRect({ 0.0f, bandY }, { screenW, bandH }, { 0.25f, 1.0f, 0.60f, (1.0f - wipeProg) * 0.75f }, BlendMode::kAdd);
			DrawSolidRect({ 0.0f, bandY + bandH * 0.5f }, { screenW, 4.0f }, { 0.90f, 1.0f, 0.95f, (1.0f - wipeProg) * 0.95f }, BlendMode::kAdd);
		}

		// 3.5s に達した瞬間、SECTOR CLEARED のスタンプがドンッと押される！
		if (endSeqTimer_ >= 3.5f) {
			float stampT = std::clamp((endSeqTimer_ - 3.5f) / 0.14f, 0.0f, 1.0f);
			float stampScale = 1.0f + 1.2f * (1.0f - stampT) * (1.0f - stampT);

			float boxW = 540.0f * stampScale;
			float boxH = 130.0f * stampScale;
			float boxX = (screenW - boxW) * 0.5f;
			float boxY = (screenH - boxH) * 0.5f;

			Vector4 stampCol = { 0.95f, 0.25f, 0.18f, 0.95f };
			DrawHollowBorder({ boxX, boxY }, { boxW, boxH }, stampCol, 4.0f * stampScale);
			DrawHollowBorder({ boxX + 6.0f * stampScale, boxY + 6.0f * stampScale }, { boxW - 12.0f * stampScale, boxH - 12.0f * stampScale }, stampCol, 1.5f * stampScale);

			TextRenderer::GetInstance()->Print(
				"HackGen",
				"SECTOR CLEARED",
				screenW * 0.5f,
				boxY + 22.0f * stampScale,
				44.0f * stampScale,
				stampCol,
				{ 0.5f, 0.0f }
			);

			TextRenderer::GetInstance()->Print(
				"HackGen",
				"(セクター制圧)",
				screenW * 0.5f,
				boxY + 74.0f * stampScale,
				22.0f * stampScale,
				{ 1.0f, 0.85f, 0.80f, 0.95f },
				{ 0.5f, 0.0f }
			);
		}

		// 項目4: 戦術マップとリザルト画面（デスクトップ）の視覚的繋がり（転送ブリッジ）
		if (mapTimer >= 0.60f) {
			float transBannerW = 740.0f;
			float transBannerH = 52.0f;
			float transBannerX = (screenW - transBannerW) * 0.5f;
			float transBannerY = screenH - 100.0f;
			DrawSolidRect({ transBannerX, transBannerY }, { transBannerW, transBannerH }, { 0.02f, 0.08f, 0.16f, 0.92f });
			DrawHollowBorder({ transBannerX, transBannerY }, { transBannerW, transBannerH }, { 0.25f, 0.85f, 1.0f, 0.95f }, 1.5f);
			TextRenderer::GetInstance()->Print(
				"HackGen",
				"TRANSFERRING TELEMETRY TO TACTICAL TERMINAL // DEBRIEF READY...",
				screenW * 0.5f,
				transBannerY + 16.0f,
				18.0f,
				{ 0.40f, 0.95f, 1.0f, 1.0f },
				{ 0.5f, 0.0f }
			);
		}
	}
}
