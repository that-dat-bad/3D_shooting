#pragma once
#include <string>
#include <vector>
#include "AirFrame.h"
#include "Engine.h"
#include "Payload/Gunpod.h"

/// @brief 機体の全体・ビジュアル設定
struct AircraftGeneralConfig {
	std::string name = "MiG-21 Fishbed";
	std::string modelPath = "Resources/models/m21.gltf";
	float environmentCoefficient = 0.0f;
	float specularIntensity = 1.0f;
	float shininess = 50.0f;
};

/// @brief 機体設定ファイル（.cfg）の読み込み・保存・管理クラス
class AircraftConfig {
public:
	AircraftConfig() = default;
	~AircraftConfig() = default;

	/// @brief .cfg ファイルから機体設定をロード
	/// @param filepath .cfg ファイルのパス
	/// @return 成功時 true
	bool LoadFromCfg(const std::string& filepath);

	/// @brief .cfg ファイルへ機体設定を保存
	/// @param filepath 保存先の .cfg ファイルパス
	/// @return 成功時 true
	bool SaveToCfg(const std::string& filepath) const;

	/// @brief デフォルトの機体設定を生成して保存
	/// @param filepath 保存先パス
	void CreateDefault(const std::string& filepath);

	/// @brief 利用可能な機体CFGファイル一覧を取得
	/// @param directory 走査するディレクトリ (デフォルト: "Resources/aircraft")
	/// @return ファイルパス一覧
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
