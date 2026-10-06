#include "Game.h"
#include <cassert>
#include "../../engine/Graphics/Model/PrimitiveModel.h"
#include "PostProcess/PostEffect.h"

#include "../../engine/Graphics/Particle/GPUParticleManager.h"
#include "../../engine/Graphics/Text/FontManager.h"
#include "../../engine/Graphics/Text/TextRenderer.h"
#include "../Scene/SceneFactory.h"



void Game::Initialize() {
	// 1. 基盤システムの初期化
	winApp = std::make_unique<WinApp>();
	winApp->Initialize(L"戦雷", L"1.0");
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
	FontManager::GetInstance()->LoadFont("Roboto", "C:/Windows/Fonts/msgothic.ttc", 96.0f);

	imguiManager = std::make_unique<ImGuiManager>();
	imguiManager->Initialize(winApp.get(), DirectXCommon::GetInstance(), srvManager.get());

	// 4. ポストエフェクト初期化
	DirectXCommon::GetInstance()->SetupRenderTextureSRV(srvManager.get());
	DirectXCommon::GetInstance()->SetupDepthTextureSRV(srvManager.get());
	PostEffect::GetInstance()->Initialize(DirectXCommon::GetInstance(), srvManager.get());

	// 5. シーンマネージャの生成 (全ての準備が整ってから)
	sceneManager = std::make_unique<SceneManager>(std::make_unique<SceneFactory>()); // 内部でシーンの Initialize が走る

	// 6. Win32 ネイティブ UIエディタ別ウィンドウの初期化
	UIEditorWindow::GetInstance()->Initialize(winApp->GetHwnd());
}

void Game::Update() {
	// システムの更新
	Input::GetInstance()->Update();
	imguiManager->Begin();

	sceneManager->Update();
	// カメラ更新
	CameraManager::GetInstance()->Update();



	ParticleManager::GetInstance()->Update();
	AudioManager::GetInstance()->Update();


#ifdef USE_IMGUI

	ImGui::Begin("Camera Controls");

	// アクティブなカメラを取得
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if (camera) {
		// 現在の値を取得
		Vector3 rotate = camera->GetRotate();
		Vector3 translate = camera->GetTranslate();

		// 第3引数は感度（ドラッグ時の変化量）です
		ImGui::DragFloat3("Rotate", &rotate.x, 0.01f);
		ImGui::DragFloat3("Translate", &translate.x, 0.1f);

		// 変更した値をカメラにセットし直す
		camera->SetRotate(rotate);
		camera->SetTranslate(translate);
	}
	ImGui::End();
#endif // USE_IMGUI

	// Win32 ネイティブ UIエディタの更新
	UIEditorWindow::GetInstance()->Update();

	// F1キーでエディタウィンドウの表示/非表示切り替え
	if (Input::GetInstance()->TriggerKey(DIK_F1)) {
		UIEditorWindow::GetInstance()->Toggle();
	}

	// 終了リクエストの例 (Escキーで終了など)
	if (Input::GetInstance()->TriggerKey(DIK_ESCAPE)) {
		endRequest_ = true;
	}
}

void Game::Draw() {
	// 描画前処理
	DirectXCommon::GetInstance()->PreDraw();
	srvManager->PreDraw();
	PrimitiveModel::GetInstance()->Reset(); // プリミティブの描画カウントをリセット
	// 3Dオブジェクト・パーティクル描画（中間レンダーターゲットへ描画）
	Object3dCommon::GetInstance()->SetupCommonState();
	sceneManager->Draw();
	ParticleManager::GetInstance()->Draw();

	// 描画後処理（3Dシーンにポストエフェクト適用し、バックバッファへ転送）
	DirectXCommon::GetInstance()->PostDraw();

	// 2D UI・テキスト描画（ポストエフェクト適用後のバックバッファに描画）
	// これによりUIが走査線・色収差・レンズ歪み・ブルームでぼやけたり見づらくならない
	SpriteCommon::GetInstance()->SetupCommonState();
	sceneManager->DrawUI();
	TextRenderer::GetInstance()->Draw();

	imguiManager->End();

	// フレーム終了処理（PRESENTに遷移、submit、present）
	DirectXCommon::GetInstance()->EndFrame();
}

void Game::Run() {
	// メインループ
	while (true) {
		if (winApp->ProcessMessage()) {
			break;
		}

		// ゲーム更新
		Update();

		// 終了リクエストが来たら抜ける
		if (IsEndRequest()) {
			break;
		}

		// 描画
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
	TextureManager::GetInstance()->Finalize();  // テクスチャクリア
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

	// 7. UIエディタウィンドウの終了
	UIEditorWindow::GetInstance()->Finalize();

	// 8. ウィンドウの終了処理
	if (winApp) {
		winApp->Finalize();
		winApp.reset();
	}
}
