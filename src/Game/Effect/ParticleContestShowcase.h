#pragma once
#ifdef USE_IMGUI

#include "../../engine/base/Math/MyMath.h"
#include "CountermeasureSystem.h"
#include "AeroVaporSystem.h"
#include "AmbientWeatherSystem.h"
#include "VirtualFlybyJet.h"

class Camera;

/// <summary>
/// EffectEditorScene (F9) 向け「Particle Contest Showcase」専用コントローラ
/// （4種コンテストエフェクトのワンクリック実演・グランドフィナーレ演出・Bloom制御・GPU統計）
/// </summary>
class ParticleContestShowcase {
public:
	void Initialize(Camera* camera);
	void Update(float dt, Camera* camera, const MyMath::Vector3& emitOrigin);
	void Draw(Camera* camera);
	void Finalize();

	/// <summary>
	/// 外部で ClearAllParticles() が呼ばれた際に環境パーティクルの再充填を予約する
	/// </summary>
	void OnParticlesCleared();

private:
	void UpdateGrandFinaleSequence(float dt, const MyMath::Vector3& origin);
	void DecayExplosionPointLight(float dt) const;
	void SyncBloomPostEffect() const;
	void DrawImGuiPanel(Camera* camera, const MyMath::Vector3& emitOrigin);

	CountermeasureSystem countermeasures_;
	AeroVaporSystem aeroVapor_;
	AmbientWeatherSystem ambientWeather_;
	VirtualFlybyJet virtualJet_;

	// 爆発テスト設定
	int explosionPresetIndex_ = 1; // 0: Tactical, 1: MassiveShowcase
	float explosionScale_ = 1.0f;

	// Bloom ポストエフェクト制御
	bool bloomEnabled_ = true;
	float bloomRadius_ = 2.2f;
	float bloomStrength_ = 1.35f;
	float bloomThreshold_ = 0.68f;

	// オールインワン・グランドフィナーレ自動演出
	bool grandFinaleActive_ = false;
	float grandFinaleTimer_ = 0.0f;
	int grandFinaleStage_ = 0;

	bool prevReadbackEnabled_ = false;
};

#endif // USE_IMGUI
