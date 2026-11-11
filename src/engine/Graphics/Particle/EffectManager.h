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

// エフェクトの停止条件
enum class EffectStopCondition : int {
	Manual = 0,     // 手動停止
	ByDuration = 1, // 時間指定
	ByCount = 2,    // 回数指定
};

// パーティクルレイヤー設定
struct EffectParticleLayer {
	std::string layerName = "Layer0";
	std::string groupName = "ExplosionSmoke";
	std::string texturePath = "assets/textures/smoke_particle.png";
	BlendMode blendMode = BlendMode::kNormal;
	uint32_t emitCount = 3u;
	ParticleParameters params{};
};

// エフェクト設定データ
struct EffectPreset {
	std::string name = "default_fire";
	std::string filePath = "Resources/effects/default_fire.effect";
	EffectStopCondition stopCondition = EffectStopCondition::Manual;
	float emitInterval = 0.12f;
	float maxDuration = 5.0f;
	uint32_t maxEmitCount = 30u;
	std::vector<EffectParticleLayer> layers;

	// ファイル読み込み
	// path: 設定ファイルのパス
	bool LoadFromEffectFile(const std::string& path);
	// ファイル保存
	// path: 設定ファイルのパス
	bool SaveToEffectFile(const std::string& path) const;
};

// エフェクトハンドル
class EffectHandle {
public:
	EffectHandle() = default;
	explicit EffectHandle(const std::string& effectName);

	// エフェクト発生
	void emit();
	// position: 発生座標
	void emit(const Vector3& position);
	void Emit() { emit(); }
	void Emit(const Vector3& position) { emit(position); }

	// エフェクト停止
	void stop();
	void Stop() { stop(); }

	// 破棄要求
	void Delete();
	void destroy() { Delete(); }
	bool IsPendingDelete() const { return pendingDelete_; }

	// 更新
	// dt: デルタタイム
	void Update(float dt);

	void SetPosition(const Vector3& position) { position_ = position; }
	const Vector3& GetPosition() const { return position_; }

	void SetEffectName(const std::string& effectName) { effectName_ = effectName; }
	const std::string& GetEffectName() const { return effectName_; }

	// 停止条件の設定
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

// リングエフェクト
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

// シリンダーエフェクト
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

// 斬撃エフェクト
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

// ビルボードパーティクル
class BillboardParticleEffect : public IEffect {
public:
	BillboardParticleEffect(const Vector3& pos, const std::string& groupName, uint32_t count);
	void Update() override;
	void Draw(Camera* camera) override;
};

// 爆発エフェクト
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

// マズルフラッシュ
class MuzzleFlashEffect : public IEffect {
public:
	MuzzleFlashEffect(const Vector3& pos, const Vector3& direction);
	void Update() override;
	void Draw(Camera* camera) override;
private:
	Vector3 direction_;
};

// エフェクト管理者
class EffectManager {
public:
	static EffectManager* GetInstance();

	EffectManager() = default;
	~EffectManager() = default;

	// 初期化
	void Initialize();
	// 更新
	void Update();
	// 描画
	// camera: 描画カメラ
	void Draw(Camera* camera);
	// 終了処理
	void Finalize();

	void AddEffect(std::unique_ptr<IEffect> effect);

	// ヒットエフェクト発生
	// position: 発生座標
	void EmitHitEffect(const Vector3& position);
	// 斬撃エフェクト発生
	// position: 発生座標
	void EmitHitPlaneEffect(const Vector3& position);
	// 破壊エフェクト発生
	// position: 発生座標
	void EmitDestroyEffect(const Vector3& position);
	// マズルフラッシュ発生
	// position: 発生座標
	void EmitMuzzleFlash(const Vector3& position, const Vector3& direction);

	// エフェクトファイルの読み込み
	// directory: 読み込み先ディレクトリ
	void LoadAllEffectsFromDirectory(const std::string& directory = "Resources/effects");
	static std::vector<std::string> GetAvailableEffectFiles(const std::string& directory = "Resources/effects");

	EffectPreset* FindPreset(const std::string& nameOrStem);
	const EffectPreset* FindPreset(const std::string& nameOrStem) const;
	EffectPreset& LoadOrCreatePreset(const std::string& filepath);
	std::vector<EffectPreset>& GetPresets() { return presets_; }
	const std::vector<EffectPreset>& GetPresets() const { return presets_; }

	// ハンドルの取得
	// handleName: ハンドル名
	EffectHandle* GetHandle(const std::string& handleName, const std::string& effectName = "");
	// ハンドルの生成
	// effectName: エフェクト名
	EffectHandle* CreateHandle(const std::string& effectName);
	// ハンドルの削除
	// handle: 削除するハンドル
	void DeleteHandle(EffectHandle*& handle);
	void StopAllHandles();

	void ApplyLayerParticleGroup(const EffectParticleLayer& layer);
	size_t GetActiveEffectCount() const { return effects_.size(); }

#ifdef _DEBUG
	// デバッグ用GUI
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
