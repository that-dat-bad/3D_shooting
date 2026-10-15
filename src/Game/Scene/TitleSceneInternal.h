#pragma once
#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "TitleScene.h"
#include "../System/GameResult.h"
#include "../System/AsyncPreloadManager.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/UI/UITextRegistry.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/SkyboxCommon.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../engine/Graphics/System/BlendMode.h"
#include "../../engine/Graphics/System/DirectXCommon.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "WinApp.h"
#include "../../engine/io/Input.h"
#include <cmath>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <string>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

namespace TitleSceneInternal {
	static constexpr float kScreenWidth = static_cast<float>(WinApp::kClientWidth);   // 1280
	static constexpr float kScreenHeight = static_cast<float>(WinApp::kClientHeight); // 720
	static constexpr float kDegToRad = 3.14159265f / 180.0f;

	inline bool TitleAssetExists(const std::string& path) {
		static std::unordered_map<std::string, bool> s_assetExistsCache;
		auto it = s_assetExistsCache.find(path);
		if (it != s_assetExistsCache.end()) {
			return it->second;
		}
		bool exists = std::filesystem::exists(path) || std::filesystem::exists("assets/" + path);
		s_assetExistsCache[path] = exists;
		return exists;
	}
}
