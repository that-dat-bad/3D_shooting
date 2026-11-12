#include "Game.h"
#include <cassert>
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "PostProcess/PostEffect.h"
#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Graphics/Text/FontManager.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../../engine/Graphics/Model/ModelManager.h"
#include "../Scene/SceneFactory.h"

void Game::Initialize() {
	// 1. 基盤システムの初期化
	winApp = std::make_unique<WinApp>();
	winApp->Initialize(L"Dawn Engine (Natural - Skybox Viewer)", L"1.0");
	DirectXCommon::GetInstance()->Initialize(winApp.get());
	Input::GetInstance()->Initialize(GetModuleHandle(nullptr), winApp->GetHwnd());

	srvManager = std::make_unique<SrvManager>();
	srvManager->Initialize(DirectXCommon::GetInstance());

	// 2. マネージャ類の初期化
	SpriteCommon::GetInstance()->Initialize(DirectXCommon::GetInstance());
	TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), srvManager.get());
	ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), srvManager.get());
	CameraManager::GetInstance()->Initialize();
	ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), srvManager.get());
	GPUParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), srvManager.get());

	Object3dCommon::GetInstance()->Initialize(DirectXCommon::GetInstance());
	SkyboxCommon::GetInstance()->Initialize(DirectXCommon::GetInstance(), srvManager.get());
	PrimitiveModel::GetInstance()->Initialize(DirectXCommon::GetInstance());
	AudioManager::GetInstance()->Initialize();

	FontManager::GetInstance()->Initialize();
	TextRenderer::GetInstance()->Initialize(SpriteCommon::GetInstance());
	TextRenderer::GetInstance()->PrewarmPool(256);

	std::vector<FontLoadRequest> fontRequests = {
		{ "Roboto", "C:/Windows/Fonts/msgothic.ttc", 64.0f },
		{ "Consolas", "C:/Windows/Fonts/consolab.ttf", 48.0f }
	};
	FontManager::GetInstance()->LoadFontsParallel(fontRequests);

	imguiManager = std::make_unique<ImGuiManager>();
	imguiManager->Initialize(winApp.get(), DirectXCommon::GetInstance(), srvManager.get());

	// 3. ポストエフェクト初期化
	DirectXCommon::GetInstance()->SetupRenderTextureSRV(srvManager.get());
	DirectXCommon::GetInstance()->SetupDepthTextureSRV(srvManager.get());
	PostEffect::GetInstance()->Initialize(DirectXCommon::GetInstance(), srvManager.get());

	// 4. シーンマネージャの生成 (DebugScene / Skybox Viewer が初期シーンとして起動)
	sceneManager = std::make_unique<SceneManager>(std::make_unique<SceneFactory>());
}

void Game::Update() {
	// システムの更新
	Input::GetInstance()->Update();
	FontManager::GetInstance()->Update();
	imguiManager->Begin();

	sceneManager->Update();
	CameraManager::GetInstance()->Update();
	ParticleManager::GetInstance()->Update();
	AudioManager::GetInstance()->Update();

	// Escキーによる終了
	if (Input::GetInstance()->TriggerKey(DIK_ESCAPE)) {
		endRequest_ = true;
	}
}

void Game::Draw() {
	// 描画前処理
	DirectXCommon::GetInstance()->PreDraw();
	srvManager->PreDraw();
	PrimitiveModel::GetInstance()->Reset();

	// 3Dオブジェクト・Skybox描画（中間レンダーターゲットへ描画）
	Object3dCommon::GetInstance()->SetupCommonState();
	sceneManager->Draw();
	ParticleManager::GetInstance()->Draw();

	// 描画後処理（ポストエフェクト適用し、バックバッファへ転送）
	DirectXCommon::GetInstance()->PostDraw();

	// 2D UI・テキスト描画
	SpriteCommon::GetInstance()->SetupCommonState();
	sceneManager->DrawUI();
	TextRenderer::GetInstance()->Draw();

	imguiManager->End();

	// フレーム終了処理
	DirectXCommon::GetInstance()->EndFrame();
}

void Game::Run() {
	// メインループ
	while (true) {
		if (winApp->ProcessMessage()) {
			break;
		}

		Update();

		if (IsEndRequest()) {
			break;
		}

		Draw();
	}
}

void Game::Finalize() {
	// 1. GPUの完了を待機
	DirectXCommon::GetInstance()->FlushTextureUploads();
	DirectXCommon::GetInstance()->WaitForGPU();

	// 2. シーンを先に解放
	sceneManager.reset();

	// 3. ImGuiの終了処理と解放
	if (imguiManager) {
		imguiManager->Finalize();
		imguiManager.reset();
	}

	// 4. マネージャ・シングルトン類の終了処理
	AudioManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	CameraManager::GetInstance()->Finalize();
	ParticleManager::GetInstance()->Finalize();
	GPUParticleManager::GetInstance()->Finalize();

	TextRenderer::GetInstance()->Finalize();
	FontManager::GetInstance()->Finalize();

	PrimitiveModel::GetInstance()->Finalize();
	SkyboxCommon::GetInstance()->Finalize();
	Object3dCommon::GetInstance()->Finalize();
	SpriteCommon::GetInstance()->Finalize();
	PostEffect::GetInstance()->Finalize();

	// 5. SRVマネージャの終了処理
	srvManager->Finalize();
	srvManager.reset();

	// 6. 基盤システムの終了処理
	DirectXCommon::GetInstance()->Finalize();

	// 7. ウィンドウの終了処理
	if (winApp) {
		winApp->Finalize();
		winApp.reset();
	}
}
