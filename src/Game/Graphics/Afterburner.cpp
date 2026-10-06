#include "Afterburner.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include <cmath>
#include <algorithm>

namespace {
	constexpr float kPi = 3.14159265358979323846f;
}

Afterburner::Afterburner() {
}

void Afterburner::Initialize() {
	TextureManager* tm = TextureManager::GetInstance();
	tm->LoadTexture("assets/textures/afterburner_flame.png");
	flameTexIndex_ = tm->GetTextureIndexByFilePath("assets/textures/afterburner_flame.png");

	tm->LoadTexture("assets/textures/shock_diamond.png");
	shockDiamondTexIndex_ = tm->GetTextureIndexByFilePath("assets/textures/shock_diamond.png");

	tm->LoadTexture("assets/textures/white1x1.png");
	whiteTexIndex_ = tm->GetTextureIndexByFilePath("assets/textures/white1x1.png");
}

void Afterburner::Update(float dt, float intensity) {
	animTimer_ += dt;
	currentIntensity_ = (std::max)(0.0f, (std::min)(1.0f, intensity));

	// アフターバーナー特有の超音速燃焼乱流・微細パルス
	flicker_ = 1.0f + 0.045f * std::sin(animTimer_ * 58.0f) + 0.025f * std::cos(animTimer_ * 112.0f);
}

void Afterburner::Draw(const MyMath::Vector3& aircraftPosition, const MyMath::Vector3& aircraftRotation, Camera* camera) {
	Matrix4x4 acWorld = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, aircraftRotation, aircraftPosition);
	Draw(acWorld, camera);
}

void Afterburner::Draw(const MyMath::Matrix4x4& aircraftWorldMatrix, Camera* camera) {
	if (currentIntensity_ <= 0.001f || !camera) {
		return;
	}

	if (isTwinEngine_) {
		// 左エンジンノズル
		Matrix4x4 leftLocal = MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, nozzleRotation_, leftNozzleOffset_);
		Matrix4x4 leftWorld = leftLocal * aircraftWorldMatrix;
		DrawSingleNozzle(leftWorld, camera);

		// 右エンジンノズル
		Matrix4x4 rightLocal = MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, nozzleRotation_, rightNozzleOffset_);
		Matrix4x4 rightWorld = rightLocal * aircraftWorldMatrix;
		DrawSingleNozzle(rightWorld, camera);
	} else {
		// 単発エンジンノズル
		Matrix4x4 singleLocal = MyMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, nozzleRotation_, singleNozzleOffset_);
		Matrix4x4 singleWorld = singleLocal * aircraftWorldMatrix;
		DrawSingleNozzle(singleWorld, camera);
	}
}

void Afterburner::DrawSingleNozzle(const MyMath::Matrix4x4& nozzleWorldMatrix, Camera* camera) {
	PrimitiveModel* pm = PrimitiveModel::GetInstance();

	const float flameLength = baseFlameLength_ * (0.90f + 0.10f * flicker_) * currentIntensity_;
	const float flameRadius = baseFlameRadius_ * (0.95f + 0.05f * flicker_);
	const uint32_t flameTex = (flameTexIndex_ != 0) ? flameTexIndex_ : whiteTexIndex_;
	const uint32_t diamondTex = (shockDiamondTexIndex_ != 0) ? shockDiamondTexIndex_ : whiteTexIndex_;

	// ========================================================
	// 1. ノズルアイリス白熱リング (Nozzle Iris Glow)
	// ========================================================
	{
		// XZ平面リングをX軸90度回転させてXY面（ノズル出口断面）に一致させる
		Matrix4x4 ringLocal = MakeAffineMatrix(
			Vector3{ flameRadius * 1.05f, 1.0f, flameRadius * 1.05f },
			Vector3{ kPi * 0.5f, 0.0f, 0.0f },
			Vector3{ 0.0f, 0.0f, 0.02f }
		);
		Vector4 ringColor = { 0.95f, 0.98f, 1.0f, 0.95f * currentIntensity_ };
		pm->DrawRing(ringLocal * nozzleWorldMatrix, ringColor, whiteTexIndex_, camera, BlendMode::kAdd);
	}

	// ========================================================
	// 2. インナー白熱コア (White-Hot Core Jet)
	// ========================================================
	{
		float coreLen = flameLength * 0.38f;
		float halfCoreLen = coreLen * 0.5f;
		float coreRadius = flameRadius * 0.58f;

		// 底面（太い）がノズル出口、先端（細い）が後方(+Z)
		// Y軸先端を+Zに向けるため X軸 -90度回転
		Matrix4x4 coreConeLocal = MakeAffineMatrix(
			Vector3{ coreRadius, halfCoreLen, coreRadius },
			Vector3{ -kPi * 0.5f, 0.0f, 0.0f },
			Vector3{ 0.0f, 0.0f, halfCoreLen }
		);
		Vector4 coreColor = { coreColor_.x, coreColor_.y, coreColor_.z, 0.95f * currentIntensity_ };
		pm->DrawCone(coreConeLocal * nozzleWorldMatrix, coreColor, flameTex, camera, BlendMode::kAdd);
	}

	// ========================================================
	// 3. ショックダイヤモンド（マッハディスク / Shock Diamonds）列
	// ========================================================
	{
		const int count = shockDiamondCount_;
		const float spacing = 0.72f * (0.92f + 0.12f * currentIntensity_ * flicker_);

		for (int i = 0; i < count; ++i) {
			float zCenter = (static_cast<float>(i) + 1.0f) * spacing;
			float fade = std::pow(1.0f - static_cast<float>(i) / (count + 1.2f), 0.85f) * currentIntensity_;

			// 各ダイヤモンドのサイズ
			float rDisc = flameRadius * (0.42f + static_cast<float>(i) * 0.030f) * flicker_;
			float halfDiamondLen = (spacing * 0.44f) * 0.5f;

			// 色のグラデーション（前方は coreColor_、後方は baseFlameColor_ へブレンド）
			float t = (count > 1) ? (static_cast<float>(i) / static_cast<float>(count - 1)) : 0.0f;
			Vector3 mixedColor = {
				coreColor_.x * (1.0f - t) + baseFlameColor_.x * t,
				coreColor_.y * (1.0f - t) + baseFlameColor_.y * t,
				coreColor_.z * (1.0f - t) + baseFlameColor_.z * t
			};
			Vector4 diamondColor = { mixedColor.x, mixedColor.y, mixedColor.z, (0.96f - 0.35f * t) * fade };

			// (A) 前半コーン（収束ショック: 先端が前方-Z、底面が中心zCenter）
			Matrix4x4 coneForeLocal = MakeAffineMatrix(
				Vector3{ rDisc, halfDiamondLen, rDisc },
				Vector3{ kPi * 0.5f, 0.0f, 0.0f },
				Vector3{ 0.0f, 0.0f, zCenter - halfDiamondLen }
			);
			pm->DrawCone(coneForeLocal * nozzleWorldMatrix, diamondColor, flameTex, camera, BlendMode::kAdd);

			// (B) 後半コーン（発散ショック: 先端が後方+Z、底面が中心zCenter）
			Matrix4x4 coneAftLocal = MakeAffineMatrix(
				Vector3{ rDisc, halfDiamondLen, rDisc },
				Vector3{ -kPi * 0.5f, 0.0f, 0.0f },
				Vector3{ 0.0f, 0.0f, zCenter + halfDiamondLen }
			);
			pm->DrawCone(coneAftLocal * nozzleWorldMatrix, diamondColor, flameTex, camera, BlendMode::kAdd);

			// (C) 節の中心に位置する十字交差ショックダイヤモンド・ディスク（光芒クワッド）
			float quadSize = rDisc * 2.4f;
			// クワッド1 (XY平面)
			Matrix4x4 quad1Local = MakeAffineMatrix(
				Vector3{ quadSize, 1.0f, quadSize },
				Vector3{ kPi * 0.5f, 0.0f, 0.0f },
				Vector3{ 0.0f, 0.0f, zCenter }
			);
			pm->DrawPlane(quad1Local * nozzleWorldMatrix, diamondColor, diamondTex, camera, BlendMode::kAdd);

			// クワッド2 (YZ平面: Y軸90度回転)
			Matrix4x4 quad2Local = MakeAffineMatrix(
				Vector3{ quadSize, 1.0f, quadSize },
				Vector3{ 0.0f, 0.0f, kPi * 0.5f },
				Vector3{ 0.0f, 0.0f, zCenter }
			);
			pm->DrawPlane(quad2Local * nozzleWorldMatrix, diamondColor, diamondTex, camera, BlendMode::kAdd);
		}
	}

	// ========================================================
	// 4. アウタープルーム (メインシリンダー・排気被膜)
	// ========================================================
	{
		float halfLen = flameLength * 0.5f;

		// CylinderはY軸方向なので、X軸90度回転でZ軸に合わせる
		Matrix4x4 cylLocal = MakeAffineMatrix(
			Vector3{ flameRadius * 0.92f * flicker_, halfLen, flameRadius * 0.92f * flicker_ },
			Vector3{ kPi * 0.5f, 0.0f, 0.0f },
			Vector3{ 0.0f, 0.0f, halfLen }
		);
		Vector4 cylColor = { baseFlameColor_.x, baseFlameColor_.y, baseFlameColor_.z, 0.72f * currentIntensity_ };
		pm->DrawCylinder(cylLocal * nozzleWorldMatrix, cylColor, flameTex, camera, BlendMode::kAdd);
	}

	// ========================================================
	// 5. アウターグローシース (拡散オーラシリンダー)
	// ========================================================
	{
		float sheathLen = flameLength * 1.15f;
		float halfSheathLen = sheathLen * 0.5f;
		float sheathRadius = flameRadius * 1.35f;

		Matrix4x4 sheathLocal = MakeAffineMatrix(
			Vector3{ sheathRadius, halfSheathLen, sheathRadius },
			Vector3{ kPi * 0.5f, 0.0f, 0.0f },
			Vector3{ 0.0f, 0.0f, halfSheathLen }
		);
		Vector4 sheathColor = { 0.10f, 0.40f, 0.85f, 0.35f * currentIntensity_ };
		pm->DrawCylinder(sheathLocal * nozzleWorldMatrix, sheathColor, flameTex, camera, BlendMode::kAdd);
	}

	// ========================================================
	// 6. 後方排気熱気トレイル (排気プルーム)
	// ========================================================
	{
		float trailLen = flameLength * 1.55f;
		float halfTrailLen = trailLen * 0.5f;
		float trailRadius = flameRadius * 1.65f;

		Matrix4x4 trailLocal = MakeAffineMatrix(
			Vector3{ trailRadius, halfTrailLen, trailRadius },
			Vector3{ kPi * 0.5f, 0.0f, 0.0f },
			Vector3{ 0.0f, 0.0f, halfTrailLen + 0.4f }
		);
		Vector4 trailColor = { 0.18f, 0.35f, 0.65f, 0.18f * currentIntensity_ };
		pm->DrawCylinder(trailLocal * nozzleWorldMatrix, trailColor, flameTex, camera, BlendMode::kAdd);
	}
}

#include <fstream>
#include <sstream>
#include <filesystem>

bool Afterburner::SaveConfig(const std::string& filePath) const {
	std::filesystem::path dir = std::filesystem::path(filePath).parent_path();
	if (!dir.empty() && !std::filesystem::exists(dir)) {
		std::filesystem::create_directories(dir);
	}

	std::ofstream ofs(filePath);
	if (!ofs.is_open()) {
		return false;
	}

	ofs << "{\n";
	ofs << "  \"isTwinEngine\": " << (isTwinEngine_ ? "true" : "false") << ",\n";
	ofs << "  \"singleNozzleOffset\": [" << singleNozzleOffset_.x << ", " << singleNozzleOffset_.y << ", " << singleNozzleOffset_.z << "],\n";
	ofs << "  \"leftNozzleOffset\": [" << leftNozzleOffset_.x << ", " << leftNozzleOffset_.y << ", " << leftNozzleOffset_.z << "],\n";
	ofs << "  \"rightNozzleOffset\": [" << rightNozzleOffset_.x << ", " << rightNozzleOffset_.y << ", " << rightNozzleOffset_.z << "],\n";
	ofs << "  \"baseFlameLength\": " << baseFlameLength_ << ",\n";
	ofs << "  \"baseFlameRadius\": " << baseFlameRadius_ << ",\n";
	ofs << "  \"shockDiamondCount\": " << shockDiamondCount_ << ",\n";
	ofs << "  \"baseFlameColor\": [" << baseFlameColor_.x << ", " << baseFlameColor_.y << ", " << baseFlameColor_.z << "],\n";
	ofs << "  \"coreColor\": [" << coreColor_.x << ", " << coreColor_.y << ", " << coreColor_.z << "],\n";
	ofs << "  \"nozzleRotation\": [" << nozzleRotation_.x << ", " << nozzleRotation_.y << ", " << nozzleRotation_.z << "],\n";
	ofs << "  \"currentIntensity\": " << currentIntensity_ << "\n";
	ofs << "}\n";

	return true;
}

bool Afterburner::LoadConfig(const std::string& filePath) {
	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		return false;
	}

	std::string line;
	while (std::getline(ifs, line)) {
		if (line.find("\"isTwinEngine\"") != std::string::npos) {
			isTwinEngine_ = (line.find("true") != std::string::npos);
		} else if (line.find("\"singleNozzleOffset\"") != std::string::npos) {
			size_t openBracket = line.find('[');
			size_t closeBracket = line.find(']');
			if (openBracket != std::string::npos && closeBracket != std::string::npos) {
				std::string vals = line.substr(openBracket + 1, closeBracket - openBracket - 1);
				float x, y, z;
				if (sscanf_s(vals.c_str(), "%f, %f, %f", &x, &y, &z) == 3) {
					singleNozzleOffset_ = MyMath::Vector3{ x, y, z };
				}
			}
		} else if (line.find("\"leftNozzleOffset\"") != std::string::npos) {
			size_t openBracket = line.find('[');
			size_t closeBracket = line.find(']');
			if (openBracket != std::string::npos && closeBracket != std::string::npos) {
				std::string vals = line.substr(openBracket + 1, closeBracket - openBracket - 1);
				float x, y, z;
				if (sscanf_s(vals.c_str(), "%f, %f, %f", &x, &y, &z) == 3) {
					leftNozzleOffset_ = MyMath::Vector3{ x, y, z };
				}
			}
		} else if (line.find("\"rightNozzleOffset\"") != std::string::npos) {
			size_t openBracket = line.find('[');
			size_t closeBracket = line.find(']');
			if (openBracket != std::string::npos && closeBracket != std::string::npos) {
				std::string vals = line.substr(openBracket + 1, closeBracket - openBracket - 1);
				float x, y, z;
				if (sscanf_s(vals.c_str(), "%f, %f, %f", &x, &y, &z) == 3) {
					rightNozzleOffset_ = MyMath::Vector3{ x, y, z };
				}
			}
		} else if (line.find("\"baseFlameLength\"") != std::string::npos) {
			size_t colon = line.find(':');
			if (colon != std::string::npos) {
				baseFlameLength_ = std::stof(line.substr(colon + 1));
			}
		} else if (line.find("\"baseFlameRadius\"") != std::string::npos) {
			size_t colon = line.find(':');
			if (colon != std::string::npos) {
				baseFlameRadius_ = std::stof(line.substr(colon + 1));
			}
		} else if (line.find("\"shockDiamondCount\"") != std::string::npos) {
			size_t colon = line.find(':');
			if (colon != std::string::npos) {
				shockDiamondCount_ = std::stoi(line.substr(colon + 1));
			}
		} else if (line.find("\"baseFlameColor\"") != std::string::npos) {
			size_t openBracket = line.find('[');
			size_t closeBracket = line.find(']');
			if (openBracket != std::string::npos && closeBracket != std::string::npos) {
				std::string vals = line.substr(openBracket + 1, closeBracket - openBracket - 1);
				float r, g, b;
				if (sscanf_s(vals.c_str(), "%f, %f, %f", &r, &g, &b) == 3) {
					baseFlameColor_ = MyMath::Vector3{ r, g, b };
				}
			}
		} else if (line.find("\"coreColor\"") != std::string::npos) {
			size_t openBracket = line.find('[');
			size_t closeBracket = line.find(']');
			if (openBracket != std::string::npos && closeBracket != std::string::npos) {
				std::string vals = line.substr(openBracket + 1, closeBracket - openBracket - 1);
				float r, g, b;
				if (sscanf_s(vals.c_str(), "%f, %f, %f", &r, &g, &b) == 3) {
					coreColor_ = MyMath::Vector3{ r, g, b };
				}
			}
		} else if (line.find("\"nozzleRotation\"") != std::string::npos) {
			size_t openBracket = line.find('[');
			size_t closeBracket = line.find(']');
			if (openBracket != std::string::npos && closeBracket != std::string::npos) {
				std::string vals = line.substr(openBracket + 1, closeBracket - openBracket - 1);
				float rx, ry, rz;
				if (sscanf_s(vals.c_str(), "%f, %f, %f", &rx, &ry, &rz) == 3) {
					nozzleRotation_ = MyMath::Vector3{ rx, ry, rz };
				}
			}
		} else if (line.find("\"currentIntensity\"") != std::string::npos) {
			size_t colon = line.find(':');
			if (colon != std::string::npos) {
				currentIntensity_ = std::stof(line.substr(colon + 1));
			}
		}
	}

	return true;
}
