#include "DamageModel.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

// ============================================================
// 定数
// ============================================================
namespace {
	constexpr float kFireDPS = 2.0f;             // 火災ダメージ (HP/秒)
	constexpr float kFireSpreadChance = 0.05f;   // 火災拡散確率 (/秒)
	constexpr float kFireExtinguishSpeed = 166.0f; // 消火可能な最低速度 (m/s ≈ 600km/h)

	// 素材別のダメージ軽減率
	float GetMaterialDamageReduction(ArmorMaterial mat) {
		switch (mat) {
		case ArmorMaterial::Dural:     return 1.0f;   // ジュラルミン: 軽減なし
		case ArmorMaterial::Steel:     return 0.8f;   // 鉄鋼: 20%軽減
		case ArmorMaterial::Armor:     return 0.5f;   // 装甲: 50%軽減
		case ArmorMaterial::Protected: return 0.7f;   // 保護系統: 30%軽減
		}
		return 1.0f;
	}

	// 簡易乱数 (0.0 ～ 1.0)
	float RandomFloat() {
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}
}


// ============================================================
// 初期化（A5M4ベースのデフォルト構成）
// ============================================================
void DamageModel::Initialize()
{
	// --- パーツ定義テーブル（War Thunder A5M4データ準拠） ---
	auto SetDef = [this](DamagePart part, float hp, ArmorMaterial mat, float dmgMult = 1.0f, bool canSep = false) {
		int idx = static_cast<int>(part);
		partDefs_[idx] = { hp, mat, dmgMult, canSep };
		partStates_[idx].currentHP = hp;
		partStates_[idx].maxHP = hp;
		partStates_[idx].statusFlags = PartStatus::Normal;
		partStates_[idx].fireTimer = 0.0f;
		partStates_[idx].leakRate = 0.0f;
	};

	//                     パーツ             HP      素材            ダメ倍率  切断可
	SetDef(DamagePart::Engine1,     49.5f, ArmorMaterial::Armor,     1.6f,   false);
	SetDef(DamagePart::Propeller1,  20.0f, ArmorMaterial::Dural,     1.0f,   true);
	SetDef(DamagePart::Wing_L,      67.0f, ArmorMaterial::Dural,     0.3f,   false);  // 付け根は切断されにくい
	SetDef(DamagePart::Wing_R,      67.0f, ArmorMaterial::Dural,     0.3f,   false);
	SetDef(DamagePart::Wing1_L,     28.4f, ArmorMaterial::Dural,     0.5f,   true);
	SetDef(DamagePart::Wing1_R,     28.4f, ArmorMaterial::Dural,     0.5f,   true);
	SetDef(DamagePart::Wing2_L,     23.1f, ArmorMaterial::Dural,     0.5f,   true);
	SetDef(DamagePart::Wing2_R,     23.1f, ArmorMaterial::Dural,     0.5f,   true);
	SetDef(DamagePart::Tail,        71.3f, ArmorMaterial::Dural,     0.7f,   true);
	SetDef(DamagePart::Fuse,        38.7f, ArmorMaterial::Dural,     1.0f,   false);
	SetDef(DamagePart::Fuse1,       42.0f, ArmorMaterial::Dural,     1.0f,   false);
	SetDef(DamagePart::Pilot,       20.0f, ArmorMaterial::Steel,     1.0f,   false);
	SetDef(DamagePart::Tank1,       27.3f, ArmorMaterial::Steel,     1.0f,   false);
	SetDef(DamagePart::Tank2,       27.3f, ArmorMaterial::Steel,     1.0f,   false);
	SetDef(DamagePart::Oil1,         9.5f, ArmorMaterial::Steel,     1.0f,   false);
	SetDef(DamagePart::TailControl, 60.5f, ArmorMaterial::Protected, 1.0f,   false);
	SetDef(DamagePart::WingControl, 39.5f, ArmorMaterial::Protected, 1.0f,   false);
	SetDef(DamagePart::Fin,         15.5f, ArmorMaterial::Dural,     1.0f,   true);
	SetDef(DamagePart::Rudder,       9.5f, ArmorMaterial::Dural,     0.3f,   true);
	SetDef(DamagePart::Elevator0,    9.5f, ArmorMaterial::Dural,     0.3f,   true);
	SetDef(DamagePart::Elevator1,    9.5f, ArmorMaterial::Dural,     0.3f,   true);
	SetDef(DamagePart::Aileron_L,   13.8f, ArmorMaterial::Dural,     0.3f,   true);
	SetDef(DamagePart::Aileron_R,   13.8f, ArmorMaterial::Dural,     0.3f,   true);


	// --- サブコライダー定義（機体ローカル座標: +Z=前, +Y=上, +X=右） ---
	subColliders_.clear();
	subColliders_ = {
		// パーツ                    中心座標                       半径（ハーフサイズ）
		{ DamagePart::Engine1,  { 0.0f,  0.0f,   5.0f },  { 0.8f,  0.6f,  1.5f } },
		{ DamagePart::Propeller1, { 0.0f, 0.0f,  6.5f },  { 2.0f,  2.0f,  0.2f } },
		{ DamagePart::Fuse,     { 0.0f,  0.0f,   0.0f },  { 0.95f, 0.95f, 7.5f } },
		{ DamagePart::Pilot,    { 0.0f,  0.5f,   1.0f },  { 0.4f,  0.5f,  0.6f } },  // コクピット（Fuseと重なるが優先判定）
		{ DamagePart::Fuse1,    { 0.0f,  0.1f,  -1.0f },  { 0.5f,  0.5f,  1.5f } },
		{ DamagePart::Tail,     { 0.0f,  0.1f,  -4.0f },  { 0.3f,  0.3f,  2.0f } },
		{ DamagePart::Fin,      { 0.0f,  0.8f,  -5.0f },  { 0.1f,  0.6f,  0.6f } },
		{ DamagePart::Wing_L,   {-2.0f, -0.1f,   0.0f },  { 1.5f,  0.15f, 1.2f } },
		{ DamagePart::Wing_R,   { 2.0f, -0.1f,   0.0f },  { 1.5f,  0.15f, 1.2f } },
		{ DamagePart::Wing1_L,  {-4.5f, -0.1f,  -0.2f },  { 1.2f,  0.12f, 1.0f } },
		{ DamagePart::Wing1_R,  { 4.5f, -0.1f,  -0.2f },  { 1.2f,  0.12f, 1.0f } },
		{ DamagePart::Wing2_L,  {-6.5f, -0.1f,  -0.4f },  { 1.0f,  0.1f,  0.8f } },
		{ DamagePart::Wing2_R,  { 6.5f, -0.1f,  -0.4f },  { 1.0f,  0.1f,  0.8f } },
		{ DamagePart::Tank1,    {-2.5f, -0.15f,  0.0f },  { 1.0f,  0.1f,  0.8f } },
		{ DamagePart::Tank2,    { 2.5f, -0.15f,  0.0f },  { 1.0f,  0.1f,  0.8f } },
	};
}


// ============================================================
// ワールド座標 → ローカル座標変換
// ============================================================
MyMath::Vector3 DamageModel::WorldToLocal(
	const MyMath::Vector3& worldPos,
	const MyMath::Vector3& origin,
	const MyMath::Quaternion& orientation) const
{
	// ワールド空間での相対位置
	MyMath::Vector3 relative = MyMath::Subtract(worldPos, origin);

	// 逆クォータニオン（共役）で回転 → ローカル空間
	// q_inv = (-x, -y, -z, w) （単位クォータニオンの場合）
	// v' = q_inv * v * q の回転公式を展開:
	// t = 2 * cross(q_inv.xyz, v)
	// v' = v + q.w * t + cross(q_inv.xyz, t)
	MyMath::Vector3 qv = { -orientation.x, -orientation.y, -orientation.z };
	float qw = orientation.w;

	// cross(qv, relative)
	MyMath::Vector3 t = {
		2.0f * (qv.y * relative.z - qv.z * relative.y),
		2.0f * (qv.z * relative.x - qv.x * relative.z),
		2.0f * (qv.x * relative.y - qv.y * relative.x)
	};

	// cross(qv, t)
	MyMath::Vector3 tt = {
		qv.y * t.z - qv.z * t.y,
		qv.z * t.x - qv.x * t.z,
		qv.x * t.y - qv.y * t.x
	};

	return {
		relative.x + qw * t.x + tt.x,
		relative.y + qw * t.y + tt.y,
		relative.z + qw * t.z + tt.z
	};
}


// ============================================================
// 点がOBBボックス内にあるか判定（ローカル空間ではAABB判定になる）
// ============================================================
bool DamageModel::IsPointInBox(const MyMath::Vector3& point, const SubColliderDef& box)
{
	return (point.x >= box.center.x - box.halfSize.x && point.x <= box.center.x + box.halfSize.x) &&
	       (point.y >= box.center.y - box.halfSize.y && point.y <= box.center.y + box.halfSize.y) &&
	       (point.z >= box.center.z - box.halfSize.z && point.z <= box.center.z + box.halfSize.z);
}


// ============================================================
// サブコライダーヒットテスト
// ============================================================
DamagePart DamageModel::TestHitPart(
	const MyMath::Vector3& bulletWorldPos,
	const MyMath::Vector3& aircraftPos,
	const MyMath::Quaternion& aircraftOrientation) const
{
	// ワールド座標をローカル座標に変換
	MyMath::Vector3 localPos = WorldToLocal(bulletWorldPos, aircraftPos, aircraftOrientation);

	// パイロット（コクピット）を先に判定（他のボックスと重なるため優先）
	for (const auto& col : subColliders_) {
		if (col.part == DamagePart::Pilot && IsPointInBox(localPos, col)) {
			return DamagePart::Pilot;
		}
	}

	// 通常のサブコライダー判定
	for (const auto& col : subColliders_) {
		if (col.part == DamagePart::Pilot) continue;  // パイロットは上で判定済み
		if (IsPointInBox(localPos, col)) {
			return col.part;
		}
	}

	// どのサブコライダーにも当たらなかった場合 → 最も近いサブコライダーを選択
	float minDistSq = 1e10f;
	DamagePart closest = DamagePart::Fuse;  // デフォルト: 胴体
	for (const auto& col : subColliders_) {
		MyMath::Vector3 diff = MyMath::Subtract(localPos, col.center);
		float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
		if (distSq < minDistSq) {
			minDistSq = distSq;
			closest = col.part;
		}
	}
	return closest;
}


// ============================================================
// 被弾処理（直接パーツ指定）
// ============================================================
HitResult DamageModel::ProcessHit(DamagePart part, float rawDamage)
{
	HitResult result;
	result.hitPart = part;

	int idx = static_cast<int>(part);
	if (idx < 0 || idx >= kPartCount) return result;

	DamagePartState& state = partStates_[idx];
	const DamagePartDef& def = partDefs_[idx];

	// 既に破壊済みのパーツにはダメージを与えない
	if (state.HasFlag(PartStatus::Destroyed)) return result;

	// ダメージ計算: 素ダメージ × 素材軽減率 × 汎用ダメージ倍率
	float effectiveDamage = rawDamage * GetMaterialDamageReduction(def.material) * def.genericDamageMult;
	state.currentHP -= effectiveDamage;
	result.damageDealt = effectiveDamage;

	// 損傷フラグ
	if (state.currentHP < state.maxHP * 0.8f) {
		state.SetFlag(PartStatus::Damaged);
	}

	// --- onHit 効果（ダメージ量に応じた即時効果） ---

	// 燃料タンク被弾 → 漏れ・火災の確率
	if (part == DamagePart::Tank1 || part == DamagePart::Tank2) {
		float hpRatio = state.GetHPRatio();
		// HP減少に応じて漏れ確率が上昇
		if (RandomFloat() < (1.0f - hpRatio) * 0.7f) {
			if (!state.HasFlag(PartStatus::Leaking)) {
				state.SetFlag(PartStatus::Leaking);
				state.leakRate = 0.5f + (1.0f - hpRatio) * 1.5f;  // 0.5 ~ 2.0 kg/s
				result.leakStarted = true;
			}
		}
		// 火災確率（低HP時に急上昇）
		if (hpRatio < 0.5f && RandomFloat() < rawDamage * 0.02f) {
			if (!state.HasFlag(PartStatus::OnFire)) {
				state.SetFlag(PartStatus::OnFire);
				result.fireStarted = true;
			}
		}
	}

	// エンジン被弾 → 火災・オイル漏れの確率
	if (part == DamagePart::Engine1) {
		if (RandomFloat() < rawDamage * 0.01f) {
			if (!state.HasFlag(PartStatus::OnFire)) {
				state.SetFlag(PartStatus::OnFire);
				result.fireStarted = true;
			}
		}
		// オイル漏れ
		DamagePartState& oilState = partStates_[static_cast<int>(DamagePart::Oil1)];
		if (RandomFloat() < 0.3f && !oilState.HasFlag(PartStatus::Leaking)) {
			oilState.SetFlag(PartStatus::Leaking);
			oilState.leakRate = 0.5f;
		}
	}

	// オイルライン被弾 → 漏れ
	if (part == DamagePart::Oil1) {
		if (!state.HasFlag(PartStatus::Leaking)) {
			state.SetFlag(PartStatus::Leaking);
			state.leakRate = 0.95f;
			result.leakStarted = true;
		}
	}

	// --- 撃破判定 ---
	if (state.currentHP <= 0.0f) {
		state.currentHP = 0.0f;
		state.SetFlag(PartStatus::Destroyed);
		result.partDestroyed = true;

		// 切断判定（翼・コントロールサーフェス）
		if (def.canSeparate) {
			result.partCut = true;
		}

		// onKill 効果
		ApplyOnKillEffects(part, rawDamage, result);
	}

	// --- 連鎖ダメージ ---
	ApplyChainDamage(part, rawDamage, result);

	return result;
}


// ============================================================
// 弾丸ワールド位置からの被弾処理（ヒットテスト + ProcessHit）
// ============================================================
HitResult DamageModel::ProcessBulletHit(
	const MyMath::Vector3& bulletWorldPos,
	const MyMath::Vector3& aircraftPos,
	const MyMath::Quaternion& aircraftOrientation,
	float rawDamage)
{
	DamagePart hitPart = TestHitPart(bulletWorldPos, aircraftPos, aircraftOrientation);
	return ProcessHit(hitPart, rawDamage);
}


// ============================================================
// 連鎖ダメージの適用
// ============================================================
void DamageModel::ApplyChainDamage(DamagePart hitPart, float damage, HitResult& result)
{
	// 連鎖ダメージは小さめ（元ダメージの10～30%）で確率的に発生
	float chainDamage = damage * 0.2f;

	switch (hitPart) {
	case DamagePart::Wing_L:
		// 左翼根元 → エルロン左、操縦系統、燃料タンク1
		if (RandomFloat() < 0.15f) ProcessHit(DamagePart::Aileron_L, chainDamage);
		if (RandomFloat() < 0.10f) ProcessHit(DamagePart::WingControl, chainDamage);
		if (RandomFloat() < 0.20f) ProcessHit(DamagePart::Tank1, chainDamage);
		break;

	case DamagePart::Wing_R:
		if (RandomFloat() < 0.15f) ProcessHit(DamagePart::Aileron_R, chainDamage);
		if (RandomFloat() < 0.10f) ProcessHit(DamagePart::WingControl, chainDamage);
		if (RandomFloat() < 0.20f) ProcessHit(DamagePart::Tank2, chainDamage);
		break;

	case DamagePart::Wing1_L:
		if (RandomFloat() < 0.20f) ProcessHit(DamagePart::Aileron_L, chainDamage);
		break;

	case DamagePart::Wing1_R:
		if (RandomFloat() < 0.20f) ProcessHit(DamagePart::Aileron_R, chainDamage);
		break;

	case DamagePart::Tail:
		// 尾部 → 操縦系統、エレベーター
		if (RandomFloat() < 0.15f) ProcessHit(DamagePart::TailControl, chainDamage);
		if (RandomFloat() < 0.10f) ProcessHit(DamagePart::Elevator0, chainDamage);
		if (RandomFloat() < 0.10f) ProcessHit(DamagePart::Elevator1, chainDamage);
		break;

	case DamagePart::Fin:
		// 垂直尾翼 → ラダー、操縦系統
		if (RandomFloat() < 0.25f) ProcessHit(DamagePart::Rudder, chainDamage);
		if (RandomFloat() < 0.10f) ProcessHit(DamagePart::TailControl, chainDamage);
		break;

	case DamagePart::Fuse:
		// 胴体前部 → パイロット、エンジン
		if (RandomFloat() < 0.05f) ProcessHit(DamagePart::Pilot, chainDamage);
		if (RandomFloat() < 0.10f) ProcessHit(DamagePart::Engine1, chainDamage);
		break;

	case DamagePart::Fuse1:
		// 胴体後部 → 操縦系統
		if (RandomFloat() < 0.15f) ProcessHit(DamagePart::WingControl, chainDamage);
		if (RandomFloat() < 0.10f) ProcessHit(DamagePart::TailControl, chainDamage);
		break;

	default:
		break;
	}
}


// ============================================================
// パーツ撃破時の効果
// ============================================================
void DamageModel::ApplyOnKillEffects(DamagePart part, float /*totalDamage*/, HitResult& result)
{
	switch (part) {
	case DamagePart::Engine1:
		// エンジン破壊 → 高確率で火災
		if (RandomFloat() < 0.7f) {
			partStates_[static_cast<int>(part)].SetFlag(PartStatus::OnFire);
			result.fireStarted = true;
		}
		// 操縦系統への連鎖
		if (RandomFloat() < 0.4f) {
			ProcessHit(DamagePart::WingControl, 15.0f);
		}
		break;

	case DamagePart::Tank1:
	case DamagePart::Tank2: {
		// 燃料タンク破壊 → 火災 + 大量漏れ
		DamagePartState& st = partStates_[static_cast<int>(part)];
		if (RandomFloat() < 0.5f) {
			st.SetFlag(PartStatus::OnFire);
			result.fireStarted = true;
		}
		st.SetFlag(PartStatus::Leaking);
		st.leakRate = 3.0f;  // 大量漏れ
		result.leakStarted = true;
		break;
	}

	case DamagePart::Fuse1:
		// 胴体後部破壊 → 尾部への連鎖
		ProcessHit(DamagePart::Tail, 20.0f);
		break;

	case DamagePart::Oil1:
		// オイルライン完全破壊 → エンジン過熱→火災
		if (RandomFloat() < 0.5f) {
			partStates_[static_cast<int>(DamagePart::Engine1)].SetFlag(PartStatus::OnFire);
		}
		break;

	default:
		break;
	}
}


// ============================================================
// 毎フレーム更新（火災DOT・漏れの進行）
// ============================================================
void DamageModel::UpdateEffects(float dt)
{
	for (int i = 0; i < kPartCount; ++i) {
		DamagePartState& state = partStates_[i];

		// --- 火災ダメージ ---
		if (state.HasFlag(PartStatus::OnFire) && !state.HasFlag(PartStatus::Destroyed)) {
			state.fireTimer += dt;
			state.currentHP -= kFireDPS * dt;

			if (state.currentHP <= 0.0f) {
				state.currentHP = 0.0f;
				state.SetFlag(PartStatus::Destroyed);
				if (partDefs_[i].canSeparate) {
					// 火災による焼失→切断扱い
				}
			}

			// 火災の拡散（隣接パーツへの延焼）
			if (RandomFloat() < kFireSpreadChance * dt) {
				DamagePart current = static_cast<DamagePart>(i);
				// エンジン火災 → 胴体前部に延焼
				if (current == DamagePart::Engine1) {
					partStates_[static_cast<int>(DamagePart::Fuse)].SetFlag(PartStatus::OnFire);
				}
				// タンク火災 → 翼に延焼
				if (current == DamagePart::Tank1) {
					partStates_[static_cast<int>(DamagePart::Wing_L)].SetFlag(PartStatus::OnFire);
				}
				if (current == DamagePart::Tank2) {
					partStates_[static_cast<int>(DamagePart::Wing_R)].SetFlag(PartStatus::OnFire);
				}
			}
		}
	}
}


// ============================================================
// 地面との物理衝突判定および部位別ダメージ処理
// ============================================================
void DamageModel::ProcessGroundCollision(
	MyMath::Vector3& position,
	const MyMath::Quaternion& orientation,
	MyMath::Vector3& velocity,
	MyMath::Vector3& angularVelocity,
	float dt,
	float groundY)
{
	float speed = MyMath::Length(velocity);

	// クォータニオンから機体のローカル3軸方向ベクトル（ワールド空間）を計算
	MyMath::Vector3 right = {
		1.0f - 2.0f * (orientation.y * orientation.y + orientation.z * orientation.z),
		2.0f * (orientation.x * orientation.y + orientation.w * orientation.z),
		2.0f * (orientation.x * orientation.z - orientation.w * orientation.y)
	};
	MyMath::Vector3 up = {
		2.0f * (orientation.x * orientation.y - orientation.w * orientation.z),
		1.0f - 2.0f * (orientation.x * orientation.x + orientation.z * orientation.z),
		2.0f * (orientation.y * orientation.z + orientation.w * orientation.x)
	};
	MyMath::Vector3 forward = {
		2.0f * (orientation.x * orientation.z + orientation.w * orientation.y),
		2.0f * (orientation.y * orientation.z - orientation.w * orientation.x),
		1.0f - 2.0f * (orientation.x * orientation.x + orientation.y * orientation.y)
	};

	float maxPenetration = 0.0f;
	bool grounded = false;
	MyMath::Vector3 totalGroundTorque = { 0.0f, 0.0f, 0.0f };

	for (const auto& col : subColliders_) {
		bool isDestroyed = IsPartDestroyed(col.part);

		// サブコライダーの中心ワールド座標
		MyMath::Vector3 centerWorld = {
			position.x + right.x * col.center.x + up.x * col.center.y + forward.x * col.center.z,
			position.y + right.y * col.center.x + up.y * col.center.y + forward.y * col.center.z,
			position.z + right.z * col.center.x + up.z * col.center.y + forward.z * col.center.z
		};

		// 垂直方向(Y軸)への半サイズ幅
		float extentY = std::fabs(col.halfSize.x * right.y) +
		                std::fabs(col.halfSize.y * up.y) +
		                std::fabs(col.halfSize.z * forward.y);

		float lowestY = centerWorld.y - extentY;

		if (lowestY < groundY) {
			float penetration = groundY - lowestY;

			// 破壊されたパーツ（HP=0%の翼端等）は剛性を失って挫屈するため、機体を空中に支えない
			if (!isDestroyed) {
				grounded = true;
				if (penetration > maxPenetration) {
					maxPenetration = penetration;
				}

				// 接地点の反発力による転倒モーメント（トルク）
				// 接触位置がCGより左(col.center.x < 0)の場合、反作用で右ロールトルクを発生させて腹から落ちる
				float normalForce = 9.81f * 3000.0f + penetration * 50000.0f;
				// Pitch torque (rz = col.center.z)
				totalGroundTorque.x += col.center.z * normalForce * 0.00005f;
				// Roll torque (rx = -col.center.x)
				totalGroundTorque.z += -col.center.x * normalForce * 0.00005f;
				// Yaw torque from ground friction (cartwheel)
				if (speed > 5.0f) {
					totalGroundTorque.y += col.center.x * speed * 0.001f;
				}
			}

			// 速度に応じた接地/激突ダメージ計算:
			float baseDamage = (speed * 1.5f + penetration * 15.0f);
			if (speed > 35.0f) {
				baseDamage += (speed - 35.0f) * 4.0f; // 激突加速ボーナス
			}

			// 毎フレームの連続ヒットとして適用
			ProcessHit(col.part, baseDamage * dt * 10.0f);
		}
	}

	// 物理レスポンス（めり込み押し戻し・跳ね返り・接地摩擦・転倒トルク）
	if (grounded) {
		position.y += maxPenetration;

		// 落下速度の減衰/バウンド
		if (velocity.y < 0.0f) {
			velocity.y = -velocity.y * 0.15f; // 微小バウンド
		}

		// 地面擦り減速（摩擦）
		velocity.x *= 0.92f;
		velocity.z *= 0.92f;

		// 接地転倒トルクを角速度に加算
		angularVelocity.x += totalGroundTorque.x * dt;
		angularVelocity.y += totalGroundTorque.y * dt;
		angularVelocity.z += totalGroundTorque.z * dt;

		// 地面接地時の回転ダンピング（倒れ込んだ後に安定して静止する）
		angularVelocity.x *= 0.90f;
		angularVelocity.y *= 0.90f;
		angularVelocity.z *= 0.90f;
	}
}


// ============================================================
// 致命的ダメージ判定
// ============================================================
bool DamageModel::IsCriticallyDamaged() const
{
	// 胴体破壊 (HP=0で強制死亡)
	if (IsPartDestroyed(DamagePart::Fuse)) return true;

	// パイロット死亡
	if (IsPartDestroyed(DamagePart::Pilot)) return true;

	// 両翼根元が破壊
	if (IsPartDestroyed(DamagePart::Wing_L) && IsPartDestroyed(DamagePart::Wing_R)) return true;

	// 尾部切断
	if (IsPartDestroyed(DamagePart::Tail)) return true;

	return false;
}

bool DamageModel::HasActiveFire() const
{
	for (int i = 0; i < kPartCount; ++i) {
		if (partStates_[i].HasFlag(PartStatus::OnFire)) return true;
	}
	return false;
}


// ============================================================
// フライトモデル連動用アクセッサ
// ============================================================

float DamageModel::GetEnginePowerFactor() const
{
	const auto& st = GetPartState(DamagePart::Engine1);
	if (st.HasFlag(PartStatus::Destroyed)) return 0.0f;
	if (st.HasFlag(PartStatus::OnFire)) return 0.3f; // 火災中は30%出力

	// オイル漏れでエンジン出力低下
	const auto& oil = GetPartState(DamagePart::Oil1);
	float oilPenalty = oil.HasFlag(PartStatus::Leaking) ? 0.2f : 0.0f;

	return std::clamp(st.GetHPRatio() - oilPenalty, 0.0f, 1.0f);
}

float DamageModel::GetPitchControlFactor() const
{
	// エレベーター + 尾翼操縦系統の状態
	const auto& el0 = GetPartState(DamagePart::Elevator0);
	const auto& el1 = GetPartState(DamagePart::Elevator1);
	const auto& tc = GetPartState(DamagePart::TailControl);

	// 両エレベーターの平均有効度
	float elevatorFactor = (el0.IsAlive() ? el0.GetHPRatio() : 0.0f) * 0.5f
	                     + (el1.IsAlive() ? el1.GetHPRatio() : 0.0f) * 0.5f;

	// 操縦系統の有効度
	float controlFactor = tc.IsAlive() ? tc.GetHPRatio() : 0.0f;

	return std::clamp(elevatorFactor * controlFactor, 0.0f, 1.0f);
}

float DamageModel::GetRollControlFactor() const
{
	const auto& alL = GetPartState(DamagePart::Aileron_L);
	const auto& alR = GetPartState(DamagePart::Aileron_R);
	const auto& wc = GetPartState(DamagePart::WingControl);

	float aileronFactor = (alL.IsAlive() ? alL.GetHPRatio() : 0.0f) * 0.5f
	                    + (alR.IsAlive() ? alR.GetHPRatio() : 0.0f) * 0.5f;
	float controlFactor = wc.IsAlive() ? wc.GetHPRatio() : 0.0f;

	return std::clamp(aileronFactor * controlFactor, 0.0f, 1.0f);
}

float DamageModel::GetYawControlFactor() const
{
	const auto& rud = GetPartState(DamagePart::Rudder);
	const auto& tc = GetPartState(DamagePart::TailControl);

	float rudderFactor = rud.IsAlive() ? rud.GetHPRatio() : 0.0f;
	float controlFactor = tc.IsAlive() ? tc.GetHPRatio() : 0.0f;

	return std::clamp(rudderFactor * controlFactor, 0.0f, 1.0f);
}

float DamageModel::GetLeftWingLiftFactor() const
{
	// 翼端から順に破壊で揚力が減る
	const auto& w0 = GetPartState(DamagePart::Wing_L);   // 付け根 (寄与40%)
	const auto& w1 = GetPartState(DamagePart::Wing1_L);  // 中間 (寄与35%)
	const auto& w2 = GetPartState(DamagePart::Wing2_L);  // 翼端 (寄与25%)

	// 外側が切断されたら内側も機能しない（翼端→中間→付け根の順で連鎖）
	float f2 = w2.IsAlive() ? w2.GetHPRatio() : 0.0f;
	float f1 = w1.IsAlive() ? w1.GetHPRatio() : 0.0f;
	float f0 = w0.IsAlive() ? w0.GetHPRatio() : 0.0f;

	// 中間が切断されたら翼端も失われる
	if (!w1.IsAlive()) f2 = 0.0f;
	// 付け根が切断されたら全て失われる
	if (!w0.IsAlive()) { f1 = 0.0f; f2 = 0.0f; }

	return f0 * 0.4f + f1 * 0.35f + f2 * 0.25f;
}

float DamageModel::GetRightWingLiftFactor() const
{
	const auto& w0 = GetPartState(DamagePart::Wing_R);
	const auto& w1 = GetPartState(DamagePart::Wing1_R);
	const auto& w2 = GetPartState(DamagePart::Wing2_R);

	float f2 = w2.IsAlive() ? w2.GetHPRatio() : 0.0f;
	float f1 = w1.IsAlive() ? w1.GetHPRatio() : 0.0f;
	float f0 = w0.IsAlive() ? w0.GetHPRatio() : 0.0f;

	if (!w1.IsAlive()) f2 = 0.0f;
	if (!w0.IsAlive()) { f1 = 0.0f; f2 = 0.0f; }

	return f0 * 0.4f + f1 * 0.35f + f2 * 0.25f;
}

float DamageModel::GetTotalFuelLeakRate() const
{
	float total = 0.0f;
	const auto& t1 = GetPartState(DamagePart::Tank1);
	const auto& t2 = GetPartState(DamagePart::Tank2);
	if (t1.HasFlag(PartStatus::Leaking)) total += t1.leakRate;
	if (t2.HasFlag(PartStatus::Leaking)) total += t2.leakRate;
	return total;
}

float DamageModel::GetDragIncreaseFactor() const
{
	// 破壊・切断されたパーツの数に応じて空気抵抗が増加
	float increase = 0.0f;
	for (int i = 0; i < kPartCount; ++i) {
		const auto& st = partStates_[i];
		if (st.HasFlag(PartStatus::Destroyed)) {
			increase += 0.03f;  // 破壊パーツ1つにつき3%の抵抗増加
		} else if (st.HasFlag(PartStatus::Damaged)) {
			increase += 0.01f;  // 損傷パーツ1つにつき1%の抵抗増加
		}
		if (st.HasFlag(PartStatus::OnFire)) {
			increase += 0.02f;  // 火災中のパーツは追加抵抗
		}
	}
	return increase;
}

float DamageModel::GetAsymmetricRollMoment() const
{
	// 左右翼の揚力差 → 非対称ロールモーメント
	float leftLift = GetLeftWingLiftFactor();
	float rightLift = GetRightWingLiftFactor();

	// 正の値 = 右翼の揚力が強い → 左にロール（左翼が落ちる）
	// 負の値 = 左翼の揚力が強い → 右にロール（右翼が落ちる）
	return (rightLift - leftLift);
}


// ============================================================
// パーツ名文字列
// ============================================================
const char* DamageModel::GetPartName(DamagePart part)
{
	switch (part) {
	case DamagePart::Engine1:     return "Engine";
	case DamagePart::Propeller1:  return "Propeller";
	case DamagePart::Wing_L:      return "Wing L (root)";
	case DamagePart::Wing_R:      return "Wing R (root)";
	case DamagePart::Wing1_L:     return "Wing1 L (mid)";
	case DamagePart::Wing1_R:     return "Wing1 R (mid)";
	case DamagePart::Wing2_L:     return "Wing2 L (tip)";
	case DamagePart::Wing2_R:     return "Wing2 R (tip)";
	case DamagePart::Tail:        return "Tail";
	case DamagePart::Fuse:        return "Fuselage (front)";
	case DamagePart::Fuse1:       return "Fuselage (rear)";
	case DamagePart::Pilot:       return "Pilot";
	case DamagePart::Tank1:       return "Fuel Tank 1";
	case DamagePart::Tank2:       return "Fuel Tank 2";
	case DamagePart::Oil1:        return "Oil Line";
	case DamagePart::TailControl: return "Tail Control";
	case DamagePart::WingControl: return "Wing Control";
	case DamagePart::Fin:         return "Fin";
	case DamagePart::Rudder:      return "Rudder";
	case DamagePart::Elevator0:   return "Elevator L";
	case DamagePart::Elevator1:   return "Elevator R";
	case DamagePart::Aileron_L:   return "Aileron L";
	case DamagePart::Aileron_R:   return "Aileron R";
	default:                      return "Unknown";
	}
}
