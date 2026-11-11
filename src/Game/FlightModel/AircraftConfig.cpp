#include "AircraftConfig.h"
#include "../../engine/io/ConfigFile.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

bool AircraftConfig::LoadFromCfg(const std::string& filepath) {
	if (!fs::exists(filepath)) {
		CreateDefault(filepath);
	}

	ConfigFile cfg;
	if (!cfg.LoadFromFile(filepath)) {
		return false;
	}

	currentFilePath_ = filepath;

	// [General]
	general_.name = cfg.GetString("General", "name", "MiG-21 Fishbed");
	general_.modelPath = cfg.GetString("General", "modelPath", "Resources/models/m21.gltf");
	general_.environmentCoefficient = cfg.GetFloat("General", "environmentCoefficient", 0.0f);
	general_.specularIntensity = cfg.GetFloat("General", "specularIntensity", 1.0f);
	general_.shininess = cfg.GetFloat("General", "shininess", 50.0f);

	// [Airframe]
	airframe_.emptyFrameMass = cfg.GetFloat("Airframe", "emptyFrameMass", 3000.0f);
	airframe_.maxInternalFuel = cfg.GetFloat("Airframe", "maxInternalFuel", 800.0f);
	airframe_.baseDrag = cfg.GetFloat("Airframe", "baseDrag", 0.02f);
	airframe_.liftCoefficient = cfg.GetFloat("Airframe", "liftCoefficient", 0.4f);
	airframe_.wingArea = cfg.GetFloat("Airframe", "wingArea", 40.0f);
	airframe_.maxHealth = cfg.GetFloat("Airframe", "maxHealth", 100.0f);
	airframe_.centerOfGravityZ = cfg.GetFloat("Airframe", "centerOfGravityZ", 0.0f);

	airframe_.criticalAoA = cfg.GetFloat("Airframe", "criticalAoA", 0.5f);
	airframe_.maxLiftCoefficient = cfg.GetFloat("Airframe", "maxLiftCoefficient", 1.5f);
	airframe_.stallLiftCoefficient = cfg.GetFloat("Airframe", "stallLiftCoefficient", 0.3f);

	airframe_.aspectRatio = cfg.GetFloat("Airframe", "aspectRatio", 6.0f);
	airframe_.oswaldEfficiency = cfg.GetFloat("Airframe", "oswaldEfficiency", 0.8f);
	airframe_.positiveGLimit = cfg.GetFloat("Airframe", "positiveGLimit", 9.0f);
	airframe_.negativeGLimit = cfg.GetFloat("Airframe", "negativeGLimit", -3.0f);

	airframe_.flapLiftBonus = cfg.GetFloat("Airframe", "flapLiftBonus", 0.5f);
	airframe_.flapDragBonus = cfg.GetFloat("Airframe", "flapDragBonus", 0.08f);
	airframe_.flapMaxSpeed = cfg.GetFloat("Airframe", "flapMaxSpeed", 97.0f);
	airframe_.flapDeploySpeed = cfg.GetFloat("Airframe", "flapDeploySpeed", 2.0f);

	airframe_.airBrakeDragBonus = cfg.GetFloat("Airframe", "airBrakeDragBonus", 0.15f);
	airframe_.airBrakeDeploySpeed = cfg.GetFloat("Airframe", "airBrakeDeploySpeed", 3.0f);

	// [Engine]
	engine_.mass = cfg.GetFloat("Engine", "mass", 500.0f);
	engine_.baseThrust = cfg.GetFloat("Engine", "baseThrust", 80000.0f);
	engine_.normalThrottleLimit = cfg.GetFloat("Engine", "normalThrottleLimit", 1.0f);
	engine_.wepThrottleLimit = cfg.GetFloat("Engine", "wepThrottleLimit", 1.1f);
	engine_.physicalSpoolSpeed = cfg.GetFloat("Engine", "physicalSpoolSpeed", 0.5f);
	engine_.baseFuelFlowRate = cfg.GetFloat("Engine", "baseFuelFlowRate", 2.0f);
	engine_.altitudeThrottleFactor = cfg.GetFloat("Engine", "altitudeThrottleFactor", 0.00004f);

	// [Gunpod]
	gunpod_.baseMass = cfg.GetFloat("Gunpod", "baseMass", 50.0f);
	gunpod_.drag = cfg.GetFloat("Gunpod", "drag", 0.005f);
	gunpod_.ammoWeight = cfg.GetFloat("Gunpod", "ammoWeight", 0.1f);
	gunpod_.maxAmmo = cfg.GetInt("Gunpod", "maxAmmo", 500);
	gunpod_.fireRate = cfg.GetFloat("Gunpod", "fireRate", 25.0f);

	return true;
}

bool AircraftConfig::SaveToCfg(const std::string& filepath) const {
	fs::path path(filepath);
	fs::path parent = path.parent_path();
	if (!parent.empty() && !fs::exists(parent)) {
		fs::create_directories(parent);
	}

	std::ofstream file(filepath);
	if (!file.is_open()) {
		return false;
	}

	file << "# ========================================================\n";
	file << "# Aircraft Configuration File (.cfg)\n";
	file << "# ========================================================\n\n";

	file << "[General]\n";
	file << "name = " << general_.name << "\n";
	file << "modelPath = " << general_.modelPath << "\n";
	file << "environmentCoefficient = " << general_.environmentCoefficient << "\n";
	file << "specularIntensity = " << general_.specularIntensity << "\n";
	file << "shininess = " << general_.shininess << "\n\n";

	file << "[Airframe]\n";
	file << "emptyFrameMass = " << airframe_.emptyFrameMass << "          # 空虚重量 (kg)\n";
	file << "maxInternalFuel = " << airframe_.maxInternalFuel << "         # 最大内蔵燃料 (kg)\n";
	file << "baseDrag = " << airframe_.baseDrag << "                 # 基本空気抵抗係数\n";
	file << "liftCoefficient = " << airframe_.liftCoefficient << "           # 迎え角0時の揚力係数\n";
	file << "wingArea = " << airframe_.wingArea << "                  # 翼面積 (m^2)\n";
	file << "maxHealth = " << airframe_.maxHealth << "                 # 最大耐久値\n";
	file << "centerOfGravityZ = " << airframe_.centerOfGravityZ << "           # 重心位置Z\n\n";

	file << "# --- 揚力・失速特性 ---\n";
	file << "criticalAoA = " << airframe_.criticalAoA << "              # 臨界迎え角 (rad)\n";
	file << "maxLiftCoefficient = " << airframe_.maxLiftCoefficient << "        # 最大揚力係数 (CLmax)\n";
	file << "stallLiftCoefficient = " << airframe_.stallLiftCoefficient << "      # 失速後残留揚力係数\n\n";

	file << "# --- 誘導抵抗・G制限 ---\n";
	file << "aspectRatio = " << airframe_.aspectRatio << "               # 翼アスペクト比\n";
	file << "oswaldEfficiency = " << airframe_.oswaldEfficiency << "          # オズワルド効率\n";
	file << "positiveGLimit = " << airframe_.positiveGLimit << "            # 最大許容 +G\n";
	file << "negativeGLimit = " << airframe_.negativeGLimit << "           # 最大許容 -G\n\n";

	file << "# --- フラップ特性 ---\n";
	file << "flapLiftBonus = " << airframe_.flapLiftBonus << "             # フラップ展開時CL増加量\n";
	file << "flapDragBonus = " << airframe_.flapDragBonus << "            # フラップ展開時Cd増加量\n";
	file << "flapMaxSpeed = " << airframe_.flapMaxSpeed << "              # フラップ制限速度 (m/s)\n";
	file << "flapDeploySpeed = " << airframe_.flapDeploySpeed << "           # フラップ展開速度 (1/s)\n\n";

	file << "# --- エアブレーキ特性 ---\n";
	file << "airBrakeDragBonus = " << airframe_.airBrakeDragBonus << "        # エアブレーキCd増加量\n";
	file << "airBrakeDeploySpeed = " << airframe_.airBrakeDeploySpeed << "      # エアブレーキ展開速度 (1/s)\n\n";

	file << "[Engine]\n";
	file << "mass = " << engine_.mass << "                     # エンジン重量 (kg)\n";
	file << "baseThrust = " << engine_.baseThrust << "             # 定格推力 (N)\n";
	file << "normalThrottleLimit = " << engine_.normalThrottleLimit << "       # 通常スロットル上限 (1.0)\n";
	file << "wepThrottleLimit = " << engine_.wepThrottleLimit << "          # WEPスロットル上限 (1.1)\n";
	file << "physicalSpoolSpeed = " << engine_.physicalSpoolSpeed << "        # スプール反応速度\n";
	file << "baseFuelFlowRate = " << engine_.baseFuelFlowRate << "          # 燃料消費率 (kg/s)\n";
	file << "altitudeThrottleFactor = " << engine_.altitudeThrottleFactor << "  # 高度推力低下係数\n\n";

	file << "[Gunpod]\n";
	file << "baseMass = " << gunpod_.baseMass << "                 # ガンポッド本体重量 (kg)\n";
	file << "drag = " << gunpod_.drag << "                     # ガンポッド抵抗係数\n";
	file << "ammoWeight = " << gunpod_.ammoWeight << "               # 弾丸1発の重量 (kg)\n";
	file << "maxAmmo = " << gunpod_.maxAmmo << "                  # 最大搭載弾数\n";
	file << "fireRate = " << gunpod_.fireRate << "                 # 発射レート (発/秒)\n";

	return true;
}

void AircraftConfig::CreateDefault(const std::string& filepath) {
	// デフォルト値 (MiG-21)
	general_.name = "MiG-21 Fishbed";
	general_.modelPath = "Resources/models/m21.gltf";
	general_.environmentCoefficient = 0.0f;
	general_.specularIntensity = 1.0f;
	general_.shininess = 50.0f;

	airframe_.emptyFrameMass = 3000.0f;
	airframe_.maxInternalFuel = 800.0f;
	airframe_.baseDrag = 0.02f;
	airframe_.liftCoefficient = 0.4f;
	airframe_.wingArea = 40.0f;
	airframe_.maxHealth = 100.0f;
	airframe_.centerOfGravityZ = 0.0f;
	airframe_.criticalAoA = 0.5f;
	airframe_.maxLiftCoefficient = 1.5f;
	airframe_.stallLiftCoefficient = 0.3f;
	airframe_.aspectRatio = 6.0f;
	airframe_.oswaldEfficiency = 0.8f;
	airframe_.positiveGLimit = 9.0f;
	airframe_.negativeGLimit = -3.0f;
	airframe_.flapLiftBonus = 0.5f;
	airframe_.flapDragBonus = 0.08f;
	airframe_.flapMaxSpeed = 97.0f;
	airframe_.flapDeploySpeed = 2.0f;
	airframe_.airBrakeDragBonus = 0.15f;
	airframe_.airBrakeDeploySpeed = 3.0f;

	engine_.mass = 500.0f;
	engine_.baseThrust = 80000.0f;
	engine_.normalThrottleLimit = 1.0f;
	engine_.wepThrottleLimit = 1.1f;
	engine_.physicalSpoolSpeed = 0.5f;
	engine_.baseFuelFlowRate = 2.0f;
	engine_.altitudeThrottleFactor = 0.00004f;

	gunpod_.baseMass = 50.0f;
	gunpod_.drag = 0.005f;
	gunpod_.ammoWeight = 0.1f;
	gunpod_.maxAmmo = 500;
	gunpod_.fireRate = 25.0f;

	SaveToCfg(filepath);
}

std::vector<std::string> AircraftConfig::GetAvailableAircraftList(const std::string& directory) {
	std::vector<std::string> list;
	fs::path dir(directory);

	if (!fs::exists(dir)) {
		fs::create_directories(dir);
	}

	try {
		for (const auto& entry : fs::directory_iterator(dir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".cfg") {
				list.push_back(entry.path().string());
			}
		}
	}
	catch (...) {}

	return list;
}
