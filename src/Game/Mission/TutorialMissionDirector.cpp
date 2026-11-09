#define NOMINMAX
#include "TutorialMissionDirector.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Input.h"
#include "../Scene/StageHUD.h"
#include "../Graphics/DebrisManager.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/Text/TextRenderer.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace {
	using namespace HalcyonDefaults;

	bool EndsWithString(const std::string& fullStr, const std::string& suffix) {
		if (suffix.size() > fullStr.size()) {
			return false;
		}
		return fullStr.compare(fullStr.size() - suffix.size(), suffix.size(), suffix) == kZeroInt;
	}

	float SmoothStep01(float t) {
		const float clamped = std::clamp(t, kZero, kUnit);
		return clamped * clamped * (kThree - kTwo * clamped);
	}

	void BuildAircraftBasis(
		const MyMath::Vector3& forwardDir,
		float rollRad,
		MyMath::Vector3& outRight,
		MyMath::Vector3& outUp,
		MyMath::Vector3& outForward) {
		const float fwdLen = MyMath::Length(forwardDir);
		if (fwdLen > kEpsilon) {
			outForward = MyMath::Multiply(kUnit / fwdLen, forwardDir);
		} else {
			outForward = { kZero, kZero, kUnit };
		}

		MyMath::Vector3 upRef = { kZero, kUnit, kZero };
		if (std::fabs(MyMath::Dot(outForward, upRef)) > kBasisParallelDotLimit) {
			upRef = { kZero, kZero, kUnit };
		}

		const MyMath::Vector3 baseRight = MyMath::Normalize(MyMath::Cross(upRef, outForward));
		const MyMath::Vector3 baseUp = MyMath::Normalize(MyMath::Cross(outForward, baseRight));

		const float cosR = std::cos(rollRad);
		const float sinR = std::sin(rollRad);

		outRight = MyMath::Add(MyMath::Multiply(cosR, baseRight), MyMath::Multiply(sinR, baseUp));
		outUp = MyMath::Add(MyMath::Multiply(-sinR, baseRight), MyMath::Multiply(cosR, baseUp));
	}

	MyMath::Matrix4x4 BuildWorldMatrixFromBasis(
		const MyMath::Vector3& right,
		const MyMath::Vector3& up,
		const MyMath::Vector3& forward,
		const MyMath::Vector3& position) {
		MyMath::Matrix4x4 mat = MyMath::Identity4x4();
		mat.m[kZeroInt][kZeroInt] = right.x;
		mat.m[kZeroInt][kOneInt] = right.y;
		mat.m[kZeroInt][kTwoInt] = right.z;
		mat.m[kZeroInt][kThreeInt] = kZero;

		mat.m[kOneInt][kZeroInt] = up.x;
		mat.m[kOneInt][kOneInt] = up.y;
		mat.m[kOneInt][kTwoInt] = up.z;
		mat.m[kOneInt][kThreeInt] = kZero;

		mat.m[kTwoInt][kZeroInt] = forward.x;
		mat.m[kTwoInt][kOneInt] = forward.y;
		mat.m[kTwoInt][kTwoInt] = forward.z;
		mat.m[kTwoInt][kThreeInt] = kZero;

		mat.m[kThreeInt][kZeroInt] = position.x;
		mat.m[kThreeInt][kOneInt] = position.y;
		mat.m[kThreeInt][kTwoInt] = position.z;
		mat.m[kThreeInt][kThreeInt] = kUnit;
		return mat;
	}

	const char* ResolveSysLogTag(SysLogCategory category) {
		switch (category) {
		case SysLogCategory::Sys:   return kTagSys;
		case SysLogCategory::Wrn:   return kTagWrn;
		case SysLogCategory::Alr:   return kTagAlr;
		case SysLogCategory::Rwr:   return kTagRwr;
		case SysLogCategory::Intel: return kTagIntel;
		default:                    return kTagSys;
		}
	}

	MyMath::Vector4 ResolveSysLogColor(SysLogCategory category) {
		switch (category) {
		case SysLogCategory::Sys:   return kSysTagColor;
		case SysLogCategory::Wrn:   return kWrnTagColor;
		case SysLogCategory::Alr:   return kAlrTagColor;
		case SysLogCategory::Rwr:   return kAlrTagColor;
		case SysLogCategory::Intel: return kIntelTagColor;
		default:                    return kSysTagColor;
		}
	}

	void PrintShadowedHudText(
		const std::string& text,
		float x,
		float y,
		float fontSize,
		const MyMath::Vector4& color,
		const Vector2& anchor = { kZero, kZero }) {
		TextRenderer* tr = TextRenderer::GetInstance();
		if (!tr || text.empty()) {
			return;
		}
		const MyMath::Vector4 shadowCol = {
			kPureBlack.x,
			kPureBlack.y,
			kPureBlack.z,
			color.w * kHudTextShadowAlpha
		};
		tr->Print(kFontName, text, x + kHudTextShadowOffset, y + kHudTextShadowOffset, fontSize, shadowCol, anchor);
		tr->Print(kFontName, text, x, y, fontSize, color, anchor);
	}
}

float TutorialMissionDirector::EvaluateCanyonCenterX(float worldZ) {
	if (worldZ <= kCanyonControlZ.front()) {
		return kCanyonControlX.front();
	}
	if (worldZ >= kCanyonControlZ.back()) {
		return kCanyonControlX.back();
	}

	for (size_t i = kZeroIndex; i + kIndexStep < kCanyonControlPointCount; ++i) {
		const float z0 = kCanyonControlZ[i];
		const float z1 = kCanyonControlZ[i + kIndexStep];
		if (worldZ >= z0 && worldZ <= z1) {
			const float span = z1 - z0;
			const float rawT = (span > kEpsilon) ? ((worldZ - z0) / span) : kZero;
			const float smoothT = SmoothStep01(rawT);
			const float x0 = kCanyonControlX[i];
			const float x1 = kCanyonControlX[i + kIndexStep];
			return x0 + (x1 - x0) * smoothT;
		}
	}
	return kCanyonControlX.back();
}

void TutorialMissionDirector::Initialize(SpriteCommon* spriteCommon) {
	spriteCommon_ = spriteCommon;
	if (!spriteCommon_) {
		return;
	}

	TextureManager::GetInstance()->LoadTexture(kWhiteTexPath);
	TextureManager::GetInstance()->LoadTexture(kRingTexPath);
	whiteTexIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(kWhiteTexPath);
	ringTexIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(kRingTexPath);

	ModelManager::GetInstance()->LoadModel(kCoraxModelPath);
	ModelManager::GetInstance()->LoadModel(kWesternFighterModelPath);

	Camera* activeCam = CameraManager::GetInstance()->GetActiveCamera();

	// 1. 飛行隊長『Corax』搭乗機 (M-21A改) のセットアップ（自機と共有するModel*の本来色を壊さないようSetMaterialColorは呼ばない）
	coraxModel_.Initialize(Object3dCommon::GetInstance(), activeCam);
	coraxModel_.SetupFromSingleModel(kCoraxModelPath);
	coraxModel_.SetMaterialProperties(kCoraxMaterialShininess, kCoraxMaterialSpecular, kCoraxMaterialEnv);
	coraxModel_.SetupCanopyMaterial();

	// 2. 西側ジェット戦闘機 (M-15 後退翼機・部位破壊対応) のセットアップ（glTF本来のマテリアル色とキャノピー色を保持）
	westernModel_.Initialize(Object3dCommon::GetInstance(), activeCam);
	westernModel_.SetupFromSingleModel(kWesternFighterModelPath);
	westernModel_.SetMaterialProperties(kWesternMaterialShininess, kWesternMaterialSpecular, kWesternMaterialEnv);
	westernModel_.SetupCanopyMaterial();

	// 3. 訓練用自動ドローン (ORANGE) のセットアップ
	// ※自機(m21.gltf)とModel*インスタンスを共有すると自機までオレンジ色に変色するため、ドローン専用のModel複製を生成して登録する
	if (!ModelManager::GetInstance()->FindModel(kDroneModelCloneKey)) {
		Model* baseM21 = ModelManager::GetInstance()->FindModel(kCoraxModelPath);
		if (baseM21 && ModelManager::GetInstance()->GetModelCommon()) {
			auto droneClone = std::make_unique<Model>();
			droneClone->InitializeFromModelData(ModelManager::GetInstance()->GetModelCommon(), baseM21->GetModelData());
			ModelManager::GetInstance()->RegisterModel(kDroneModelCloneKey, std::move(droneClone));
		}
	}
	droneObject_ = std::make_unique<Object3d>();
	droneObject_->Initialize(Object3dCommon::GetInstance());
	droneObject_->SetCamera(activeCam);
	droneObject_->SetModel(
		ModelManager::GetInstance()->FindModel(kDroneModelCloneKey) ? kDroneModelCloneKey : kCoraxModelPath);
	droneObject_->SetScale({ kDroneScale, kDroneScale, kDroneScale });
	if (ModelManager::GetInstance()->FindModel(kDroneModelCloneKey)) {
		droneObject_->SetMaterialColor(kPrimaryMaterialIndex, kDroneOrangeColor);
		droneObject_->SetMaterialEmissive(
			kPrimaryMaterialIndex,
			{ kDroneOrangeColor.x, kDroneOrangeColor.y, kDroneOrangeColor.z },
			kDroneEmissiveIntensity);
	}

	// 4. DirectX12 安全描画用2Dスプライトプール生成
	rectSpritePool_.clear();
	rectSpritePool_.reserve(kInitialRectPoolCapacity);
	for (size_t i = kZeroIndex; i < kInitialRectPoolCapacity; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_, kWhiteTexPath);
		rectSpritePool_.push_back(std::move(sp));
	}
	rectSpritePoolIndex_ = kZeroIndex;

	ringSpritePool_.clear();
	ringSpritePool_.reserve(kInitialRingPoolCapacity);
	for (size_t i = kZeroIndex; i < kInitialRingPoolCapacity; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_, kRingTexPath);
		ringSpritePool_.push_back(std::move(sp));
	}
	ringSpritePoolIndex_ = kZeroIndex;

	isInitialized_ = true;
}

void TutorialMissionDirector::ResetForMission(
	const MissionData& mission,
	const std::string& missionFilePath,
	const MyMath::Vector3& playerStartPos) {
	sysLogs_.clear();
	readyForClearScene_ = false;
	missionClock_ = kZero;
	phaseTimer_ = kZero;
	animTimer_ = kZero;
	currentPlayerThrottle_ = kZero;
	hitMarkerTimer_ = kZero;
	fadeOutAlpha_ = kZero;
	isPullUpWarning_ = false;
	isHighAltitudeCaution_ = false;

	const bool matchByName =
		(mission.name == kMissionTitle) ||
		(mission.name == kObuchenieTitle) ||
		(mission.aliasName == kZadanie00Alias);
	const bool matchByFile = EndsWithString(missionFilePath, kMissionFileSuffix);
	isActive_ = (matchByName || matchByFile);

	if (!isActive_) {
		phase_ = HalcyonPhase::Inactive;
		return;
	}

	layout_ = mission.tutorialLayout;
	customGatePositions_.clear();
	for (const auto& wpPath : mission.waypointPaths) {
		if (!wpPath.points.empty()) {
			customGatePositions_.reserve(wpPath.points.size());
			for (const auto& pt : wpPath.points) {
				customGatePositions_.push_back(pt.position);
			}
			break;
		}
	}

	phase_ = HalcyonPhase::Phase1_BasicControls;

	// Phase 1 状態初期化
	p1Log0Sent_ = false;
	p1Log1Sent_ = false;
	p1Log2Sent_ = false;
	p1Radio1Sent_ = false;
	p1Radio2Sent_ = false;
	p1Radio3Sent_ = false;
	hasPrevAimDir_ = false;
	prevAimDir_ = { kZero, kZero, kUnit };
	p1MoveAccumSeconds_ = kZero;
	p1CompletedWaitingDelay_ = false;
	p1PostClearTimer_ = kZero;

	// Phase 2 状態初期化
	p2Started_ = false;
	p2ThrottleHoldSeconds_ = kZero;

	// Phase 3 状態初期化
	const size_t initialGateCount = customGatePositions_.empty() ? kGateCount : customGatePositions_.size();
	gates_.assign(initialGateCount, CanyonGate{});
	passedGateCount_ = kZeroInt;
	p3CompletedWaitingDelay_ = false;
	p3PostClearTimer_ = kZero;

	// 峡谷監視所群 ＆ Phase 4 状態初期化（監視塔はミッション開始時から全基配置、爆発炎上はPhase 4で発生）
	outpostVisible_ = true;
	outpostIgnited_ = false;
	watchtowersGrounded_ = false;
	p4Wrn2Sent_ = false;
	if (EffectManager::GetInstance()) {
		outpostPlumeEffect_ = EffectManager::GetInstance()->GetHandle(kOutpostPlumeEffectName, kOutpostPlumeEffectName);
		damageSmokeEffect_ = EffectManager::GetInstance()->GetHandle(kDamageSmokeEffectName, kDamageSmokeEffectName);
		crashPlumeEffect_ = EffectManager::GetInstance()->GetHandle(kCrashPlumeEffectName, kCrashPlumeEffectName);
		if (outpostPlumeEffect_) { outpostPlumeEffect_->stop(); }
		if (damageSmokeEffect_) { damageSmokeEffect_->stop(); }
		if (crashPlumeEffect_) { crashPlumeEffect_->stop(); }
	}
	InitializeWatchtowers(playerStartPos, nullptr);

	// Phase 5 状態初期化
	droneAlive_ = false;
	p5Log2Sent_ = false;
	dronePos_ = {};
	droneVel_ = {};
	droneHp_ = kDroneMaxHp;
	droneCruiseZ_ = kZero;
	p5CompletedWaitingDelay_ = false;
	p5PostKillTimer_ = kZero;

	// Phase 6 状態初期化
	westernModel_.ResetAllPartsVisible();
	westernModel_.SetupCanopyMaterial();
	westernVisible_ = false;
	westernInDivePass_ = false;
	westernDownedSpiral_ = false;
	westernCrashedFloor_ = false;
	p6LogRwrSent_ = false;
	p6LogSpecSent_ = false;
	p6LogIntelSent_ = false;
	p6EngageRadioSent_ = false;
	westernPos_ = {};
	westernVel_ = {};
	westernCruiseZ_ = kZero;
	westernStrafeTimer_ = kZero;
	portWingHp_ = kWesternWingPartMaxHp;
	stbdWingHp_ = kWesternWingPartMaxHp;
	engineCoreHp_ = kWesternEnginePartMaxHp;
	brokenPart_ = BrokenPartType::None;

	// Phase 7 状態初期化
	exitBoundaryVisible_ = false;
	exitBoundaryPos_ = {};

	// 隊長機『Corax』を自機の斜め前方またはBlender配置座標に配置
	coraxModel_.ResetAllPartsVisible();
	if (layout_.useCustomLayout) {
		coraxPos_ = layout_.coraxStartPos;
	} else {
		coraxPos_ = {
			playerStartPos.x + kCoraxLateralOffsetX,
			playerStartPos.y + kGateTickLength,
			playerStartPos.z + kCoraxLeadDistanceZ
		};
	}
	coraxForward_ = { kZero, kZero, kUnit };
	coraxVel_ = { kZero, kZero, kCoraxMinSpeed };
	coraxRollRad_ = kZero;
	coraxFireTimer_ = kZero;

	cachedPlayerPos_ = playerStartPos;
	cachedPlayerVel_ = { kZero, kZero, kCoraxMinSpeed };

	MyMath::Vector3 right{}, up{}, fwd{};
	BuildAircraftBasis(coraxForward_, coraxRollRad_, right, up, fwd);
	coraxModel_.Update(BuildWorldMatrixFromBasis(right, up, fwd, coraxPos_), kZero);
}

void TutorialMissionDirector::ApplyLiveLayout(
	const MissionData& mission,
	const TerrainChunkManager* terrain) {
	if (!isActive_) {
		return;
	}
	layout_ = mission.tutorialLayout;
	customGatePositions_.clear();
	for (const auto& wpPath : mission.waypointPaths) {
		if (!wpPath.points.empty()) {
			customGatePositions_.reserve(wpPath.points.size());
			for (const auto& pt : wpPath.points) {
				customGatePositions_.push_back(pt.position);
			}
			break;
		}
	}

	if (phase_ == HalcyonPhase::Phase3_CanyonWaypoints && !customGatePositions_.empty()) {
		gates_.resize(customGatePositions_.size());
		for (size_t i = kZeroIndex; i < customGatePositions_.size(); ++i) {
			gates_[i].position = customGatePositions_[i];
			gates_[i].isPassed = (static_cast<int>(i) < passedGateCount_);
		}
		if (passedGateCount_ > static_cast<int>(gates_.size())) {
			passedGateCount_ = static_cast<int>(gates_.size());
		}
	}

	if (layout_.useCustomLayout) {
		if (missionClock_ <= kEpsilon) {
			coraxPos_ = layout_.coraxStartPos;
		}
		if (exitBoundaryVisible_) {
			exitBoundaryPos_ = layout_.exitBoundaryPosition;
		}
	}
	InitializeWatchtowers(cachedPlayerPos_, terrain);
}

void TutorialMissionDirector::InitializeWatchtowers(
	const MyMath::Vector3& playerStartPos,
	const TerrainChunkManager* terrain) {
	outpostPositions_.clear();
	if (layout_.useCustomLayout) {
		if (!layout_.outpostPositions.empty()) {
			outpostPositions_ = layout_.outpostPositions;
		} else {
			outpostPositions_ = {
				MissionSchemaDefaults::kDefaultOutpost01Pos,
				MissionSchemaDefaults::kDefaultOutpost02Pos,
				MissionSchemaDefaults::kDefaultOutpost03Pos,
				layout_.outpostPosition
			};
		}
		outpostPos_ = layout_.outpostPosition;
		if (!outpostPositions_.empty()) {
			outpostPos_ = outpostPositions_.back();
		}
	} else {
		outpostPositions_.reserve(kDefaultWatchtowerCount);
		for (size_t i = kZeroIndex; i < kDefaultWatchtowerCount; ++i) {
			const float wz = playerStartPos.z + kWatchtowerFirstOffsetZ + static_cast<float>(i) * kWatchtowerSpacingZ;
			const float cx = EvaluateCanyonCenterX(wz);
			const float bankSign = ((i % kTwoInt) == kZeroIndex) ? kUnit : -kUnit;
			const float wx = cx + bankSign * kOutpostShelfOffsetX;
			const float wy = QueryTerrainY(terrain, wx, wz);
			outpostPositions_.push_back({ wx, wy, wz });
		}
		outpostPos_ = outpostPositions_.back();
	}

	watchtowersGrounded_ = false;
	GroundWatchtowersToTerrain(terrain);
}

void TutorialMissionDirector::GroundWatchtowersToTerrain(const TerrainChunkManager* terrain) {
	if (!terrain || !terrain->IsLoaded()) {
		return;
	}
	for (auto& pos : outpostPositions_) {
		pos.y = QueryTerrainY(terrain, pos.x, pos.z);
	}
	outpostPos_.y = QueryTerrainY(terrain, outpostPos_.x, outpostPos_.z);
	if (!outpostPositions_.empty()) {
		outpostPositions_.back() = outpostPos_;
	}
	watchtowersGrounded_ = true;
}

float TutorialMissionDirector::QueryTerrainY(
	const TerrainChunkManager* terrain,
	float worldX,
	float worldZ) const {
	if (terrain && terrain->IsLoaded()) {
		return terrain->GetTerrainHeightAt(worldX, worldZ, kFallbackCanyonFloorY);
	}
	return kFallbackCanyonFloorY;
}

void TutorialMissionDirector::PushSystemLog(
	SysLogCategory category,
	const std::string& message,
	bool includeTimestamp) {
	SysLogEntry entry{};
	entry.category = category;
	entry.message = message;
	entry.ageSeconds = kZero;

	if (includeTimestamp) {
		const int totalSec = static_cast<int>(std::max(kZero, missionClock_));
		const int mins = totalSec / kSecondsPerMinute;
		const int secs = totalSec % kSecondsPerMinute;
		char tsBuf[kTimestampBufferSize]{};
		std::snprintf(tsBuf, sizeof(tsBuf), "[%02d:%02d] ", mins, secs);
		entry.timestampText = tsBuf;
	}

	sysLogs_.push_back(std::move(entry));
	while (sysLogs_.size() > kMaxVisibleSysLogs) {
		sysLogs_.pop_front();
	}
}

MyMath::Vector3 TutorialMissionDirector::ComputeLeadInterceptPoint(
	const MyMath::Vector3& shooterPos,
	const MyMath::Vector3& shooterVel,
	const MyMath::Vector3& targetPos,
	const MyMath::Vector3& targetVel) {
	// 自機銃弾は bulletVel = shooterVel + muzzleSpeed * fireDir で発射されるため、
	// 相対運動方程式 |(targetPos - muzzlePos) + (targetVel - shooterVel) * t| = muzzleSpeed * t を厳密に解く
	MyMath::Vector3 shooterFwd = { kZero, kZero, kUnit };
	const float shooterSpeed = MyMath::Length(shooterVel);
	if (shooterSpeed > kEpsilon) {
		shooterFwd = MyMath::Multiply(kUnit / shooterSpeed, shooterVel);
	}
	const MyMath::Vector3 muzzlePos = MyMath::Add(shooterPos, MyMath::Multiply(kGunMuzzleOffsetForward, shooterFwd));
	const MyMath::Vector3 relPos = MyMath::Subtract(targetPos, muzzlePos);
	const MyMath::Vector3 relVel = MyMath::Subtract(targetVel, shooterVel);

	const float a = MyMath::Dot(relVel, relVel) - (kGunBulletMuzzleSpeed * kGunBulletMuzzleSpeed);
	const float b = kTwo * MyMath::Dot(relPos, relVel);
	const float c = MyMath::Dot(relPos, relPos);

	float interceptTime = kZero;
	if (std::fabs(a) < kEpsilon) {
		if (std::fabs(b) > kEpsilon) {
			interceptTime = -c / b;
		}
	} else {
		const float discriminant = (b * b) - (static_cast<float>(kFourInt) * a * c);
		if (discriminant >= kZero) {
			const float sqrtDisc = std::sqrt(discriminant);
			const float t1 = (-b - sqrtDisc) / (kTwo * a);
			const float t2 = (-b + sqrtDisc) / (kTwo * a);
			if (t1 > kEpsilon && t2 > kEpsilon) {
				interceptTime = std::min(t1, t2);
			} else if (t1 > kEpsilon) {
				interceptTime = t1;
			} else if (t2 > kEpsilon) {
				interceptTime = t2;
			}
		}
	}

	if (interceptTime <= kEpsilon) {
		const float dist = MyMath::Length(relPos);
		interceptTime = dist / std::max(kMinClosingBulletSpeed, kGunBulletMuzzleSpeed);
	}
	interceptTime = std::clamp(interceptTime, kZero, kMaxLeadInterceptTime);

	// リードマーカーは「この位置に機首レティクルを向ければ命中する空間点」を表す
	// bulletPos(t) = muzzlePos + shooterVel*t + muzzleSpeed*fireDir*t == targetPos + targetVel*t
	// よって muzzlePos + muzzleSpeed*fireDir*t = targetPos + (targetVel - shooterVel)*t
	return MyMath::Add(targetPos, MyMath::Multiply(interceptTime, relVel));
}

void TutorialMissionDirector::Update(const TutorialUpdateContext& ctx) {
	if (!isActive_ || !isInitialized_ || phase_ == HalcyonPhase::Inactive) {
		return;
	}

	const float dt = std::max(kZero, ctx.deltaTime);
	animTimer_ += dt;
	currentPlayerThrottle_ = ctx.playerThrottle;

	if (!watchtowersGrounded_ && ctx.terrain && ctx.terrain->IsLoaded()) {
		GroundWatchtowersToTerrain(ctx.terrain);
	}

	if (ctx.playerFlight) {
		cachedPlayerPos_ = ctx.playerFlight->GetPosition();
		cachedPlayerVel_ = ctx.playerFlight->GetVelocity();
	}

	// 出撃ブート演出中も隊長機『Corax』が前方を飛行しているよう3D姿勢を更新
	if (!ctx.isGameplayActive && phase_ != HalcyonPhase::Phase8_DebriefFadeOut) {
		coraxPos_ = {
			cachedPlayerPos_.x + kCoraxLateralOffsetX,
			cachedPlayerPos_.y + kGateTickLength,
			cachedPlayerPos_.z + kCoraxLeadDistanceZ
		};
		MyMath::Vector3 right{}, up{}, fwd{};
		BuildAircraftBasis(coraxForward_, coraxRollRad_, right, up, fwd);
		coraxModel_.Update(BuildWorldMatrixFromBasis(right, up, fwd, coraxPos_), dt);
		return;
	}

	missionClock_ += dt;
	phaseTimer_ += dt;

	if (hitMarkerTimer_ > kZero) {
		hitMarkerTimer_ = std::max(kZero, hitMarkerTimer_ - dt);
	}

	// チュートリアル進行中に弾切れで詰まないよう残弾が閾値を下回ったら自動補給
	if (ctx.playerGunpod && ctx.playerGunpod->GetCurrentAmmo() < kMinTutorialAmmoReserve) {
		ctx.playerGunpod->ReplenishAmmo();
	}

	// 対地高度（AGL）警告＆高高度逸脱注意の判定（Phase 3以降で有効化）
	const float groundY = QueryTerrainY(ctx.terrain, cachedPlayerPos_.x, cachedPlayerPos_.z);
	const float currentAgl = cachedPlayerPos_.y - groundY;
	const bool enableTerrainAlerts =
		(phase_ != HalcyonPhase::Phase1_BasicControls &&
		 phase_ != HalcyonPhase::Phase2_ThrottleCruise &&
		 phase_ != HalcyonPhase::Phase8_DebriefFadeOut);
	isPullUpWarning_ = enableTerrainAlerts && (currentAgl < kPullUpWarningAglMeters);
	isHighAltitudeCaution_ =
		(phase_ == HalcyonPhase::Phase3_CanyonWaypoints) &&
		!isPullUpWarning_ &&
		(cachedPlayerPos_.y > kHighAltitudeCautionY);

	// 飛行隊長『Corax』の誘導飛行更新
	UpdateCoraxWingman(ctx);

	// 監視塔の黒煙・炎パーティクル継続放出
	UpdateOutpostEffects(dt);

	// フェーズ別進行ロジック
	switch (phase_) {
	case HalcyonPhase::Phase1_BasicControls:
		UpdatePhase1(ctx);
		break;
	case HalcyonPhase::Phase2_ThrottleCruise:
		UpdatePhase2(ctx);
		break;
	case HalcyonPhase::Phase3_CanyonWaypoints:
		UpdatePhase3(ctx);
		break;
	case HalcyonPhase::Phase4_OutpostIncident:
		UpdatePhase4(ctx);
		break;
	case HalcyonPhase::Phase5_DroneLeadSight:
		UpdatePhase5(ctx);
		break;
	case HalcyonPhase::Phase6_WesternAmbush:
		UpdatePhase6(ctx);
		break;
	case HalcyonPhase::Phase7_ExitBoundary:
		UpdatePhase7Exit(ctx);
		break;
	case HalcyonPhase::Phase8_DebriefFadeOut:
		UpdatePhase8FadeOut(ctx);
		break;
	default:
		break;
	}

	// システムログの寿命更新
	for (auto& log : sysLogs_) {
		log.ageSeconds += dt;
	}
	while (!sysLogs_.empty() && sysLogs_.front().ageSeconds >= kSysLogLifetimeSeconds) {
		sysLogs_.pop_front();
	}
}

void TutorialMissionDirector::UpdateCoraxWingman(const TutorialUpdateContext& ctx) {
	const float dt = std::max(kZero, ctx.deltaTime);
	if (dt <= kEpsilon) {
		return;
	}

	const float playerSpeed = std::clamp(MyMath::Length(cachedPlayerVel_), kCoraxMinSpeed, kCoraxMaxSpeed);
	MyMath::Vector3 desiredPos{};

	const bool inActiveDogfight =
		(phase_ == HalcyonPhase::Phase6_WesternAmbush &&
		 westernVisible_ &&
		 !westernInDivePass_ &&
		 !westernDownedSpiral_);

	if (inActiveDogfight) {
		// Phase 6 ドッグファイト中は西側機を追尾しながら牽制射撃を行う
		desiredPos = {
			westernPos_.x + kCoraxDogfightOffsetX * std::cos(animTimer_ * kGateSpinSpeedRad),
			westernPos_.y + kCoraxDogfightOffsetY,
			westernPos_.z - kCoraxDogfightBehindTargetZ
		};
	} else {
		const float targetZ = cachedPlayerPos_.z + kCoraxLeadDistanceZ;
		const float canyonCenterX = EvaluateCanyonCenterX(targetZ);
		const float desiredX = canyonCenterX + kCoraxLateralOffsetX;
		const float floorY = QueryTerrainY(ctx.terrain, desiredX, targetZ);
		desiredPos = {
			desiredX,
			floorY + kCoraxPatrolAglMeters,
			targetZ
		};
	}

	const MyMath::Vector3 toTarget = MyMath::Subtract(desiredPos, coraxPos_);
	MyMath::Vector3 desiredVel = MyMath::Multiply(kCoraxFollowStiffness, toTarget);
	desiredVel.z = std::max(desiredVel.z, playerSpeed);
	const float desiredSpeed = MyMath::Length(desiredVel);
	if (desiredSpeed > kCoraxMaxSpeed) {
		desiredVel = MyMath::Multiply(kCoraxMaxSpeed / desiredSpeed, desiredVel);
	}

	const float velBlend = std::clamp(kCoraxPosSmoothRate * dt, kZero, kUnit);
	coraxVel_ = MyMath::Lerp(coraxVel_, desiredVel, velBlend);
	coraxPos_ = MyMath::Add(coraxPos_, MyMath::Multiply(dt, coraxVel_));

	// 地形貫通防止
	const float coraxGroundY = QueryTerrainY(ctx.terrain, coraxPos_.x, coraxPos_.z);
	const float minSafeY = coraxGroundY + kPullUpWarningAglMeters;
	if (coraxPos_.y < minSafeY) {
		coraxPos_.y = minSafeY;
	}

	const float speedNow = MyMath::Length(coraxVel_);
	if (speedNow > kEpsilon) {
		const MyMath::Vector3 nextFwd = MyMath::Multiply(kUnit / speedNow, coraxVel_);
		const float lateralTurn = nextFwd.x - coraxForward_.x;
		const float targetRoll = std::clamp(-lateralTurn * (kCoraxBankGain / std::max(dt, kEpsilon)) * dt, -kCoraxMaxBankRad, kCoraxMaxBankRad);
		coraxRollRad_ = coraxRollRad_ + (targetRoll - coraxRollRad_) * std::clamp(kCoraxRollSmoothRate * dt, kZero, kUnit);
		coraxForward_ = nextFwd;
	}

	// ドッグファイト中のCorax牽制射撃演出（敵機の部位HPは削らずプレイヤーに撃墜体験を譲る）
	if (inActiveDogfight && ctx.bulletManager) {
		const float cyclePos = std::fmod(phaseTimer_, kCoraxDogfightBurstCycle);
		coraxFireTimer_ -= dt;
		if (cyclePos <= kCoraxDogfightBurstWindow && coraxFireTimer_ <= kZero) {
			coraxFireTimer_ = kCoraxDogfightFireInterval;
			const MyMath::Vector3 aimDir = MyMath::Normalize(MyMath::Subtract(westernPos_, coraxPos_));
			const MyMath::Vector3 muzzlePos = MyMath::Add(coraxPos_, MyMath::Multiply(kGunMuzzleOffsetForward, aimDir));
			const MyMath::Vector3 tracerVel = MyMath::Multiply(kCoraxTracerSpeed, aimDir);
			ctx.bulletManager->SpawnBullet(muzzlePos, tracerVel, kZero, false);
		}
	}

	Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
	if (cam) {
		coraxModel_.SetCamera(cam);
	}
	MyMath::Vector3 right{}, up{}, fwd{};
	BuildAircraftBasis(coraxForward_, coraxRollRad_, right, up, fwd);
	coraxModel_.Update(BuildWorldMatrixFromBasis(right, up, fwd, coraxPos_), dt);
}

void TutorialMissionDirector::UpdatePhase1(const TutorialUpdateContext& ctx) {
	const float dt = std::max(kZero, ctx.deltaTime);

	// タイムライン準拠のDAWN_OSシステムログ出力
	if (!p1Log0Sent_ && missionClock_ >= kPhase1SysLog0Time) {
		p1Log0Sent_ = true;
		PushSystemLog(SysLogCategory::Sys, kSysLogInit, true);
	}
	if (!p1Log1Sent_ && missionClock_ >= kPhase1SysLog1Time) {
		p1Log1Sent_ = true;
		PushSystemLog(SysLogCategory::Sys, kSysLogUnit, true);
	}
	if (!p1Log2Sent_ && missionClock_ >= kPhase1SysLog2Time) {
		p1Log2Sent_ = true;
		PushSystemLog(SysLogCategory::Sys, kSysLogInstructor, true);
	}

	// タイムライン準拠のCorax無線交信
	if (ctx.radio) {
		if (!p1Radio1Sent_ && missionClock_ >= kPhase1Radio1Time) {
			p1Radio1Sent_ = true;
			ctx.radio->Enqueue(kCoraxSpeakerName, kPhase1Radio1Text, RadioFaction::Ally, kPhase1Radio1Hold);
		}
		if (!p1Radio2Sent_ && missionClock_ >= kPhase1Radio2Time) {
			p1Radio2Sent_ = true;
			ctx.radio->Enqueue(kCoraxSpeakerName, kPhase1Radio2Text, RadioFaction::Ally, kPhase1Radio2Hold);
		}
		if (!p1Radio3Sent_ && missionClock_ >= kPhase1Radio3Time) {
			p1Radio3Sent_ = true;
			ctx.radio->Enqueue(kCoraxSpeakerName, kPhase1Radio3Text, RadioFaction::Ally, kPhase1Radio3Hold);
		}
	}

	if (p1CompletedWaitingDelay_) {
		p1PostClearTimer_ += dt;
		if (p1PostClearTimer_ >= kPhase2StartDelaySeconds) {
			phase_ = HalcyonPhase::Phase2_ThrottleCruise;
			phaseTimer_ = kZero;
			p2Started_ = false;
			p2ThrottleHoldSeconds_ = kZero;
		}
		return;
	}

	// Coraxが動翼点検を指示した時点（00:11）からマウス視線・姿勢変化を累計検知
	if (missionClock_ >= kPhase1Radio3Time) {
		bool movedThisFrame = false;

		if (ctx.mouseAim) {
			const MyMath::Vector3 curAim = ctx.mouseAim->GetTargetDirection();
			if (hasPrevAimDir_) {
				const float aimDelta = MyMath::Length(MyMath::Subtract(curAim, prevAimDir_));
				if (aimDelta >= kAimDirectionDeltaThreshold) {
					movedThisFrame = true;
				}
			}
			prevAimDir_ = curAim;
			hasPrevAimDir_ = true;
		}

		Input* input = Input::GetInstance();
		if (input) {
			const Input::MouseMove mm = input->GetMouseMove();
			if (std::labs(mm.lX) >= kMousePixelDeltaThreshold ||
			    std::labs(mm.lY) >= kMousePixelDeltaThreshold) {
				movedThisFrame = true;
			}
		}

		if (movedThisFrame) {
			p1MoveAccumSeconds_ = std::min(kPhase1RequiredMoveSeconds, p1MoveAccumSeconds_ + dt);
		}

		// 00:13のUIチュートリアル表示以降かつ累計1.5秒達成でクリア進行
		if (missionClock_ >= kPhase1UiAppearTime && p1MoveAccumSeconds_ >= kPhase1RequiredMoveSeconds) {
			p1CompletedWaitingDelay_ = true;
			p1PostClearTimer_ = kZero;
		}
	}
}

void TutorialMissionDirector::UpdatePhase2(const TutorialUpdateContext& ctx) {
	const float dt = std::max(kZero, ctx.deltaTime);

	if (!p2Started_) {
		p2Started_ = true;
		if (ctx.radio) {
			ctx.radio->Enqueue(kCoraxSpeakerName, kPhase2RadioText, RadioFaction::Ally, kPhase2RadioHold);
		}
	}

	if (ctx.playerThrottle >= kPhase2ThrottleThreshold) {
		p2ThrottleHoldSeconds_ += dt;
	} else {
		p2ThrottleHoldSeconds_ = kZero;
	}

	if (p2ThrottleHoldSeconds_ >= kPhase2RequiredHoldSeconds) {
		PushSystemLog(SysLogCategory::Sys, kPhase2SysLogComplete, false);
		SpawnPhase3Gates(cachedPlayerPos_, ctx.terrain);
		if (ctx.radio) {
			ctx.radio->Enqueue(kCoraxSpeakerName, kPhase3Radio1Text, RadioFaction::Ally, kPhase3Radio1Hold);
			ctx.radio->Enqueue(kCoraxSpeakerName, kPhase3Radio2Text, RadioFaction::Ally, kPhase3Radio2Hold);
		}
		phase_ = HalcyonPhase::Phase3_CanyonWaypoints;
		phaseTimer_ = kZero;
	}
}

void TutorialMissionDirector::SpawnPhase3Gates(
	const MyMath::Vector3& playerPos,
	const TerrainChunkManager* terrain) {
	if (!customGatePositions_.empty()) {
		const float firstAuthoredZ = customGatePositions_.front().z;
		const float zShift = (firstAuthoredZ < (playerPos.z + kMinForwardSafetyOffsetZ))
			? ((playerPos.z + kFirstGateDistanceAheadZ) - firstAuthoredZ)
			: kZero;

		gates_.assign(customGatePositions_.size(), CanyonGate{});
		for (size_t i = kZeroIndex; i < customGatePositions_.size(); ++i) {
			const MyMath::Vector3& src = customGatePositions_[i];
			const float gz = src.z + zShift;
			const float minSafeY = QueryTerrainY(terrain, src.x, gz) + kPullUpWarningAglMeters;
			gates_[i].position = { src.x, std::max(src.y, minSafeY), gz };
			gates_[i].isPassed = false;
			gates_[i].passFlashTimer = kZero;
		}
	} else {
		const std::array<float, kGateCount> aglOffsets = {
			kGateAglHeight0,
			kGateAglHeight1,
			kGateAglHeight2
		};

		gates_.assign(kGateCount, CanyonGate{});
		for (size_t i = kZeroIndex; i < kGateCount; ++i) {
			const float gateZ = playerPos.z + kFirstGateDistanceAheadZ + static_cast<float>(i) * kSubsequentGateSpacingZ;
			const float gateX = EvaluateCanyonCenterX(gateZ);
			const float floorY = QueryTerrainY(terrain, gateX, gateZ);

			gates_[i].position = { gateX, floorY + aglOffsets[i], gateZ };
			gates_[i].isPassed = false;
			gates_[i].passFlashTimer = kZero;
		}
	}
	passedGateCount_ = kZeroInt;
	p3CompletedWaitingDelay_ = false;
	p3PostClearTimer_ = kZero;
}

void TutorialMissionDirector::UpdatePhase3(const TutorialUpdateContext& ctx) {
	const float dt = std::max(kZero, ctx.deltaTime);

	for (auto& gate : gates_) {
		if (gate.passFlashTimer > kZero) {
			gate.passFlashTimer = std::max(kZero, gate.passFlashTimer - dt);
		}
	}

	if (p3CompletedWaitingDelay_) {
		p3PostClearTimer_ += dt;
		if (p3PostClearTimer_ >= kPhase4StartDelaySeconds) {
			// Phase 4: 異変（通信途絶と監視所炎上）開始
			SpawnOutpost04(cachedPlayerPos_, ctx.terrain);
			PushSystemLog(SysLogCategory::Wrn, kPhase4WrnLog, false);
			p4Wrn2Sent_ = false;

			if (ctx.radio) {
				ctx.radio->Interrupt(kOutpostSpeakerName, kPhase4OutpostRadioText, RadioFaction::System, kPhase4OutpostRadioHold);
				ctx.radio->Enqueue(kCoraxSpeakerName, kPhase4CoraxRadio1Text, RadioFaction::Ally, kPhase4CoraxRadio1Hold);
				ctx.radio->Enqueue(kCoraxSpeakerName, kPhase4CoraxRadio2Text, RadioFaction::Ally, kPhase4CoraxRadio2Hold);
			}
			phase_ = HalcyonPhase::Phase4_OutpostIncident;
			phaseTimer_ = kZero;
		}
		return;
	}

	if (passedGateCount_ >= static_cast<int>(gates_.size())) {
		return;
	}

	CanyonGate& currentGate = gates_[static_cast<size_t>(passedGateCount_)];
	const MyMath::Vector3 diff = MyMath::Subtract(cachedPlayerPos_, currentGate.position);
	const float distToCenter = MyMath::Length(diff);

	const bool insideSphere = (distToCenter <= kGateCaptureSphereRadius);
	const bool crossedGatePlane =
		(cachedPlayerPos_.z >= currentGate.position.z) &&
		(std::fabs(diff.x) <= kGatePlaneCorridorX) &&
		(std::fabs(diff.y) <= kGatePlaneCorridorY);

	if (insideSphere || crossedGatePlane) {
		currentGate.isPassed = true;
		currentGate.passFlashTimer = kGatePassFlashDuration;
		passedGateCount_ += kOneInt;

		if (passedGateCount_ >= static_cast<int>(gates_.size())) {
			p3CompletedWaitingDelay_ = true;
			p3PostClearTimer_ = kZero;
			if (ctx.radio) {
				ctx.radio->Enqueue(kCoraxSpeakerName, kPhase3ClearRadioText, RadioFaction::Ally, kPhase3ClearRadioHold);
			}
		}
	}
}

void TutorialMissionDirector::SpawnOutpost04(
	const MyMath::Vector3& playerPos,
	const TerrainChunkManager* terrain) {
	if (outpostPositions_.empty()) {
		InitializeWatchtowers(playerPos, terrain);
	}

	if (layout_.useCustomLayout) {
		const MyMath::Vector3 authoredTarget = !outpostPositions_.empty()
			? outpostPositions_.back()
			: layout_.outpostPosition;
		const float targetZ = (authoredTarget.z >= (playerPos.z + kMinForwardSafetyOffsetZ))
			? authoredTarget.z
			: (playerPos.z + kOutpostSpawnAheadZ);
		const float targetX = authoredTarget.x;
		const float groundY = QueryTerrainY(terrain, targetX, targetZ);
		outpostPos_ = { targetX, std::max(authoredTarget.y, groundY), targetZ };
	} else {
		const float outpostZ = playerPos.z + kOutpostSpawnAheadZ;
		const float centerX = EvaluateCanyonCenterX(outpostZ);
		const float outpostX = centerX + kOutpostShelfOffsetX;
		const float groundY = QueryTerrainY(terrain, outpostX, outpostZ);
		outpostPos_ = { outpostX, groundY, outpostZ };
	}

	if (!outpostPositions_.empty()) {
		outpostPositions_.back() = outpostPos_;
	} else {
		outpostPositions_.push_back(outpostPos_);
	}

	outpostVisible_ = true;
	outpostIgnited_ = true;

	// 監視塔被弾跡の初期爆炎エフェクト
	const MyMath::Vector3 towerTop = {
		outpostPos_.x,
		outpostPos_.y + kOutpostTowerHeight,
		outpostPos_.z
	};
	if (EffectManager::GetInstance()) {
		EffectManager::GetInstance()->EmitDestroyEffect(towerTop);
	}
}

void TutorialMissionDirector::UpdateOutpostEffects(float dt) {
	(void)dt;
	if (outpostVisible_ && outpostIgnited_ && outpostPlumeEffect_) {
		const MyMath::Vector3 fireOrigin = {
			outpostPos_.x,
			outpostPos_.y + kOutpostTowerHeight,
			outpostPos_.z
		};
		outpostPlumeEffect_->emit(fireOrigin);
	}
}

void TutorialMissionDirector::UpdatePhase4(const TutorialUpdateContext& ctx) {
	if (!p4Wrn2Sent_ && phaseTimer_ >= kPhase4WarnLog2Delay) {
		p4Wrn2Sent_ = true;
		PushSystemLog(SysLogCategory::Alr, kPhase4AlrLog, false);
	}

	// 炎上する監視塔の角を通過した瞬間、Phase 5（残存ドローン偏差射撃）へ移行
	const bool passedWatchtowerCorner =
		(cachedPlayerPos_.z >= (outpostPos_.z + kOutpostCornerTriggerOffsetZ)) &&
		(phaseTimer_ >= kOutpostMinPhaseDuration);

	if (passedWatchtowerCorner) {
		SpawnTrainingDrone(cachedPlayerPos_, ctx.terrain);
		PushSystemLog(SysLogCategory::Sys, kPhase5SysLog1, false);
		p5Log2Sent_ = false;

		if (ctx.radio) {
			ctx.radio->Enqueue(kCoraxSpeakerName, kPhase5Radio1Text, RadioFaction::Ally, kPhase5Radio1Hold);
			ctx.radio->Enqueue(kCoraxSpeakerName, kPhase5Radio2Text, RadioFaction::Ally, kPhase5Radio2Hold);
		}
		phase_ = HalcyonPhase::Phase5_DroneLeadSight;
		phaseTimer_ = kZero;
	}
}

void TutorialMissionDirector::SpawnTrainingDrone(
	const MyMath::Vector3& playerPos,
	const TerrainChunkManager* terrain) {
	if (layout_.useCustomLayout) {
		droneCruiseZ_ = (layout_.droneSpawnPosition.z >= (playerPos.z + kMinForwardSafetyOffsetZ))
			? layout_.droneSpawnPosition.z
			: (playerPos.z + kDroneSpawnAheadZ);
		const float floorY = QueryTerrainY(terrain, layout_.droneSpawnPosition.x, droneCruiseZ_);
		dronePos_ = {
			layout_.droneSpawnPosition.x,
			std::max(layout_.droneSpawnPosition.y, floorY + kPullUpWarningAglMeters),
			droneCruiseZ_
		};
	} else {
		droneCruiseZ_ = playerPos.z + kDroneSpawnAheadZ;
		const float centerX = EvaluateCanyonCenterX(droneCruiseZ_);
		const float floorY = QueryTerrainY(terrain, centerX, droneCruiseZ_);
		dronePos_ = { centerX, floorY + kDroneBaseAglMeters, droneCruiseZ_ };
	}
	droneVel_ = { kZero, kZero, kDroneMinSpeed };
	droneHp_ = kDroneMaxHp;
	droneAlive_ = true;
	p5CompletedWaitingDelay_ = false;
	p5PostKillTimer_ = kZero;

	if (droneObject_) {
		droneObject_->SetTranslate(dronePos_);
		droneObject_->SetRotate({ kZero, kPi, kZero });
		droneObject_->Update();
	}
}

void TutorialMissionDirector::UpdatePhase5(const TutorialUpdateContext& ctx) {
	const float dt = std::max(kZero, ctx.deltaTime);

	if (!p5Log2Sent_ && phaseTimer_ >= kPhase5Log2Delay) {
		p5Log2Sent_ = true;
		PushSystemLog(SysLogCategory::Sys, kPhase5SysLog2, false);
	}

	if (p5CompletedWaitingDelay_) {
		p5PostKillTimer_ += dt;
		if (p5PostKillTimer_ >= kPhase6StartDelaySeconds) {
			StartPhase6Ambush(ctx);
		}
		return;
	}

	if (!droneAlive_ || dt <= kEpsilon) {
		return;
	}

	// ドローンが常にプレイヤーの前方射程帯（220m〜470m）を滑らかに蛇行飛行するよう制御（明確な偏差リード角を発生させる）
	const float playerSpeed = MyMath::Length(cachedPlayerVel_);
	float forwardSpeed = std::clamp(playerSpeed * kDroneSpeedMatchRatio, kDroneMinSpeed, kDroneMaxSpeed);
	const float distAheadZ = droneCruiseZ_ - cachedPlayerPos_.z;
	if (distAheadZ < kDroneMinDistanceAheadZ) {
		forwardSpeed = std::max(forwardSpeed, playerSpeed + kDroneMinSpeed * kHalf);
	} else if (distAheadZ > kDroneMaxDistanceAheadZ) {
		forwardSpeed = kDroneMinSpeed;
	}

	const MyMath::Vector3 prevPos = dronePos_;
	droneCruiseZ_ += forwardSpeed * dt;

	const float canyonCenterX = EvaluateCanyonCenterX(droneCruiseZ_);
	const float weaveOffsetX = std::sin(phaseTimer_ * kDroneWeaveFrequency * kTwoPi * kQuarter) * kDroneWeaveAmplitudeX;
	const float desiredX = canyonCenterX + weaveOffsetX;
	const float floorY = QueryTerrainY(ctx.terrain, desiredX, droneCruiseZ_);
	const float weaveOffsetY = std::sin(phaseTimer_ * kDroneWeaveFreqY) * kDroneWeaveAmplitudeY;
	const float desiredY = std::max(floorY + kPullUpWarningAglMeters, floorY + kDroneBaseAglMeters + weaveOffsetY);

	dronePos_ = { desiredX, desiredY, droneCruiseZ_ };
	droneVel_ = MyMath::Multiply(kUnit / dt, MyMath::Subtract(dronePos_, prevPos));

	if (droneObject_) {
		Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
		if (cam) {
			droneObject_->SetCamera(cam);
		}
		const float yawAngle = std::atan2(droneVel_.x, std::max(kUnit, droneVel_.z)) + kPi;
		const float pitchAngle = -std::atan2(droneVel_.y, std::max(kUnit, std::hypot(droneVel_.x, droneVel_.z)));
		const float bankAngle = -droneVel_.x * kDroneBankGain;
		droneObject_->SetTranslate(dronePos_);
		droneObject_->SetRotate({ pitchAngle, yawAngle, bankAngle });
		droneObject_->Update();
	}

	// 機銃弾とターゲットドローンの当たり判定
	if (ctx.bulletManager) {
		for (auto& bullet : ctx.bulletManager->GetBullets()) {
			if (!bullet.IsAlive() || bullet.IsEnemyBullet()) {
				continue;
			}
			const float hitDist = MyMath::Length(MyMath::Subtract(bullet.GetPosition(), dronePos_));
			if (hitDist <= kDroneHitRadius) {
				droneHp_ -= bullet.GetDamage();
				bullet.Kill();
				hitMarkerTimer_ = kHitMarkerDuration;

				if (EffectManager::GetInstance()) {
					EffectManager::GetInstance()->EmitHitEffect(dronePos_);
				}

				if (droneHp_ <= kZero) {
					droneAlive_ = false;
					p5CompletedWaitingDelay_ = true;
					p5PostKillTimer_ = kZero;

					if (EffectManager::GetInstance()) {
						EffectManager::GetInstance()->EmitDestroyEffect(dronePos_);
					}
					PushSystemLog(SysLogCategory::Sys, kPhase5SysLogDestroyed, false);
					break;
				}
			}
		}
	}
}

void TutorialMissionDirector::StartPhase6Ambush(const TutorialUpdateContext& ctx) {
	phase_ = HalcyonPhase::Phase6_WesternAmbush;
	phaseTimer_ = kZero;

	westernModel_.ResetAllPartsVisible();
	westernModel_.SetMaterialColor(kWesternAirframeColor);
	westernVisible_ = true;
	westernInDivePass_ = true;
	westernDownedSpiral_ = false;
	westernCrashedFloor_ = false;
	p6LogRwrSent_ = false;
	p6LogSpecSent_ = false;
	p6LogIntelSent_ = false;
	p6EngageRadioSent_ = false;
	westernStrafeTimer_ = kZero;
	if (damageSmokeEffect_) { damageSmokeEffect_->stop(); }
	if (crashPlumeEffect_) { crashPlumeEffect_->stop(); }

	portWingHp_ = kWesternWingPartMaxHp;
	stbdWingHp_ = kWesternWingPartMaxHp;
	engineCoreHp_ = kWesternEnginePartMaxHp;
	brokenPart_ = BrokenPartType::None;

	// 高空方位040・降下角-45度相当から自機頭上を掠めて前方峡谷へ抜ける急降下パスを設定
	if (layout_.useCustomLayout) {
		const float zDelta = (layout_.westernPullUp.z >= (cachedPlayerPos_.z + kMinForwardSafetyOffsetZ))
			? kZero
			: ((cachedPlayerPos_.z + kWesternPullUpOffset.z) - layout_.westernPullUp.z);
		westernDiveStartPos_ = {
			layout_.westernDiveStart.x,
			layout_.westernDiveStart.y,
			layout_.westernDiveStart.z + zDelta
		};
		westernDiveMidPos_ = {
			layout_.westernDiveMid.x,
			layout_.westernDiveMid.y,
			layout_.westernDiveMid.z + zDelta
		};
		const float pullUpZ = layout_.westernPullUp.z + zDelta;
		const float pullUpFloorY = QueryTerrainY(ctx.terrain, layout_.westernPullUp.x, pullUpZ);
		westernDiveEndPos_ = {
			layout_.westernPullUp.x,
			std::max(layout_.westernPullUp.y, pullUpFloorY + kPullUpWarningAglMeters),
			pullUpZ
		};
	} else {
		westernDiveStartPos_ = MyMath::Add(cachedPlayerPos_, kWesternDiveSpawnOffset);
		westernDiveMidPos_ = MyMath::Add(cachedPlayerPos_, kWesternDivePassOffset);
		const float pullUpZ = cachedPlayerPos_.z + kWesternPullUpOffset.z;
		const float pullUpCenterX = EvaluateCanyonCenterX(pullUpZ);
		const float pullUpFloorY = QueryTerrainY(ctx.terrain, pullUpCenterX, pullUpZ);
		westernDiveEndPos_ = {
			pullUpCenterX + kWesternPullUpOffset.x,
			std::max(cachedPlayerPos_.y + kWesternPullUpOffset.y, pullUpFloorY + kWesternBaseAglMeters),
			pullUpZ
		};
	}

	westernPos_ = westernDiveStartPos_;
	westernVel_ = { kZero, -kCoraxMaxSpeed, kCoraxMaxSpeed };
	westernCruiseZ_ = westernDiveEndPos_.z;

	// 急襲開始時のRWR警報ログ＆Corax緊急割り込み無線
	PushSystemLog(SysLogCategory::Alr, kPhase6LogAlr, false);
	if (ctx.radio) {
		ctx.radio->Interrupt(kCoraxSpeakerName, kPhase6RadioAmbush, RadioFaction::Ally, kPhase6WarnRadioHold);
	}
	if (CameraManager::GetInstance()->GetActiveCamera()) {
		CameraManager::GetInstance()->GetActiveCamera()->Shake(kWesternFlybyShakeIntensity, kWesternFlybyShakeDuration);
	}
}

void TutorialMissionDirector::UpdatePhase6(const TutorialUpdateContext& ctx) {
	const float dt = std::max(kZero, ctx.deltaTime);
	if (dt <= kEpsilon) {
		return;
	}

	// 段階的システム解析ログ出力
	if (!p6LogRwrSent_ && phaseTimer_ >= kPhase6LogRwrDelay) {
		p6LogRwrSent_ = true;
		PushSystemLog(SysLogCategory::Rwr, kPhase6LogRwr, false);
	}
	if (!p6LogSpecSent_ && phaseTimer_ >= kPhase6LogSpecDelay) {
		p6LogSpecSent_ = true;
		PushSystemLog(SysLogCategory::Sys, kPhase6LogSys, false);
	}
	if (!p6LogIntelSent_ && phaseTimer_ >= kPhase6LogIntelDelay) {
		p6LogIntelSent_ = true;
		PushSystemLog(SysLogCategory::Intel, kPhase6LogIntel, false);
	}

	// --- 1. 上空からの高速急降下・機関砲掃射パス ---
	if (westernInDivePass_) {
		const MyMath::Vector3 prevPos = westernPos_;
		const float t = std::clamp(phaseTimer_ / kPhase6DivePassDuration, kZero, kUnit);
		const float invT = kUnit - t;

		// 2次ベジェ曲線で急降下→頭上高速フライバイ→谷底引き起こしを滑らかに補間
		const MyMath::Vector3 term0 = MyMath::Multiply(invT * invT, westernDiveStartPos_);
		const MyMath::Vector3 term1 = MyMath::Multiply(kTwo * invT * t, westernDiveMidPos_);
		const MyMath::Vector3 term2 = MyMath::Multiply(t * t, westernDiveEndPos_);
		westernPos_ = MyMath::Add(MyMath::Add(term0, term1), term2);
		westernVel_ = MyMath::Multiply(kUnit / dt, MyMath::Subtract(westernPos_, prevPos));

		// 頭上通過中に機銃トレーサーを掃射（キャノピー脇を通過させる迫力演出）
		westernStrafeTimer_ -= dt;
		if (westernStrafeTimer_ <= kZero && t < kPhase6LogSpecDelay && ctx.bulletManager) {
			westernStrafeTimer_ = kWesternStrafeFireInterval;
			const MyMath::Vector3 strafeTarget = {
				cachedPlayerPos_.x + kWesternStrafeSafeMissOffset,
				cachedPlayerPos_.y + kWesternStrafeSafeMissOffset * kHalf,
				cachedPlayerPos_.z + kCoraxLateralOffsetX
			};
			const MyMath::Vector3 strafeDir = MyMath::Normalize(MyMath::Subtract(strafeTarget, westernPos_));
			const MyMath::Vector3 bulletVel = MyMath::Multiply(kWesternStrafeBulletSpeed, strafeDir);
			ctx.bulletManager->SpawnBullet(westernPos_, bulletVel, kWesternStrafeDamage, true);
		}

		if (t >= kUnit) {
			westernInDivePass_ = false;
			westernCruiseZ_ = westernPos_.z;
			if (!p6EngageRadioSent_ && ctx.radio) {
				p6EngageRadioSent_ = true;
				ctx.radio->Enqueue(kCoraxSpeakerName, kPhase6RadioIdentify, RadioFaction::Ally, kPhase6IdRadioHold);
				ctx.radio->Enqueue(kCoraxSpeakerName, kPhase6RadioEngage, RadioFaction::Ally, kPhase6EngageRadioHold);
			}
			if (CameraManager::GetInstance()->GetActiveCamera()) {
				CameraManager::GetInstance()->GetActiveCamera()->Shake(kWesternFlybyShakeIntensity, kWesternFlybyShakeDuration);
			}
		}

		Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
		if (cam) {
			westernModel_.SetCamera(cam);
		}
		MyMath::Vector3 right{}, up{}, fwd{};
		BuildAircraftBasis(westernVel_, -westernVel_.x * kWesternBankGain, right, up, fwd);
		westernModel_.Update(BuildWorldMatrixFromBasis(right, up, fwd, westernPos_), dt);
		return;
	}

	// --- 2. 通常ドッグファイト機動＆部位ダメージ反映 ---
	const float portRatio = std::clamp(portWingHp_ / kWesternWingPartMaxHp, kZero, kUnit);
	const float stbdRatio = std::clamp(stbdWingHp_ / kWesternWingPartMaxHp, kZero, kUnit);
	const float engRatio = std::clamp(engineCoreHp_ / kWesternEnginePartMaxHp, kZero, kUnit);
	const float minWingRatio = std::min(portRatio, stbdRatio);
	const bool isWingDamaged = (minWingRatio < kWesternDamagedThresholdRatio);
	const bool isEngineDamaged = (engRatio < kWesternDamagedThresholdRatio);

	// 部位損傷が蓄積すると旋回振幅・周波数・推力が明確に低下（「部位へ弾を集中させて運動性を奪え」の体現）
	const float currentWeaveAmpX = isWingDamaged ? kWesternWeaveDegradedAmpX : kWesternWeaveBaseAmpX;
	const float currentWeaveFreq = isWingDamaged ? kWesternWeaveDegradedFreq : kWesternWeaveBaseFreq;
	const float engineSpeedFactor = isEngineDamaged ? kWesternDamagedSpeedPenalty : kUnit;

	const float playerSpeed = MyMath::Length(cachedPlayerVel_);
	float targetForwardSpeed = std::clamp(
		playerSpeed * kWesternSpeedMatchRatio * engineSpeedFactor,
		kDroneMinSpeed,
		kCoraxMaxSpeed);

	const float distAheadZ = westernCruiseZ_ - cachedPlayerPos_.z;
	if (distAheadZ < kWesternMinAheadZ) {
		targetForwardSpeed = std::max(targetForwardSpeed, playerSpeed + kCoraxLateralOffsetX);
	} else if (distAheadZ > kWesternMaxAheadZ) {
		targetForwardSpeed = kDroneMinSpeed;
	}

	const MyMath::Vector3 prevPos = westernPos_;
	westernCruiseZ_ += targetForwardSpeed * dt;

	const float dogfightTime = std::max(kZero, phaseTimer_ - kPhase6DivePassDuration);
	const float centerX = EvaluateCanyonCenterX(westernCruiseZ_);
	const float desiredX = centerX + std::sin(dogfightTime * currentWeaveFreq) * currentWeaveAmpX;
	const float floorY = QueryTerrainY(ctx.terrain, desiredX, westernCruiseZ_);
	const float desiredY = std::max(
		floorY + kPullUpWarningAglMeters,
		floorY + kWesternBaseAglMeters + std::sin(dogfightTime * kWesternWeaveFreqY) * kWesternWeaveAmpY);

	westernPos_ = { desiredX, desiredY, westernCruiseZ_ };
	westernVel_ = MyMath::Multiply(kUnit / dt, MyMath::Subtract(westernPos_, prevPos));

	const float bankRoll = -westernVel_.x * kWesternBankGain;
	MyMath::Vector3 right{}, up{}, fwd{};
	BuildAircraftBasis(westernVel_, bankRoll, right, up, fwd);
	const MyMath::Matrix4x4 westernWorldMat = BuildWorldMatrixFromBasis(right, up, fwd, westernPos_);

	Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
	if (cam) {
		westernModel_.SetCamera(cam);
	}
	westernModel_.Update(westernWorldMat, dt);

	// 損傷部位からの黒煙放出（Resources/effects/damage_smoke.effect 駆動）
	if ((isWingDamaged || isEngineDamaged) && damageSmokeEffect_) {
		damageSmokeEffect_->emit(westernPos_);
	}

	// --- 3. 機体ローカル座標系での3部位（左主翼・右主翼・エンジン）被弾判定 ---
	if (ctx.bulletManager) {
		for (auto& bullet : ctx.bulletManager->GetBullets()) {
			if (!bullet.IsAlive() || bullet.IsEnemyBullet()) {
				continue;
			}
			const float bulletDamage = bullet.GetDamage();
			if (bulletDamage <= kEpsilon) {
				// Coraxの演出用トレーサー（damage == 0）は火花のみ出して部位HPは削らない
				const float cosmeticDist = MyMath::Length(MyMath::Subtract(bullet.GetPosition(), westernPos_));
				if (cosmeticDist <= kDroneHitRadius) {
					bullet.Kill();
					if (EffectManager::GetInstance()) {
						EffectManager::GetInstance()->EmitHitEffect(bullet.GetPosition());
					}
				}
				continue;
			}

			const MyMath::Vector3 relHit = MyMath::Subtract(bullet.GetPosition(), westernPos_);
			const float distSq = MyMath::Dot(relHit, relHit);
			if (distSq > (kWesternOuterHitEnvelope * kWesternOuterHitEnvelope)) {
				continue;
			}

			const float localX = MyMath::Dot(relHit, right);
			const float localZ = MyMath::Dot(relHit, fwd);

			bullet.Kill();
			hitMarkerTimer_ = kHitMarkerDuration;
			if (EffectManager::GetInstance()) {
				EffectManager::GetInstance()->EmitHitEffect(bullet.GetPosition());
			}

			if (localX <= -kWesternWingSpanSplitLocalX) {
				portWingHp_ = std::max(kZero, portWingHp_ - bulletDamage);
			} else if (localX >= kWesternWingSpanSplitLocalX) {
				stbdWingHp_ = std::max(kZero, stbdWingHp_ - bulletDamage);
			} else if (localZ <= kWesternEngineRearSplitLocalZ) {
				engineCoreHp_ = std::max(kZero, engineCoreHp_ - bulletDamage);
			} else {
				// 胴体中央への命中弾は主翼付け根（左翼優先）とエンジン双方へ構造応力ダメージを伝播
				portWingHp_ = std::max(kZero, portWingHp_ - bulletDamage * (kUnit - kWesternCenterToWingStressRatio));
				engineCoreHp_ = std::max(kZero, engineCoreHp_ - bulletDamage * kWesternCenterToWingStressRatio);
			}

			if (portWingHp_ <= kZero) {
				TriggerWesternStructuralBreak(BrokenPartType::PortWing, ctx);
				break;
			}
			if (stbdWingHp_ <= kZero) {
				TriggerWesternStructuralBreak(BrokenPartType::StbdWing, ctx);
				break;
			}
			if (engineCoreHp_ <= kZero) {
				TriggerWesternStructuralBreak(BrokenPartType::EngineCore, ctx);
				break;
			}
		}
	}
}

void TutorialMissionDirector::TriggerWesternStructuralBreak(
	BrokenPartType brokenPart,
	const TutorialUpdateContext& ctx) {
	brokenPart_ = brokenPart;
	westernDownedSpiral_ = true;
	westernCrashedFloor_ = false;
	if (damageSmokeEffect_) {
		damageSmokeEffect_->stop();
	}

	Model* westernRawModel = ModelManager::GetInstance()->FindModel(kWesternFighterModelPath);
	MyMath::Vector3 right{}, up{}, fwd{};
	BuildAircraftBasis(westernVel_, kZero, right, up, fwd);

	// 破壊部位に応じた3Dパーツ切断＆物理破片（DebrisManager）放出＋システムログ出力
	if (brokenPart == BrokenPartType::PortWing) {
		const MyMath::Matrix4x4 wingMat = westernModel_.GetPartWorldMatrix(DamagePart::Wing_L);
		westernModel_.SetPartVisible(DamagePart::Wing_L, false);
		westernModel_.SetPartVisible(DamagePart::Wing1_L, false);
		westernModel_.SetPartVisible(DamagePart::Wing2_L, false);

		if (westernRawModel && DebrisManager::GetInstance()) {
			const MyMath::Vector3 eject = MyMath::Multiply(-kWesternDebrisEjectSpeed, right);
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, westernVel_, eject, kNodeWingL);
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, westernVel_, eject, kNodeWing1L);
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, westernVel_, eject, kNodeWing2L);
		}
		PushSystemLog(SysLogCategory::Sys, kSysLogPortWingDown, false);
	} else if (brokenPart == BrokenPartType::StbdWing) {
		const MyMath::Matrix4x4 wingMat = westernModel_.GetPartWorldMatrix(DamagePart::Wing_R);
		westernModel_.SetPartVisible(DamagePart::Wing_R, false);
		westernModel_.SetPartVisible(DamagePart::Wing1_R, false);
		westernModel_.SetPartVisible(DamagePart::Wing2_R, false);

		if (westernRawModel && DebrisManager::GetInstance()) {
			const MyMath::Vector3 eject = MyMath::Multiply(kWesternDebrisEjectSpeed, right);
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, westernVel_, eject, kNodeWingR);
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, westernVel_, eject, kNodeWing1R);
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, wingMat, westernVel_, eject, kNodeWing2R);
		}
		PushSystemLog(SysLogCategory::Sys, kSysLogStbdWingDown, false);
	} else {
		const MyMath::Matrix4x4 tailMat = westernModel_.GetPartWorldMatrix(DamagePart::Tail);
		westernModel_.SetPartVisible(DamagePart::Tail, false);
		westernModel_.SetPartVisible(DamagePart::Rudder, false);

		if (westernRawModel && DebrisManager::GetInstance()) {
			const MyMath::Vector3 eject = MyMath::Multiply(kWesternDebrisEjectSpeed, up);
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, tailMat, westernVel_, eject, kNodeTail);
			DebrisManager::GetInstance()->SpawnDebris(westernRawModel, tailMat, westernVel_, eject, kNodeRudder);
		}
		PushSystemLog(SysLogCategory::Sys, kSysLogEngineDown, false);
	}

	if (EffectManager::GetInstance()) {
		EffectManager::GetInstance()->EmitDestroyEffect(westernPos_);
	}
	if (CameraManager::GetInstance()->GetActiveCamera()) {
		CameraManager::GetInstance()->GetActiveCamera()->Shake(kWesternFlybyShakeIntensity, kWesternFlybyShakeDuration);
	}

	// 仕様書準拠の撃破時Corax無線3連続交信
	if (ctx.radio) {
		ctx.radio->Interrupt(kCoraxSpeakerName, kPhase6RadioDown1, RadioFaction::Ally, kPhase6DownRadio1Hold);
		ctx.radio->Enqueue(kCoraxSpeakerName, kPhase6RadioDown2, RadioFaction::Ally, kPhase6DownRadio2Hold);
		ctx.radio->Enqueue(kCoraxSpeakerName, kPhase6RadioDown3, RadioFaction::Ally, kPhase6DownRadio3Hold);
	}

	// 前方の峡谷帰投境界ラインを生成してPhase 7へ移行
	if (layout_.useCustomLayout) {
		const float exitZ = (layout_.exitBoundaryPosition.z >= (cachedPlayerPos_.z + kMinForwardSafetyOffsetZ))
			? layout_.exitBoundaryPosition.z
			: (cachedPlayerPos_.z + kExitBoundaryAheadDistanceZ);
		const float exitFloorY = QueryTerrainY(ctx.terrain, layout_.exitBoundaryPosition.x, exitZ);
		exitBoundaryPos_ = {
			layout_.exitBoundaryPosition.x,
			std::max(layout_.exitBoundaryPosition.y, exitFloorY + kPullUpWarningAglMeters),
			exitZ
		};
	} else {
		const float exitZ = cachedPlayerPos_.z + kExitBoundaryAheadDistanceZ;
		const float exitX = EvaluateCanyonCenterX(exitZ);
		const float exitFloorY = QueryTerrainY(ctx.terrain, exitX, exitZ);
		exitBoundaryPos_ = { exitX, exitFloorY + kCoraxPatrolAglMeters, exitZ };
	}
	exitBoundaryVisible_ = true;

	phase_ = HalcyonPhase::Phase7_ExitBoundary;
	phaseTimer_ = kZero;
}

void TutorialMissionDirector::UpdatePhase7Exit(const TutorialUpdateContext& ctx) {
	const float dt = std::max(kZero, ctx.deltaTime);

	// 撃墜された西側機が黒煙と炎を曳きながら谷底へきりもみ墜落していく物理演出
	if (westernVisible_ && westernDownedSpiral_ && !westernCrashedFloor_ && dt > kEpsilon) {
		westernVel_.y -= kWesternCrashGravity * dt;
		westernVel_.x *= std::max(kZero, kUnit - kCrashHorizDragRate * dt);
		westernPos_ = MyMath::Add(westernPos_, MyMath::Multiply(dt, westernVel_));

		const float crashRollDir = (brokenPart_ == BrokenPartType::StbdWing) ? kUnit : -kUnit;
		const float spiralRoll = phaseTimer_ * kWesternCrashRollRate * crashRollDir;

		Camera* cam = CameraManager::GetInstance()->GetActiveCamera();
		if (cam) {
			westernModel_.SetCamera(cam);
		}
		MyMath::Vector3 right{}, up{}, fwd{};
		BuildAircraftBasis(westernVel_, spiralRoll, right, up, fwd);
		westernModel_.Update(BuildWorldMatrixFromBasis(right, up, fwd, westernPos_), dt);

		if (crashPlumeEffect_) {
			crashPlumeEffect_->emit(westernPos_);
		}

		const float crashGroundY = QueryTerrainY(ctx.terrain, westernPos_.x, westernPos_.z);
		if (westernPos_.y <= crashGroundY + kCrashGroundOffsetY) {
			westernPos_.y = crashGroundY + kCrashGroundOffsetY;
			westernCrashedFloor_ = true;
			westernVisible_ = false;
			if (crashPlumeEffect_) {
				crashPlumeEffect_->stop();
			}
			if (EffectManager::GetInstance()) {
				EffectManager::GetInstance()->EmitDestroyEffect(westernPos_);
			}
		}
	}

	// 峡谷境界ライン通過、またはCorax最終帰投無線の完了後にフェードアウトへ移行
	const bool crossedBoundary = (cachedPlayerPos_.z >= exitBoundaryPos_.z);
	const bool autoReturnReady = (phaseTimer_ >= kExitBoundaryAutoTriggerSeconds && (!ctx.radio || !ctx.radio->IsActive()));

	if (crossedBoundary || autoReturnReady) {
		PushSystemLog(SysLogCategory::Sys, kSysLogBoundaryCross, false);
		phase_ = HalcyonPhase::Phase8_DebriefFadeOut;
		phaseTimer_ = kZero;
		fadeOutAlpha_ = kZero;
	}
}

void TutorialMissionDirector::UpdatePhase8FadeOut(const TutorialUpdateContext& ctx) {
	const float dt = std::max(kZero, ctx.deltaTime);
	fadeOutAlpha_ = std::clamp(phaseTimer_ / kFadeOutDurationSeconds, kZero, kUnit);
	(void)dt;

	if (phaseTimer_ >= kFadeOutDurationSeconds) {
		fadeOutAlpha_ = kUnit;
		readyForClearScene_ = true;
	}
}

void TutorialMissionDirector::Draw3D(Camera* camera) {
	if (!isActive_ || !isInitialized_ || !camera || phase_ == HalcyonPhase::Inactive) {
		return;
	}

	// 1. Object3dパイプライン（隊長機Corax・訓練ドローン・西側ジェット戦闘機）
	Object3dCommon::GetInstance()->SetupCommonState();
	Object3dCommon::GetInstance()->SetLightType(kOneInt);

	coraxModel_.Draw();

	if (phase_ == HalcyonPhase::Phase5_DroneLeadSight && droneAlive_ && droneObject_) {
		droneObject_->Draw();
	}

	if (westernVisible_) {
		westernModel_.Draw();
	}

	// 2. PrimitiveModelパイプライン（ウェイポイントリング・炎上監視塔・帰投境界ライン）
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim) {
		return;
	}

	// Phase 3: 緑のウェイポイントゲート描画
	if (phase_ == HalcyonPhase::Phase3_CanyonWaypoints) {
		for (size_t i = kZeroIndex; i < gates_.size(); ++i) {
			const CanyonGate& gate = gates_[i];
			if (gate.isPassed && gate.passFlashTimer <= kZero) {
				continue;
			}

			const bool isCurrentTarget = (static_cast<int>(i) == passedGateCount_ && !gate.isPassed);
			const float spinAngle = animTimer_ * kGateSpinSpeedRad * ((i % kTwoInt == kZeroIndex) ? kUnit : -kUnit);

			MyMath::Vector4 mainCol = isCurrentTarget ? kGateActiveColor : kGateUpcomingColor;
			float radius = kGateVisualRadius;
			if (gate.passFlashTimer > kZero) {
				const float flashRatio = gate.passFlashTimer / kGatePassFlashDuration;
				radius *= (kUnit + (kUnit - flashRatio) * kQuarter);
				mainCol = { kUnit, kUnit, kUnit, flashRatio };
			}

			prim->DrawRing(
				{ radius, radius, kGateRingDepthScale },
				{ kZero, kZero, spinAngle },
				gate.position,
				mainCol,
				ringTexIndex_,
				camera,
				BlendMode::kAdd);

			if (isCurrentTarget) {
				prim->DrawRing(
					{ radius * kGateOuterGlowScale, radius * kGateOuterGlowScale, kGateRingDepthScale },
					{ kZero, kZero, -spinAngle * kHalf },
					gate.position,
					kGateGlowColor,
					ringTexIndex_,
					camera,
					BlendMode::kAdd);

				// 上下左右のガイドティック線
				const MyMath::Vector3& gp = gate.position;
				prim->DrawLine3D(
					{ gp.x, gp.y + radius, gp.z },
					{ gp.x, gp.y + radius + kGateTickLength, gp.z },
					kGateActiveColor,
					camera);
				prim->DrawLine3D(
					{ gp.x, gp.y - radius, gp.z },
					{ gp.x, gp.y - radius - kGateTickLength, gp.z },
					kGateActiveColor,
					camera);
				prim->DrawLine3D(
					{ gp.x - radius, gp.y, gp.z },
					{ gp.x - radius - kGateTickLength, gp.y, gp.z },
					kGateActiveColor,
					camera);
				prim->DrawLine3D(
					{ gp.x + radius, gp.y, gp.z },
					{ gp.x + radius + kGateTickLength, gp.y, gp.z },
					kGateActiveColor,
					camera);
			}
		}
	}

	// 峡谷監視所群の常時描画（Phase 1から全監視塔を描画し、Phase 4以降は第4監視所に赤熱グローを追加）
	if (outpostVisible_) {
		const size_t towerCount = outpostPositions_.empty() ? kIndexStep : outpostPositions_.size();
		for (size_t i = kZeroIndex; i < towerCount; ++i) {
			const MyMath::Vector3& towerPos = outpostPositions_.empty() ? outpostPos_ : outpostPositions_[i];
			const bool isIncidentTower = (i + kIndexStep == towerCount);
			const bool isBurningNow = outpostIgnited_ && isIncidentTower;

			const MyMath::Vector3 towerCenter = {
				towerPos.x,
				towerPos.y + kOutpostTowerHeight * kHalf,
				towerPos.z
			};
			const MyMath::Vector3 deckCenter = {
				towerPos.x,
				towerPos.y + kOutpostTowerHeight + kOutpostDeckHeight * kHalf,
				towerPos.z
			};
			const MyMath::Vector3 mastBase = {
				towerPos.x,
				towerPos.y + kOutpostTowerHeight + kOutpostDeckHeight,
				towerPos.z
			};
			const MyMath::Vector3 mastTop = {
				towerPos.x + (isBurningNow ? kGateCylinderThickness : kZero),
				mastBase.y + kOutpostMastHeight,
				towerPos.z
			};

			prim->DrawCylinder(
				{ kOutpostTowerRadius, kOutpostTowerHeight, kOutpostTowerRadius },
				{ kZero, kZero, kZero },
				towerCenter,
				kOutpostConcreteColor,
				whiteTexIndex_,
				camera,
				BlendMode::kNormal);

			prim->DrawCylinder(
				{ kOutpostDeckRadius, kOutpostDeckHeight, kOutpostDeckRadius },
				{ kZero, kZero, kZero },
				deckCenter,
				kOutpostDeckColor,
				whiteTexIndex_,
				camera,
				BlendMode::kNormal);

			prim->DrawLine3D(
				mastBase,
				mastTop,
				isBurningNow ? kOutpostEmberColor : kOutpostIntactMastColor,
				camera);

			if (isBurningNow) {
				const float flicker = kBlinkMinAlpha + kBlinkAmpAlpha * (kHalf + kHalf * std::sin(animTimer_ * kAlertBlinkFrequency));
				MyMath::Vector4 emberCol = kOutpostEmberColor;
				emberCol.w *= flicker;
				prim->DrawRing(
					{ kOutpostFireGlowRadius, kOutpostFireGlowRadius, kGateRingDepthScale },
					{ kHalfPi, kZero, animTimer_ },
					mastBase,
					emberCol,
					ringTexIndex_,
					camera,
					BlendMode::kAdd);
			}
		}
	}

	// Phase 5: オレンジドローン視認補助ビーコンリング
	if (phase_ == HalcyonPhase::Phase5_DroneLeadSight && droneAlive_) {
		prim->DrawRing(
			{ kDroneBeaconRadius, kDroneBeaconRadius, kUnit },
			{ kZero, kZero, animTimer_ * kTwo },
			dronePos_,
			kDroneHudColor,
			ringTexIndex_,
			camera,
			BlendMode::kAdd);
	}

	// Phase 7: 峡谷帰投境界ライン（シアンゲート＆境界ライン）
	if (exitBoundaryVisible_ && (phase_ == HalcyonPhase::Phase7_ExitBoundary || phase_ == HalcyonPhase::Phase8_DebriefFadeOut)) {
		prim->DrawRing(
			{ kExitBoundaryGateRadius, kExitBoundaryGateRadius, kGateRingDepthScale },
			{ kZero, kZero, animTimer_ * kGateSpinSpeedRad },
			exitBoundaryPos_,
			kBoundaryGateColor,
			ringTexIndex_,
			camera,
			BlendMode::kAdd);

		prim->DrawLine3D(
			{ exitBoundaryPos_.x - kGatePlaneCorridorX, exitBoundaryPos_.y, exitBoundaryPos_.z },
			{ exitBoundaryPos_.x + kGatePlaneCorridorX, exitBoundaryPos_.y, exitBoundaryPos_.z },
			kBoundaryGateColor,
			camera);
	}

	// 後続のObject3d描画パス向けに共通パイプライン状態を復元
	Object3dCommon::GetInstance()->SetupCommonState();
	Object3dCommon::GetInstance()->SetLightType(kOneInt);
}

Sprite* TutorialMissionDirector::AcquireRectSprite() {
	if (!spriteCommon_) {
		return nullptr;
	}
	if (rectSpritePoolIndex_ >= rectSpritePool_.size()) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_, kWhiteTexPath);
		rectSpritePool_.push_back(std::move(sp));
	}
	return rectSpritePool_[rectSpritePoolIndex_++].get();
}

Sprite* TutorialMissionDirector::AcquireRingSprite() {
	if (!spriteCommon_) {
		return nullptr;
	}
	if (ringSpritePoolIndex_ >= ringSpritePool_.size()) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_, kRingTexPath);
		ringSpritePool_.push_back(std::move(sp));
	}
	return ringSpritePool_[ringSpritePoolIndex_++].get();
}

void TutorialMissionDirector::DrawSolidRect(
	const Vector2& pos,
	const Vector2& size,
	const MyMath::Vector4& color,
	BlendMode blend) {
	Sprite* sp = AcquireRectSprite();
	if (!sp) {
		return;
	}
	sp->SetBlendMode(blend);
	sp->SetAnchorPoint({ kZero, kZero });
	sp->SetRotation(kZero);
	sp->SetPosition(pos);
	sp->SetSize(size);
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

void TutorialMissionDirector::DrawLine2D(
	const Vector2& p1,
	const Vector2& p2,
	float thickness,
	const MyMath::Vector4& color,
	BlendMode blend) {
	const float dx = p2.x - p1.x;
	const float dy = p2.y - p1.y;
	const float length = std::hypot(dx, dy);
	if (length <= kEpsilon) {
		return;
	}
	Sprite* sp = AcquireRectSprite();
	if (!sp) {
		return;
	}
	sp->SetBlendMode(blend);
	sp->SetAnchorPoint({ kZero, kHalf });
	sp->SetPosition(p1);
	sp->SetSize({ length, thickness });
	sp->SetRotation(std::atan2(dy, dx));
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

void TutorialMissionDirector::DrawRing2D(
	const Vector2& center,
	float radius,
	const MyMath::Vector4& color,
	BlendMode blend) {
	Sprite* sp = AcquireRingSprite();
	if (!sp) {
		return;
	}
	const float diameter = radius * kTwo;
	sp->SetBlendMode(blend);
	sp->SetAnchorPoint({ kHalf, kHalf });
	sp->SetRotation(kZero);
	sp->SetPosition(center);
	sp->SetSize({ diameter, diameter });
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

void TutorialMissionDirector::DrawTacticalFrame(
	const Vector2& pos,
	const Vector2& size,
	const MyMath::Vector4& bgCol,
	const MyMath::Vector4& borderCol) {
	DrawSolidRect(pos, size, bgCol);
	DrawSolidRect(pos, { size.x, kGuideBorderThickness }, borderCol);
	DrawSolidRect({ pos.x, pos.y + size.y - kGuideBorderThickness }, { size.x, kGuideBorderThickness }, borderCol);
	DrawSolidRect(pos, { kGuideBorderThickness, size.y }, borderCol);
	DrawSolidRect({ pos.x + size.x - kGuideBorderThickness, pos.y }, { kGuideBorderThickness, size.y }, borderCol);

	// 四隅の強調ブラケット
	const float cLen = kGuideCornerLen;
	const float cThick = kGuideBorderThickness * kTwo;
	DrawSolidRect(pos, { cLen, cThick }, borderCol);
	DrawSolidRect(pos, { cThick, cLen }, borderCol);
	DrawSolidRect({ pos.x + size.x - cLen, pos.y }, { cLen, cThick }, borderCol);
	DrawSolidRect({ pos.x + size.x - cThick, pos.y }, { cThick, cLen }, borderCol);
	DrawSolidRect({ pos.x, pos.y + size.y - cThick }, { cLen, cThick }, borderCol);
	DrawSolidRect({ pos.x, pos.y + size.y - cLen }, { cThick, cLen }, borderCol);
	DrawSolidRect({ pos.x + size.x - cLen, pos.y + size.y - cThick }, { cLen, cThick }, borderCol);
	DrawSolidRect({ pos.x + size.x - cThick, pos.y + size.y - cLen }, { cThick, cLen }, borderCol);
}

void TutorialMissionDirector::DrawTargetBrackets2D(
	const Vector2& center,
	float halfSize,
	const MyMath::Vector4& color) {
	const float x0 = center.x - halfSize;
	const float y0 = center.y - halfSize;
	const float x1 = center.x + halfSize;
	const float y1 = center.y + halfSize;
	const float c = kTargetCornerLen;
	const float t = kTargetLineThickness;

	DrawLine2D({ x0, y0 }, { x0 + c, y0 }, t, color);
	DrawLine2D({ x0, y0 }, { x0, y0 + c }, t, color);

	DrawLine2D({ x1, y0 }, { x1 - c, y0 }, t, color);
	DrawLine2D({ x1, y0 }, { x1, y0 + c }, t, color);

	DrawLine2D({ x0, y1 }, { x0 + c, y1 }, t, color);
	DrawLine2D({ x0, y1 }, { x0, y1 - c }, t, color);

	DrawLine2D({ x1, y1 }, { x1 - c, y1 }, t, color);
	DrawLine2D({ x1, y1 }, { x1, y1 - c }, t, color);
}

void TutorialMissionDirector::DrawSystemLogsHUD(float hudScale) {
	if (sysLogs_.empty()) {
		return;
	}

	const float baseX = kSysLogPanelX * hudScale;
	const float baseY = kSysLogPanelY * hudScale;
	const float rowH = kSysLogRowHeight * hudScale;
	const float fontSize = kSysLogFontSize * hudScale;
	const float bgW = kSysLogBgWidth * hudScale;
	const float accentW = kSysLogAccentWidth * hudScale;
	const float padX = kSysLogBgPadX * hudScale;

	for (size_t i = kZeroIndex; i < sysLogs_.size(); ++i) {
		const SysLogEntry& entry = sysLogs_[i];
		float alpha = kUnit;
		if (entry.ageSeconds < kSysLogFadeInSeconds) {
			alpha = entry.ageSeconds / kSysLogFadeInSeconds;
		} else if (entry.ageSeconds > (kSysLogLifetimeSeconds - kSysLogFadeOutSeconds)) {
			alpha = (kSysLogLifetimeSeconds - entry.ageSeconds) / kSysLogFadeOutSeconds;
		}
		alpha = std::clamp(alpha, kZero, kUnit);

		const float rowY = baseY + static_cast<float>(i) * rowH;
		MyMath::Vector4 bgCol = kLogBgColor;
		bgCol.w *= alpha;
		DrawSolidRect({ baseX, rowY }, { bgW, rowH - kTwo }, bgCol);

		MyMath::Vector4 tagCol = ResolveSysLogColor(entry.category);
		tagCol.w *= alpha;
		DrawSolidRect({ baseX, rowY }, { accentW, rowH - kTwo }, tagCol);

		const std::string fullLine =
			entry.timestampText + std::string(ResolveSysLogTag(entry.category)) + " " + entry.message;
		PrintShadowedHudText(fullLine, baseX + accentW + padX, rowY + kTwo, fontSize, tagCol);
	}
}

void TutorialMissionDirector::DrawTutorialGuideHUD(
	float screenWidth,
	float screenHeight,
	float hudScale) {
	std::string titleText;
	std::string subText;
	bool showProgressBar = false;
	float progressRatio = kZero;

	switch (phase_) {
	case HalcyonPhase::Phase1_BasicControls:
		if (missionClock_ < kPhase1UiAppearTime) {
			return;
		}
		titleText = kPhase1GuideTitle;
		subText = kPhase1GuideSub;
		showProgressBar = true;
		progressRatio = std::clamp(p1MoveAccumSeconds_ / kPhase1RequiredMoveSeconds, kZero, kUnit);
		break;

	case HalcyonPhase::Phase2_ThrottleCruise: {
		titleText = kPhase2GuideTitle;
		char subBuf[kFormatBufferSize]{};
		std::snprintf(
			subBuf,
			sizeof(subBuf),
			"%s  [現在開度: %d%%]",
			kPhase2GuideSub,
			static_cast<int>(std::round(currentPlayerThrottle_ * kPercentScale)));
		subText = subBuf;
		showProgressBar = true;
		progressRatio = std::clamp(p2ThrottleHoldSeconds_ / kPhase2RequiredHoldSeconds, kZero, kUnit);
		break;
	}

	case HalcyonPhase::Phase3_CanyonWaypoints: {
		const size_t totalGates = std::max(kIndexStep, gates_.size());
		char titleBuf[kFormatBufferSize]{};
		std::snprintf(
			titleBuf,
			sizeof(titleBuf),
			kPhase3GuideFormat,
			passedGateCount_,
			static_cast<int>(totalGates));
		titleText = titleBuf;
		subText = kPhase3GuideSub;
		showProgressBar = true;
		progressRatio = std::clamp(
			static_cast<float>(passedGateCount_) / static_cast<float>(totalGates),
			kZero,
			kUnit);
		break;
	}

	case HalcyonPhase::Phase4_OutpostIncident:
		titleText = kPhase4GuideTitle;
		subText = kPhase4GuideSub;
		break;

	case HalcyonPhase::Phase5_DroneLeadSight:
		titleText = kPhase5GuideTitle;
		subText = kPhase5GuideSub;
		showProgressBar = true;
		progressRatio = droneAlive_
			? std::clamp(kUnit - (droneHp_ / kDroneMaxHp), kZero, kUnit)
			: kUnit;
		break;

	case HalcyonPhase::Phase6_WesternAmbush:
		if (westernInDivePass_) {
			return;
		}
		titleText = kPhase6GuideTitle;
		subText = kPhase6GuideSub;
		break;

	case HalcyonPhase::Phase7_ExitBoundary:
		titleText = kExitGuideTitle;
		subText = kExitGuideSub;
		break;

	default:
		return;
	}

	const float panelW = kGuidePanelWidth * hudScale;
	const float panelH = kGuidePanelHeight * hudScale;
	const float panelX = (screenWidth - panelW) * kHalf;
	const float panelY = screenHeight - (kGuidePanelBottomMargin * hudScale) - panelH;

	DrawTacticalFrame({ panelX, panelY }, { panelW, panelH }, kGuidePanelBg, kGuidePanelBorder);

	const float centerX = screenWidth * kHalf;
	PrintShadowedHudText(
		titleText,
		centerX,
		panelY + kGuideTitleOffsetY * hudScale,
		kGuideTitleFontSize * hudScale,
		kGuideTitleColor,
		{ kHalf, kZero });

	PrintShadowedHudText(
		subText,
		centerX,
		panelY + kGuideSubOffsetY * hudScale,
		kGuideSubFontSize * hudScale,
		kGuideSubColor,
		{ kHalf, kZero });

	if (showProgressBar) {
		const float barMarginX = kGuideProgressBarMarginX * hudScale;
		const float barW = panelW - barMarginX * kTwo;
		const float barH = kGuideProgressBarHeight * hudScale;
		const float barX = panelX + barMarginX;
		const float barY = panelY + kGuideProgressBarOffsetY * hudScale;

		DrawSolidRect({ barX, barY }, { barW, barH }, kGuideBarBg);
		if (progressRatio > kZero) {
			DrawSolidRect({ barX, barY }, { barW * progressRatio, barH }, kGuideBarFill);
		}
	}
}

void TutorialMissionDirector::DrawAlertsHUD(
	float screenWidth,
	float screenHeight,
	float hudScale) {
	const float blink = kBlinkMinAlpha + kBlinkAmpAlpha * (kHalf + kHalf * std::sin(animTimer_ * kAlertBlinkFrequency));
	const float bannerW = kAlertBannerWidth * hudScale;
	const float bannerH = kAlertBannerHeight * hudScale;
	const float bannerX = (screenWidth - bannerW) * kHalf;

	// 1. 地面接近警告 [ ! PULL UP ! 地面接近警告 ] または 高高度注意
	if (isPullUpWarning_) {
		const float bannerY = (screenHeight * kHalf) + (kPullUpBannerCenterOffsetY * hudScale);
		MyMath::Vector4 bg = kAlertPanelBg;
		MyMath::Vector4 border = kAlertBorderCol;
		bg.w *= blink;
		border.w *= blink;
		DrawTacticalFrame({ bannerX, bannerY }, { bannerW, bannerH }, bg, border);
		PrintShadowedHudText(
			kPullUpAlertText,
			screenWidth * kHalf,
			bannerY + kGuideTitleOffsetY * hudScale * kHalf,
			kGuideTitleFontSize * hudScale,
			border,
			{ kHalf, kZero });
	} else if (isHighAltitudeCaution_) {
		const float bannerY = (screenHeight * kHalf) + (kPullUpBannerCenterOffsetY * hudScale);
		DrawTacticalFrame({ bannerX, bannerY }, { bannerW, bannerH }, kCautionPanelBg, kCautionBorderCol);
		PrintShadowedHudText(
			kHighAltCautionText,
			screenWidth * kHalf,
			bannerY + kGuideTitleOffsetY * hudScale * kHalf,
			kSysLogFontSize * hudScale,
			kCautionBorderCol,
			{ kHalf, kZero });
	}

	// 2. Phase 6 急襲時のRWR警報バナー
	if (phase_ == HalcyonPhase::Phase6_WesternAmbush && westernInDivePass_) {
		const float rwrY = kRwrBannerTopY * hudScale;
		MyMath::Vector4 bg = kAlertPanelBg;
		MyMath::Vector4 border = kAlertBorderCol;
		bg.w *= blink;
		border.w *= blink;
		DrawTacticalFrame({ bannerX, rwrY }, { bannerW, bannerH }, bg, border);
		PrintShadowedHudText(
			kRwrLockAlertText,
			screenWidth * kHalf,
			rwrY + kGuideTitleOffsetY * hudScale * kHalf,
			kSysLogFontSize * hudScale,
			border,
			{ kHalf, kZero });
	}
}

void TutorialMissionDirector::DrawWorldMarkersAndLeadSightHUD(
	Camera* camera,
	float screenWidth,
	float screenHeight,
	float hudScale) {
	if (!camera) {
		return;
	}

	const MyMath::Matrix4x4& vpMat = camera->GetViewProjectionMatrix();
	const float clampMargin = kHudClampMarginPx * hudScale;
	const float boxHalf = kTargetBoxHalfSize * hudScale;
	const float labelFont = kMarkerLabelFontSize * hudScale;

	auto drawWorldMarker = [&](
		const MyMath::Vector3& worldPos,
		const char* labelText,
		const MyMath::Vector4& markerCol) -> Vector2 {
		Vector2 screenPt{};
		bool isClamped = false;
		StageHUD::ProjectToScreenClamped(worldPos, vpMat, screenWidth, screenHeight, clampMargin, screenPt, isClamped);

		DrawTargetBrackets2D(screenPt, boxHalf, markerCol);

		const int distMeters = static_cast<int>(MyMath::Length(MyMath::Subtract(worldPos, cachedPlayerPos_)));
		char distBuf[kTimestampBufferSize]{};
		std::snprintf(distBuf, sizeof(distBuf), "%d m", distMeters);

		if (!isClamped) {
			PrintShadowedHudText(
				labelText,
				screenPt.x + kMarkerLabelOffsetX * hudScale,
				screenPt.y + kMarkerLabelOffsetY * hudScale,
				labelFont,
				markerCol);
			PrintShadowedHudText(
				distBuf,
				screenPt.x + kMarkerLabelOffsetX * hudScale,
				screenPt.y + kMarkerDistOffsetY * hudScale,
				labelFont,
				markerCol);
		} else {
			PrintShadowedHudText(
				distBuf,
				screenPt.x,
				screenPt.y + boxHalf + kTwo,
				labelFont,
				markerCol,
				{ kHalf, kZero });
		}
		return screenPt;
	};

	// 1. 飛行隊長『Corax』味方IFFマーカー
	drawWorldMarker(coraxPos_, kCoraxHudLabel, kAllyHudColor);

	// 2. Phase 3: アクティブウェイポイントマーカー
	if (phase_ == HalcyonPhase::Phase3_CanyonWaypoints &&
	    passedGateCount_ < static_cast<int>(gates_.size())) {
		const CanyonGate& activeGate = gates_[static_cast<size_t>(passedGateCount_)];
		char wpLabel[kTimestampBufferSize]{};
		std::snprintf(wpLabel, sizeof(wpLabel), "WAYPOINT 0%d", passedGateCount_ + kOneInt);
		drawWorldMarker(activeGate.position, wpLabel, kGateActiveColor);
	}

	// 3. Phase 4: 炎上中の第4監視所マーカー
	if (phase_ == HalcyonPhase::Phase4_OutpostIncident && outpostVisible_) {
		const MyMath::Vector3 outpostTop = {
			outpostPos_.x,
			outpostPos_.y + kOutpostTowerHeight,
			outpostPos_.z
		};
		drawWorldMarker(outpostTop, kOutpostHudLabel, kWrnTagColor);
	}

	// 4. Phase 5: 訓練ドローン ＆ リードコンピューティングサイト（偏差射撃マーカー ○）
	if (phase_ == HalcyonPhase::Phase5_DroneLeadSight && droneAlive_) {
		const Vector2 targetScreen = drawWorldMarker(dronePos_, kDroneHudLabel, kDroneHudColor);

		const MyMath::Vector3 leadWorldPos = ComputeLeadInterceptPoint(
			cachedPlayerPos_,
			cachedPlayerVel_,
			dronePos_,
			droneVel_);

		Vector2 leadScreen{};
		bool leadClamped = false;
		StageHUD::ProjectToScreenClamped(leadWorldPos, vpMat, screenWidth, screenHeight, clampMargin, leadScreen, leadClamped);

		if (!leadClamped) {
			MyMath::Vector4 lineCol = kLeadCircleColor;
			lineCol.w *= kLeadConnectLineAlpha;
			DrawLine2D(targetScreen, leadScreen, kLeadConnectLineThickness, lineCol, BlendMode::kAdd);
			DrawRing2D(leadScreen, kLeadMarkerRadiusPx * hudScale, kLeadCircleColor, BlendMode::kAdd);
			DrawRing2D(leadScreen, kLeadInnerDotRadiusPx * hudScale, kLeadCircleColor, BlendMode::kAdd);
		}

		if (hitMarkerTimer_ > kZero) {
			const float inner = kHitCrossInnerPx * hudScale;
			const float outer = kHitCrossOuterPx * hudScale;
			DrawLine2D({ leadScreen.x - outer, leadScreen.y - outer }, { leadScreen.x - inner, leadScreen.y - inner }, kHitCrossThickness, kHitFlashColor);
			DrawLine2D({ leadScreen.x + outer, leadScreen.y - outer }, { leadScreen.x + inner, leadScreen.y - inner }, kHitCrossThickness, kHitFlashColor);
			DrawLine2D({ leadScreen.x - outer, leadScreen.y + outer }, { leadScreen.x - inner, leadScreen.y + inner }, kHitCrossThickness, kHitFlashColor);
			DrawLine2D({ leadScreen.x + outer, leadScreen.y + outer }, { leadScreen.x + inner, leadScreen.y + inner }, kHitCrossThickness, kHitFlashColor);
		}
	}

	// 5. Phase 6: 西側ジェット戦闘機 ＆ リードマーカー ○
	if (phase_ == HalcyonPhase::Phase6_WesternAmbush && westernVisible_ && !westernDownedSpiral_) {
		const Vector2 targetScreen = drawWorldMarker(westernPos_, kWesternHudLabel, kHostileHudColor);

		if (!westernInDivePass_) {
			const MyMath::Vector3 leadWorldPos = ComputeLeadInterceptPoint(
				cachedPlayerPos_,
				cachedPlayerVel_,
				westernPos_,
				westernVel_);

			Vector2 leadScreen{};
			bool leadClamped = false;
			StageHUD::ProjectToScreenClamped(leadWorldPos, vpMat, screenWidth, screenHeight, clampMargin, leadScreen, leadClamped);

			if (!leadClamped) {
				MyMath::Vector4 lineCol = kLeadCircleColor;
				lineCol.w *= kLeadConnectLineAlpha;
				DrawLine2D(targetScreen, leadScreen, kLeadConnectLineThickness, lineCol, BlendMode::kAdd);
				DrawRing2D(leadScreen, kLeadMarkerRadiusPx * hudScale, kLeadCircleColor, BlendMode::kAdd);
				DrawRing2D(leadScreen, kLeadInnerDotRadiusPx * hudScale, kLeadCircleColor, BlendMode::kAdd);
			}

			if (hitMarkerTimer_ > kZero) {
				const float inner = kHitCrossInnerPx * hudScale;
				const float outer = kHitCrossOuterPx * hudScale;
				DrawLine2D({ targetScreen.x - outer, targetScreen.y - outer }, { targetScreen.x - inner, targetScreen.y - inner }, kHitCrossThickness, kHitFlashColor);
				DrawLine2D({ targetScreen.x + outer, targetScreen.y - outer }, { targetScreen.x + inner, targetScreen.y - inner }, kHitCrossThickness, kHitFlashColor);
				DrawLine2D({ targetScreen.x - outer, targetScreen.y + outer }, { targetScreen.x - inner, targetScreen.y + inner }, kHitCrossThickness, kHitFlashColor);
				DrawLine2D({ targetScreen.x + outer, targetScreen.y + outer }, { targetScreen.x + inner, targetScreen.y + inner }, kHitCrossThickness, kHitFlashColor);
			}
		}
	}

	// 6. Phase 7: 峡谷帰投境界マーカー
	if (phase_ == HalcyonPhase::Phase7_ExitBoundary && exitBoundaryVisible_) {
		drawWorldMarker(exitBoundaryPos_, kBoundaryHudLabel, kBoundaryGateColor);
	}
}

void TutorialMissionDirector::DrawWesternPartStatusHUD(float screenWidth, float hudScale) {
	const bool showPanel =
		(phase_ == HalcyonPhase::Phase6_WesternAmbush && westernVisible_ && !westernInDivePass_) ||
		(phase_ == HalcyonPhase::Phase7_ExitBoundary);
	if (!showPanel) {
		return;
	}

	const float panelW = kPartGaugePanelWidth * hudScale;
	const float panelH = kPartGaugePanelHeight * hudScale;
	const float panelX = screenWidth - (kPartGaugeRightMargin * hudScale) - panelW;
	const float panelY = kPartGaugeTopY * hudScale;

	DrawTacticalFrame({ panelX, panelY }, { panelW, panelH }, kGuidePanelBg, kHostileHudColor);

	PrintShadowedHudText(
		kPartHeaderTitle,
		panelX + kPartLabelPadX * hudScale,
		panelY + kPartHeaderOffsetY * hudScale,
		kPartRowFontSize * hudScale,
		kHostileHudColor);

	auto drawPartRow = [&](int rowIndex, const char* label, float curHp, float maxHp, bool isBroken) {
		const float rowY = panelY + (kPartFirstRowOffsetY + static_cast<float>(rowIndex) * kPartRowStepY) * hudScale;
		const float ratio = isBroken ? kZero : std::clamp(curHp / maxHp, kZero, kUnit);
		const MyMath::Vector4 rowCol = isBroken
			? kAlrTagColor
			: (ratio < kWesternDamagedThresholdRatio ? kWrnTagColor : kSysTagColor);

		PrintShadowedHudText(
			label,
			panelX + kPartLabelPadX * hudScale,
			rowY,
			kPartRowFontSize * hudScale,
			rowCol);

		const float barX = panelX + kPartBarOffsetX * hudScale;
		const float barY = rowY + kPartBarOffsetY * hudScale;
		const float barW = kPartBarWidth * hudScale;
		const float barH = kPartBarHeight * hudScale;

		DrawSolidRect({ barX, barY }, { barW, barH }, kGuideBarBg);
		if (isBroken) {
			PrintShadowedHudText(
				kBrokenStatusLabel,
				barX + barW * kHalf,
				rowY,
				kPartRowFontSize * hudScale,
				kAlrTagColor,
				{ kHalf, kZero });
		} else if (ratio > kZero) {
			DrawSolidRect({ barX, barY }, { barW * ratio, barH }, rowCol);
		}
	};

	drawPartRow(kZeroInt, kPartPortWingLabel, portWingHp_, kWesternWingPartMaxHp, brokenPart_ == BrokenPartType::PortWing);
	drawPartRow(kOneInt, kPartStbdWingLabel, stbdWingHp_, kWesternWingPartMaxHp, brokenPart_ == BrokenPartType::StbdWing);
	drawPartRow(kTwoInt, kPartEngineLabel, engineCoreHp_, kWesternEnginePartMaxHp, brokenPart_ == BrokenPartType::EngineCore);
}

void TutorialMissionDirector::DrawUI(Camera* camera, float screenWidth, float screenHeight) {
	if (!isActive_ || !isInitialized_ || phase_ == HalcyonPhase::Inactive) {
		return;
	}

	rectSpritePoolIndex_ = kZeroIndex;
	ringSpritePoolIndex_ = kZeroIndex;

	const float scaleX = screenWidth / kReferenceScreenWidth;
	const float scaleY = screenHeight / kReferenceScreenHeight;
	const float hudScale = std::clamp(std::min(scaleX, scaleY), kMinHudScale, kMaxHudScale);

	DrawWorldMarkersAndLeadSightHUD(camera, screenWidth, screenHeight, hudScale);
	DrawSystemLogsHUD(hudScale);
	DrawTutorialGuideHUD(screenWidth, screenHeight, hudScale);
	DrawWesternPartStatusHUD(screenWidth, hudScale);
	DrawAlertsHUD(screenWidth, screenHeight, hudScale);

	// フェードアウト時はテキストを先にフラッシュしてから最前面に暗転オーバーレイを重ねる
	if (fadeOutAlpha_ > kZero) {
		TextRenderer::GetInstance()->Flush();
		MyMath::Vector4 fadeCol = kPureBlack;
		fadeCol.w = std::clamp(fadeOutAlpha_, kZero, kUnit);
		DrawSolidRect({ kZero, kZero }, { screenWidth, screenHeight }, fadeCol, BlendMode::kNormal);
	}
}
