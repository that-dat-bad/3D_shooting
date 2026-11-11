#pragma once
#include <string>
#include <vector>
#include "AirFrame.h"
#include "Engine.h"
#include "Payload/Gunpod.h"

// 機体の全体設定
struct AircraftGeneralConfig {
	std::string name = "MiG-21 Fishbed";
	std::string modelPath = "Resources/models/m21.gltf";
	float environmentCoefficient = 0.45f;
	float specularIntensity = 1.35f;
	float shininess = 48.0f;
};

// 機体設定ファイルの管理クラス
class AircraftConfig {
public:
	AircraftConfig() = default;
	~AircraftConfig() = default;

	// 設定ファイルの読み込み
	// filepath: ファイルパス
	bool LoadFromCfg(const std::string& filepath);

	// 設定ファイルの保存
	// filepath: 保存先パス
	bool SaveToCfg(const std::string& filepath) const;

	// デフォルト設定の生成
	// filepath: 保存先パス
	void CreateDefault(const std::string& filepath);

	// 利用可能な機体設定の一覧取得
	// directory: 走査するディレクトリ
	static std::vector<std::string> GetAvailableAircraftList(const std::string& directory = "Resources/aircraft");

	// === アクセッサ ===
	const AircraftGeneralConfig& GetGeneral() const { return general_; }
	AircraftGeneralConfig& GetGeneral() { return general_; }

	const AirframeData& GetAirframe() const { return airframe_; }
	AirframeData& GetAirframe() { return airframe_; }

	const EngineData& GetEngine() const { return engine_; }
	EngineData& GetEngine() { return engine_; }

	const GunPodData& GetGunpod() const { return gunpod_; }
	GunPodData& GetGunpod() { return gunpod_; }

	const std::string& GetCurrentFilePath() const { return currentFilePath_; }
	void SetCurrentFilePath(const std::string& path) { currentFilePath_ = path; }

private:
	AircraftGeneralConfig general_{};
	AirframeData airframe_{};
	EngineData engine_{};
	GunPodData gunpod_{};
	std::string currentFilePath_ = "Resources/aircraft/mig21.cfg";
};
