#include "ConfigFile.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

std::string ConfigFile::Trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return "";
	size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, (last - first + 1));
}

std::string ConfigFile::RemoveComments(const std::string& line) {
	// コメント開始文字: #, ;, //
	size_t commentPos = std::string::npos;

	size_t hashPos = line.find('#');
	size_t semiPos = line.find(';');
	size_t slashPos = line.find("//");

	if (hashPos != std::string::npos) commentPos = (std::min)(commentPos, hashPos);
	if (semiPos != std::string::npos) commentPos = (std::min)(commentPos, semiPos);
	if (slashPos != std::string::npos) commentPos = (std::min)(commentPos, slashPos);

	if (commentPos != std::string::npos) {
		return line.substr(0, commentPos);
	}
	return line;
}

void ConfigFile::Clear() {
	data_.clear();
	sectionOrder_.clear();
	keyOrder_.clear();
}

bool ConfigFile::LoadFromFile(const std::string& filepath) {
	std::ifstream file(filepath);
	if (!file.is_open()) {
		return false;
	}

	Clear();
	std::string currentSection = "General";
	std::string line;

	while (std::getline(file, line)) {
		std::string cleanLine = Trim(RemoveComments(line));
		if (cleanLine.empty()) continue;

		// セクション判定 [SectionName]
		if (cleanLine.front() == '[' && cleanLine.back() == ']') {
			currentSection = Trim(cleanLine.substr(1, cleanLine.size() - 2));
			if (data_.find(currentSection) == data_.end()) {
				sectionOrder_.push_back(currentSection);
			}
			continue;
		}

		// Key = Value 判定
		size_t eqPos = cleanLine.find('=');
		if (eqPos != std::string::npos) {
			std::string key = Trim(cleanLine.substr(0, eqPos));
			std::string value = Trim(cleanLine.substr(eqPos + 1));

			if (!key.empty()) {
				if (data_[currentSection].find(key) == data_[currentSection].end()) {
					if (data_.find(currentSection) == data_.end()) {
						sectionOrder_.push_back(currentSection);
					}
					keyOrder_[currentSection].push_back(key);
				}
				data_[currentSection][key] = value;
			}
		}
	}

	return true;
}

bool ConfigFile::SaveToFile(const std::string& filepath) const {
	std::ofstream file(filepath);
	if (!file.is_open()) {
		return false;
	}

	for (const auto& section : sectionOrder_) {
		file << "[" << section << "]\n";
		auto itKeyOrder = keyOrder_.find(section);
		if (itKeyOrder != keyOrder_.end()) {
			auto itSectionData = data_.find(section);
			if (itSectionData != data_.end()) {
				for (const auto& key : itKeyOrder->second) {
					auto itVal = itSectionData->second.find(key);
					if (itVal != itSectionData->second.end()) {
						file << key << " = " << itVal->second << "\n";
					}
				}
			}
		}
		file << "\n";
	}

	return true;
}

std::string ConfigFile::GetString(const std::string& section, const std::string& key, const std::string& defaultValue) const {
	auto itSec = data_.find(section);
	if (itSec != data_.end()) {
		auto itVal = itSec->second.find(key);
		if (itVal != itSec->second.end()) {
			return itVal->second;
		}
	}
	return defaultValue;
}

float ConfigFile::GetFloat(const std::string& section, const std::string& key, float defaultValue) const {
	std::string valStr = GetString(section, key);
	if (valStr.empty()) return defaultValue;
	try {
		return std::stof(valStr);
	}
	catch (...) {
		return defaultValue;
	}
}

int ConfigFile::GetInt(const std::string& section, const std::string& key, int defaultValue) const {
	std::string valStr = GetString(section, key);
	if (valStr.empty()) return defaultValue;
	try {
		return std::stoi(valStr);
	}
	catch (...) {
		return defaultValue;
	}
}

bool ConfigFile::GetBool(const std::string& section, const std::string& key, bool defaultValue) const {
	std::string valStr = GetString(section, key);
	if (valStr.empty()) return defaultValue;
	
	std::string lower = valStr;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
	
	if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") return true;
	if (lower == "false" || lower == "0" || lower == "no" || lower == "off") return false;
	return defaultValue;
}

void ConfigFile::SetString(const std::string& section, const std::string& key, const std::string& value) {
	if (data_.find(section) == data_.end()) {
		sectionOrder_.push_back(section);
	}
	if (data_[section].find(key) == data_[section].end()) {
		keyOrder_[section].push_back(key);
	}
	data_[section][key] = value;
}

void ConfigFile::SetFloat(const std::string& section, const std::string& key, float value) {
	std::ostringstream ss;
	ss << value;
	SetString(section, key, ss.str());
}

void ConfigFile::SetInt(const std::string& section, const std::string& key, int value) {
	SetString(section, key, std::to_string(value));
}

void ConfigFile::SetBool(const std::string& section, const std::string& key, bool value) {
	SetString(section, key, value ? "true" : "false");
}

bool ConfigFile::HasSection(const std::string& section) const {
	return data_.find(section) != data_.end();
}

bool ConfigFile::HasKey(const std::string& section, const std::string& key) const {
	auto it = data_.find(section);
	if (it != data_.end()) {
		return it->second.find(key) != it->second.end();
	}
	return false;
}

std::vector<std::string> ConfigFile::GetSections() const {
	return sectionOrder_;
}
