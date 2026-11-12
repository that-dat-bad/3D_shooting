#pragma once
#ifdef USE_IMGUI
#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/imgui_impl_dx12.h"
#include "../../../external/imgui/imgui_impl_win32.h"
#endif // USE_IMGUI
#include "../Graphics/System/DirectXCommon.h"
#include "../base/WinApp.h"

class SrvManager; 

class ImGuiManager {
public:
	// 初期化
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);

	// 更新処理
	void Begin();

	// 描画処理
	void End();

	// 終了処理
	void Finalize();

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
};