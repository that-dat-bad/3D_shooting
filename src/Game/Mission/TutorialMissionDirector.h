#pragma once
#include <array>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/base/Math/Vector2.h"
#include "../../engine/Graphics/Camera/Camera.h"
#include "../../engine/Graphics/Model/Object3d.h"
#include "../../engine/Graphics/Sprite/Sprite.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../Bullet/BulletManager.h"
#include "../Environment/TerrainChunkManager.h"
#include "../FlightModel/FlightModel.h"
#include "../FlightModel/MouseAimController.h"
#include "../FlightModel/Payload/Gunpod.h"
#include "../Graphics/AircraftVisualModel.h"
#include "../Graphics/Afterburner.h"
#include "../System/MissionManager.h"
#include "../UI/RadioCommunicator.h"

class EffectHandle;

// チュートリアル定数
namespace HalcyonDefaults {
	// --- 基本数値定数 ---
	inline constexpr int kZeroInt = 0;
	inline constexpr int kOneInt = 1;
	inline constexpr int kTwoInt = 2;
	inline constexpr int kThreeInt = 3;
	inline constexpr int kFourInt = 4;
	inline constexpr size_t kZeroIndex = 0;
	inline constexpr size_t kIndexStep = 1;
	inline constexpr int kSecondsPerMinute = 60;

	inline constexpr float kZero = 0.0f;
	inline constexpr float kQuarter = 0.25f;
	inline constexpr float kHalf = 0.5f;
	inline constexpr float kUnit = 1.0f;
	inline constexpr float kTwo = 2.0f;
	inline constexpr float kThree = 3.0f;
	inline constexpr float kPi = 3.14159265358979f;
	inline constexpr float kTwoPi = 6.28318530717958f;
	inline constexpr float kHalfPi = 1.57079632679489f;
	inline constexpr float kEpsilon = 0.0001f;
	inline constexpr float kPercentScale = 100.0f;

	// --- 画面・スプライトプール定数 ---
	inline constexpr size_t kInitialRectPoolCapacity = 192;
	inline constexpr size_t kInitialRingPoolCapacity = 24;
	inline constexpr float kReferenceScreenWidth = 1280.0f;
	inline constexpr float kReferenceScreenHeight = 720.0f;
	inline constexpr float kMinHudScale = 0.70f;
	inline constexpr float kMaxHudScale = 1.45f;
	inline constexpr float kHudClampMarginPx = 44.0f;

	// --- ミッション識別・モデル・テクスチャパス定数 ---
	inline constexpr const char* kMissionTitle = "Operation Halcyon Days";
	inline constexpr const char* kObuchenieTitle = "Obuchenie";
	inline constexpr const char* kZadanie00Alias = "ZADANIE 00";
	inline constexpr const char* kMissionFileSuffix = "mission00.json";
	inline constexpr float kMinForwardSafetyOffsetZ = 600.0f;
	inline constexpr const char* kFontName = "HackGen";
	inline constexpr const char* kWhiteTexPath = "assets/textures/white1x1.png";
	inline constexpr const char* kRingTexPath = "assets/textures/circle2.png";
	inline constexpr const char* kCoraxModelPath = "Resources/models/m21.gltf";
	inline constexpr const char* kDroneModelPath = "Resources/models/quadcopter.obj";
	inline constexpr const char* kWesternFighterModelPath = "Resources/models/m15.gltf";
	inline constexpr const char* kExplosionFireGroup = "ExplosionFire";
	inline constexpr const char* kExplosionSmokeGroup = "ExplosionSmoke";
	inline constexpr const char* kExplosionSparkGroup = "ExplosionSpark";

	// --- 登場人物・コールサイン定数 ---
	inline constexpr const char* kCoraxSpeakerName = "Corax";
	inline constexpr const char* kOutpostSpeakerName = "第4監視所";
	inline constexpr const char* kCoraxHudLabel = "ALLY // CORAX [M-21A]";
	inline constexpr const char* kOutpostHudLabel = "OUTPOST 04 // CARRIER LOST";
	inline constexpr const char* kOutpostIntactHudLabel = "OUTPOST 04";
	inline constexpr const char* kDroneHudLabel = "TRAINING DRONE [ORANGE]";
	inline constexpr const char* kWesternHudLabel = "UNKNOWN FIGHTER [WESTERN]";
	inline constexpr const char* kBoundaryHudLabel = "RTB SECTOR BOUNDARY";

	// --- 峡谷ルート・地形高度定数 ---
	inline constexpr size_t kCanyonControlPointCount = 9;
	inline constexpr float kFallbackCanyonFloorY = -485.0f;
	inline constexpr float kPullUpWarningAglMeters = 64.0f;
	inline constexpr float kHighAltitudeCautionY = -65.0f;
	inline constexpr float kCoraxPatrolAglMeters = 148.0f;
	inline constexpr float kCoraxLeadDistanceZ = 135.0f;
	inline constexpr float kCoraxLateralOffsetX = 24.0f;
	inline constexpr float kCoraxMinSpeed = 68.0f;
	inline constexpr float kCoraxMaxSpeed = 235.0f;
	inline constexpr float kCoraxFollowStiffness = 2.6f;
	inline constexpr float kCoraxPosSmoothRate = 4.2f;
	inline constexpr float kCoraxBankGain = 1.6f;
	inline constexpr float kCoraxMaxBankRad = 0.95f;
	inline constexpr float kCoraxRollSmoothRate = 5.0f;
	inline constexpr float kCoraxMaterialShininess = 48.0f;
	inline constexpr float kCoraxMaterialSpecular = 1.15f;
	inline constexpr float kCoraxMaterialEnv = 0.42f;
	inline constexpr MyMath::Vector4 kCoraxTintColor = { 0.86f, 0.92f, 0.98f, 1.00f };

	// --- Phase 1: 起動と基本操作（視線誘導・水平復帰）定数 ---
	inline constexpr float kPhase1SysLog0Time = 0.00f;
	inline constexpr float kPhase1SysLog1Time = 1.00f;
	inline constexpr float kPhase1SysLog2Time = 2.00f;
	inline constexpr float kPhase1Radio1Time = 3.00f;
	inline constexpr float kPhase1Radio2Time = 6.00f;
	inline constexpr float kPhase1Radio3Time = 11.00f;
	inline constexpr float kPhase1UiAppearTime = 13.00f;
	inline constexpr float kPhase1Radio1Hold = 2.60f;
	inline constexpr float kPhase1Radio2Hold = 4.50f;
	inline constexpr float kPhase1Radio3Hold = 4.80f;
	inline constexpr float kPhase1RequiredMoveSeconds = 1.50f;
	inline constexpr float kAimDirectionDeltaThreshold = 0.0025f;
	inline constexpr long kMousePixelDeltaThreshold = 1;

	inline constexpr const char* kSysLogInit = "DAWN_OS [v4.12] // AVIONICS INITIALIZED";
	inline constexpr const char* kSysLogUnit = "UNIT: STAR-BORDER-04 // CALLSIGN: ROOKIE";
	inline constexpr const char* kSysLogInstructor = "FLIGHT INSTRUCTOR: ACTIVE // LEVEL STABILIZER: ON";
	inline constexpr const char* kPhase1Radio1Text = "……感度良好。聞こえているな、新人。";
	inline constexpr const char* kPhase1Radio2Text =
		"ようこそ、条約軍（СТАР）外縁国境警備空隊へ。\n中央からこんな極夜の最果てに飛ばされてくるとは、運のない奴だ。";
	inline constexpr const char* kPhase1Radio3Text =
		"まずはアビオニクスと動翼の点検だ。マウスを動かして正面の視界を振ってみろ。\n手を離せばインストラクターが勝手に水平に戻してくれる。";
	inline constexpr const char* kPhase1GuideTitle = "【マウス移動】視界・機首の変更";
	inline constexpr const char* kPhase1GuideSub = "※マウス操作を止めると、インストラクター機能により自動で水平飛行へ復帰します。";

	// --- Phase 2: 加減速とスロットル操作定数 ---
	inline constexpr float kPhase2StartDelaySeconds = 0.50f;
	inline constexpr float kPhase2ThrottleThreshold = 0.80f;
	inline constexpr float kPhase2RequiredHoldSeconds = 3.00f;
	inline constexpr float kPhase2RadioHold = 4.60f;

	inline constexpr const char* kPhase2RadioText =
		"動翼の反応は問題ないな。次は推力だ。\nスロットルを押し込んで巡航速度まで上げろ。遅れると置いていくぞ。";
	inline constexpr const char* kPhase2GuideTitle = "[ W ] スロットル加速（WEP） ／ [ S ] エアブレーキ減速";
	inline constexpr const char* kPhase2GuideSub = "スロットル開度 80% 以上を 3.0 秒間維持してください。";
	inline constexpr const char* kPhase2SysLogComplete = "THROTTLE SPOOL-UP CONFIRMED: 85% // CRUISE SPEED ACHIEVED";

	// --- Phase 3: 峡谷通過（低空地形追従）定数 ---
	inline constexpr size_t kGateCount = 3;
	inline constexpr float kFirstGateDistanceAheadZ = 950.0f;
	inline constexpr float kSubsequentGateSpacingZ = 900.0f;
	inline constexpr float kGateAglHeight0 = 138.0f;
	inline constexpr float kGateAglHeight1 = 128.0f;
	inline constexpr float kGateAglHeight2 = 122.0f;
	inline constexpr float kGateVisualRadius = 52.0f;
	inline constexpr float kGateOuterGlowScale = 1.25f;
	inline constexpr float kGateRingDepthScale = 4.0f;
	inline constexpr float kGateTickLength = 16.0f;
	inline constexpr float kGateCaptureSphereRadius = 65.0f;
	inline constexpr float kGatePlaneCorridorX = 65.0f;
	inline constexpr float kGatePlaneCorridorY = 60.0f;
	inline constexpr float kGatePassOvershootToleranceZ = 280.0f;
	inline constexpr float kGatePlaneThicknessZ = 45.0f;
	inline constexpr float kGateSpinSpeedRad = 0.55f;
	inline constexpr float kPhase3Radio1Hold = 3.60f;
	inline constexpr float kPhase3Radio2Hold = 4.50f;
	inline constexpr float kPhase3ClearRadioHold = 3.80f;

	inline constexpr const char* kPhase3Radio1Text = "峡谷の哨戒ルートに入る。前方のガイドマーカーを順に通っていけ。";
	inline constexpr const char* kPhase3Radio2Text =
		"高度を上げすぎるなよ。上空は突風と防空網の管轄だ。\n谷底を這うのがこの辺境の生き残り方だ。";
	inline constexpr const char* kPhase3ClearRadioText = "フン、機体は擦らずに済んだようだな。点検飛行はこれで終わり――";
	inline constexpr const char* kPhase3GuideFormat = "ウェイポイント（緑のゲート）を通過してください（%d/%d）";
	inline constexpr const char* kPhase3GuideSub = "峡谷の谷底ルートに沿って低空を維持し、緑のリングマーカーを通過せよ。";
	inline constexpr const char* kPullUpAlertText = "[ ! PULL UP ! 地面接近警告 ]";
	inline constexpr const char* kHighAltCautionText = "[ ! ALTITUDE CAUTION : 峡谷低空を維持せよ ! ]";

	// --- Phase 4: 異変（通信途絶と監視所炎上）定数 ---
	inline constexpr const char* kWatchtowerModelPath = "Resources/models/tower.obj";
	inline constexpr float kWatchtowerModelScale = 2.8f;
	inline constexpr float kWatchtowerModelHeight = 12.83f;
	inline constexpr size_t kDefaultWatchtowerCount = 4;
	inline constexpr float kWatchtowerFirstOffsetZ = 1150.0f;
	inline constexpr float kWatchtowerSpacingZ = 980.0f;
	inline constexpr float kPhase4StartDelaySeconds = 4.20f;
	inline constexpr float kOutpostSpawnAheadZ = 1200.0f;
	inline constexpr float kOutpostShelfOffsetX = 118.0f;
	inline constexpr float kOutpostTowerRadius = 5.5f;
	inline constexpr float kOutpostTowerHeight = kWatchtowerModelHeight * kWatchtowerModelScale;
	inline constexpr float kOutpostDeckRadius = 11.5f;
	inline constexpr float kOutpostDeckHeight = 4.5f;
	inline constexpr float kOutpostMastHeight = 14.0f;
	inline constexpr float kOutpostColliderRadius = 6.5f;
	inline constexpr float kOutpostTotalHeight = kOutpostTowerHeight;
	inline constexpr float kOutpostFireGlowRadius = 16.0f;
	inline constexpr float kOutpostSmokeEmitInterval = 0.16f;
	inline constexpr float kOutpostCornerTriggerOffsetZ = -45.0f;
	inline constexpr float kOutpostMinPhaseDuration = 4.20f;
	inline constexpr float kPhase4WarnLog2Delay = 0.65f;
	inline constexpr float kPhase4OutpostRadioHold = 4.20f;
	inline constexpr float kPhase4CoraxRadio1Hold = 3.40f;
	inline constexpr float kPhase4CoraxRadio2Hold = 3.40f;

	inline constexpr const char* kPhase4WrnLog = "UNENCRYPTED FREQ DETECTED // FREQ: 148.95 MHz";
	inline constexpr const char* kPhase4AlrLog = "OUTPOST 04 TELEMETRY: CARRIER LOST (SIGNAL TERMINATED)";
	inline constexpr const char* kPhase4OutpostRadioText =
		"……zzt……本部！ 応答しろ！\n峡谷上空から所属不明機が……kzz……急降下して――（爆発音・ノイズ）";
	inline constexpr const char* kPhase4CoraxRadio1Text = "……おい、第4監視所が落ちたぞ！？ 新人、前方の視界を開けろ！";
	inline constexpr const char* kPhase4CoraxRadio2Text = "監視塔から黒煙が上がっている……なんだありゃあ？";
	inline constexpr const char* kPhase4GuideTitle = "【緊急確認】黒煙の上がる第4監視所へ接近せよ";
	inline constexpr const char* kPhase4GuideSub = "前方の峡谷コーナーに位置する監視塔の上空を通過して状況を確認してください。";

	// --- Phase 5: 模擬標的／偏差射撃定数 (4ローター・クアッドコプター大型低速ドローン) ---
	inline constexpr float kDroneSpawnAheadZ = 380.0f;
	inline constexpr float kDroneBaseAglMeters = 110.0f;
	inline constexpr float kDroneScale = 2.4f;
	inline constexpr float kDroneMaxHp = 40.0f;
	inline constexpr float kDroneHitRadius = 9.5f;
	inline constexpr float kDroneWeaveAmplitudeX = 42.0f;
	inline constexpr float kDroneWeaveFrequency = 0.42f;
	inline constexpr float kDroneWeaveAmplitudeY = 12.0f;
	inline constexpr float kDroneWeaveFreqY = 0.55f;
	inline constexpr float kDroneMinDistanceAheadZ = 160.0f;
	inline constexpr float kDroneMaxDistanceAheadZ = 350.0f;
	inline constexpr float kDroneMinSpeed = 26.0f;
	inline constexpr float kDroneMaxSpeed = 42.0f;
	inline constexpr float kDroneSpeedMatchRatio = 0.35f;
	inline constexpr float kDroneBeaconRadius = 4.8f;
	inline constexpr float kDroneRotorVisualRadius = 1.45f;
	inline constexpr MyMath::Vector4 kDroneOrangeColor = { 1.00f, 0.46f, 0.06f, 1.00f };
	inline constexpr float kPhase5Log2Delay = 0.45f;
	inline constexpr float kPhase5Radio1Hold = 3.60f;
	inline constexpr float kPhase5Radio2Hold = 4.60f;
	inline constexpr int kMinTutorialAmmoReserve = 180;

	// リードコンピューティングサイト（偏差射撃計算）定数
	inline constexpr float kGunMuzzleOffsetForward = 8.0f;
	inline constexpr float kGunBulletMuzzleSpeed = 500.0f;
	inline constexpr float kMinClosingBulletSpeed = 180.0f;
	inline constexpr float kMaxLeadInterceptTime = 2.80f;
	inline constexpr float kLeadMarkerRadiusPx = 15.0f;
	inline constexpr float kLeadInnerDotRadiusPx = 2.5f;
	inline constexpr float kHitMarkerDuration = 0.22f;

	inline constexpr const char* kPhase5SysLog1 = "TARGET ACQUIRED // IFF: TRAINING DRONE [ORANGE]";
	inline constexpr const char* kPhase5SysLog2 = "LEAD COMPUTING SIGHT: ENGAGED";
	inline constexpr const char* kPhase5Radio1Text = "防衛用の自動ドローンが暴走して飛び出したか。銃の試射代わりに撃ち落とせ！";
	inline constexpr const char* kPhase5Radio2Text =
		"機体本体を直接撃つなよ。弾が届く未来位置――\n照準円の少し前（リードマーカー）を狙って引き金を引け！";
	inline constexpr const char* kPhase5GuideTitle = "【左クリック】ガンポッド射撃";
	inline constexpr const char* kPhase5GuideSub = "目標前方に表示される【リードマーカー（○）】を狙って撃墜してください。";
	inline constexpr const char* kPhase5SysLogDestroyed = "TARGET DESTROYED // RESIDUAL BALLISTICS EXPENDED";

	// --- Phase 6: 実戦・西側機の急襲（部位破壊体験）定数 ---
	inline constexpr float kPhase6StartDelaySeconds = 1.50f;
	inline constexpr float kPhase6LogRwrDelay = 0.35f;
	inline constexpr float kPhase6LogSpecDelay = 0.75f;
	inline constexpr float kPhase6LogIntelDelay = 1.15f;
	inline constexpr float kPhase6DivePassDuration = 2.35f;
	inline constexpr MyMath::Vector3 kWesternDiveSpawnOffset = { 195.0f, 285.0f, 240.0f };
	inline constexpr MyMath::Vector3 kWesternDivePassOffset = { -32.0f, 26.0f, 155.0f };
	inline constexpr MyMath::Vector3 kWesternPullUpOffset = { -65.0f, 65.0f, 360.0f };
	inline constexpr float kWesternStrafeFireInterval = 0.055f;
	inline constexpr float kWesternStrafeBulletSpeed = 620.0f;
	inline constexpr float kWesternStrafeSafeMissOffset = 15.0f;
	inline constexpr float kWesternStrafeDamage = 4.0f;
	inline constexpr float kWesternFlybyShakeIntensity = 0.38f;
	inline constexpr float kWesternFlybyShakeDuration = 0.50f;

	// 西側機ドッグファイト＆部位破壊定数
	inline constexpr MyMath::Vector4 kWesternAirframeColor = { 0.56f, 0.61f, 0.68f, 1.00f };
	inline constexpr float kWesternWingPartMaxHp = 45.0f;
	inline constexpr float kWesternEnginePartMaxHp = 50.0f;
	inline constexpr float kWesternOuterHitEnvelope = 15.5f;
	inline constexpr float kWesternWingSpanSplitLocalX = 1.25f;
	inline constexpr float kWesternEngineRearSplitLocalZ = -1.20f;
	inline constexpr float kWesternCenterToWingStressRatio = 0.42f;
	inline constexpr float kWesternDamagedThresholdRatio = 0.60f;
	inline constexpr float kWesternWeaveBaseAmpX = 125.0f;
	inline constexpr float kWesternWeaveDegradedAmpX = 42.0f;
	inline constexpr float kWesternWeaveBaseFreq = 0.78f;
	inline constexpr float kWesternWeaveDegradedFreq = 0.34f;
	inline constexpr float kWesternWeaveAmpY = 28.0f;
	inline constexpr float kWesternWeaveFreqY = 1.05f;
	inline constexpr float kWesternBaseAglMeters = 156.0f;
	inline constexpr float kWesternMinAheadZ = 240.0f;
	inline constexpr float kWesternMaxAheadZ = 460.0f;
	inline constexpr float kWesternSpeedMatchRatio = 0.92f;
	inline constexpr float kWesternDamagedSpeedPenalty = 0.78f;
	inline constexpr float kWesternDebrisEjectSpeed = 9.5f;
	inline constexpr float kWesternCrashRollRate = 5.4f;
	inline constexpr float kWesternCrashPitchDownRate = 0.65f;
	inline constexpr float kWesternCrashGravity = 26.0f;
	inline constexpr float kWesternSmokeEmitInterval = 0.045f;
	inline constexpr float kCoraxDogfightFireInterval = 0.14f;
	inline constexpr float kCoraxDogfightBurstWindow = 0.55f;
	inline constexpr float kCoraxDogfightBurstCycle = 2.40f;

	inline constexpr float kPhase6WarnRadioHold = 2.40f;
	inline constexpr float kPhase6IdRadioHold = 4.60f;
	inline constexpr float kPhase6EngageRadioHold = 4.40f;
	inline constexpr float kPhase6DownRadio1Hold = 3.00f;
	inline constexpr float kPhase6DownRadio2Hold = 4.20f;
	inline constexpr float kPhase6DownRadio3Hold = 5.20f;

	inline constexpr const char* kPhase6LogAlr = "WARNING: UNIDENTIFIED RADAR LOCK (AIR-TO-AIR)";
	inline constexpr const char* kPhase6LogRwr = "HIGH-ENERGY CONTACT: BEARING 040, DIVE ANGLE -45";
	inline constexpr const char* kPhase6LogSys = "CROSS-REFERENCING ALLIANCE SPEC... MATCH FAILED";
	inline constexpr const char* kPhase6LogIntel = "UNKNOWN AIRFRAME: WESTERN BLOC ORIGIN DETECTED";

	inline constexpr const char* kPhase6RadioAmbush = "上空から急速接近！ 回避しろ！！";
	inline constexpr const char* kPhase6RadioIdentify =
		"……待て、あの細身のシルエットに後退翼、連邦の機体じゃない……\n西側の大国のジェットだぞ！ なぜこんな境界の奥に！？";
	inline constexpr const char* kPhase6RadioEngage =
		"交戦規定解除！ 撃ち落とせ！\n奴の主翼かエンジンを狙い撃ってへし折れ！";
	inline constexpr const char* kPhase6GuideTitle = "【実戦】侵入した敵戦闘機を排除せよ";
	inline constexpr const char* kPhase6GuideSub = "部位（主翼・エンジン）へ弾を集中させて運動性を奪え。";

	inline constexpr const char* kSysLogPortWingDown = "TARGET HIT: PORT WING [STRUCTURAL BREAK] -> DOWNED";
	inline constexpr const char* kSysLogStbdWingDown = "TARGET HIT: STBD WING [STRUCTURAL BREAK] -> DOWNED";
	inline constexpr const char* kSysLogEngineDown = "TARGET HIT: ENGINE CORE [STRUCTURAL BREAK] -> DOWNED";

	inline constexpr const char* kPhase6RadioDown1 = "命中確認！ 敵機、谷底へ落ちていくぞ！";
	inline constexpr const char* kPhase6RadioDown2 = "……外縁のボロ哨戒任務のはずが、とんでもないものを引き当てちまったな。";
	inline constexpr const char* kPhase6RadioDown3 = "全機、これより基地へ緊急帰投する。……急ぐぞ、これから何かが始まる……！";

	// --- クリア後: 峡谷境界ライン＆フェードアウト定数 ---
	inline constexpr float kExitBoundaryAheadDistanceZ = 920.0f;
	inline constexpr float kExitBoundaryGateRadius = 64.0f;
	inline constexpr float kExitBoundaryAutoTriggerSeconds = 15.5f;
	inline constexpr float kFadeOutDurationSeconds = 2.20f;
	inline constexpr const char* kExitGuideTitle = "【緊急帰投】前方の峡谷境界ラインを通過せよ";
	inline constexpr const char* kExitGuideSub = "飛行隊長『Corax』に続き、帰投境界ラインを通過して作戦空域を離脱してください。";
	inline constexpr const char* kSysLogBoundaryCross = "SECTOR BOUNDARY CROSSED // RETURNING TO BASE...";

	// --- システムテレメトリログ＆チュートリアルHUD描画定数 ---
	inline constexpr size_t kMaxVisibleSysLogs = 4;
	inline constexpr float kSysLogLifetimeSeconds = 9.0f;
	inline constexpr float kSysLogFadeInSeconds = 0.16f;
	inline constexpr float kSysLogFadeOutSeconds = 0.80f;
	inline constexpr float kSysLogPanelX = 22.0f;
	inline constexpr float kSysLogPanelY = 160.0f;         // 左上計器(InfoUI: Y=32~145)と重ならないよう下部に配置
	inline constexpr float kSysLogRowHeight = 20.0f;
	inline constexpr float kSysLogFontSize = 12.0f;
	inline constexpr float kSysLogAccentWidth = 3.0f;
	inline constexpr float kSysLogBgPadX = 8.0f;
	inline constexpr float kSysLogBgWidth = 360.0f;        // 中央無線字幕との干渉を排除するコンパクト幅

	inline constexpr float kGuidePanelWidth = 620.0f;
	inline constexpr float kGuidePanelHeight = 72.0f;
	inline constexpr float kGuidePanelBottomMargin = 16.0f; // 自機後部を遮らないよう画面最下部付近に配置
	inline constexpr float kGuideBorderThickness = 1.5f;
	inline constexpr float kGuideCornerLen = 10.0f;
	inline constexpr float kGuideTitleFontSize = 15.5f;
	inline constexpr float kGuideSubFontSize = 12.0f;
	inline constexpr float kGuideTitleOffsetY = 8.0f;
	inline constexpr float kGuideSubOffsetY = 31.0f;
	inline constexpr float kGuideProgressBarMarginX = 22.0f;
	inline constexpr float kGuideProgressBarOffsetY = 54.0f;
	inline constexpr float kGuideProgressBarHeight = 6.0f;

	inline constexpr float kAlertBannerWidth = 420.0f;
	inline constexpr float kAlertBannerHeight = 36.0f;
	inline constexpr float kPullUpBannerCenterOffsetY = 88.0f;
	inline constexpr float kRwrBannerTopY = 168.0f;
	inline constexpr float kAlertBlinkFrequency = 14.0f;

	inline constexpr float kTargetBoxHalfSize = 15.0f;
	inline constexpr float kTargetCornerLen = 6.0f;
	inline constexpr float kTargetLineThickness = 1.8f;
	inline constexpr float kMarkerLabelFontSize = 11.5f;
	inline constexpr float kPartGaugePanelWidth = 228.0f;
	inline constexpr float kPartGaugePanelHeight = 84.0f;
	inline constexpr float kPartGaugeRightMargin = 26.0f;
	inline constexpr float kPartGaugeTopY = 92.0f;
	inline constexpr float kPartBarWidth = 104.0f;
	inline constexpr float kPartBarHeight = 6.0f;

	// --- HUDカラーパレット定数 ---
	inline constexpr MyMath::Vector4 kSysTagColor = { 0.36f, 1.00f, 0.72f, 0.98f };
	inline constexpr MyMath::Vector4 kWrnTagColor = { 1.00f, 0.84f, 0.28f, 0.98f };
	inline constexpr MyMath::Vector4 kAlrTagColor = { 1.00f, 0.30f, 0.28f, 1.00f };
	inline constexpr MyMath::Vector4 kIntelTagColor = { 0.56f, 0.90f, 1.00f, 0.98f };
	inline constexpr MyMath::Vector4 kLogBgColor = { 0.01f, 0.04f, 0.06f, 0.72f };

	inline constexpr MyMath::Vector4 kGuidePanelBg = { 0.01f, 0.05f, 0.04f, 0.84f };
	inline constexpr MyMath::Vector4 kGuidePanelBorder = { 0.26f, 0.92f, 0.62f, 0.88f };
	inline constexpr MyMath::Vector4 kGuideTitleColor = { 0.86f, 1.00f, 0.92f, 1.00f };
	inline constexpr MyMath::Vector4 kGuideSubColor = { 0.68f, 0.92f, 0.82f, 0.95f };
	inline constexpr MyMath::Vector4 kGuideBarBg = { 0.06f, 0.16f, 0.12f, 0.85f };
	inline constexpr MyMath::Vector4 kGuideBarFill = { 0.32f, 1.00f, 0.68f, 0.98f };

	inline constexpr MyMath::Vector4 kAlertPanelBg = { 0.22f, 0.02f, 0.02f, 0.88f };
	inline constexpr MyMath::Vector4 kAlertBorderCol = { 1.00f, 0.24f, 0.20f, 0.98f };
	inline constexpr MyMath::Vector4 kCautionPanelBg = { 0.20f, 0.12f, 0.02f, 0.84f };
	inline constexpr MyMath::Vector4 kCautionBorderCol = { 1.00f, 0.82f, 0.24f, 0.95f };

	inline constexpr MyMath::Vector4 kGateActiveColor = { 0.30f, 1.00f, 0.65f, 1.00f };
	inline constexpr MyMath::Vector4 kGateGlowColor = { 0.45f, 1.00f, 0.80f, 0.60f };
	inline constexpr MyMath::Vector4 kGateUpcomingColor = { 0.22f, 0.85f, 0.72f, 0.55f };
	inline constexpr MyMath::Vector4 kBoundaryGateColor = { 0.28f, 0.92f, 1.00f, 0.90f };

	inline constexpr MyMath::Vector4 kOutpostConcreteColor = { 0.34f, 0.35f, 0.38f, 1.00f };
	inline constexpr MyMath::Vector4 kOutpostDeckColor = { 0.24f, 0.25f, 0.28f, 1.00f };
	inline constexpr MyMath::Vector4 kOutpostIntactMastColor = { 0.42f, 0.72f, 0.64f, 0.72f };
	inline constexpr MyMath::Vector4 kOutpostIntactColor = { 0.42f, 0.82f, 0.70f, 0.70f };
	inline constexpr MyMath::Vector4 kOutpostEmberColor = { 1.00f, 0.42f, 0.08f, 0.75f };
	inline constexpr MyMath::Vector4 kObstacleBeaconColor = { 1.00f, 0.18f, 0.12f, 0.95f };
	inline constexpr float kObstacleBeaconRadius = 3.2f;
	inline constexpr float kObstacleBeaconBlinkRate = 2.4f;

	inline constexpr MyMath::Vector4 kAllyHudColor = { 0.68f, 0.98f, 0.84f, 0.96f };
	inline constexpr MyMath::Vector4 kDroneHudColor = { 1.00f, 0.58f, 0.16f, 0.98f };
	inline constexpr MyMath::Vector4 kLeadCircleColor = { 0.38f, 1.00f, 0.72f, 0.98f };
	inline constexpr MyMath::Vector4 kHostileHudColor = { 1.00f, 0.28f, 0.26f, 0.98f };
	inline constexpr MyMath::Vector4 kHitFlashColor = { 1.00f, 0.96f, 0.62f, 1.00f };
	inline constexpr MyMath::Vector4 kPureBlack = { 0.00f, 0.00f, 0.00f, 1.00f };

	// --- 峡谷中心線テーブル・ノード名・パーティクル・HUD詳細レイアウト定数 ---
	inline constexpr std::array<float, kCanyonControlPointCount> kCanyonControlZ = {
		500.0f, 2500.0f, 4500.0f, 6500.0f, 9000.0f, 11500.0f, 14000.0f, 16500.0f, 18500.0f
	};
	inline constexpr std::array<float, kCanyonControlPointCount> kCanyonControlX = {
		220.0f, 220.0f, -150.0f, -450.0f, -750.0f, -750.0f, -500.0f, -150.0f, -100.0f
	};

	inline constexpr size_t kFormatBufferSize = 192;
	inline constexpr size_t kTimestampBufferSize = 32;
	inline constexpr uint32_t kPrimaryMaterialIndex = 0;
	inline constexpr float kBasisParallelDotLimit = 0.99f;
	inline constexpr float kGatePassFlashDuration = 0.55f;
	inline constexpr float kGateCylinderThickness = 1.6f;
	inline constexpr float kCoraxTracerSpeed = 560.0f;
	inline constexpr float kCoraxDogfightOffsetX = -38.0f;
	inline constexpr float kCoraxDogfightOffsetY = 18.0f;
	inline constexpr float kCoraxDogfightBehindTargetZ = 145.0f;
	inline constexpr float kDroneEmissiveIntensity = 0.48f;
	inline constexpr float kDroneBankGain = 0.012f;
	inline constexpr float kWesternBankGain = 0.014f;
	inline constexpr float kWesternMaterialShininess = 42.0f;
	inline constexpr float kWesternMaterialSpecular = 0.65f;
	inline constexpr float kWesternMaterialEnv = 0.22f;
	inline constexpr float kCrashGroundOffsetY = 2.5f;
	inline constexpr float kCrashHorizDragRate = 0.18f;

	// 外部 .effect プリセット識別子（Resources/effects/*.effect で外部データ駆動）
	inline constexpr const char* kOutpostPlumeEffectName = "outpost_plume";
	inline constexpr const char* kDamageSmokeEffectName = "damage_smoke";
	inline constexpr const char* kCrashPlumeEffectName = "crash_plume";

	// マルチパーツノード名定数
	inline constexpr const char* kNodeWingL = "Wing_L";
	inline constexpr const char* kNodeWing1L = "Wing1_L";
	inline constexpr const char* kNodeWing2L = "Wing2_L";
	inline constexpr const char* kNodeWingR = "Wing_R";
	inline constexpr const char* kNodeWing1R = "Wing1_R";
	inline constexpr const char* kNodeWing2R = "Wing2_R";
	inline constexpr const char* kNodeTail = "Tail";
	inline constexpr const char* kNodeRudder = "Rudder";

	// HUDマーカー・ゲージ詳細オフセット定数
	inline constexpr float kHudTextShadowOffset = 1.4f;
	inline constexpr float kHudTextShadowAlpha = 0.72f;
	inline constexpr float kMarkerLabelOffsetX = 20.0f;
	inline constexpr float kMarkerLabelOffsetY = -7.0f;
	inline constexpr float kMarkerDistOffsetY = 8.0f;
	inline constexpr float kHitCrossInnerPx = 5.0f;
	inline constexpr float kHitCrossOuterPx = 14.0f;
	inline constexpr float kHitCrossThickness = 2.2f;
	inline constexpr float kLeadConnectLineAlpha = 0.38f;
	inline constexpr float kLeadConnectLineThickness = 1.2f;
	inline constexpr float kPartHeaderOffsetY = 7.0f;
	inline constexpr float kPartFirstRowOffsetY = 27.0f;
	inline constexpr float kPartRowStepY = 18.0f;
	inline constexpr float kPartLabelPadX = 10.0f;
	inline constexpr float kPartBarOffsetX = 112.0f;
	inline constexpr float kPartBarOffsetY = 4.0f;
	inline constexpr float kPartRowFontSize = 11.0f;
	inline constexpr float kBlinkMinAlpha = 0.35f;
	inline constexpr float kBlinkAmpAlpha = 0.65f;

	inline constexpr const char* kTagSys = "[SYS]";
	inline constexpr const char* kTagWrn = "[WRN]";
	inline constexpr const char* kTagAlr = "[ALR]";
	inline constexpr const char* kTagRwr = "[RWR]";
	inline constexpr const char* kTagIntel = "[INTEL]";
	inline constexpr const char* kRwrLockAlertText = "[ ! RWR WARNING : HIGH-ENERGY CONTACT DIVING ! ]";
	inline constexpr const char* kPartHeaderTitle = "TARGET STRUCTURAL TELEMETRY";
	inline constexpr const char* kPartPortWingLabel = "PORT WING";
	inline constexpr const char* kPartStbdWingLabel = "STBD WING";
	inline constexpr const char* kPartEngineLabel = "ENGINE";
	inline constexpr const char* kBrokenStatusLabel = "BROKEN";
}

// 進行フェーズ
enum class HalcyonPhase {
	Inactive,
	Phase1_BasicControls,
	Phase2_ThrottleCruise,
	Phase3_CanyonWaypoints,
	Phase4_OutpostIncident,
	Phase5_DroneLeadSight,
	Phase6_WesternAmbush,
	Phase7_ExitBoundary,
	Phase8_DebriefFadeOut
};

// システムログ種別
enum class SysLogCategory {
	Sys,
	Wrn,
	Alr,
	Rwr,
	Intel
};

// 更新用パラメータ
struct TutorialUpdateContext {
	float deltaTime = HalcyonDefaults::kZero;
	float missionElapsedTime = HalcyonDefaults::kZero;
	float playerThrottle = HalcyonDefaults::kZero;
	bool isGameplayActive = false;
	FlightModel* playerFlight = nullptr;
	MouseAimController* mouseAim = nullptr;
	GunPod* playerGunpod = nullptr;
	BulletManager* bulletManager = nullptr;
	RadioCommunicator* radio = nullptr;
	TerrainChunkManager* terrain = nullptr;
};

// チュートリアル管理クラス
class TutorialMissionDirector {
public:
	TutorialMissionDirector() = default;
	~TutorialMissionDirector() = default;

	// 初期化
	void Initialize(SpriteCommon* spriteCommon);

	// ミッション開始時のリセット
	void ResetForMission(
		const MissionData& mission,
		const std::string& missionFilePath,
		const MyMath::Vector3& playerStartPos);

	// 配置変更の反映
	void ApplyLiveLayout(const MissionData& mission, const TerrainChunkManager* terrain);

	// 更新
	void Update(const TutorialUpdateContext& ctx);

	// 3D描画
	void Draw3D(Camera* camera);

	// UI描画
	void DrawUI(Camera* camera, float screenWidth, float screenHeight);

	bool IsTutorialActive() const { return isActive_ && phase_ != HalcyonPhase::Inactive; }
	bool IsReadyForDebriefScene() const { return readyForClearScene_; }
	HalcyonPhase GetCurrentPhase() const { return phase_; }

	bool IsGuideDialogVisible() const;
	bool IsPlayerSteeringAllowed() const;
	bool IsPlayerThrottleAllowed() const;
	bool IsPlayerWeaponAllowed() const;

	static float EvaluateCanyonCenterX(float worldZ);

private:
	struct SysLogEntry {
		SysLogCategory category = SysLogCategory::Sys;
		std::string timestampText;
		std::string message;
		float ageSeconds = HalcyonDefaults::kZero;
	};

	struct CanyonGate {
		MyMath::Vector3 position{};
		bool isPassed = false;
		float passFlashTimer = HalcyonDefaults::kZero;
	};

	enum class BrokenPartType {
		None,
		PortWing,
		StbdWing,
		EngineCore
	};

	// --- フェーズ別更新サブルーチン ---
	void UpdateCoraxWingman(const TutorialUpdateContext& ctx);
	void UpdatePhase1(const TutorialUpdateContext& ctx);
	void UpdatePhase2(const TutorialUpdateContext& ctx);
	void UpdatePhase3(const TutorialUpdateContext& ctx);
	void UpdatePhase4(const TutorialUpdateContext& ctx);
	void UpdatePhase5(const TutorialUpdateContext& ctx);
	void UpdatePhase6(const TutorialUpdateContext& ctx);
	void UpdatePhase7Exit(const TutorialUpdateContext& ctx);
	void UpdatePhase8FadeOut(const TutorialUpdateContext& ctx);

	// --- スポーン＆イベント補助 ---
	void InitializeWatchtowers(const MyMath::Vector3& playerStartPos, const TerrainChunkManager* terrain);
	void GroundWatchtowersToTerrain(const TerrainChunkManager* terrain);
	void SyncWatchtowerObjects(Camera* camera);
	void SpawnPhase3Gates(const MyMath::Vector3& playerPos, const TerrainChunkManager* terrain);
	void SpawnOutpost04(const MyMath::Vector3& playerPos, const TerrainChunkManager* terrain);
	void SpawnTrainingDrone(const MyMath::Vector3& playerPos, const TerrainChunkManager* terrain);
	void StartPhase6Ambush(const TutorialUpdateContext& ctx);
	void TriggerWesternStructuralBreak(BrokenPartType brokenPart, const TutorialUpdateContext& ctx);
	void UpdateOutpostEffects(float dt);
	void PushSystemLog(SysLogCategory category, const std::string& message, bool includeTimestamp = false);
	float QueryTerrainY(const TerrainChunkManager* terrain, float worldX, float worldZ) const;

	static MyMath::Vector3 ComputeLeadInterceptPoint(
		const MyMath::Vector3& shooterPos,
		const MyMath::Vector3& shooterVel,
		const MyMath::Vector3& targetPos,
		const MyMath::Vector3& targetVel);

	// --- 2D HUD描画サブルーチン ---
	Sprite* AcquireRectSprite();
	Sprite* AcquireRingSprite();
	void DrawSolidRect(
		const Vector2& pos,
		const Vector2& size,
		const MyMath::Vector4& color,
		BlendMode blend = BlendMode::kNormal);
	void DrawLine2D(
		const Vector2& p1,
		const Vector2& p2,
		float thickness,
		const MyMath::Vector4& color,
		BlendMode blend = BlendMode::kNormal);
	void DrawRing2D(
		const Vector2& center,
		float radius,
		const MyMath::Vector4& color,
		BlendMode blend = BlendMode::kNormal);
	void DrawTacticalFrame(
		const Vector2& pos,
		const Vector2& size,
		const MyMath::Vector4& bgCol,
		const MyMath::Vector4& borderCol);
	void DrawTargetBrackets2D(
		const Vector2& center,
		float halfSize,
		const MyMath::Vector4& color);

	void DrawSystemLogsHUD(float hudScale);
	void DrawTutorialGuideHUD(float screenWidth, float screenHeight, float hudScale);
	void DrawAlertsHUD(float screenWidth, float screenHeight, float hudScale);
	void DrawWorldMarkersAndLeadSightHUD(Camera* camera, float screenWidth, float screenHeight, float hudScale);
	void DrawWesternPartStatusHUD(float screenWidth, float hudScale);
	void CheckWatchtowerCollisions(const TutorialUpdateContext& ctx);

private:
	SpriteCommon* spriteCommon_ = nullptr;
	uint32_t whiteTexIndex_ = HalcyonDefaults::kZeroInt;
	uint32_t ringTexIndex_ = HalcyonDefaults::kZeroInt;
	bool isInitialized_ = false;
	bool isActive_ = false;
	bool readyForClearScene_ = false;

	HalcyonPhase phase_ = HalcyonPhase::Inactive;
	float missionClock_ = HalcyonDefaults::kZero;
	float phaseTimer_ = HalcyonDefaults::kZero;
	float animTimer_ = HalcyonDefaults::kZero;
	float currentPlayerThrottle_ = HalcyonDefaults::kZero;
	float hitMarkerTimer_ = HalcyonDefaults::kZero;
	float fadeOutAlpha_ = HalcyonDefaults::kZero;
	bool isPullUpWarning_ = false;
	bool isHighAltitudeCaution_ = false;

	// --- システムログキュー ---
	std::deque<SysLogEntry> sysLogs_;

	// --- 隊長機 Corax 状態 ---
	AircraftVisualModel coraxModel_;
	std::unique_ptr<Afterburner> coraxAfterburner_;
	MyMath::Vector3 coraxPos_{};
	MyMath::Vector3 coraxVel_{};
	MyMath::Vector3 coraxForward_{ HalcyonDefaults::kZero, HalcyonDefaults::kZero, HalcyonDefaults::kUnit };
	float coraxRollRad_ = HalcyonDefaults::kZero;
	float coraxFireTimer_ = HalcyonDefaults::kZero;
	MyMath::Vector3 lastCoraxWingL_{};
	MyMath::Vector3 lastCoraxWingR_{};
	bool hasLastCoraxWings_ = false;

	// --- Phase 1 進行状態 ---
	bool p1Log0Sent_ = false;
	bool p1Log1Sent_ = false;
	bool p1Log2Sent_ = false;
	bool p1Radio1Sent_ = false;
	bool p1Radio2Sent_ = false;
	bool p1Radio3Sent_ = false;
	bool hasPrevAimDir_ = false;
	MyMath::Vector3 prevAimDir_{ HalcyonDefaults::kZero, HalcyonDefaults::kZero, HalcyonDefaults::kUnit };
	float p1MoveAccumSeconds_ = HalcyonDefaults::kZero;
	bool p1CompletedWaitingDelay_ = false;
	float p1PostClearTimer_ = HalcyonDefaults::kZero;

	// --- Phase 2 進行状態 ---
	bool p2Started_ = false;
	float p2ThrottleHoldSeconds_ = HalcyonDefaults::kZero;

	// --- Blender連携用カスタム空間配置キャッシュ ---
	TutorialSpatialLayout layout_{};
	std::vector<MyMath::Vector3> customGatePositions_;

	// --- Phase 3 ウェイポイント状態 ---
	std::vector<CanyonGate> gates_;
	int passedGateCount_ = HalcyonDefaults::kZeroInt;
	bool p3CompletedWaitingDelay_ = false;
	float p3PostClearTimer_ = HalcyonDefaults::kZero;

	// --- 峡谷監視所群 ＆ Phase 4 第4監視所（炎上監視塔）状態 ---
	bool outpostVisible_ = true;
	bool outpostIgnited_ = false;
	bool watchtowersGrounded_ = false;
	bool p4Wrn2Sent_ = false;
	std::vector<MyMath::Vector3> outpostPositions_;
	MyMath::Vector3 outpostPos_{};
	EffectHandle* outpostPlumeEffect_ = nullptr;
	std::vector<std::unique_ptr<Object3d>> watchtowerObjects_;

	// --- Phase 5 訓練用オレンジドローン状態 ---
	std::unique_ptr<Object3d> droneObject_;
	bool droneAlive_ = false;
	bool p5Log2Sent_ = false;
	MyMath::Vector3 dronePos_{};
	MyMath::Vector3 droneVel_{};
	float droneHp_ = HalcyonDefaults::kDroneMaxHp;
	float droneCruiseZ_ = HalcyonDefaults::kZero;
	float droneRotorAngle_ = HalcyonDefaults::kZero;
	bool p5CompletedWaitingDelay_ = false;
	float p5PostKillTimer_ = HalcyonDefaults::kZero;

	// --- Phase 6 西側ジェット戦闘機（部位破壊対応）状態 ---
	AircraftVisualModel westernModel_;
	bool westernVisible_ = false;
	bool westernInDivePass_ = false;
	bool westernDownedSpiral_ = false;
	bool westernCrashedFloor_ = false;
	bool p6LogRwrSent_ = false;
	bool p6LogSpecSent_ = false;
	bool p6LogIntelSent_ = false;
	bool p6EngageRadioSent_ = false;
	MyMath::Vector3 westernPos_{};
	MyMath::Vector3 westernVel_{};
	MyMath::Vector3 westernDiveStartPos_{};
	MyMath::Vector3 westernDiveMidPos_{};
	MyMath::Vector3 westernDiveEndPos_{};
	MyMath::Quaternion westernOrientation_{
		HalcyonDefaults::kZero,
		HalcyonDefaults::kZero,
		HalcyonDefaults::kZero,
		HalcyonDefaults::kUnit
	};
	float westernCruiseZ_ = HalcyonDefaults::kZero;
	float westernCurrentSpeed_ = HalcyonDefaults::kCoraxMinSpeed;
	float westernWeavePhaseX_ = HalcyonDefaults::kZero;
	float westernWeavePhaseY_ = HalcyonDefaults::kZero;
	float westernSmoothedGroundY_ = HalcyonDefaults::kFallbackCanyonFloorY;
	float westernStrafeTimer_ = HalcyonDefaults::kZero;
	EffectHandle* damageSmokeEffect_ = nullptr;
	EffectHandle* crashPlumeEffect_ = nullptr;
	float portWingHp_ = HalcyonDefaults::kWesternWingPartMaxHp;
	float stbdWingHp_ = HalcyonDefaults::kWesternWingPartMaxHp;
	float engineCoreHp_ = HalcyonDefaults::kWesternEnginePartMaxHp;
	BrokenPartType brokenPart_ = BrokenPartType::None;

	// --- Phase 7 帰投境界ライン状態 ---
	bool exitBoundaryVisible_ = false;
	MyMath::Vector3 exitBoundaryPos_{};

	// --- キャッシュ済み自機情報（HUD投影用） ---
	MyMath::Vector3 cachedPlayerPos_{};
	MyMath::Vector3 cachedPlayerVel_{};

	// --- DirectX12 複数描画安全スプライトプール ---
	std::vector<std::unique_ptr<Sprite>> rectSpritePool_;
	size_t rectSpritePoolIndex_ = HalcyonDefaults::kZeroIndex;
	std::vector<std::unique_ptr<Sprite>> ringSpritePool_;
	size_t ringSpritePoolIndex_ = HalcyonDefaults::kZeroIndex;
};
