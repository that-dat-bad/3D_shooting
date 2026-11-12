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

	// 隊長機『Corax』アフターバーナーのセットアップ
	coraxAfterburner_ = std::make_unique<Afterburner>();
	coraxAfterburner_->Initialize();
	coraxAfterburner_->SetTwinEngine(false);
	coraxAfterburner_->SetSingleNozzleOffset({ 0.0f, 0.011f, 7.60f });
	coraxAfterburner_->SetFlameRadius(0.38f);
	coraxAfterburner_->SetFlameLength(5.8f);
	coraxAfterburner_->SetShockDiamondCount(4);
	coraxAfterburner_->SetFlameColor({ 0.28f, 0.65f, 1.0f });
	coraxAfterburner_->SetCoreColor({ 0.88f, 0.95f, 1.0f });
	coraxAfterburner_->SetIntensity(0.85f);
	coraxAfterburner_->LoadConfig("assets/configs/afterburner_m21.json");

	// 2. 西側ジェット戦闘機 (M-15 後退翼機・部位破壊対応) のセットアップ（glTF本来のマテリアル色とキャノピー色を保持）
	westernModel_.Initialize(Object3dCommon::GetInstance(), activeCam);
	westernModel_.SetupFromSingleModel(kWesternFighterModelPath);
	westernModel_.SetMaterialProperties(kWesternMaterialShininess, kWesternMaterialSpecular, kWesternMaterialEnv);
	westernModel_.SetupCanopyMaterial();

	// 3. 峡谷監視塔 (tower.obj) の事前ロード
	ModelManager::GetInstance()->LoadModel(kWatchtowerModelPath);

	// 4. 訓練用自動クアッドコプタードローン (ORANGE) のセットアップ
	ModelManager::GetInstance()->LoadModel(kDroneModelPath);
	droneObject_ = std::make_unique<Object3d>();
	droneObject_->Initialize(Object3dCommon::GetInstance());
	droneObject_->SetCamera(activeCam);
	droneObject_->SetModel(kDroneModelPath);
	droneObject_->SetScale({ kDroneScale, kDroneScale, kDroneScale });
	droneObject_->SetMaterialColor(kPrimaryMaterialIndex, kDroneOrangeColor);
	droneObject_->SetMaterialEmissive(
		kPrimaryMaterialIndex,
		{ kDroneOrangeColor.x, kDroneOrangeColor.y, kDroneOrangeColor.z },
		kDroneEmissiveIntensity);

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
		(mission.name.find("Obuchenie") != std::string::npos) ||
		(mission.difficulty == "TUTORIAL") ||
		(mission.aliasName == kZadanie00Alias);
	const bool matchByFile = EndsWithString(missionFilePath, kMissionFileSuffix) || (missionFilePath.find("mission00") != std::string::npos);
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
	westernCurrentSpeed_ = kCoraxMinSpeed;
	westernWeavePhaseX_ = kZero;
	westernWeavePhaseY_ = kZero;
	westernSmoothedGroundY_ = kFallbackCanyonFloorY;
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
	SyncWatchtowerObjects(nullptr);
}

void TutorialMissionDirector::GroundWatchtowersToTerrain(const TerrainChunkManager* terrain) {
	if (!terrain || !terrain->IsLoaded()) {
		return;
	}
	for (auto& pos : outpostPositions_) {
		const float groundY = QueryTerrainY(terrain, pos.x, pos.z);
		// 地中に埋没しないよう、最低地表高度 -472.0f と比較して高い方を採用
		pos.y = std::max(groundY, -472.0f);
	}
	outpostPos_.y = std::max(QueryTerrainY(terrain, outpostPos_.x, outpostPos_.z), -472.0f);
	if (!outpostPositions_.empty()) {
		outpostPositions_.back() = outpostPos_;
	}
	watchtowersGrounded_ = true;
	SyncWatchtowerObjects(nullptr);
}

void TutorialMissionDirector::SyncWatchtowerObjects(Camera* camera) {
	Camera* activeCam = camera;
	if (!activeCam && CameraManager::GetInstance()) {
		activeCam = CameraManager::GetInstance()->GetActiveCamera();
	}

	if (watchtowerObjects_.size() != outpostPositions_.size()) {
		watchtowerObjects_.clear();
		watchtowerObjects_.reserve(outpostPositions_.size());
		for (size_t i = kZeroIndex; i < outpostPositions_.size(); ++i) {
			auto obj = std::make_unique<Object3d>();
			obj->Initialize(Object3dCommon::GetInstance());
			obj->SetModel(kWatchtowerModelPath);
			obj->SetScale({ kWatchtowerModelScale, kWatchtowerModelScale, kWatchtowerModelScale });
			// 暗い極夜でも黒潰れせず立体感が出るようマテリアル色を設定（スチール支柱＋デッキ・屋根）
			obj->SetMaterialColor(0, { 0.40f, 0.44f, 0.50f, 1.0f });
			obj->SetMaterialColor(1, { 0.60f, 0.58f, 0.54f, 1.0f });
			if (activeCam) {
				obj->SetCamera(activeCam);
			}
			watchtowerObjects_.push_back(std::move(obj));
		}
	}

	for (size_t i = kZeroIndex; i < outpostPositions_.size() && i < watchtowerObjects_.size(); ++i) {
		auto& obj = watchtowerObjects_[i];
		if (!obj) {
			continue;
		}
		if (activeCam) {
			obj->SetCamera(activeCam);
		}
		obj->SetScale({ kWatchtowerModelScale, kWatchtowerModelScale, kWatchtowerModelScale });
		obj->SetMaterialColor(0, { 0.40f, 0.44f, 0.50f, 1.0f });
		obj->SetMaterialColor(1, { 0.60f, 0.58f, 0.54f, 1.0f });
		obj->SetTranslate(outpostPositions_[i]);
		const float rotY = static_cast<float>(i) * 0.785398f;
		obj->SetRotate({ kZero, rotY, kZero });
		obj->Update();
	}
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
		if (coraxAfterburner_) {
			coraxAfterburner_->Update(dt, 0.75f);
		}
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

	// 監視塔の当たり判定処理（自機の激突衝突 ＆ 機銃弾の着弾消滅）
	CheckWatchtowerCollisions(ctx);
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
		// 自機との編隊高度（自機の少し上空 +14m）を基準とし、最低地上安全高度も保証して急降下を防ぐ
		const float safeFormationY = std::max(cachedPlayerPos_.y + 14.0f, floorY + kCoraxPatrolAglMeters);
		desiredPos = {
			desiredX,
			safeFormationY,
			targetZ
		};
	}

	const MyMath::Vector3 toTarget = MyMath::Subtract(desiredPos, coraxPos_);
	MyMath::Vector3 desiredVel = MyMath::Multiply(kCoraxFollowStiffness, toTarget);
	// 垂直昇降速度を穏やかな範囲（最大±20m/s）に制限し、スタート直後の急降下ダイブを完全に抑止
	desiredVel.y = std::clamp(desiredVel.y, -20.0f, 25.0f);
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
		MyMath::Vector3 nextFwd = MyMath::Multiply(kUnit / speedNow, coraxVel_);
		// 下向きピッチ角（ダイブ）を最大 -15度（sin -0.26f）に制限して、不自然な真下ダイブを防ぐ
		nextFwd.y = std::clamp(nextFwd.y, -0.26f, 0.35f);
		nextFwd = MyMath::Normalize(nextFwd);
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

	// 隊長機『Corax』アフターバーナーの更新（速度に応じた推力炎のリアルタイム調整）
	if (coraxAfterburner_) {
		const float coraxThrottle = std::clamp(speedNow / 110.0f, 0.70f, 1.0f);
		coraxAfterburner_->Update(dt, coraxThrottle);
	}

	// 隊長機『Corax』翼端渦（ベイパートレイル）の発生演出
	// 翼端から連続する細いベイパーを引かせ、追従先としての誘導視認性を高める
	const MyMath::Vector3 wingL = MyMath::Add(MyMath::Add(coraxPos_, MyMath::Multiply(-6.0f, right)), MyMath::Multiply(-2.0f, fwd));
	const MyMath::Vector3 wingR = MyMath::Add(MyMath::Add(coraxPos_, MyMath::Multiply(6.0f, right)), MyMath::Multiply(-2.0f, fwd));

	if (hasLastCoraxWings_) {
		const MyMath::Vector3 deltaL = MyMath::Subtract(wingL, lastCoraxWingL_);
		const MyMath::Vector3 deltaR = MyMath::Subtract(wingR, lastCoraxWingR_);
		const float distL = MyMath::Length(deltaL);

		ParticleParameters params;
		params.minVelocity = { 0.0f, 0.0f, 0.0f };
		params.maxVelocity = { 0.0f, 0.0f, 0.0f };
		params.randomPositionRange = 0.0f;
		params.minColor = { 0.92f, 0.96f, 1.0f, 0.35f };
		params.maxColor = { 0.92f, 0.96f, 1.0f, 0.35f };
		params.minLifeTime = 0.50f;
		params.maxLifeTime = 0.50f;
		params.minScale = 0.09f;
		params.maxScale = 0.09f;
		params.endScale = 0.14f;
		params.fadeOut = true;
		params.isStretched = true;
		params.stretchFactor = 12.0f;

		int steps = static_cast<int>(std::ceil(distL / 0.35f));
		if (steps < 1) { steps = 1; }
		if (steps > 30) { steps = 30; }

		const MyMath::Vector3 stepDeltaL = MyMath::Multiply(1.0f / static_cast<float>(steps), deltaL);
		const MyMath::Vector3 stepDeltaR = MyMath::Multiply(1.0f / static_cast<float>(steps), deltaR);

		for (int i = 0; i < steps; ++i) {
			const float t = static_cast<float>(i) / static_cast<float>(steps);
			const MyMath::Vector3 interpL = MyMath::Lerp(lastCoraxWingL_, wingL, t);
			const MyMath::Vector3 interpR = MyMath::Lerp(lastCoraxWingR_, wingR, t);

			params.stretchDir = stepDeltaL;
			ParticleManager::GetInstance()->Emit("Vortex", interpL, params, 1);

			params.stretchDir = stepDeltaR;
			ParticleManager::GetInstance()->Emit("Vortex", interpR, params, 1);
		}
	}
	lastCoraxWingL_ = wingL;
	lastCoraxWingR_ = wingR;
	hasLastCoraxWings_ = true;
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
		float prevGateZ = playerPos.z;
		for (size_t i = kZeroIndex; i < customGatePositions_.size(); ++i) {
			const MyMath::Vector3& src = customGatePositions_[i];
			float gz = src.z + zShift;
			// 前のゲートとの最小間隔を保証
			if (i > kZeroIndex && gz < prevGateZ + kSubsequentGateSpacingZ) {
				gz = prevGateZ + kSubsequentGateSpacingZ;
			}
			prevGateZ = gz;

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

	// 1. ゲートの球判定（リングの中心近傍）
	const bool insideSphere = (distToCenter <= kGateCaptureSphereRadius);

	// 2. ゲートの通過平面判定（ゲートのZ面を通過した瞬間、かつXYがリング枠内）
	// ※上限Zチェック（currentGate.position.z + kGatePlaneThicknessZ）により、
	// はるか後方から一気に複数ゲートを連続誤判定する現象を完全に防止
	const bool crossedGatePlane =
		(cachedPlayerPos_.z >= currentGate.position.z) &&
		(cachedPlayerPos_.z <= currentGate.position.z + kGatePlaneThicknessZ) &&
		(std::fabs(diff.x) <= kGatePlaneCorridorX) &&
		(std::fabs(diff.y) <= kGatePlaneCorridorY);

	// 3. ゲートをくぐらずに奥へ大きく通り過ぎてしまった場合（救済スキップ）
	const bool missedGateOvershoot = (cachedPlayerPos_.z >= currentGate.position.z + kGatePassOvershootToleranceZ);

	if (insideSphere || crossedGatePlane || missedGateOvershoot) {
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

	// 監視塔（第4監視所）の位置：ゲーム開始時から配置されている座標を厳密に維持し、一切ワープさせない
	if (!outpostPositions_.empty()) {
		outpostPos_ = outpostPositions_.back();
	} else if (layout_.useCustomLayout) {
		outpostPos_ = layout_.outpostPosition;
		outpostPositions_.push_back(outpostPos_);
	}

	// 地形接地が未完了の場合は地表高度に合わせる
	if (terrain && terrain->IsLoaded()) {
		outpostPos_.y = std::max(QueryTerrainY(terrain, outpostPos_.x, outpostPos_.z), -472.0f);
		if (!outpostPositions_.empty()) {
			outpostPositions_.back() = outpostPos_;
		}
	}

	// 3Dモデルの位置を同期
	SyncWatchtowerObjects(nullptr);

	outpostVisible_ = true;
	outpostIgnited_ = true;

	// 監視塔被弾跡の初期大爆炎エフェクト（自然な爆発初速・巨大な火球）
	const MyMath::Vector3 towerTop = {
		outpostPos_.x,
		outpostPos_.y + kOutpostTowerHeight,
		outpostPos_.z
	};
	if (EffectManager::GetInstance()) {
		EffectManager::GetInstance()->EmitDestroyEffect(towerTop);
	}
	if (ParticleManager::GetInstance()) {
		ParticleParameters initFire{};
		initFire.minVelocity = { -0.08f, 0.04f, -0.08f };
		initFire.maxVelocity = {  0.08f, 0.16f,  0.08f };
		initFire.randomPositionRange = 3.5f;
		initFire.minScale = 22.0f;
		initFire.maxScale = 34.0f;
		initFire.endScale = 12.0f;
		initFire.minLifeTime = 0.8f;
		initFire.maxLifeTime = 1.4f;
		initFire.fadeOut = true;
		initFire.minColor = { 1.0f, 0.45f, 0.08f, 1.0f };
		initFire.maxColor = { 1.0f, 0.88f, 0.32f, 1.0f };
		ParticleManager::GetInstance()->Emit("ExplosionFire", towerTop, initFire, 12);

		ParticleParameters initSmoke{};
		initSmoke.minVelocity = { -0.06f, 0.03f, -0.06f };
		initSmoke.maxVelocity = {  0.06f, 0.12f,  0.06f };
		initSmoke.randomPositionRange = 4.0f;
		initSmoke.minScale = 18.0f;
		initSmoke.maxScale = 28.0f;
		initSmoke.endScale = 52.0f;
		initSmoke.minLifeTime = 3.5f;
		initSmoke.maxLifeTime = 5.5f;
		initSmoke.fadeOut = true;
		initSmoke.minColor = { 0.05f, 0.05f, 0.06f, 0.80f };
		initSmoke.maxColor = { 0.10f, 0.10f, 0.12f, 0.95f };
		ParticleManager::GetInstance()->Emit("ExplosionSmoke", towerTop, initSmoke, 10);
	}
}

void TutorialMissionDirector::UpdateOutpostEffects(float dt) {
	if (!outpostVisible_ || !outpostIgnited_) {
		return;
	}

	ParticleManager* pm = ParticleManager::GetInstance();
	if (!pm) {
		return;
	}

	const MyMath::Vector3 fireOrigin = {
		outpostPos_.x,
		outpostPos_.y + kOutpostTowerHeight - 2.0f,
		outpostPos_.z
	};

	// 1. 激しく燃え盛る火炎の塊（ExplosionFire）：
	// ※ParticleManager::Update は毎フレーム velocity を直足しするため、
	// 速度は 0.015〜0.045m/frame (秒速0.9〜2.7m) に設定して塔頂部に炎を密集・滞留させる。
	// 加算合成（kAdd）により多数の炎が重なり合って中心が眩しく白熱し、本物の大火災の火柱を形成する。
	{
		ParticleParameters fireParams{};
		fireParams.minVelocity = { -0.015f, 0.015f, -0.015f };
		fireParams.maxVelocity = {  0.015f, 0.045f,  0.015f };
		fireParams.randomPositionRange = 3.2f;
		fireParams.minScale = 16.0f;
		fireParams.maxScale = 26.0f;
		fireParams.endScale = 10.0f;
		fireParams.scaleEasing = 0.35f;
		fireParams.minLifeTime = 0.55f;
		fireParams.maxLifeTime = 0.95f;
		fireParams.fadeOut = true;
		fireParams.minColor = { 1.0f, 0.38f, 0.06f, 0.95f };
		fireParams.maxColor = { 1.0f, 0.88f, 0.30f, 1.0f };
		fireParams.minRotation = 0.0f;
		fireParams.maxRotation = kTwoPi;
		fireParams.minRotationSpeed = -1.0f;
		fireParams.maxRotationSpeed = 1.0f;
		pm->Emit("ExplosionFire", fireOrigin, fireParams, 2);
	}

	// 2. 上空へ巨大に広がる濃黒煙プルーム（ExplosionSmoke）：
	// 上昇速度 0.05〜0.11m/frame (秒速3.0〜6.6m) でモクモクと上空へ立ち昇り、
	// 最大直径 48m の巨大な黒煙柱を形成して遠距離（1km以上）からも視認可能にする。
	static float smokeAccumTimer = 0.0f;
	smokeAccumTimer += dt;
	if (smokeAccumTimer >= 0.045f) {
		smokeAccumTimer = 0.0f;
		const MyMath::Vector3 smokeOrigin = {
			fireOrigin.x + static_cast<float>((rand() % 5) - 2) * 0.8f,
			fireOrigin.y + 3.0f,
			fireOrigin.z + static_cast<float>((rand() % 5) - 2) * 0.8f
		};

		ParticleParameters smokeParams{};
		smokeParams.minVelocity = { -0.020f, 0.050f, -0.020f };
		smokeParams.maxVelocity = {  0.020f, 0.110f,  0.020f };
		smokeParams.randomPositionRange = 3.5f;
		smokeParams.minScale = 15.0f;
		smokeParams.maxScale = 24.0f;
		smokeParams.endScale = 50.0f;
		smokeParams.scaleEasing = 0.55f;
		smokeParams.minLifeTime = 4.2f;
		smokeParams.maxLifeTime = 6.5f;
		smokeParams.fadeOut = true;
		smokeParams.minColor = { 0.04f, 0.04f, 0.05f, 0.75f };
		smokeParams.maxColor = { 0.09f, 0.09f, 0.11f, 0.90f };
		smokeParams.minRotation = 0.0f;
		smokeParams.maxRotation = kTwoPi;
		smokeParams.minRotationSpeed = -0.15f;
		smokeParams.maxRotationSpeed = 0.15f;
		pm->Emit("ExplosionSmoke", smokeOrigin, smokeParams, 1);
	}

	// 3. 塔頂近傍でパチパチと小さく弾ける火の粉（ExplosionSpark）：
	// ※空中に遠く舞い散って星屑のように見えないよう、速度を 0.03〜0.07m/frame に抑え、
	// 発生源の火炎周辺でのみ短寿命（0.35〜0.65秒）で小さく弾けさせる。
	if ((rand() % 2) == 0) {
		ParticleParameters sparkParams{};
		sparkParams.minVelocity = { -0.035f, 0.030f, -0.035f };
		sparkParams.maxVelocity = {  0.035f, 0.075f,  0.035f };
		sparkParams.randomPositionRange = 2.0f;
		sparkParams.minScale = 0.40f;
		sparkParams.maxScale = 0.85f;
		sparkParams.endScale = 0.05f;
		sparkParams.minLifeTime = 0.35f;
		sparkParams.maxLifeTime = 0.65f;
		sparkParams.fadeOut = true;
		sparkParams.minColor = { 1.0f, 0.55f, 0.10f, 1.0f };
		sparkParams.maxColor = { 1.0f, 0.95f, 0.40f, 1.0f };
		pm->Emit("ExplosionSpark", fireOrigin, sparkParams, 1);
	}
}

void TutorialMissionDirector::CheckWatchtowerCollisions(const TutorialUpdateContext& ctx) {
	if (!outpostVisible_ || outpostPositions_.empty()) {
		return;
	}

	const float radiusSq = kOutpostColliderRadius * kOutpostColliderRadius;

	// 1. 自機との激突衝突判定
	if (ctx.playerFlight) {
		const MyMath::Vector3& pPos = ctx.playerFlight->GetPosition();
		for (const auto& towerPos : outpostPositions_) {
			const float dx = pPos.x - towerPos.x;
			const float dz = pPos.z - towerPos.z;
			const float horizDistSq = dx * dx + dz * dz;

			if (horizDistSq <= radiusSq) {
				const float towerBaseY = towerPos.y;
				const float towerTopY = towerPos.y + kOutpostTotalHeight;
				if (pPos.y >= towerBaseY - 5.0f && pPos.y <= towerTopY + 5.0f) {
					// 監視塔への自機激突：大ダメージ＆爆炎発生
					ctx.playerFlight->GetDamageModel().ProcessHit(DamagePart::Fuse, 150.0f);
					if (EffectManager::GetInstance()) {
						EffectManager::GetInstance()->EmitDestroyEffect(pPos);
					}
					break;
				}
			}
		}
	}

	// 2. 機銃弾（自機弾）と監視塔の着弾・遮蔽判定
	if (ctx.bulletManager) {
		for (auto& bullet : ctx.bulletManager->GetBullets()) {
			if (!bullet.IsAlive() || bullet.IsEnemyBullet()) {
				continue;
			}
			const MyMath::Vector3& bPos = bullet.GetPosition();
			for (const auto& towerPos : outpostPositions_) {
				const float dx = bPos.x - towerPos.x;
				const float dz = bPos.z - towerPos.z;
				const float horizDistSq = dx * dx + dz * dz;

				if (horizDistSq <= radiusSq) {
					const float towerBaseY = towerPos.y;
					const float towerTopY = towerPos.y + kOutpostTotalHeight;
					if (bPos.y >= towerBaseY && bPos.y <= towerTopY) {
						// 監視塔に着弾：火花発生＆弾丸消滅
						if (EffectManager::GetInstance()) {
							EffectManager::GetInstance()->EmitHitEffect(bPos);
						}
						bullet.Kill();
						break;
					}
				}
			}
		}
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
		droneObject_->SetRotate({ kZero, kZero, kZero });
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

	// 4基のローターブレードの高速自転アニメーション
	droneRotorAngle_ += dt * 52.0f;

	// クアッドコプタードローンが前方でゆっくりとホバリング・蛇行飛行（落ち着いて偏差照準・射撃できる速度）
	const float playerSpeed = MyMath::Length(cachedPlayerVel_);
	float forwardSpeed = std::clamp(playerSpeed * kDroneSpeedMatchRatio, kDroneMinSpeed, kDroneMaxSpeed);
	const float distAheadZ = droneCruiseZ_ - cachedPlayerPos_.z;
	if (distAheadZ < kDroneMinDistanceAheadZ) {
		forwardSpeed = std::max(forwardSpeed, playerSpeed * 0.70f);
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
		const float yawAngle = std::atan2(droneVel_.x, std::max(kUnit, droneVel_.z));
		const float pitchAngle = -std::atan2(droneVel_.y, std::max(kUnit, std::hypot(droneVel_.x, droneVel_.z))) * 0.45f;
		const float bankAngle = -droneVel_.x * 0.012f;
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
		const float pullUpCenterX = EvaluateCanyonCenterX(pullUpZ);
		const float pullUpFloorY = QueryTerrainY(ctx.terrain, pullUpCenterX, pullUpZ);
		westernDiveEndPos_ = {
			pullUpCenterX,
			std::max(layout_.westernPullUp.y, pullUpFloorY + kWesternBaseAglMeters),
			pullUpZ
		};
	} else {
		westernDiveStartPos_ = MyMath::Add(cachedPlayerPos_, kWesternDiveSpawnOffset);
		westernDiveMidPos_ = MyMath::Add(cachedPlayerPos_, kWesternDivePassOffset);
		const float pullUpZ = cachedPlayerPos_.z + kWesternPullUpOffset.z;
		const float pullUpCenterX = EvaluateCanyonCenterX(pullUpZ);
		const float pullUpFloorY = QueryTerrainY(ctx.terrain, pullUpCenterX, pullUpZ);
		westernDiveEndPos_ = {
			pullUpCenterX,
			std::max(cachedPlayerPos_.y + kWesternPullUpOffset.y, pullUpFloorY + kWesternBaseAglMeters),
			pullUpZ
		};
	}

	westernPos_ = westernDiveStartPos_;
	westernVel_ = { kZero, -kCoraxMaxSpeed, kCoraxMaxSpeed };
	westernCruiseZ_ = westernDiveEndPos_.z;
	westernCurrentSpeed_ = kCoraxMaxSpeed * 0.85f;
	westernWeavePhaseX_ = kZero;
	westernWeavePhaseY_ = kZero;
	westernSmoothedGroundY_ = westernDiveEndPos_.y - kWesternBaseAglMeters;

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
			westernPos_ = westernDiveEndPos_;
			westernCruiseZ_ = westernDiveEndPos_.z;
			westernCurrentSpeed_ = std::max(kDroneMinSpeed, MyMath::Length(cachedPlayerVel_) * kWesternSpeedMatchRatio);
			westernWeavePhaseX_ = kZero;
			westernWeavePhaseY_ = kZero;
			westernSmoothedGroundY_ = westernDiveEndPos_.y - kWesternBaseAglMeters;

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

	// 部位損傷が蓄積すると旋回振幅・周波数・推力が低下
	const float currentWeaveAmpX = isWingDamaged ? kWesternWeaveDegradedAmpX : kWesternWeaveBaseAmpX;
	const float currentWeaveFreq = isWingDamaged ? kWesternWeaveDegradedFreq : kWesternWeaveBaseFreq;
	const float engineSpeedFactor = isEngineDamaged ? kWesternDamagedSpeedPenalty : kUnit;

	// 位相の積分（周波数が変化しても位相ジャンプによる瞬間移動が絶対に起きない）
	westernWeavePhaseX_ += currentWeaveFreq * dt;
	westernWeavePhaseY_ += kWesternWeaveFreqY * dt;

	// 前後目標速度の滑らかな補間（急激な速度切り替えの防止）
	const float playerSpeed = MyMath::Length(cachedPlayerVel_);
	float desiredForwardSpeed = std::clamp(
		playerSpeed * kWesternSpeedMatchRatio * engineSpeedFactor,
		kDroneMinSpeed,
		kCoraxMaxSpeed);

	const float distAheadZ = westernCruiseZ_ - cachedPlayerPos_.z;
	if (distAheadZ < kWesternMinAheadZ) {
		desiredForwardSpeed = std::min(kCoraxMaxSpeed, playerSpeed * 1.12f);
	} else if (distAheadZ > kWesternMaxAheadZ) {
		desiredForwardSpeed = std::max(kDroneMinSpeed, playerSpeed * 0.78f);
	}

	westernCurrentSpeed_ += (desiredForwardSpeed - westernCurrentSpeed_) * std::clamp(dt * 3.0f, kZero, kUnit);
	westernCruiseZ_ += westernCurrentSpeed_ * dt;

	// 峡谷中心線および地形高度の滑らかな追従（断崖絶壁による上下瞬間移動を完全に排除）
	const float canyonCenterX = EvaluateCanyonCenterX(westernCruiseZ_);
	const float rawFloorY = QueryTerrainY(ctx.terrain, canyonCenterX, westernCruiseZ_);
	// 地形高度のローパスフィルター平滑化（崖の急激な段差を吸収）
	westernSmoothedGroundY_ += (rawFloorY - westernSmoothedGroundY_) * std::clamp(dt * 2.2f, kZero, kUnit);

	// 峡谷の安全回廊内に収まるよう振幅を制限（最大42m：岩壁へのめり込み防止）
	const float effectiveAmpX = std::min(currentWeaveAmpX, 42.0f);
	const float targetX = canyonCenterX + std::sin(westernWeavePhaseX_) * effectiveAmpX;
	const float targetY = westernSmoothedGroundY_ + kWesternBaseAglMeters + std::sin(westernWeavePhaseY_) * kWesternWeaveAmpY;
	const MyMath::Vector3 targetPos = { targetX, targetY, westernCruiseZ_ };

	// 目標位置への滑らかな物理追従（PD追従制御：テレポートを根絶し、滑らかな飛行軌道を実現）
	const MyMath::Vector3 toTarget = MyMath::Subtract(targetPos, westernPos_);
	westernVel_ = {
		toTarget.x * 4.2f,
		toTarget.y * 3.2f,
		westernCurrentSpeed_
	};
	westernPos_ = MyMath::Add(westernPos_, MyMath::Multiply(dt, westernVel_));

	const float bankRoll = -std::clamp(westernVel_.x * 0.015f, -0.65f, 0.65f);
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

	// 峡谷境界ライン通過、およびCorax最終帰投無線「急ぐぞ、これから何かが始まる……！」の完了後にフェードアウトへ移行
	const bool radioActive = (ctx.radio && ctx.radio->IsActive());
	const bool crossedBoundary = (cachedPlayerPos_.z >= exitBoundaryPos_.z);
	const bool autoReturnReady = (phaseTimer_ >= kExitBoundaryAutoTriggerSeconds);

	// ★重要: 峡谷境界ラインを通過していても、最終無線「急ぐぞ、これから何かが始まる……！」が
	// 画面上に完全に表示され、読み終えて消えるまでは絶対にフェードアウト・ミッション終了しない！
	if ((crossedBoundary || autoReturnReady) && !radioActive) {
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

	// 1. Object3dパイプライン（隊長機Corax・峡谷監視塔群・訓練ドローン・西側戦闘機）
	Object3dCommon::GetInstance()->SetupCommonState();
	Object3dCommon::GetInstance()->SetLightType(kOneInt);

	coraxModel_.Draw();

	// 峡谷監視塔群（Resources/models/tower.obj）の常時描画（最初から常時配置・描画）
	if (outpostVisible_) {
		for (size_t i = kZeroIndex; i < watchtowerObjects_.size(); ++i) {
			auto& obj = watchtowerObjects_[i];
			if (obj) {
				obj->SetCamera(camera);
				obj->Update();
				obj->Draw();
			}
		}
	}

	if (phase_ == HalcyonPhase::Phase5_DroneLeadSight && droneAlive_ && droneObject_) {
		droneObject_->Draw();
	}

	if (westernVisible_) {
		westernModel_.Draw();
	}

	// 2. PrimitiveModelパイプライン（アフターバーナー・ウェイポイントリング・航空障害灯・帰投境界ライン）
	PrimitiveModel* prim = PrimitiveModel::GetInstance();
	if (!prim) {
		return;
	}

	// 隊長機『Corax』アフターバーナー（推力噴射炎）の描画
	// ※PrimitiveModel描画のため、Object3dパイプライン終了後に描画してステート競合を防ぐ
	if (coraxAfterburner_) {
		MyMath::Vector3 right{}, up{}, fwd{};
		BuildAircraftBasis(coraxForward_, coraxRollRad_, right, up, fwd);
		const MyMath::Matrix4x4 coraxWorld = BuildWorldMatrixFromBasis(right, up, fwd, coraxPos_);
		const MyMath::Matrix4x4 baseRotate = MyMath::MakeAffineMatrix(
			MyMath::Vector3{ kUnit, kUnit, kUnit },
			MyMath::Vector3{ kZero, kPi, kZero },
			MyMath::Vector3{ kZero, kZero, kZero });
		const MyMath::Matrix4x4 visualWorld = MyMath::Multiply(baseRotate, coraxWorld);
		coraxAfterburner_->Draw(visualWorld, camera);
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

		// Phase 3: 次のゲートへ向かう誘導パスライン（光のストリーマー・点線誘導）
		if (passedGateCount_ < static_cast<int>(gates_.size())) {
			const MyMath::Vector3& targetGatePos = gates_[passedGateCount_].position;
			MyMath::Vector3 startPos = cachedPlayerPos_;
			if (passedGateCount_ > kZeroInt) {
				startPos = gates_[passedGateCount_ - 1].position;
			}
			const MyMath::Vector3 delta = MyMath::Subtract(targetGatePos, startPos);
			const float totalDist = MyMath::Length(delta);
			if (totalDist > kUnit) {
				const MyMath::Vector3 dir = MyMath::Multiply(kUnit / totalDist, delta);
				constexpr float kSegLen = 22.0f;
				constexpr float kGapLen = 16.0f;
				constexpr float kCycleLen = kSegLen + kGapLen;
				const float offset = std::fmod(animTimer_ * 48.0f, kCycleLen);
				for (float d = offset; d < totalDist; d += kCycleLen) {
					const float dEnd = std::min(d + kSegLen, totalDist);
					const MyMath::Vector3 p1 = MyMath::Add(startPos, MyMath::Multiply(d, dir));
					const MyMath::Vector3 p2 = MyMath::Add(startPos, MyMath::Multiply(dEnd, dir));
					prim->DrawLine3D(p1, p2, { 0.28f, 0.98f, 0.55f, 0.55f }, camera);
				}
			}

			// さらにその次のゲート（passedGateCount_ + 1）への先行誘導ラインも薄く描画
			if (passedGateCount_ + 1 < static_cast<int>(gates_.size())) {
				const MyMath::Vector3& nextGatePos = gates_[passedGateCount_ + 1].position;
				const MyMath::Vector3 deltaNext = MyMath::Subtract(nextGatePos, targetGatePos);
				const float distNext = MyMath::Length(deltaNext);
				if (distNext > kUnit) {
					const MyMath::Vector3 dirNext = MyMath::Multiply(kUnit / distNext, deltaNext);
					constexpr float kSegLen = 22.0f;
					constexpr float kGapLen = 22.0f;
					constexpr float kCycleLen = kSegLen + kGapLen;
					for (float d = kZero; d < distNext; d += kCycleLen) {
						const float dEnd = std::min(d + kSegLen, distNext);
						const MyMath::Vector3 p1 = MyMath::Add(targetGatePos, MyMath::Multiply(d, dirNext));
						const MyMath::Vector3 p2 = MyMath::Add(targetGatePos, MyMath::Multiply(dEnd, dirNext));
						prim->DrawLine3D(p1, p2, { 0.18f, 0.72f, 0.38f, 0.30f }, camera);
					}
				}
			}
		}
	}

	// 峡谷監視塔群の航空障害灯（夜間衝突防止ビーコン）
	// ※ミッション開始時から全基の塔頂に赤色点滅ビーコンを常時点灯し、遠方からの視認性を確保
	if (outpostVisible_ && !outpostPositions_.empty()) {
		const size_t towerCount = outpostPositions_.size();
		for (size_t i = kZeroIndex; i < towerCount; ++i) {
			const bool isIncidentTower = (i + kIndexStep == towerCount);
			const MyMath::Vector3& towerPos = outpostPositions_[i];
			const MyMath::Vector3 beaconPos = {
				towerPos.x,
				towerPos.y + kOutpostTowerHeight,
				towerPos.z
			};

			if (isIncidentTower && outpostIgnited_) {
				// 被弾炎上後は電気系統ショートにより激しく不規則に明滅
				const float shortFlicker = (std::sin(animTimer_ * 22.0f) > 0.1f) ? 0.85f : 0.15f;
				MyMath::Vector4 shortCol = kObstacleBeaconColor;
				shortCol.w *= shortFlicker;
				prim->DrawRing(
					{ kObstacleBeaconRadius * 0.9f, kObstacleBeaconRadius * 0.9f, kGateRingDepthScale },
					{ kHalfPi, kZero, animTimer_ },
					beaconPos,
					shortCol,
					ringTexIndex_,
					camera,
					BlendMode::kAdd);
			} else {
				// 平常時：夜間航空安全のための赤色点滅ビーコン（1.5Hzパルス明滅）
				const float pulse = std::max(0.0f, std::sin(animTimer_ * kObstacleBeaconBlinkRate));
				const float beaconAlpha = std::pow(pulse, 3.0f); // 鋭いパルス点灯
				if (beaconAlpha > 0.05f) {
					MyMath::Vector4 beaconCol = kObstacleBeaconColor;
					beaconCol.w *= beaconAlpha;
					prim->DrawRing(
						{ kObstacleBeaconRadius, kObstacleBeaconRadius, kGateRingDepthScale },
						{ kHalfPi, kZero, kZero },
						beaconPos,
						beaconCol,
						ringTexIndex_,
						camera,
						BlendMode::kAdd);

					// 遠距離からの視認性を高める十字クロス光
					constexpr float kCrossLen = 2.2f;
					prim->DrawLine3D(
						{ beaconPos.x - kCrossLen, beaconPos.y, beaconPos.z },
						{ beaconPos.x + kCrossLen, beaconPos.y, beaconPos.z },
						beaconCol,
						camera);
					prim->DrawLine3D(
						{ beaconPos.x, beaconPos.y, beaconPos.z - kCrossLen },
						{ beaconPos.x, beaconPos.y, beaconPos.z + kCrossLen },
						beaconCol,
						camera);
				}
			}
		}
	}

	// Phase 5: 4ローター・クアッドコプタードローン視認補助ビーコン＆回転ローター光輪
	if (phase_ == HalcyonPhase::Phase5_DroneLeadSight && droneAlive_) {
		prim->DrawRing(
			{ kDroneBeaconRadius, kDroneBeaconRadius, kUnit },
			{ kZero, kZero, animTimer_ * kTwo },
			dronePos_,
			kDroneHudColor,
			ringTexIndex_,
			camera,
			BlendMode::kAdd);

		// クアッドコプターの4基の高速回転ローターブレード光輪を描画
		static constexpr MyMath::Vector3 kRotorOffsets[4] = {
			{ -1.60f * kDroneScale, 0.35f * kDroneScale,  1.60f * kDroneScale }, // FL
			{  1.60f * kDroneScale, 0.35f * kDroneScale,  1.60f * kDroneScale }, // FR
			{ -1.60f * kDroneScale, 0.35f * kDroneScale, -1.60f * kDroneScale }, // RL
			{  1.60f * kDroneScale, 0.35f * kDroneScale, -1.60f * kDroneScale }, // RR
		};
		const MyMath::Vector4 rotorCol = { 0.30f, 0.80f, 1.00f, 0.60f };
		for (int r = 0; r < 4; ++r) {
			const MyMath::Vector3 rotorWorldPos = MyMath::Add(dronePos_, kRotorOffsets[r]);
			prim->DrawRing(
				{ kDroneRotorVisualRadius, kDroneRotorVisualRadius, 1.0f },
				{ kHalfPi, kZero, droneRotorAngle_ * ((r % 2 == 0) ? 1.0f : -1.0f) },
				rotorWorldPos,
				rotorCol,
				ringTexIndex_,
				camera,
				BlendMode::kAdd);
		}
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

bool TutorialMissionDirector::IsGuideDialogVisible() const {
	if (!isActive_ || !isInitialized_ || phase_ == HalcyonPhase::Inactive) {
		return false;
	}

	switch (phase_) {
	case HalcyonPhase::Phase1_BasicControls:
		// 00:13の操作案内出現〜動翼点検クリアまで表示（ミッション開始直後の会話中やクリア待機中は非表示）
		return (missionClock_ >= kPhase1UiAppearTime) && !p1CompletedWaitingDelay_;

	case HalcyonPhase::Phase2_ThrottleCruise:
		return true;

	case HalcyonPhase::Phase3_CanyonWaypoints:
		// 全ゲート通過後の待機遅延中（4.2秒）は非表示
		return !p3CompletedWaitingDelay_;

	case HalcyonPhase::Phase4_OutpostIncident:
		return true;

	case HalcyonPhase::Phase5_DroneLeadSight:
		// ドローン生存中のみ表示（撃破後は非表示）
		return droneAlive_ && !p5CompletedWaitingDelay_;

	case HalcyonPhase::Phase6_WesternAmbush:
		// 急襲フライバイ中（westernInDivePass_）および撃墜きりもみ中（westernDownedSpiral_）は非表示
		return !westernInDivePass_ && !westernDownedSpiral_;

	case HalcyonPhase::Phase7_ExitBoundary:
		// 帰投境界通過前のみガイド表示（境界通過後はガイドを消して最終無線字幕に集中）
		return exitBoundaryVisible_ && (cachedPlayerPos_.z < exitBoundaryPos_.z);

	case HalcyonPhase::Phase8_DebriefFadeOut:
	default:
		return false;
	}
}

bool TutorialMissionDirector::IsPlayerSteeringAllowed() const {
	if (!isActive_) {
		return true;
	}
	return IsGuideDialogVisible();
}

bool TutorialMissionDirector::IsPlayerThrottleAllowed() const {
	if (!isActive_) {
		return true;
	}
	// Phase 1 では基礎姿勢制御の訓練のためスロットルは自動巡航固定
	if (phase_ == HalcyonPhase::Phase1_BasicControls) {
		return false;
	}
	return IsGuideDialogVisible();
}

bool TutorialMissionDirector::IsPlayerWeaponAllowed() const {
	if (!isActive_) {
		return true;
	}
	// 射撃はドローン迎撃（Phase 5）および西側機ドッグファイト（Phase 6）かつガイド表示中のみ許可
	const bool combatPhase =
		(phase_ == HalcyonPhase::Phase5_DroneLeadSight) ||
		(phase_ == HalcyonPhase::Phase6_WesternAmbush);
	return combatPhase && IsGuideDialogVisible();
}

void TutorialMissionDirector::DrawTutorialGuideHUD(
	float screenWidth,
	float screenHeight,
	float hudScale) {
	if (!IsGuideDialogVisible()) {
		return;
	}

	std::string titleText;
	std::string subText;
	bool showProgressBar = false;
	float progressRatio = kZero;

	switch (phase_) {
	case HalcyonPhase::Phase1_BasicControls:
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

	// 3. 峡谷監視所HUDマーカー（ミッション開始時から常時表示し、Phase 4で緊急警報マーカーへ変化）
	if (outpostVisible_ && !outpostPositions_.empty()) {
		const MyMath::Vector3 outpostTop = {
			outpostPos_.x,
			outpostPos_.y + kOutpostTowerHeight,
			outpostPos_.z
		};
		if (phase_ == HalcyonPhase::Phase4_OutpostIncident || outpostIgnited_) {
			drawWorldMarker(outpostTop, kOutpostHudLabel, kWrnTagColor);
		} else if (phase_ <= HalcyonPhase::Phase4_OutpostIncident) {
			drawWorldMarker(outpostTop, kOutpostIntactHudLabel, kOutpostIntactColor);
		}
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
