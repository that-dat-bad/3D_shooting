#include "UITextRegistry.h"
#include "UIText.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>

// 文字列トリム用ヘルパー
static inline std::string Trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return "";
	size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, (last - first + 1));
}

// INIデータのマップ型: Section -> (Key -> Value)
using IniData = std::map<std::string, std::map<std::string, std::string>>;

// INIファイル読み込み関数
static IniData ParseIniFile(const std::string& filePath) {
	IniData data;
	std::ifstream ifs(filePath);
	if (!ifs.is_open()) return data;

	std::string currentSection = "";
	std::string line;

	while (std::getline(ifs, line)) {
		std::string trimmed = Trim(line);
		if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
			continue; // 空行またはコメント
		}

		// セクション判定: [SectionName]
		if (trimmed.front() == '[' && trimmed.back() == ']') {
			currentSection = Trim(trimmed.substr(1, trimmed.size() - 2));
			continue;
		}

		// キー=値 判定
		if (!currentSection.empty()) {
			size_t eqPos = trimmed.find('=');
			if (eqPos != std::string::npos) {
				std::string key = Trim(trimmed.substr(0, eqPos));
				std::string val = Trim(trimmed.substr(eqPos + 1));
				data[currentSection][key] = val;
			}
		}
	}
	return data;
}

// INIファイル書き出し関数
static bool WriteIniFile(const std::string& filePath, const IniData& data) {
	std::ofstream ofs(filePath);
	if (!ofs.is_open()) return false;

	ofs << "; ==========================================\n";
	ofs << "; UI Text Styles & Motion Configuration File (INI)\n";
	ofs << "; ==========================================\n\n";

	for (const auto& [section, kvMap] : data) {
		ofs << "[" << section << "]\n";
		for (const auto& [key, val] : kvMap) {
			ofs << key << "=" << val << "\n";
		}
		ofs << "\n";
	}
	return true;
}

UITextRegistry* UITextRegistry::GetInstance() {
	static UITextRegistry instance;
	return &instance;
}

void UITextRegistry::Register(const std::string& name, UIText* text) {
	if (text) {
		registry_[name] = text;
		// 登録時に自動的にINIの保存済みスタイルを適用
		ApplyConfig(name, text);
	}
}

void UITextRegistry::Unregister(const std::string& name) {
	registry_.erase(name);
}

void UITextRegistry::Clear() {
	registry_.clear();
}

std::vector<std::string> UITextRegistry::GetSortedNames() const {
	std::vector<std::string> names;
	names.reserve(registry_.size());
	for (const auto& [name, _] : registry_) {
		names.push_back(name);
	}
	std::sort(names.begin(), names.end());
	return names;
}

static Vector4 ParseColorString(const std::string& str, const Vector4& defaultCol = { 1, 1, 1, 1 }) {
	std::stringstream ss(str);
	std::string rStr, gStr, bStr, aStr;
	if (std::getline(ss, rStr, ',') && std::getline(ss, gStr, ',') &&
		std::getline(ss, bStr, ',') && std::getline(ss, aStr, ',')) {
		try {
			return { std::stof(rStr), std::stof(gStr), std::stof(bStr), std::stof(aStr) };
		} catch (...) {}
	}
	return defaultCol;
}

void UITextRegistry::ApplyConfig(const std::string& name, UIText* text) {
	if (!text) return;

	IniData ini = ParseIniFile("assets/ui/text_styles.ini");
	auto it = ini.find(name);
	if (it == ini.end()) return;

	const auto& props = it->second;

	auto itText = props.find("text");
	if (itText != props.end()) text->SetText(itText->second);

	auto itFont = props.find("fontName");
	if (itFont != props.end()) text->SetFontName(itFont->second);

	auto itSize = props.find("fontSize");
	if (itSize != props.end()) {
		try { text->SetFontSize(std::stof(itSize->second)); } catch (...) {}
	}

	auto itPosX = props.find("posX");
	auto itPosY = props.find("posY");
	if (itPosX != props.end() && itPosY != props.end()) {
		try {
			text->SetPosition({ std::stof(itPosX->second), std::stof(itPosY->second) });
		} catch (...) {}
	}

	auto itAncX = props.find("anchorX");
	auto itAncY = props.find("anchorY");
	if (itAncX != props.end() && itAncY != props.end()) {
		try {
			text->SetAnchorPoint({ std::stof(itAncX->second), std::stof(itAncY->second) });
		} catch (...) {}
	}

	auto itColor = props.find("color");
	if (itColor != props.end()) {
		text->SetColor(ParseColorString(itColor->second, text->GetColor()));
	}

	auto itVis = props.find("visible");
	if (itVis != props.end()) {
		text->SetVisible(itVis->second == "1" || itVis->second == "true" || itVis->second == "TRUE");
	}

	// 状態別スタイル（Normal, Hovered, Selected, Pressed, LowHP, Warning, Complete等）の読み込み
	auto& styles = text->GetStyles();
	for (const auto& [key, val] : props) {
		size_t underPos = key.find('_');
		std::string stateName;
		std::string propName;
		if (underPos != std::string::npos) {
			stateName = key.substr(0, underPos);
			propName = key.substr(underPos + 1);
		} else if (key.rfind("normal", 0) == 0 && key.size() > 6) {
			stateName = "Normal";
			propName = key.substr(6);
			if (!propName.empty()) propName[0] = static_cast<char>(tolower(propName[0]));
		} else {
			continue;
		}

		auto& st = styles[stateName];
		if (propName == "color") {
			st.color = ParseColorString(val, st.color);
		} else if (propName == "scale") {
			try { st.scale = std::stof(val); } catch (...) {}
		} else if (propName == "offsetY" || propName == "offsety") {
			try { st.offset.y = std::stof(val); } catch (...) {}
		} else if (propName == "offsetX" || propName == "offsetx") {
			try { st.offset.x = std::stof(val); } catch (...) {}
		} else if (propName == "motion" || propName == "loopMotion") {
			st.loopMotion = StringToLoopMotion(val);
		} else if (propName == "speed" || propName == "motionSpeed") {
			try { st.motionSpeed = std::stof(val); } catch (...) {}
		} else if (propName == "intensity" || propName == "motionIntensity") {
			try { st.motionIntensity = std::stof(val); } catch (...) {}
		}
	}
}

void UITextRegistry::LoadConfig(const std::string& filePath) {
	IniData ini = ParseIniFile(filePath);
	if (ini.empty()) return;

	for (const auto& [name, text] : registry_) {
		if (!text) continue;
		ApplyConfig(name, text);
	}
}

void UITextRegistry::SaveConfig(const std::string& filePath) {
	// 既存のINIファイルを読み込んでマージ（他シーンの登録テキストも保持）
	IniData ini = ParseIniFile(filePath);

	for (const auto& [name, text] : registry_) {
		if (!text) continue;
		Vector2 pos = text->GetPosition();
		Vector2 anchor = text->GetAnchorPoint();
		Vector4 color = text->GetColor();

		std::stringstream ssSize, ssPosX, ssPosY, ssAncX, ssAncY, ssCol;
		ssSize << std::fixed << std::setprecision(1) << text->GetFontSize();
		ssPosX << std::fixed << std::setprecision(1) << pos.x;
		ssPosY << std::fixed << std::setprecision(1) << pos.y;
		ssAncX << std::fixed << std::setprecision(2) << anchor.x;
		ssAncY << std::fixed << std::setprecision(2) << anchor.y;
		ssCol  << std::fixed << std::setprecision(2)
			   << color.x << "," << color.y << "," << color.z << "," << color.w;

		ini[name]["text"]     = text->GetText();
		ini[name]["fontName"] = text->GetFontName();
		ini[name]["fontSize"] = ssSize.str();
		ini[name]["posX"]     = ssPosX.str();
		ini[name]["posY"]     = ssPosY.str();
		ini[name]["anchorX"]  = ssAncX.str();
		ini[name]["anchorY"]  = ssAncY.str();
		ini[name]["color"]    = ssCol.str();
		ini[name]["visible"]  = text->IsVisible() ? "1" : "0";

		// スタイル・モーション情報の保存
		for (const auto& [stateName, st] : text->GetStyles()) {
			std::string prefix = (stateName == "Normal") ? "normal" : (stateName + "_");
			std::stringstream ssStCol, ssStScale, ssStOffY, ssStSpeed, ssStInt;
			ssStCol   << std::fixed << std::setprecision(2) << st.color.x << "," << st.color.y << "," << st.color.z << "," << st.color.w;
			ssStScale << std::fixed << std::setprecision(2) << st.scale;
			ssStOffY  << std::fixed << std::setprecision(1) << st.offset.y;
			ssStSpeed << std::fixed << std::setprecision(1) << st.motionSpeed;
			ssStInt   << std::fixed << std::setprecision(2) << st.motionIntensity;

			if (stateName == "Normal") {
				ini[name]["normalColor"]     = ssStCol.str();
				ini[name]["normalScale"]     = ssStScale.str();
				ini[name]["normalOffsetY"]   = ssStOffY.str();
				ini[name]["normalMotion"]    = LoopMotionToString(st.loopMotion);
				ini[name]["normalSpeed"]     = ssStSpeed.str();
				ini[name]["normalIntensity"] = ssStInt.str();
			} else {
				ini[name][prefix + "color"]     = ssStCol.str();
				ini[name][prefix + "scale"]     = ssStScale.str();
				ini[name][prefix + "offsetY"]   = ssStOffY.str();
				ini[name][prefix + "motion"]    = LoopMotionToString(st.loopMotion);
				ini[name][prefix + "speed"]     = ssStSpeed.str();
				ini[name][prefix + "intensity"] = ssStInt.str();
			}
		}
	}

	WriteIniFile(filePath, ini);
}
