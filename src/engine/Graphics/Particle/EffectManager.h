#pragma once
#include <memory>
#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "../base/Math/MyMath.h"
#include "../System/BlendMode.h"
#include "../Model/PrimitiveModel.h"
#include "ParticleManager.h"
#include "../Camera/CameraManager.h"

using namespace MyMath;

/// @brief エフェクト自動停止条件（stop()手動停止 / 時間指定 / 回数指定）
enum class EffectStopCondition : int {
	Manual = 0,     ///< sample->stop() が呼ばれるまで継続発生
	ByDuration = 1, ///< 指定時間 (maxDuration [秒]) 経過で自動停止
	ByCount = 2,    ///< 指定回数 (maxEmitCount [回]) 発生で自動停止
};

/// @brief .effect ファイル内の1パーティクルレイヤー設定
struct EffectParticleLayer {
	std::string layerName = "Layer0";
	std::string groupName = "ExplosionSmoke";
	std::string texturePath = "assets/textures/smoke_particle.png";
	BlendMode blendMode = BlendMode::kNormal;
	uint32_t emitCount = 3u;
	ParticleParameters params{};
};

/// @brief 外部ファイル (.effect) から読み込まれるエフェクト設定データ
struct EffectPreset {
	std::string name = "default_fire";
	std::string filePath = "Resources/effects/default_fire.effect";
	EffectStopCondition stopCondition = EffectStopCondition::Manual;
	float emitInterval = 0.12f;
	float maxDuration = 5.0f;
	uint32_t maxEmitCount = 30u;
	std::vector<EffectParticleLayer> layers;

	bool LoadFromEffectFile(const std::string& path);
	bool SaveToEffectFile(const std::string& path) const;
};

/// @brief コードから `if(条件){ sample->emit(); }` / `sample->stop()` / `sample->Delete()` で扱えるハンドル
class EffectHandle {
public:
	EffectHandle() = default;
	explicit EffectHandle(const std::string& effectName);

	/// @brief エフェクト発生（if(条件){ sample->emit(); } で呼び出し）
	void emit();
	void emit(const Vector3& position);
	void Emit() { emit(); }
	void Emit(const Vector3& position) { emit(position); }

	/// @brief エフェクト停止（sample->stop() で呼び出し）
	void stop();
	void Stop() { stop(); }

	/// @brief ハンドルの破棄要求（EffectManagerから自動削除される）
	void Delete();
	void destroy() { Delete(); }
	bool IsPendingDelete() const { return pendingDelete_; }

	/// @brief 毎フレーム更新（EffectManager::Update() から自動呼び出し）
	void Update(float dt);

	/// @brief 発生座標の設定・取得
	void SetPosition(const Vector3& position) { position_ = position; }
	const Vector3& GetPosition() const { return position_; }

	/// @brief 参照する .effect 名の設定・取得
	void SetEffectName(const std::string& effectName) { effectName_ = effectName; }
	const std::string& GetEffectName() const { return effectName_; }

	/// @brief 停止条件のコード側オーバーライド設定
	void SetStopCondition(EffectStopCondition condition) {
		overrideStopCondition_ = true;
		customStopCondition_ = condition;
	}
	void SetStopByDuration(float seconds) {
		overrideStopCondition_ = true;
		customStopCondition_ = EffectStopCondition::ByDuration;
		customMaxDuration_ = seconds;
	}
	void SetStopByCount(uint32_t maxCount) {
		overrideStopCondition_ = true;
		customStopCondition_ = EffectStopCondition::ByCount;
		customMaxEmitCount_ = maxCount;
	}
	void ClearStopConditionOverride() { overrideStopCondition_ = false; }

	/// @brief 状態取得
	bool IsEmitting() const { return isEmitting_; }
	bool IsCompleted() const { return isCompleted_; }
	float GetElapsedSeconds() const { return elapsedSeconds_; }
	uint32_t GetEmittedBurstCount() const { return emittedBurstCount_; }
	uint32_t GetTotalEmittedParticles() const { return totalEmittedParticles_; }

	EffectStopCondition GetEffectiveStopCondition() const;
	float GetEffectiveMaxDuration() const;
	uint32_t GetEffectiveMaxEmitCount() const;
	float GetEffectiveEmitInterval() const;

private:
	const EffectPreset* GetResolvedPreset() const;
	void EmitBurstNow();
	void CheckAutoStopCondition();

	std::string effectName_ = "default_fire";
	Vector3 position_ = { 0.0f, 0.0f, 0.0f };

	bool isEmitting_ = false;
	bool isCompleted_ = false;
	bool calledThisFrame_ = false;
	bool wasCalledLastFrame_ = false;
	bool pendingDelete_ = false;

	float elapsedSeconds_ = 0.0f;
	float intervalTimer_ = 0.0f;
	uint32_t emittedBurstCount_ = 0u;
	uint32_t totalEmittedParticles_ = 0u;

	bool overrideStopCondition_ = false;
	EffectStopCondition customStopCondition_ = EffectStopCondition::Manual;
	float customMaxDuration_ = 5.0f;
	uint32_t customMaxEmitCount_ = 30u;
};

class IEffect {
public:
	virtual ~IEffect() = default;
	virtual void Update() = 0;
	virtual void Draw(Camera* camera) = 0;
	bool IsDead() const { return isDead_; }

protected:
	bool isDead_ = false;
	Vector3 position_ = { 0,0,0 };
	float lifeTime_ = 1.0f;
	float currentTime_ = 0.0f;
};

// ============================================
// リングエフェクト
// ============================================
class RingEffect : public IEffect {
public:
	RingEffect(const Vector3& pos, const Vector4& color, float lifeTime, float maxRadius);
	void Update() override;
	void Draw(Camera* camera) override;
private:
	Vector4 color_;
	uint32_t textureIndex_;
	float currentRadius_ = 0.0f;
	float maxRadius_ = 5.0f;
	Vector3 scale_{ 0,0,0 };
};

// ============================================
// シリンダー柱エフェクト
// ============================================
class CylinderEffect : public IEffect {
public:
	CylinderEffect(const Vector3& pos, const Vector4& color, float lifeTime, float maxScaleY, float maxScaleX);
	void Update() override;
	void Draw(Camera* camera) override;
private:
	Vector4 color_;
	uint32_t textureIndex_;
	Vector3 scale_{ 0,0,0 };
	float maxScaleY_ = 10.0f;
	float maxScaleX_ = 2.0f;
};

// ============================================
// 回転するPlane(斬撃)エフェクト
// ============================================
class HitRotPlaneEffect : public IEffect {
public:
	HitRotPlaneEffect(const Vector3& pos, const Vector4& color, float lifeTime, float size);
	void Update() override;
	void Draw(Camera* camera) override;
private:
	Vector4 color_;
	uint32_t textureIndex_;
	Vector3 rotationEulers_{ 0,0,0 };
	Vector3 rotationSpeed_{ 0,0,0 };
	Vector3 scale_{ 1,1,1 };
	float targetSize_ = 2.0f;
};

// ============================================
// ビルボードパーティクルエフェクト (ラッパー)
// ============================================
class BillboardParticleEffect : public IEffect {
public:
	BillboardParticleEffect(const Vector3& pos, const std::string& groupName, uint32_t count);
	void Update() override;
	void Draw(Camera* camera) override;
};

// ============================================
// 爆発パーティクルエフェクト（火花+炎+煙の複合演出）
// ============================================
class ExplosionParticleEffect : public IEffect {
public:
	ExplosionParticleEffect(const Vector3& pos);
	void Update() override;
	void Draw(Camera* camera) override;

private:
	float flashScale_ = 1.0f;
	float flashAlpha_ = 1.0f;
	
	float shockwaveScale_ = 1.0f;
	float shockwaveAlpha_ = 1.0f;

	struct Debris {
		Vector3 pos;
		Vector3 vel;
	};
	std::vector<Debris> debris_;
	
	float lightIntensity_ = 0.0f;
};

// ============================================
// マズルフラッシュエフェクト（発砲時の火花+閃光）
// ============================================
class MuzzleFlashEffect : public IEffect {
public:
	MuzzleFlashEffect(const Vector3& pos, const Vector3& direction);
	void Update() override;
	void Draw(Camera* camera) override;
private:
	Vector3 direction_;
};

/// <summary>
/// エフェクト管理クラス (外部 .effect ファイル駆動・ハンドル管理・Debug専用GUI)
/// </summary>
class EffectManager {
public:
	static EffectManager* GetInstance();

	EffectManager() = default;
	~EffectManager() = default;

	void Initialize();
	void Update();
	void Draw(Camera* camera);
	void Finalize();

	void AddEffect(std::unique_ptr<IEffect> effect);

	// 既存互換エフェクト発行関数
	void EmitHitEffect(const Vector3& position);
	void EmitHitPlaneEffect(const Vector3& position);
	void EmitDestroyEffect(const Vector3& position);
	void EmitMuzzleFlash(const Vector3& position, const Vector3& direction);

	// ============================================
	// 外部 .effect ファイル管理 & ハンドル操作 (emit / stop / delete)
	// ============================================
	void LoadAllEffectsFromDirectory(const std::string& directory = "Resources/effects");
	static std::vector<std::string> GetAvailableEffectFiles(const std::string& directory = "Resources/effects");

	EffectPreset* FindPreset(const std::string& nameOrStem);
	const EffectPreset* FindPreset(const std::string& nameOrStem) const;
	EffectPreset& LoadOrCreatePreset(const std::string& filepath);
	std::vector<EffectPreset>& GetPresets() { return presets_; }
	const std::vector<EffectPreset>& GetPresets() const { return presets_; }

	/// @brief 名前付き共有ハンドルの取得（存在しなければ生成）
	EffectHandle* GetHandle(const std::string& handleName, const std::string& effectName = "");
	/// @brief 独立ハンドルの生成（不要時に handle->Delete() または DeleteHandle(handle) で破棄可能）
	EffectHandle* CreateHandle(const std::string& effectName);
	/// @brief ハンドルの明示的削除
	void DeleteHandle(EffectHandle*& handle);
	void StopAllHandles();

	void ApplyLayerParticleGroup(const EffectParticleLayer& layer);
	size_t GetActiveEffectCount() const { return effects_.size(); }

#ifdef _DEBUG
	/// @brief Debugビルド専用のエフェクトパラメータ編集GUI（量・上昇速度・上限高さ・加速度・寿命・停止条件）
	void DrawDebugGui(EffectHandle* previewHandle = nullptr);
#endif

private:
	EffectManager(const EffectManager&) = delete;
	EffectManager& operator=(const EffectManager&) = delete;

	void EnsureCoreParticleGroupsCreated();

	static std::unique_ptr<EffectManager> instance_;

	std::list<std::unique_ptr<IEffect>> effects_;
	std::vector<EffectPreset> presets_;
	std::unordered_map<std::string, std::unique_ptr<EffectHandle>> namedHandles_;
	std::vector<std::unique_ptr<EffectHandle>> handles_;
	bool isInitialized_ = false;

#ifdef _DEBUG
	int selectedPresetIndex_ = 0;
	int selectedLayerIndex_ = 0;
	char newEffectFileName_[128] = "custom_effect";
	std::string statusMessage_;
#endif
};
