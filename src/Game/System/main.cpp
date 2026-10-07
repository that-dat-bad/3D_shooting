#include <cassert>
#include <dxgi1_3.h>
#include <mfapi.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "dxguid.lib")
#include "../engine/Debug/dump.h"
#include "Game.h"
#include <dxgidebug.h>
#include <filesystem>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
	// 実行ファイルが存在するディレクトリを作業ディレクトリ（カレントディレクトリ）に固定
	wchar_t exePath[MAX_PATH];
	if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
		std::filesystem::current_path(std::filesystem::path(exePath).parent_path());
	}

	Dump::Install();

	HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	assert(SUCCEEDED(hr));

	{
		Game game;
		game.Initialize();

		game.Run();

		game.Finalize();
	}

	hr = MFShutdown();
	assert(SUCCEEDED(hr));

#ifdef _DEBUG
	// すべてのオブジェクトが破棄されているか確認
	IDXGIDebug* pDebug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug)))) {
		// アプリが直接保持しているリソースのみ表示 (内部参照のみのオブジェクトは除外)
		OutputDebugStringW(L"\n===== DXGI Live Objects Report =====\n");
		pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		pDebug->Release();
	}
#endif

	return 0;
}
