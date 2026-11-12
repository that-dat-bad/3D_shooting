#pragma once
#define DIRECTINPUT_VERSION     0x0800
#include <dinput.h>
#include "GameOverScene.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/System/TextureManager.h"
#include "../../engine/Graphics/PostProcess/PostEffect.h"
#include "../System/GameResult.h"
#include "../System/AsyncPreloadManager.h"
#include "../../engine/Graphics/UI/UITextRegistry.h"
#include "../../io/Input.h"
#include "WinApp.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace GameOverSceneInternal {
    static constexpr float kScreenWidth = static_cast<float>(WinApp::kClientWidth);
    static constexpr float kScreenHeight = static_cast<float>(WinApp::kClientHeight);
}
