#pragma once
#include "StageScene.h"
#include "../System/GameResult.h"
#include "../System/AsyncPreloadManager.h"
#include "../../engine/Graphics/Camera/CameraManager.h"
#include "../../engine/Graphics/Model/Object3dCommon.h"
#include "../../engine/Graphics/Model/SkyboxCommon.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "WinApp.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../../engine/Graphics/Particle/EffectManager.h"
#include "../../engine/Graphics/Particle/ParticleManager.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Physics/Collision3DManager.h"
#include "../Environment/TerrainSyncManager.h"
#include "../../engine/Graphics/UI/UITextRegistry.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include <cmath>
#include <algorithm>
#include <filesystem>

#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#endif

namespace StageSceneInternal {
    // 固定フレームレートの deltaTime (60FPS想定)
    static constexpr float kDeltaTime = 1.0f / 60.0f;
}
