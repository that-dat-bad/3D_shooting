#pragma once
#include <cstdint>
#include <vector>
#include "math/MyMath.h"

// ============================================================
// パーツ定義（War Thunder A5M4 ベース・簡略化版）
// ============================================================

/// @brief ダメージパーツ識別子
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

/// @brief パーツの素材タイプ（被弾時の耐性に影響）
enum class ArmorMaterial : uint8_t {
	Dural,          // ジュラルミン（軽量・低耐久）
	Steel,          // 鉄鋼（中耐久）
	Armor,          // 装甲（高耐久・ダメージ軽減）
	Protected,      // 保護系統（操縦索など）
};

/// @brief パーツの状態フラグ（ビットフラグ）
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

/// @brief パーツ定義データ（不変）
struct DamagePartDef {
	float maxHP = 10.0f;
	ArmorMaterial material = ArmorMaterial::Dural;
	float genericDamageMult = 1.0f;  // 汎用ダメージ倍率
	bool canSeparate = false;        // 切断可能か（翼系パーツ）
};

/// @brief パーツの現在状態（可変）
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

/// @brief サブコライダー定義（機体ローカル空間のOBBボックス）
struct SubColliderDef {
	DamagePart part;                // 対応するパーツ
	MyMath::Vector3 center;         // ローカル空間での中心位置
	MyMath::Vector3 halfSize;       // 半径（各軸方向のハーフサイズ）
};

/// @brief 被弾結果
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

/// @brief パーツベースのダメージモデル（War Thunder風）
/// 
/// 機体の各パーツにHP・素材・状態フラグを持たせ、
/// ローポリサブコライダーで被弾パーツを判定する。
class DamageModel {
public:
	DamageModel() = default;
	~DamageModel() = default;

	/// @brief 初期化（A5M4ベースのデフォルトパーツ構成）
	void Initialize();

	// ============================================================
	// 被弾処理
	// ============================================================

	/// @brief 指定パーツに直接ダメージを与える
	/// @param part 被弾パーツ
	/// @param rawDamage 素のダメージ量
	/// @return 被弾結果
	HitResult ProcessHit(DamagePart part, float rawDamage);

	/// @brief ワールド空間の弾丸位置から被弾パーツを判定してダメージを与える
	/// @param bulletWorldPos 弾丸のワールド座標
	/// @param aircraftPos 機体のワールド座標
	/// @param aircraftOrientation 機体の姿勢（クォータニオン）
	/// @param rawDamage 素のダメージ量
	/// @return 被弾結果
	HitResult ProcessBulletHit(
		const MyMath::Vector3& bulletWorldPos,
		const MyMath::Vector3& aircraftPos,
		const MyMath::Quaternion& aircraftOrientation,
		float rawDamage
	);

	/// @brief サブコライダーに対するヒットテスト（パーツ判定のみ）
	/// @return 被弾パーツ（ミスの場合は DamagePart::Count）
	DamagePart TestHitPart(
		const MyMath::Vector3& bulletWorldPos,
		const MyMath::Vector3& aircraftPos,
		const MyMath::Quaternion& aircraftOrientation
	) const;

	// ============================================================
	// 毎フレーム更新
	// ============================================================

	/// @brief 火災・漏れの経過処理
	/// @param dt デルタタイム
	void UpdateEffects(float dt);

	/// @brief 地面との物理衝突判定および部位別ダメージ処理
	/// @param position 機体の位置（めり込み押し戻しが適用される）
	/// @param orientation 機体の姿勢
	/// @param velocity 機体の速度（バウンド・摩擦が適用される）
	/// @param angularVelocity 機体の角速度（転倒トルク・摩擦ダンピングが適用される）
	/// @param dt デルタタイム
	/// @param groundY 地面の高さ
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

	/// @brief 致命的なダメージを受けているか（パイロット死亡 or 機体崩壊）
	bool IsCriticallyDamaged() const;

	/// @brief いずれかのパーツが火災中か
	bool HasActiveFire() const;

	// ============================================================
	// フライトモデル連動用アクセッサ
	// ============================================================

	/// @brief エンジン出力係数（0.0=完全停止, 1.0=正常）
	float GetEnginePowerFactor() const;

	/// @brief ピッチ操舵有効度（0.0=不能, 1.0=正常）
	float GetPitchControlFactor() const;

	/// @brief ロール操舵有効度
	float GetRollControlFactor() const;

	/// @brief ヨー操舵有効度
	float GetYawControlFactor() const;

	/// @brief 左翼揚力係数（0.0=翼なし, 1.0=正常）
	float GetLeftWingLiftFactor() const;

	/// @brief 右翼揚力係数
	float GetRightWingLiftFactor() const;

	/// @brief 燃料漏れ速度（kg/s）
	float GetTotalFuelLeakRate() const;

	/// @brief ダメージによる追加空気抵抗
	float GetDragIncreaseFactor() const;

	/// @brief 翼の非対称ダメージによるロールモーメント（-1.0～1.0）
	float GetAsymmetricRollMoment() const;

	// ============================================================
	// デバッグ・ユーティリティ
	// ============================================================

	/// @brief パーツ名の文字列を取得
	static const char* GetPartName(DamagePart part);

	/// @brief サブコライダー一覧を取得（デバッグ描画用）
	const std::vector<SubColliderDef>& GetSubColliders() const { return subColliders_; }

	/// @brief パーツ総数
	static constexpr int kPartCount = static_cast<int>(DamagePart::Count);

private:
	/// @brief 被弾パーツから連鎖ダメージを適用
	void ApplyChainDamage(DamagePart hitPart, float damage, HitResult& result);

	/// @brief パーツ撃破時の効果（fire/cut/leak 等）
	void ApplyOnKillEffects(DamagePart part, float totalDamage, HitResult& result);

	/// @brief ワールド座標をローカル座標に変換
	MyMath::Vector3 WorldToLocal(
		const MyMath::Vector3& worldPos,
		const MyMath::Vector3& origin,
		const MyMath::Quaternion& orientation
	) const;

	/// @brief 点がOBBボックス内にあるか判定
	static bool IsPointInBox(const MyMath::Vector3& point, const SubColliderDef& box);

	// パーツ定義（不変）と状態（可変）
	DamagePartDef  partDefs_[kPartCount]{};
	DamagePartState partStates_[kPartCount]{};

	// サブコライダー群
	std::vector<SubColliderDef> subColliders_;
};
