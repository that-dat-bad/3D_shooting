#include "TitleSceneInternal.h"

using namespace TitleSceneInternal;

void TitleScene::InitializeEntryState() {
	TitleEntryReason entryReason = GameResult::GetInstance().titleEntryReason;
	if (entryReason == TitleEntryReason::RebootFromGameOver) {
		// 項目8: ゲームオーバーからの復帰時、起動コンソール画面に繋がり、再びOSが立ち上がってタイトルメニューに戻る
		state_ = TitleState::ConsoleBoot;
		consoleBootTimer_ = 0.0f;
		currentConsoleDuration_ = 0.85f; // 緊急リブート: 2.5倍速で切迫感のあるタイピング速度
		consoleExitTimer_ = 0.0f;
		loadingProgress_ = 0.0f;
		isLoadingComplete_ = false;
		completeHoldTimer_ = 0.0f;
		prewarmPhase_ = 0;
		isTitleCamerasPrewarmed_ = false;
		loadingPhaseText_ = "INITIALIZING OPTICAL CAMERAS...";
		isRebootLog_ = true;
		previousSortieLog_ = "PREVIOUS SORTIE: KERNEL PANIC // EMERGENCY SYSTEM RECOVERY ACTIVE";
		previousSortieColor_ = { 1.0f, 0.40f, 0.35f, 0.90f };
	} else if (entryReason == TitleEntryReason::ReturnFromClear) {
		// 項目10: クリアからの復帰時、セッション終了・再認証セキュリティ演出
		state_ = TitleState::ReAuthentication;
		reauthTimer_ = 0.0f;
		isRebootLog_ = false;
		currentConsoleDuration_ = 1.40f;
		previousSortieLog_ = "PREVIOUS SORTIE: MISSION ACCOMPLISHED // FLIGHT LOG ARCHIVED";
		previousSortieColor_ = { 0.35f, 1.0f, 0.65f, 0.90f };
	} else {
		state_ = TitleState::DroneView;
		isRebootLog_ = false;
		currentConsoleDuration_ = 1.40f;
		previousSortieLog_ = "";
	}
	// エントリ理由をリセット
	GameResult::GetInstance().titleEntryReason = TitleEntryReason::InitialBoot;

	stateTimer_ = 0.0f;
	cutTimer_ = 0.0f;
	cameraTheta_ = 0.0f;
}

void TitleScene::UpdateDroneViewState(float dt) {
	SetCursorVisible(false); // ドローン画面中はマウスカーソル非表示（項目7）

	// スペースキー、Enterキー、またはマウスクリック（左クリック）でOSウィンドウ最小化演出を経てコンソールへ（項目1）
	Input* input = Input::GetInstance();
	bool clickTriggered = input->TriggerMouse(0) || ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && !wasLButtonDown_);
	bool enterTriggered = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN)
		|| ((::GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) || ((::GetAsyncKeyState(VK_RETURN) & 0x8000) != 0);
	wasLButtonDown_ = ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) || input->PushMouse(0);

	if (enterTriggered || clickTriggered) {
		state_ = TitleState::WindowClose;
		windowCloseTimer_ = 0.0f;
		if (CurrentLocation().id == TitleLocation::CrashedForest) {
			ChangeLocation(0); // 通信途絶現場からメニュー移行時は基地メイン回線（格納庫）に接続復帰
		}
		// ローディング中にバックグラウンドで並行準備できるようプリロードを開始
		AsyncPreloadManager::GetInstance()->StartStagePreloadAsync();
	}

	// ポストエフェクト (ドローン風 + ブルーム発光)
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->ClearActiveEffects();

	LocationData& loc = CurrentLocation();

	// ブルーム（過剰発光・白飛びを抑制し、機体ディテールを鮮明に保つ：項目1）
	ActivePostEffect bloom;
	bloom.type = PostEffectType::kBloom;
	bloom.intensity = (loc.id == TitleLocation::Tunnel) ? 2.6f : 2.8f; // ブラー半径
	bloom.dirX = (loc.id == TitleLocation::Tunnel) ? 1.05f : 1.15f;    // ブルーム強度 (過剰な2.4fから適正な1.05fへ抑制)
	bloom.dirY = (loc.id == TitleLocation::Tunnel) ? 0.78f : 0.74f;    // 輝度閾値 (0.60fから0.78fに引き上げ、高輝度部のみ自然に光らせる)
	postEffect->AddActiveEffect(bloom);

	// 墜落現場専用：適度で落ち着いた砂嵐モニター演出（過剰な激しさを抑制）
	if (loc.id == TitleLocation::CrashedForest) {
		// 控えめなグリッチスパイク
		float glitchSpike = (rand() % 100 < 8) ? (0.08f + static_cast<float>(rand() % 100) / 100.0f * 0.12f) : 0.0f;

		// 1. 落ち着いた砂嵐ノイズ（0.65fから0.22fへ大幅抑制）
		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.22f + glitchSpike;
		postEffect->AddActiveEffect(noise);

		// 2. 繊細なCRT走査線（0.28fから0.12fへ抑制、ゆっくりスクロール）
		ActivePostEffect scanline;
		scanline.type = PostEffectType::kScanLine;
		scanline.intensity = 0.12f + glitchSpike * 0.2f;
		scanline.dirX = 520.0f; // 走査線の細かさ
		scanline.dirY = 6.0f;   // 穏やかなスクロール
		postEffect->AddActiveEffect(scanline);

		// 3. 控えめで上品な色収差（0.032fから0.012fへ抑制し、文字の可読性を確保）
		ActivePostEffect chromatic;
		chromatic.type = PostEffectType::kChromaticAberration;
		chromatic.intensity = 0.012f + glitchSpike * 0.015f;
		postEffect->AddActiveEffect(chromatic);

		// 4. 控えめなレンズ歪み（0.036fから0.010fへ抑制し、過剰な歪曲を解消）
		ActivePostEffect lens;
		lens.type = PostEffectType::kLensDistortion;
		lens.intensity = 0.010f;
		postEffect->AddActiveEffect(lens);
	} else {
		// 通常ロケーション（Tunnel など）
		// カメラ切替時の砂嵐ノイズトランジション演出（項目10）
		if (transitionTimer_ > 0.0f) {
			float t = transitionTimer_ / kTransitionDuration;
			ActivePostEffect noise;
			noise.type = PostEffectType::kRandom;
			noise.intensity = 0.35f * t;
			postEffect->AddActiveEffect(noise);

			ActivePostEffect glitchScan;
			glitchScan.type = PostEffectType::kScanLine;
			glitchScan.intensity = 0.18f * t;
			postEffect->AddActiveEffect(glitchScan);
		} else {
			// 通常時の繊細な走査線（項目9）
			ActivePostEffect scanline;
			scanline.type = PostEffectType::kScanLine;
			scanline.intensity = 0.045f;
			postEffect->AddActiveEffect(scanline);
		}

		// 控えめで上品な色収差（項目1, 9: 残像・過剰ボケを防止）
		ActivePostEffect chromatic;
		chromatic.type = PostEffectType::kChromaticAberration;
		chromatic.intensity = 0.006f;
		postEffect->AddActiveEffect(chromatic);

		// 控えめなレンズ歪み
		ActivePostEffect lens;
		lens.type = PostEffectType::kLensDistortion;
		lens.intensity = 0.008f;
		postEffect->AddActiveEffect(lens);
	}

	// ロケーション固有の色味（ビネット + カラーオーバーレイ）
	if (loc.grade.vignette > 0.0f || loc.grade.tintIntensity > 0.0f) {
		ActivePostEffect vignette;
		vignette.type = PostEffectType::kVignette;
		vignette.intensity = loc.grade.vignette;
		vignette.dirX = loc.grade.tintIntensity;
		vignette.colorR = loc.grade.tint.x;
		vignette.colorG = loc.grade.tint.y;
		vignette.colorB = loc.grade.tint.z;
		postEffect->AddActiveEffect(vignette);
	}

	if (pressSpaceBannerBg_) {
		pressSpaceBannerBg_->Update();
	}
	for (auto& sp : pressSpaceBorders_) {
		sp->Update();
	}
	pressSpaceText_.Update();
}

void TitleScene::UpdateWindowCloseState(float dt) {
	SetCursorVisible(false); // ウィンドウ縮小中もOSカーソル非表示
	windowCloseTimer_ += dt;

	Input* input = Input::GetInstance();
	bool clickTriggered = input->TriggerMouse(0) || ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && !wasLButtonDown_);
	wasLButtonDown_ = ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) || input->PushMouse(0);
	bool enterTriggered = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN)
		|| ((::GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) || ((::GetAsyncKeyState(VK_RETURN) & 0x8000) != 0);

	// クリックやSpaceで即時スキップ、または時間経過で即コンソールへ
	if (enterTriggered || clickTriggered || windowCloseTimer_ >= kWindowCloseDuration) {
		state_ = TitleState::ConsoleBoot;
		consoleBootTimer_ = 0.0f;
		consoleExitTimer_ = 0.0f;
		loadingProgress_ = 0.0f;
		isLoadingComplete_ = false;
		completeHoldTimer_ = 0.0f;
		prewarmPhase_ = 0;
		isTitleCamerasPrewarmed_ = false;
		loadingPhaseText_ = "INITIALIZING OPTICAL CAMERAS...";
	}

	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->ClearActiveEffects();

	float normT = std::clamp(windowCloseTimer_ / kWindowCloseDuration, 0.0f, 1.0f);
	ActivePostEffect scanline;
	scanline.type = PostEffectType::kScanLine;
	scanline.intensity = 0.10f + 0.15f * normT;
	postEffect->AddActiveEffect(scanline);
}

void TitleScene::UpdateConsoleBootState(float dt) {
	SetCursorVisible(false); // コマンド・ローディング演出中もマウスカーソル非表示
	consoleBootTimer_ += dt;

	// コマンドタイピング時間経過後にローディングバーを開始
	float loadTime = consoleBootTimer_ - kCommandTypingDuration;
	if (loadTime < 0.0f) {
		loadingProgress_ = 0.0f;
	} else {
		// タイトルの全カメラ・全3Dロケーションモデルを段階的に先行読み込み・ウォームアップ！
		if (!isTitleCamerasPrewarmed_) {
			PrewarmAllTitleCamerasAndLocations(dt);
		}

		// バックグラウンドでのプリロード完了判定
		bool isStageReady = AsyncPreloadManager::GetInstance()->IsStagePreloadReady();

		// フェーズ進行度に基づく最低進捗率 (0.0f, 0.25f, 0.50f, 0.75f, 1.0f)
		float phaseProgress = 0.25f * static_cast<float>(prewarmPhase_);
		float timeProgress = loadTime / 0.85f;
		float targetProgress = (std::min)(0.95f, (std::max)(phaseProgress * 0.95f, timeProgress));

		// 全カメラ・ロケーションのウォームアップ完了 & ステージプリロード完了 & 最低バー演出時間（0.70秒）経過で 100% (1.0f) へ
		if (isTitleCamerasPrewarmed_ && isStageReady && loadTime >= 0.70f) {
			targetProgress = 1.0f;
		}

		// 滑らかにプログレスを追従
		loadingProgress_ += (targetProgress - loadingProgress_) * (std::min)(1.0f, dt * 8.0f);
		if (targetProgress >= 1.0f && (1.0f - loadingProgress_) < 0.008f) {
			loadingProgress_ = 1.0f;
		}

		// 100% 達成で complete 状態に突入
		if (loadingProgress_ >= 1.0f && !isLoadingComplete_) {
			isLoadingComplete_ = true;
			completeHoldTimer_ = 0.0f;
		}

		if (isLoadingComplete_) {
			completeHoldTimer_ += dt;
			// complete 表示が少しキープされたらメニューへスムーズに遷移
			if (completeHoldTimer_ >= kCompleteHoldDuration) {
				consoleExitTimer_ += dt;
				if (consoleExitTimer_ >= kConsoleExitDuration) {
					state_ = TitleState::Menu;
					stateTimer_ = 0.0f;
					consoleExitTimer_ = 0.0f;
					menuEnterTimer_ = kMenuEnterDuration; // メニュー起動トランジション開始！
					SetCursorVisible(true);
					if (CurrentLocation().id == TitleLocation::CrashedForest) {
						ChangeLocation(0);
					}
					// メインカメラを TitleCamera に確実に同期
					CameraManager::GetInstance()->SetActiveCamera("TitleCamera");
					UpdateLocationCamera(CurrentLocation(), 0.0f, true);
				}
			}
		}
	}

	if (consoleBgSprite_) {
		consoleBgSprite_->Update();
	}

	// 背景を完全な黒（アルファ1.0）にして格納庫の透けを完全排除
	backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	backgroundPanel_.Update();

	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->ClearActiveEffects();

	// 切り替えフェードアウト時の短いCRT走査線グリッチ演出（項目3）
	if (consoleExitTimer_ > 0.0f) {
		float exitRatio = consoleExitTimer_ / kConsoleExitDuration;
		ActivePostEffect scanline;
		scanline.type = PostEffectType::kScanLine;
		scanline.intensity = 0.22f * exitRatio;
		postEffect->AddActiveEffect(scanline);

		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.15f * exitRatio;
		postEffect->AddActiveEffect(noise);
	}
}

void TitleScene::UpdateReAuthState(float dt) {
	// 項目10: セッション終了・再認証セキュリティ演出の更新
	SetCursorVisible(false);
	reauthTimer_ += dt;

	Input* input = Input::GetInstance();
	bool clickTriggered = input->TriggerMouse(0) || ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && !wasLButtonDown_);
	wasLButtonDown_ = ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) || input->PushMouse(0);
	bool skipTriggered = (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) || clickTriggered);

	if (skipTriggered || reauthTimer_ >= kReauthDuration) {
		state_ = TitleState::Menu;
		stateTimer_ = 0.0f;
		menuEnterTimer_ = kMenuEnterDuration;
		SetCursorVisible(true);
		if (CurrentLocation().id == TitleLocation::CrashedForest) {
			ChangeLocation(0);
		}
	}

	if (consoleBgSprite_) {
		consoleBgSprite_->Update();
	}
	backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	backgroundPanel_.Update();
}

void TitleScene::UpdateMenuState(float dt) {
	SetCursorVisible(false); // OS標準白カーソルを非表示にし、サイバーグリーンの専用タクティカルポインターを表示（項目5）
	if (menuEnterTimer_ > 0.0f) {
		menuEnterTimer_ -= dt;
	}

	Input* input = Input::GetInstance();
	HWND hwnd = input->GetHwnd();
	Input::MousePosition mousePos = input->GetMouseScreenPosition();
	float mx = static_cast<float>(mousePos.x);
	float my = static_cast<float>(mousePos.y);

	// クライアント領域スケーリング（DPIやリサイズによるゲーム解像度との不一致を完全解消）
	if (hwnd) {
		RECT rc;
		if (::GetClientRect(hwnd, &rc) && (rc.right - rc.left > 0) && (rc.bottom - rc.top > 0)) {
			float actualW = static_cast<float>(rc.right - rc.left);
			float actualH = static_cast<float>(rc.bottom - rc.top);
			mx *= (kScreenWidth / actualW);
			my *= (kScreenHeight / actualH);
		}
	}

	// マウスボタン押下判定（DirectInput ＋ Windows API の多重フォールバック）
	bool isLButtonDown = input->PushMouse(0) || ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
	bool isLButtonTrigger = input->TriggerMouse(0) || (isLButtonDown && !wasLButtonDown_);
	wasLButtonDown_ = isLButtonDown;

	// フルOSデスクトップ環境の更新（タスクバー、マルチウィンドウドラッグ・最小化・フォーカス、デスクトップアイコン、カスタムカーソル）
	UpdateOSDesktop(dt, { mx, my }, isLButtonDown, isLButtonTrigger);

	// メインターミナル（Window 0）がフォーカスされており、非最小化・表示中かつ非ドラッグ時のみメニュー操作を更新
	bool isTermActive = (activeWindowId_ == 0 && osWindows_.size() > 0 && osWindows_[0].isOpen && !osWindows_[0].isMinimized);
	if (isTermActive && !osWindows_[0].isDragging && osWindows_[0].animScale >= 0.85f) {
		selectionManager_.Update();

		// マウスクリックでのダイレクトボタン判定（万が一の判定漏れ防止用フォールバック）
		if (isLButtonTrigger) {
			const float btnX = osWindows_[0].pos.x + 65.0f;
			const float btnW = 390.0f;
			const float btnH = 46.0f;
			const float btnStartY = osWindows_[0].pos.y + 196.0f;
			const float btnSpacing = 56.0f;
			if (mx >= btnX && mx <= btnX + btnW) {
				if (my >= btnStartY && my <= btnStartY + btnH) {
					startButton_.Click();
				} else if (my >= btnStartY + btnSpacing && my <= btnStartY + btnSpacing + btnH) {
					editorButton_.Click();
				} else if (my >= btnStartY + btnSpacing * 2.0f && my <= btnStartY + btnSpacing * 2.0f + btnH) {
					settingsButton_.Click();
				} else if (my >= btnStartY + btnSpacing * 3.0f && my <= btnStartY + btnSpacing * 3.0f + btnH) {
					exitButton_.Click();
				}
			}
		}
	}

	// メニュー選択連動サブウィンドウの切替検知
	int curSelIdx = selectionManager_.GetSelectedIndex();
	if (curSelIdx != lastMenuSelIdx_) {
		lastMenuSelIdx_ = curSelIdx;
		subWindowSwitchTimer_ = kSubWindowSwitchDuration;
	}
	if (subWindowSwitchTimer_ > 0.0f) {
		subWindowSwitchTimer_ -= dt;
		if (subWindowSwitchTimer_ < 0.0f) {
			subWindowSwitchTimer_ = 0.0f;
		}
	}

	if (isEngineTesting_) {
		engineTestTimer_ -= dt;
		if (engineTestTimer_ <= 0.0f) {
			isEngineTesting_ = false;
			engineTestTimer_ = 0.0f;
		}
	}

	UpdateMenuConsoleVisuals();

	// 現在のロケーションをゆっくりOrbitする
	UpdateLocationCamera(CurrentLocation(), dt, true);

	// 背景オーバーレイ（背景の3D映像が美しく透ける半透明ダークガラス：圧倒的なOSデスクトップ感）
	float bgAlpha = (std::min)(stateTimer_ * 2.5f, 0.78f);
	backgroundPanel_.SetBackgroundColor({ 0.0f, 0.0f, 0.0f, bgAlpha });
	backgroundPanel_.Update();

	// メニュー用ポストエフェクト（背景被写界深度ブラー ＋ ブルーム ＋ 走査線 ＋ スタートメニュートランジション）
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->ClearActiveEffects();

	// 項目6: 背景（格納庫）の被写界深度（ブラー）- 手前UI視認性向上のため背景に軽いブラーを適用
	ActivePostEffect bgBlur;
	bgBlur.type = PostEffectType::kKawaseBlur;
	bgBlur.intensity = 1.8f;
	bgBlur.kernelSize = 1;
	postEffect->AddActiveEffect(bgBlur);

	// 高輝度部を光らせるブルーム（1パスで軽量＆鮮やか）
	ActivePostEffect bloom;
	bloom.type = PostEffectType::kBloom;
	bloom.intensity = 2.0f;
	bloom.dirX = 1.1f;
	bloom.dirY = 0.72f;
	postEffect->AddActiveEffect(bloom);

	// 繊細な走査線
	ActivePostEffect scanline;
	scanline.type = PostEffectType::kScanLine;
	scanline.intensity = 0.030f;
	postEffect->AddActiveEffect(scanline);

	// ビネット
	ActivePostEffect vignette;
	vignette.type = PostEffectType::kVignette;
	vignette.intensity = 0.65f;
	postEffect->AddActiveEffect(vignette);

	// 項目4: スタートメニュー操作時の暗転・ノイズクッション演出
	if (startMenuTransitionTimer_ > 0.0f) {
		float smRatio = startMenuTransitionTimer_ / kStartMenuTransitionDuration;
		ActivePostEffect smNoise;
		smNoise.type = PostEffectType::kRandom;
		smNoise.intensity = 0.22f * smRatio;
		postEffect->AddActiveEffect(smNoise);

		ActivePostEffect smScan;
		smScan.type = PostEffectType::kScanLine;
		smScan.intensity = 0.18f * smRatio;
		postEffect->AddActiveEffect(smScan);
	}

	menuCardPanel_.Update();
	menuHeaderPanel_.Update();
	if (titleLogoSprite_) {
		titleLogoSprite_->Update();
	}
	titleText_.Update();
	subtitleText_.Update();
}

void TitleScene::UpdateSortieState(float dt) {
	SetCursorVisible(false); // OSカーソル非表示を徹底（項目9）
	sortieTimer_ += dt;
	if (sortieTimer_ >= kSortieDuration) {
		sceneID = SCENE::STAGE; // 出撃演出完了後にステージへシームレス移行（項目1）
	}

	// 背景カメラをゆっくり進める
	UpdateLocationCamera(CurrentLocation(), dt, true);

	// 出撃シーケンスのポストエフェクト（項目3: ボタンクリック時の波紋グリッチ ＋ アフターバーナー加速感）
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->ClearActiveEffects();

	float normT = std::clamp(sortieTimer_ / kSortieDuration, 0.0f, 1.0f);

	if (sortieTimer_ < 0.35f) {
		// クリック直後の一瞬の強烈な波紋グリッチ（項目3）
		float shock = 1.0f - (sortieTimer_ / 0.35f);
		ActivePostEffect noise;
		noise.type = PostEffectType::kRandom;
		noise.intensity = 0.38f * shock;
		postEffect->AddActiveEffect(noise);

		ActivePostEffect ab;
		ab.type = PostEffectType::kChromaticAberration;
		ab.intensity = 0.025f * shock;
		postEffect->AddActiveEffect(ab);

		ActivePostEffect scanline;
		scanline.type = PostEffectType::kScanLine;
		scanline.intensity = 0.18f * shock;
		postEffect->AddActiveEffect(scanline);
	} else {
		// 黒フェードアウト進行に伴う減光処理
		float fadeProgress = 0.0f;
		if (sortieTimer_ > 1.65f) {
			fadeProgress = std::clamp((sortieTimer_ - 1.65f) / (kSortieDuration - 1.65f), 0.0f, 1.0f);
		}
		float fadeMult = 1.0f - fadeProgress * 0.75f;

		ActivePostEffect bloom;
		bloom.type = PostEffectType::kBloom;
		bloom.intensity = (1.8f + 1.8f * normT) * fadeMult;
		bloom.dirX = 1.3f;
		bloom.dirY = 0.70f;
		postEffect->AddActiveEffect(bloom);

		ActivePostEffect ab;
		ab.type = PostEffectType::kChromaticAberration;
		ab.intensity = (0.008f + 0.018f * normT) * fadeMult;
		postEffect->AddActiveEffect(ab);

		ActivePostEffect scanline;
		scanline.type = PostEffectType::kScanLine;
		scanline.intensity = 0.04f * fadeMult;
		postEffect->AddActiveEffect(scanline);
	}
}
