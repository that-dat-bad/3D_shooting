#pragma once
#include <cstdint>
#include <vector>
#include "math/MyMath.h"

// ============================================================
// パーツ定義（War Thunder A5M4 ベース・簡略化版）
// ============================================================

// ダメージパーツ識別子
enum class DamagePart : uint8_t {
	Engine1,        // エンジン
	Propeller1,     // プロペラ
	Wing_L,         // 左翼（付け根）
	Wing_R,         // 右翼（付け根）
	Wing1_L,        // 左翼（中間）
	Wing1_R,        // 右翼（中間）
	Wing2_L,        // 左翼（翼端）
	Wing2_R,        // 右翼（翼端）
	Tail,           // 尾部
	Fuse,           // 胴体前部
	Fuse1,          // 胴体後部
	Pilot,          // パイロット
	Tank1,          // 燃料タンク1（左翼）
	Tank2,          // 燃料タンク2（右翼）
	Oil1,           // オイルライン
	TailControl,    // 尾翼操縦系統
	WingControl,    // 主翼操縦系統
	Fin,            // 垂直尾翼
	Rudder,         // ラダー
	Elevator0,      // エレベーター左
	Elevator1,      // エレベーター右
	Aileron_L,      // エルロン左
	Aileron_R,      // エルロン右
	Count
};

// パーツの素材タイプ
enum class ArmorMaterial : uint8_t {
	Dural,          // ジュラルミン（軽量・低耐久）
	Steel,          // 鉄鋼（中耐久）
	Armor,          // 装甲（高耐久・ダメージ軽減）
	Protected,      // 保護系統（操縦索など）
};

// パーツの状態フラグ
namespace PartStatus {
	constexpr uint8_t Normal    = 0;
	constexpr uint8_t Damaged   = 1 << 0;  // 損傷あり
	constexpr uint8_t OnFire    = 1 << 1;  // 火災中
	constexpr uint8_t Leaking   = 1 << 2;  // 漏れ（燃料/オイル）
	constexpr uint8_t Destroyed = 1 << 3;  // 完全破壊/切断
}

// ============================================================
// データ構造体
// ============================================================

// パーツ定義データ
struct DamagePartDef {
	float maxHP = 10.0f;
	ArmorMaterial material = ArmorMaterial::Dural;
	float genericDamageMult = 1.0f;  // 汎用ダメージ倍率
	bool canSeparate = false;        // 切断可能か（翼系パーツ）
};

// パーツの現在状態
struct DamagePartState {
	float currentHP = 0.0f;
	float maxHP = 0.0f;
	uint8_t statusFlags = PartStatus::Normal;
	float fireTimer = 0.0f;     // 火災経過時間
	float leakRate = 0.0f;      // 漏れ速度 (kg/s)

	bool HasFlag(uint8_t flag) const { return (statusFlags & flag) != 0; }
	void SetFlag(uint8_t flag) { statusFlags |= flag; }
	void ClearFlag(uint8_t flag) { statusFlags &= ~flag; }
	float GetHPRatio() const { return (maxHP > 0.0f) ? currentHP / maxHP : 0.0f; }
	bool IsAlive() const { return !HasFlag(PartStatus::Destroyed); }
};

// サブコライダー定義（機体ローカル空間のOBBボックス）
struct SubColliderDef {
	DamagePart part;                // 対応するパーツ
	MyMath::Vector3 center;         // ローカル空間での中心位置
	MyMath::Vector3 halfSize;       // 半径（各軸方向のハーフサイズ）
};

// 被弾結果
struct HitResult {
	DamagePart hitPart = DamagePart::Count;   // 被弾パーツ（Count = ミス）
	float damageDealt = 0.0f;                 // 実際に与えたダメージ
	bool partDestroyed = false;               // このヒットでパーツが破壊されたか
	bool fireStarted = false;                 // 火災が発生したか
	bool leakStarted = false;                 // 漏れが発生したか
	bool partCut = false;                     // 切断が発生したか
	bool isValid() const { return hitPart != DamagePart::Count; }
};

// ============================================================
// DamageModel クラス
// ============================================================

// ダメージモデルクラス
class DamageModel {
public:
	DamageModel() = default;
	~DamageModel() = default;

	// 初期化
	void Initialize();

	// ============================================================
	// 被弾処理
	// ============================================================

	// 指定パーツへのダメージ処理
	// part: 被弾パーツ
	// rawDamage: ダメージ量
	HitResult ProcessHit(DamagePart part, float rawDamage);

	// 無敵モード設定
	void SetInvulnerable(bool inv) { isInvulnerable_ = inv; }
	bool IsInvulnerable() const { return isInvulnerable_; }

	// 弾丸の被弾判定とダメージ処理
	// bulletWorldPos: 弾丸のワールド座標
	// aircraftPos: 機体のワールド座標
	// rawDamage: ダメージ量
	HitResult ProcessBulletHit(
		const MyMath::Vector3& bulletWorldPos,
		const MyMath::Vector3& aircraftPos,
		const MyMath::Quaternion& aircraftOrientation,
		float rawDamage
	);

	// サブコライダーに対するヒットテスト
	DamagePart TestHitPart(
		const MyMath::Vector3& bulletWorldPos,
		const MyMath::Vector3& aircraftPos,
		const MyMath::Quaternion& aircraftOrientation
	) const;

	// ============================================================
	// 毎フレーム更新
	// ============================================================

	// 火災・漏れの経過処理
	// dt: デルタタイム
	void UpdateEffects(float dt);

	// 地面衝突判定とダメージ処理
	// position: 機体位置
	// orientation: 姿勢
	// velocity: 速度
	// angularVelocity: 角速度
	// dt: デルタタイム
	// groundY: 地面高さ
	void ProcessGroundCollision(
		MyMath::Vector3& position,
		const MyMath::Quaternion& orientation,
		MyMath::Vector3& velocity,
		MyMath::Vector3& angularVelocity,
		float dt,
		float groundY = 0.0f
	);

	// ============================================================
	// パーツ状態の取得
	// ============================================================

	const DamagePartState& GetPartState(DamagePart part) const { return partStates_[static_cast<int>(part)]; }
	bool IsPartDestroyed(DamagePart part) const { return GetPartState(part).HasFlag(PartStatus::Destroyed); }
	bool IsPartOnFire(DamagePart part) const { return GetPartState(part).HasFlag(PartStatus::OnFire); }
	bool IsPartLeaking(DamagePart part) const { return GetPartState(part).HasFlag(PartStatus::Leaking); }

	// 致命的ダメージ判定
	bool IsCriticallyDamaged() const;

	// 火災中判定
	bool HasActiveFire() const;

	// ============================================================
	// フライトモデル連動用アクセッサ
	// ============================================================

	float GetEnginePowerFactor() const;
	float GetPitchControlFactor() const;
	float GetRollControlFactor() const;
	float GetYawControlFactor() const;
	float GetLeftWingLiftFactor() const;
	float GetRightWingLiftFactor() const;
	float GetTotalFuelLeakRate() const;
	float GetDragIncreaseFactor() const;
	float GetAsymmetricRollMoment() const;

	// ============================================================
	// デバッグ・ユーティリティ
	// ============================================================

	static const char* GetPartName(DamagePart part);
	const std::vector<SubColliderDef>& GetSubColliders() const { return subColliders_; }

	static constexpr int kPartCount = static_cast<int>(DamagePart::Count);

private:
	// 連鎖ダメージ適用
	void ApplyChainDamage(DamagePart hitPart, float damage, HitResult& result);

	// パーツ破壊時効果
	void ApplyOnKillEffects(DamagePart part, float totalDamage, HitResult& result);

	// ローカル座標変換
	MyMath::Vector3 WorldToLocal(
		const MyMath::Vector3& worldPos,
		const MyMath::Vector3& origin,
		const MyMath::Quaternion& orientation
	) const;

	// OBB判定
	static bool IsPointInBox(const MyMath::Vector3& point, const SubColliderDef& box);

	// パーツ定義（不変）と状態（可変）
	DamagePartDef  partDefs_[kPartCount]{};
	DamagePartState partStates_[kPartCount]{};

	// サブコライダー群
	std::vector<SubColliderDef> subColliders_;

	// 無敵フラグ
	bool isInvulnerable_ = false;
};
