#include "TitleSceneInternal.h"

using namespace TitleSceneInternal;

// =========================================================================
// フルOSデスクトップ環境の実装 (DAWN_OS Desktop Environment)
// =========================================================================

void TitleScene::InitializeOSDesktop(SpriteCommon* spriteCommon) {
	// スプライトプールの初期化（512枚：プール枯渇防止）
	osSpritePool_.clear();
	for (int i = 0; i < 512; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition({ 0.0f, 0.0f });
		sp->SetSize({ 1.0f, 1.0f });
		sp->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		sp->Update();
		osSpritePool_.push_back(std::move(sp));
	}
	osSpritePoolIdx_ = 0;

	// 専用カーソルスプライトの初期化（24個：プール枯渇と完全分離：項目5）
	cursorSprites_.clear();
	for (int i = 0; i < 24; ++i) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		sp->SetPosition({ 0.0f, 0.0f });
		sp->SetSize({ 1.0f, 1.0f });
		sp->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		sp->Update();
		cursorSprites_.push_back(std::move(sp));
	}

	// 疑似OS共通基盤の初期化（テーマ設定）
	pseudoOS_.Initialize(spriteCommon, PseudoOS::Theme::TerminalGreen);

	ResetOSDesktopWindows();
}

void TitleScene::ResetOSDesktopWindows() {
	osWindows_.clear();

	// Window 0: メインターミナル (DAWN_OS)
	OSWindow win0;
	win0.id = 0;
	win0.title = "TERMINAL : DAWN_OS [v4.12]";
	win0.pos = { 392.0f, 44.0f };
	win0.size = { 520.0f, 616.0f };
	win0.normalPos = win0.pos;
	win0.normalSize = win0.size;
	win0.titleBarHeight = 34.0f;
	win0.isOpen = true;
	win0.isMinimized = false;
	win0.animScale = 1.0f;
	osWindows_.push_back(win0);

	// Window 1: 監視カメラ (CAM-01: OPTICAL FEED)
	OSWindow win1;
	win1.id = 1;
	win1.title = "CAM-01 : OPTICAL LIVE FEED";
	win1.pos = { 18.0f, 44.0f };
	win1.size = { 356.0f, 260.0f };
	win1.normalPos = win1.pos;
	win1.normalSize = win1.size;
	win1.titleBarHeight = 26.0f;
	win1.isOpen = true;
	win1.isMinimized = false;
	win1.animScale = 1.0f;
	osWindows_.push_back(win1);

	// Window 2: 機体テレメトリ (SYS_DIAG: TELEMETRY)
	OSWindow win2;
	win2.id = 2;
	win2.title = "SYS_DIAG : AVIONICS TELEMETRY";
	win2.pos = { 18.0f, 320.0f };
	win2.size = { 356.0f, 344.0f };
	win2.normalPos = win2.pos;
	win2.normalSize = win2.size;
	win2.titleBarHeight = 26.0f;
	win2.isOpen = true;
	win2.isMinimized = false;
	win2.animScale = 1.0f;
	osWindows_.push_back(win2);

	activeWindowId_ = 0; // メインターミナルがデフォルトアクティブ
	windowZOrder_ = { 1, 2, 0 }; // メインターミナル（0）が最前面

	desktopIcons_.clear();
}

void TitleScene::BringWindowToFront(int windowId) {
	activeWindowId_ = windowId;
	auto it = std::find(windowZOrder_.begin(), windowZOrder_.end(), windowId);
	if (it != windowZOrder_.end()) {
		windowZOrder_.erase(it);
	}
	windowZOrder_.push_back(windowId);
}

void TitleScene::DrawOSRect(const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
	if (osSpritePoolIdx_ >= osSpritePool_.size()) { return; }
	auto& sp = osSpritePool_[osSpritePoolIdx_++];
	sp->SetPosition(pos);
	sp->SetSize(size);
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

void TitleScene::DrawOSBorder(const Vector2& pos, const Vector2& size, const MyMath::Vector4& color, float thick) {
	DrawOSRect(pos, { size.x, thick }, color);
	DrawOSRect({ pos.x, pos.y + size.y - thick }, { size.x, thick }, color);
	DrawOSRect(pos, { thick, size.y }, color);
	DrawOSRect({ pos.x + size.x - thick, pos.y }, { thick, size.y }, color);
}

void TitleScene::DrawOSPanel(const Vector2& pos, const Vector2& size, const MyMath::Vector4& bgCol, const MyMath::Vector4& borderCol, float thick) {
	DrawOSRect(pos, size, bgCol);
	DrawOSBorder(pos, size, borderCol, thick);
}

void TitleScene::UpdateOSDesktop(float dt, const Vector2& mousePos, bool isLDown, bool isLTrig) {
	customCursorPos_ = mousePos;
	if (cursorRippleTimer_ > 0.0f) {
		cursorRippleTimer_ -= dt;
	}
	if (isLTrig) {
		cursorRippleTimer_ = 0.30f;
		cursorRipplePos_ = mousePos;
	}

	float mx = mousePos.x;
	float my = mousePos.y;
	const float tbY = 688.0f;

	// --- ウィンドウの最小化・復元アニメーション補間（項目3: トランジション追加）---
	for (auto& win : osWindows_) {
		float targetScale = (win.isOpen && !win.isMinimized) ? 1.0f : 0.0f;
		if (win.animScale < targetScale) {
			win.animScale = (std::min)(targetScale, win.animScale + dt * 6.5f);
		} else if (win.animScale > targetScale) {
			win.animScale = (std::max)(targetScale, win.animScale - dt * 6.5f);
		}
	}

	// --- スタートメニュー選択時のクッション演出（項目4: ハードカット解消）---
	if (startMenuTransitionTimer_ > 0.0f) {
		startMenuTransitionTimer_ -= dt;
		if (startMenuTransitionTimer_ <= 0.0f) {
			if (pendingStartMenuAction_ == 1) {
				state_ = TitleState::SortieSequence;
				sortieTimer_ = 0.0f;
			} else if (pendingStartMenuAction_ == 2) {
				sceneID = SCENE::MISSION_EDITOR;
			} else if (pendingStartMenuAction_ == 3) {
				if (osWindows_.size() >= 3) {
					osWindows_[0].pos = osWindows_[0].normalPos;
					osWindows_[0].isOpen = true; osWindows_[0].isMinimized = false;
					osWindows_[1].pos = osWindows_[1].normalPos;
					osWindows_[1].isOpen = true; osWindows_[1].isMinimized = false;
					osWindows_[2].pos = osWindows_[2].normalPos;
					osWindows_[2].isOpen = true; osWindows_[2].isMinimized = false;
					activeWindowId_ = 0;
				}
			} else if (pendingStartMenuAction_ == 4) {
				state_ = TitleState::ConsoleBoot;
				consoleBootTimer_ = 0.0f;
				consoleExitTimer_ = 0.0f;
				loadingProgress_ = 0.0f;
				isLoadingComplete_ = false;
				completeHoldTimer_ = 0.0f;
				prewarmPhase_ = 0;
				isTitleCamerasPrewarmed_ = false;
				loadingPhaseText_ = "INITIALIZING OPTICAL CAMERAS...";
			} else if (pendingStartMenuAction_ == 5) {
				PostQuitMessage(0);
			}
			pendingStartMenuAction_ = -1;
		}
		return; // クッション演出中は以降の入力を遮断
	}

	// --- 1. タスクバー & スタートメニューの判定 ---
	// スタートボタン (X: 8〜114, Y: 690〜718)
	if (isLTrig && mx >= 8.0f && mx <= 114.0f && my >= 690.0f && my <= 718.0f) {
		isStartMenuOpen_ = !isStartMenuOpen_;
	}

	// スタートメニューが開いている時の項目選択
	if (isStartMenuOpen_) {
		const float smX = 8.0f;
		const float smY = tbY - 188.0f;
		const float smW = 250.0f;
		const float smH = 184.0f;

		if (isLTrig) {
			if (mx >= smX && mx <= smX + smW && my >= smY && my <= smY + smH) {
				float relY = my - smY;
				if (relY >= 32.0f && relY < 60.0f) {
					// [01] EXECUTE SORTIE -> クッション演出経由（項目4）
					startMenuTransitionTimer_ = kStartMenuTransitionDuration;
					pendingStartMenuAction_ = 1;
					isStartMenuOpen_ = false;
				} else if (relY >= 60.0f && relY < 88.0f) {
					// [02] MISSION EDITOR
					startMenuTransitionTimer_ = kStartMenuTransitionDuration;
					pendingStartMenuAction_ = 2;
					isStartMenuOpen_ = false;
				} else if (relY >= 88.0f && relY < 116.0f) {
					// [03] RESET WINDOW LAYOUT
					startMenuTransitionTimer_ = kStartMenuTransitionDuration;
					pendingStartMenuAction_ = 3;
					isStartMenuOpen_ = false;
				} else if (relY >= 116.0f && relY < 144.0f) {
					// [04] REBOOT TERMINAL
					startMenuTransitionTimer_ = kStartMenuTransitionDuration;
					pendingStartMenuAction_ = 4;
					isStartMenuOpen_ = false;
				} else if (relY >= 148.0f && relY < 178.0f) {
					// [05] SHUTDOWN
					PostQuitMessage(0);
				}
			} else if (my < tbY) {
				isStartMenuOpen_ = false;
			}
		}
	}

	// タスクバーアイテムクリック判定 (項目9: 最小化ウィンドウも常時表示＆クリックで即復元)
	if (isLTrig && my >= tbY && my <= kScreenHeight) {
		float tabX = 120.0f;
		for (auto& win : osWindows_) {
			float tabW = 142.0f;
			if (mx >= tabX && mx <= tabX + tabW) {
				if (!win.isOpen || win.isMinimized) {
					// 最小化・閉状態からの拡大復元アニメーション（項目3 & 9）
					win.isOpen = true;
					win.isMinimized = false;
					BringWindowToFront(win.id);
				} else if (activeWindowId_ == win.id) {
					// アクティブウィンドウならタスクバーへ縮小最小化
					win.isMinimized = true;
				} else {
					// フォーカス切替＆最前面化
					BringWindowToFront(win.id);
				}
				break;
			}
			tabX += tabW + 6.0f;
		}
	}

	// --- 2. ウィンドウドラッグ＆操作判定（項目1: 残像を一切出さず単一位置更新）---
	for (auto& win : osWindows_) {
		if (win.isDragging) {
			if (!isLDown) {
				win.isDragging = false;
			} else {
				win.pos = { mx - win.dragOffset.x, my - win.dragOffset.y };
				win.pos.x = std::clamp(win.pos.x, -win.size.x + 80.0f, kScreenWidth - 80.0f);
				win.pos.y = std::clamp(win.pos.y, 0.0f, tbY - win.titleBarHeight);
			}
		}
	}

	// 手前（最前面＝windowZOrder_ の末尾）から奥に向かって逆順に判定
	std::vector<int> testOrder;
	for (auto it = windowZOrder_.rbegin(); it != windowZOrder_.rend(); ++it) {
		testOrder.push_back(*it);
	}

	if (isLTrig && !isStartMenuOpen_ && my < tbY) {
		for (int winId : testOrder) {
			if (winId < 0 || winId >= static_cast<int>(osWindows_.size())) { continue; }
			auto& win = osWindows_[winId];
			if (!win.isOpen || win.isMinimized || win.animScale < 0.5f) { continue; }

			const float btnW = 20.0f;
			const float btnH = 18.0f;
			const float btnY = win.pos.y + (win.titleBarHeight - btnH) * 0.5f;
			const float closeX = win.pos.x + win.size.x - 26.0f;
			const float maxmX  = closeX - 22.0f;
			const float miniX  = maxmX - 22.0f;

			// 右上制御ボタン判定
			if (my >= btnY && my <= btnY + btnH) {
				if (mx >= closeX && mx <= closeX + btnW) {
					// [X] ボタン: タスクバーへ最小化トランジション（項目3）
					win.isMinimized = true;
					break;
				} else if (mx >= maxmX && mx <= maxmX + btnW) {
					// [口] ボタン: 定位置リセット
					win.pos = win.normalPos;
					BringWindowToFront(win.id);
					break;
				} else if (mx >= miniX && mx <= miniX + btnW) {
					// [_] ボタン: タスクバーへ最小化トランジション（項目3）
					win.isMinimized = true;
					break;
				}
			}

			// タイトルバードラッグ開始
			if (mx >= win.pos.x && mx <= win.pos.x + win.size.x &&
				my >= win.pos.y && my <= win.pos.y + win.titleBarHeight) {
				win.isDragging = true;
				win.dragOffset = { mx - win.pos.x, my - win.pos.y };
				BringWindowToFront(win.id);
				break;
			}

			// ウィンドウ内クリックで最前面化
			if (mx >= win.pos.x && mx <= win.pos.x + win.size.x &&
				my >= win.pos.y && my <= win.pos.y + win.size.y) {
				BringWindowToFront(win.id);

				if (win.id == 1) {
					// 左上サブウィンドウのボタン判定
					int selIdx = selectionManager_.GetSelectedIndex();
					const float btnRowY = win.pos.y + win.titleBarHeight + 176.0f;
					const float btnW = 68.0f;
					const float btnH = 22.0f;
					for (int b = 0; b < 3; ++b) {
						float bx = win.pos.x + 14.0f + b * 74.0f;
						if (mx >= bx && mx <= bx + btnW && my >= btnRowY && my <= btnRowY + btnH) {
							cursorRippleTimer_ = 0.30f;
							cursorRipplePos_ = { mx, my };
							if (selIdx == 0) {
								ChangeLocation(b);
							} else if (selIdx == 1) {
								activeSatLayer_ = b;
							} else if (selIdx == 2) {
								activeSysDetailTab_ = b;
							} else if (selIdx == 3) {
								if (b == 0) {
									state_ = TitleState::DroneView;
									stateTimer_ = 0.0f;
									SetCursorVisible(false);
								} else if (b == 1) {
									::PostQuitMessage(0);
								} else if (b == 2) {
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
							}
							break;
						}
					}
				} else if (win.id == 2) {
					// 左下サブウィンドウのボタン判定
					int selIdx = selectionManager_.GetSelectedIndex();
					const float actBtnY = win.pos.y + win.titleBarHeight + 276.0f;
					const float actBtnW = 104.0f;
					const float actBtnH = 24.0f;
					for (int b = 0; b < 3; ++b) {
						float bx = win.pos.x + 14.0f + b * 112.0f;
						if (mx >= bx && mx <= bx + actBtnW && my >= actBtnY && my <= actBtnY + actBtnH) {
							cursorRippleTimer_ = 0.30f;
							cursorRipplePos_ = { mx, my };
							if (selIdx == 0) {
								if (b == 0) {
									// [ENG TEST]: 推力テスト開始！
									isEngineTesting_ = true;
									engineTestTimer_ = 4.5f;
								} else if (b == 1) {
									// [DEFCON 1/2]: 警戒レベル切替
									defconLevel_ = (defconLevel_ == 2) ? 1 : 2;
								} else if (b == 2) {
									// [SORTIE >>]: 出撃シーケンス発火！
									startButton_.Click();
								}
							} else if (selIdx == 1) {
								if (b == 0) {
									activeIntelTarget_ = 0;
								} else if (b == 1) {
									activeIntelTarget_ = 1;
								} else if (b == 2) {
									// [EDIT MISS >>]: ミッションエディタ起動
									editorButton_.Click();
								}
							} else if (selIdx == 2) {
								if (b == 0) {
									// [MUTE BGM]
								} else if (b == 1) {
									// [RE-SCAN]
								} else if (b == 2) {
									// [CALIBRATE]
								}
							} else if (selIdx == 3) {
								if (b == 0) {
									// [CANCEL]
								} else if (b == 1) {
									// [SUSPEND]
								} else if (b == 2) {
									// [EXIT >>]: 終了
									exitButton_.Click();
								}
							}
							break;
						}
					}
				}
				break;
			}
		}
	}

	if (osWindows_.size() > 0) {
		menuWindowPos_ = osWindows_[0].pos;
		UpdateMenuWindowPosition(menuWindowPos_);
	}

	// メニュー選択に連動したサブウィンドウタイトルの動的更新
	if (osWindows_.size() > 2) {
		int selIdx = selectionManager_.GetSelectedIndex();
		if (selIdx == 0) {
			osWindows_[1].title = "CAM-01 : HANGAR LIVE FEED [OPTICAL]";
			osWindows_[2].title = "SYS_DIAG : AVIONICS TELEMETRY [MiG-21]";
		} else if (selIdx == 1) {
			osWindows_[1].title = "SAT_MAP : TACTICAL SATELLITE RECON";
			osWindows_[2].title = "INTEL : ENEMY FORCES ANALYSIS";
		} else if (selIdx == 2) {
			osWindows_[1].title = "SYS_CFG : HARDWARE & GRAPHICS BUS";
			osWindows_[2].title = "SYS_CFG : AUDIO & CONTROL MATRIX";
		} else {
			osWindows_[1].title = "PWR_MGMT : SYSTEM SHUTDOWN SEQ";
			osWindows_[2].title = "SECURITY : SESSION TERMINATION";
		}
	}
}

void TitleScene::DrawOSTaskbar() {
	TextRenderer* tr = TextRenderer::GetInstance();
	const float tbY = 688.0f;
	const float tbH = 32.0f;

	// 1. タスクバー背景 & 上部境界線
	DrawOSRect({ 0.0f, tbY }, { kScreenWidth, tbH }, { 0.01f, 0.04f, 0.02f, 0.98f });
	DrawOSRect({ 0.0f, tbY }, { kScreenWidth, 1.5f }, { 0.25f, 0.95f, 0.55f, 0.95f });

	// 2. スタートボタン [ ⯈ DAWN_OS ]
	bool smHover = (customCursorPos_.x >= 8.0f && customCursorPos_.x <= 114.0f && customCursorPos_.y >= 690.0f && customCursorPos_.y <= 718.0f);
	Vector4 startBg = (isStartMenuOpen_ || smHover) ? Vector4{ 0.10f, 0.40f, 0.20f, 0.92f } : Vector4{ 0.03f, 0.14f, 0.07f, 0.90f };
	Vector4 startLine = (isStartMenuOpen_ || smHover) ? Vector4{ 0.45f, 1.0f, 0.70f, 1.0f } : Vector4{ 0.25f, 0.85f, 0.50f, 0.85f };
	DrawOSPanel({ 8.0f, tbY + 3.0f }, { 104.0f, 26.0f }, startBg, startLine, 1.5f);
	tr->Print("HackGen", "[⯈ DAWN_OS]", 14.0f, tbY + 8.0f, 12.0f, { 0.40f, 1.0f, 0.65f, 0.98f });

	// 3. 全ウィンドウのタスクバータブ常時表示（項目9: 最小化中も明確に表示しクリックで復元）
	float tabX = 120.0f;
	for (auto& win : osWindows_) {
		float tabW = 142.0f;
		float tabH = 24.0f;
		float tabY = tbY + 4.0f;

		bool isAct = (activeWindowId_ == win.id && !win.isMinimized && win.isOpen);
		bool isMin = (win.isMinimized || !win.isOpen);
		bool isHov = (customCursorPos_.x >= tabX && customCursorPos_.x <= tabX + tabW && customCursorPos_.y >= tabY && customCursorPos_.y <= tabY + tabH);

		Vector4 tBg = isAct ? Vector4{ 0.08f, 0.35f, 0.18f, 0.95f }
		            : (isMin ? Vector4{ 0.04f, 0.10f, 0.06f, 0.85f }
		            : (isHov ? Vector4{ 0.05f, 0.22f, 0.11f, 0.85f } : Vector4{ 0.02f, 0.09f, 0.05f, 0.80f }));

		Vector4 tBorder = isAct ? Vector4{ 0.45f, 1.0f, 0.70f, 0.98f }
		                : (isMin ? Vector4{ 0.70f, 0.60f, 0.18f, 0.80f }
		                : (isHov ? Vector4{ 0.35f, 0.90f, 0.55f, 0.85f } : Vector4{ 0.20f, 0.60f, 0.32f, 0.70f }));

		DrawOSPanel({ tabX, tabY }, { tabW, tabH }, tBg, tBorder, isAct ? 1.5f : 1.0f);
		if (isAct) {
			DrawOSRect({ tabX, tabY }, { tabW, 2.0f }, { 0.75f, 1.0f, 0.88f, 1.0f }); // アクティブタブ上端ネオンライン
		}

		// ステータスLED（最小化中はアンバー点滅：項目9）
		bool amberBlink = (std::fmod(animTimer_, 0.6f) < 0.35f);
		Vector4 dotCol = isAct ? Vector4{ 0.35f, 1.0f, 0.60f, 1.0f }
		               : (isMin ? (amberBlink ? Vector4{ 1.0f, 0.80f, 0.20f, 1.0f } : Vector4{ 0.45f, 0.35f, 0.08f, 0.6f })
		                        : Vector4{ 0.25f, 0.65f, 0.35f, 0.8f });
		DrawOSRect({ tabX + 7.0f, tabY + 8.0f }, { 6.0f, 6.0f }, dotCol);

		std::string tabLabel;
		if (win.id == 0) {
			tabLabel = isMin ? "TERM [_MIN]" : (isAct ? "* TERM: DAWN" : "TERM: DAWN_OS");
		} else if (win.id == 1) {
			int selIdx = selectionManager_.GetSelectedIndex();
			if (isMin) {
				tabLabel = "SUB1 [_MIN]";
			} else {
				if (selIdx == 0) tabLabel = "CAM-01: OPT";
				else if (selIdx == 1) tabLabel = "SAT: RECON";
				else if (selIdx == 2) tabLabel = "SYS: HW BUS";
				else tabLabel = "PWR: SHUTDN";
			}
		} else if (win.id == 2) {
			int selIdx = selectionManager_.GetSelectedIndex();
			if (isMin) {
				tabLabel = "SUB2 [_MIN]";
			} else {
				if (selIdx == 0) tabLabel = "SYS: TELEM";
				else if (selIdx == 1) tabLabel = "INTEL: THREAT";
				else if (selIdx == 2) tabLabel = "SYS: AUDIO";
				else tabLabel = "SEC: LOGOUT";
			}
		}

		Vector4 textCol = isAct ? Vector4{ 0.90f, 1.0f, 0.95f, 1.0f }
		                : (isMin ? Vector4{ 0.95f, 0.85f, 0.35f, 0.90f }
		                : (isHov ? Vector4{ 0.70f, 1.0f, 0.80f, 0.95f } : Vector4{ 0.35f, 0.75f, 0.50f, 0.85f }));
		tr->Print("HackGen", tabLabel, tabX + 18.0f, tabY + 6.0f, 10.5f, textCol);

		tabX += tabW + 6.0f;
	}

	// 4. システムトレイ
	float cpuNoise = 14.0f + 5.0f * std::sin(animTimer_ * 1.5f) + 3.0f * std::sin(animTimer_ * 3.7f);
	float memNoise = 38.0f + 2.0f * std::sin(animTimer_ * 0.7f);
	char sysBuf[64];
	snprintf(sysBuf, sizeof(sysBuf), "CPU: %d%% | MEM: %d%%", static_cast<int>(cpuNoise), static_cast<int>(memNoise));
	tr->Print("HackGen", sysBuf, kScreenWidth - 290.0f, tbY + 9.0f, 11.5f, { 0.30f, 0.80f, 0.45f, 0.85f });

	bool netBlink = (std::fmod(animTimer_, 0.8f) < 0.65f);
	Vector4 netLedCol = netBlink ? Vector4{ 0.25f, 1.0f, 0.55f, 0.95f } : Vector4{ 0.10f, 0.35f, 0.20f, 0.40f };
	DrawOSRect({ kScreenWidth - 152.0f, tbY + 12.0f }, { 6.0f, 6.0f }, netLedCol);
	tr->Print("HackGen", "NET: 10G", kScreenWidth - 140.0f, tbY + 9.0f, 11.0f, { 0.35f, 0.90f, 0.55f, 0.85f });

	char timeBuf[32];
	int totalSec = static_cast<int>(animTimer_);
	int hrs = (totalSec / 3600) % 24;
	int mins = (totalSec / 60) % 60;
	int secs = totalSec % 60;
	snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d UTC", (16 + hrs) % 24, mins, secs);
	tr->Print("HackGen", timeBuf, kScreenWidth - 82.0f, tbY + 9.0f, 11.5f, { 0.40f, 1.0f, 0.65f, 0.95f });

	// 5. スタートメニューポップアップ
	if (isStartMenuOpen_) {
		const float smX = 8.0f;
		const float smY = tbY - 188.0f;
		const float smW = 250.0f;
		const float smH = 184.0f;

		DrawOSPanel({ smX, smY }, { smW, smH }, { 0.01f, 0.04f, 0.02f, 0.98f }, { 0.35f, 1.0f, 0.60f, 0.95f }, 1.5f);
		DrawOSRect({ smX, smY }, { smW, 26.0f }, { 0.03f, 0.16f, 0.08f, 0.98f });
		tr->Print("HackGen", "DAWN_OS : ROOT SESSION", smX + 10.0f, smY + 6.0f, 11.5f, { 0.40f, 1.0f, 0.70f, 0.95f });

		const char* menuItems[] = {
			"[01] EXECUTE COMBAT SORTIE",
			"[02] LAUNCH MISSION EDITOR",
			"[03] RESET WINDOW LAYOUT",
			"[04] REBOOT TERMINAL",
			"[05] SHUTDOWN SYSTEM"
		};

		float itemY = smY + 34.0f;
		for (int i = 0; i < 5; ++i) {
			if (i == 4) {
				DrawOSRect({ smX + 8.0f, itemY - 2.0f }, { smW - 16.0f, 1.0f }, { 0.18f, 0.55f, 0.30f, 0.50f });
				itemY += 6.0f;
			}
			bool itemHov = (customCursorPos_.x >= smX && customCursorPos_.x <= smX + smW &&
			                customCursorPos_.y >= itemY - 4.0f && customCursorPos_.y < itemY + 22.0f);
			if (itemHov) {
				DrawOSRect({ smX + 4.0f, itemY - 2.0f }, { smW - 8.0f, 22.0f }, { 0.08f, 0.30f, 0.15f, 0.85f });
			}
			Vector4 itemCol = itemHov ? Vector4{ 1.0f, 0.95f, 0.30f, 1.0f }
			                : (i == 4 ? Vector4{ 0.90f, 0.40f, 0.35f, 0.90f } : Vector4{ 0.35f, 0.90f, 0.55f, 0.90f });
			tr->Print("HackGen", menuItems[i], smX + 12.0f, itemY + 2.0f, 11.5f, itemCol);
			itemY += 26.0f;
		}
	}
}

void TitleScene::DrawOSWindowCam(const OSWindow& win) {
	if (win.animScale <= 0.01f) { return; } // 完全最小化中は描画スキップ
	TextRenderer* tr = TextRenderer::GetInstance();

	// 最小化・展開トランジション座標計算（項目3）
	float s = win.animScale;
	float smoothS = s * s * (3.0f - 2.0f * s);
	const float tabTargetX = 120.0f + 148.0f + 70.0f;
	const float tabTargetY = 688.0f + 16.0f;
	auto lerpVal = [](float a, float b, float t) { return a + (b - a) * t; };
	Vector2 curPos = {
		lerpVal(tabTargetX - (win.size.x * 0.5f) * smoothS, win.pos.x, smoothS),
		lerpVal(tabTargetY - (win.size.y * 0.5f) * smoothS, win.pos.y, smoothS)
	};
	Vector2 curSize = { win.size.x * smoothS, win.size.y * smoothS };

	bool isActive = (activeWindowId_ == win.id);
	Vector4 borderCol = isActive ? Vector4{ 0.45f, 1.0f, 0.70f, 0.98f * smoothS } : Vector4{ 0.10f, 0.30f, 0.15f, 0.40f * smoothS };
	Vector4 titleBarBg = isActive ? Vector4{ 0.06f, 0.25f, 0.12f, 0.98f } : Vector4{ 0.015f, 0.05f, 0.025f, 0.85f };

	// 連動切り替え演出（切り替え直後のサイバー発光）
	float switchRatio = (kSubWindowSwitchDuration > 0.0f) ? (subWindowSwitchTimer_ / kSubWindowSwitchDuration) : 0.0f;
	if (switchRatio > 0.0f) {
		borderCol.x = (std::min)(1.0f, borderCol.x + 0.50f * switchRatio);
		borderCol.y = (std::min)(1.0f, borderCol.y + 0.30f * switchRatio);
		borderCol.z = (std::min)(1.0f, borderCol.z + 0.45f * switchRatio);
	}

	// 1. ウィンドウ本体パネル（背後ウィンドウのテキスト・スプライトを100%遮蔽する完全不透明プレート）
	DrawOSPanel(curPos, curSize, { 0.01f, 0.04f, 0.02f, 1.0f }, borderCol, isActive ? 2.0f : 1.0f);

	if (smoothS < 0.40f) { return; } // アニメーション極小時は枠のみ描画

	// 2. タイトルバー
	DrawOSRect(curPos, { curSize.x, win.titleBarHeight * smoothS }, titleBarBg);
	DrawOSRect({ curPos.x, curPos.y + win.titleBarHeight * smoothS - 1.0f }, { curSize.x, 1.0f }, borderCol);
	if (isActive) {
		DrawOSRect(curPos, { curSize.x, 2.0f }, { 0.70f, 1.0f, 0.85f, 1.0f }); // 上端ネオンアクセントライン
		DrawOSRect({ curPos.x + 8.0f, curPos.y + 9.0f }, { 6.0f, 6.0f }, { 0.40f, 1.0f, 0.70f, 1.0f }); // 高輝度LED
		tr->Print("HackGen", "[FOCUS] " + win.title, curPos.x + 18.0f, curPos.y + 6.0f, 11.5f * smoothS, { 0.85f, 1.0f, 0.92f, 1.0f });
	} else {
		DrawOSRect({ curPos.x + 8.0f, curPos.y + 9.0f }, { 6.0f, 6.0f }, { 0.12f, 0.30f, 0.16f, 0.45f }); // 消灯LED
		tr->Print("HackGen", "[IDLE]  " + win.title, curPos.x + 18.0f, curPos.y + 6.0f, 11.5f * smoothS, { 0.20f, 0.48f, 0.28f, 0.60f });
	}

	const float btnY = curPos.y + 5.0f;
	const float closeX = curPos.x + curSize.x - 24.0f;
	const float maxmX  = closeX - 22.0f;
	const float miniX  = maxmX - 22.0f;

	tr->Print("HackGen", "[_]", miniX, btnY + 1.0f, 10.5f, { 0.40f, 0.80f, 0.55f, 0.85f });
	tr->Print("HackGen", "[口]", maxmX, btnY + 1.0f, 10.5f, { 0.40f, 0.80f, 0.55f, 0.85f });
	tr->Print("HackGen", "[X]", closeX, btnY + 1.0f, 10.5f, { 0.95f, 0.35f, 0.30f, 0.95f });

	// ボタンホバー判定ラムダ
	auto isBtnHov = [&](float bx, float by, float bw, float bh) {
		return (customCursorPos_.x >= bx && customCursorPos_.x <= bx + bw &&
		        customCursorPos_.y >= by && customCursorPos_.y <= by + bh);
	};

	// 3. ウィンドウ内コンテンツ（中央メニュー連動 ＋ 実際のインタラクティブ操作）
	int selIdx = selectionManager_.GetSelectedIndex();
	float cy = curPos.y + win.titleBarHeight + 10.0f;

	if (selIdx == 0) {
		// ==========================================
		// [01] EXECUTE SORTIE: ハンガーのライブ映像
		// ==========================================
		bool recBlink = (std::fmod(animTimer_, 1.0f) < 0.65f);
		Vector4 recCol = recBlink ? Vector4{ 1.0f, 0.25f, 0.20f, 0.98f } : Vector4{ 0.40f, 0.08f, 0.08f, 0.35f };
		DrawOSRect({ curPos.x + 14.0f, cy + 3.0f }, { 8.0f, 8.0f }, recCol);
		tr->Print("HackGen", "REC [LIVE STREAM]", curPos.x + 28.0f, cy, 11.5f, { 0.95f, 0.95f, 0.95f, 0.95f });
		tr->Print("HackGen", "1080p 60FPS", curPos.x + curSize.x - 90.0f, cy, 11.0f, { 0.35f, 0.85f, 0.55f, 0.85f });
		cy += 24.0f;

		// プレビュー枠
		DrawOSPanel({ curPos.x + 14.0f, cy }, { curSize.x - 28.0f, 112.0f }, { 0.005f, 0.02f, 0.01f, 0.85f }, { 0.15f, 0.50f, 0.28f, 0.60f }, 1.0f);

		float cx = curPos.x + curSize.x * 0.5f;
		float midY = cy + 56.0f;
		DrawOSRect({ cx - 8.0f, midY }, { 16.0f, 1.0f }, { 0.25f, 0.85f, 0.45f, 0.70f });
		DrawOSRect({ cx, midY - 8.0f }, { 1.0f, 16.0f }, { 0.25f, 0.85f, 0.45f, 0.70f });

		const char* locNames[] = { "LOC: HANGAR_BAY_01", "LOC: SUB_TUNNEL_04", "LOC: SECTOR_07_CRASH" };
		int curIdx = std::clamp(currentLocationIndex_, 0, 2);
		tr->Print("HackGen", locNames[curIdx], cx, midY - 26.0f, 12.0f, { 0.45f, 1.0f, 0.70f, 0.95f }, { 0.5f, 0.5f });
		tr->Print("HackGen", "AZ: 042° | EL: +12° | OPTICAL LINK: OK", cx, midY + 18.0f, 10.5f, { 0.30f, 0.75f, 0.45f, 0.80f }, { 0.5f, 0.5f });
		cy += 124.0f;

		// チャンネル切替ボタン（クリックで実際にカメラ位置が切替）
		tr->Print("HackGen", "CAMERA CHANNEL (CLICK TO SWITCH):", curPos.x + 16.0f, cy + 2.0f, 10.5f, { 0.35f, 0.75f, 0.50f, 0.85f });
		cy += 18.0f;

		const char* chLabels[] = { "[CH 01]", "[CH 02]", "[CH 03]" };
		for (int c = 0; c < 3; ++c) {
			float bx = curPos.x + 14.0f + c * 74.0f;
			bool isCur = (currentLocationIndex_ == c);
			bool isHov = isBtnHov(bx, cy, 68.0f, 22.0f);
			Vector4 bBg = isCur ? Vector4{ 0.12f, 0.45f, 0.22f, 0.90f }
			            : (isHov ? Vector4{ 0.06f, 0.25f, 0.12f, 0.85f } : Vector4{ 0.02f, 0.10f, 0.05f, 0.70f });
			Vector4 bBorder = isCur ? Vector4{ 0.55f, 1.0f, 0.75f, 1.0f }
			                : (isHov ? Vector4{ 0.40f, 0.90f, 0.60f, 0.90f } : Vector4{ 0.18f, 0.50f, 0.28f, 0.65f });
			DrawOSPanel({ bx, cy }, { 68.0f, 22.0f }, bBg, bBorder, 1.0f);
			Vector4 textCol = isCur ? Vector4{ 1.0f, 0.95f, 0.30f, 1.0f }
			                : (isHov ? Vector4{ 0.85f, 1.0f, 0.90f, 1.0f } : Vector4{ 0.35f, 0.85f, 0.55f, 0.85f });
			tr->Print("HackGen", chLabels[c], bx + 34.0f, cy + 4.0f, 11.0f, textCol, { 0.5f, 0.0f });
		}
	} else if (selIdx == 1) {
		// ==========================================
		// [02] MISSION EDITOR: 作戦エリアの衛星マップ
		// ==========================================
		bool satBlink = (std::fmod(animTimer_, 0.8f) < 0.50f);
		Vector4 satCol = satBlink ? Vector4{ 0.20f, 0.85f, 1.0f, 0.98f } : Vector4{ 0.05f, 0.25f, 0.45f, 0.40f };
		DrawOSRect({ curPos.x + 14.0f, cy + 3.0f }, { 8.0f, 8.0f }, satCol);
		tr->Print("HackGen", "SAT_LINK [KEYHOLE-12]", curPos.x + 28.0f, cy, 11.5f, { 0.40f, 0.90f, 1.0f, 0.95f });
		tr->Print("HackGen", "RES: 0.25m / MULTISPEC", curPos.x + curSize.x - 146.0f, cy, 10.5f, { 0.30f, 0.75f, 0.85f, 0.85f });
		cy += 24.0f;

		// 衛星マップ描画枠
		const float mapW = curSize.x - 28.0f;
		const float mapH = 112.0f;
		float mapStartX = curPos.x + 14.0f;
		float mapStartY = cy;
		DrawOSPanel({ mapStartX, mapStartY }, { mapW, mapH }, { 0.008f, 0.025f, 0.035f, 0.90f }, { 0.15f, 0.50f, 0.70f, 0.75f }, 1.0f);

		// マップ内の格子グリッド線
		for (int gx = 1; gx < 4; ++gx) {
			DrawOSRect({ mapStartX + mapW * (gx * 0.25f), mapStartY }, { 1.0f, mapH }, { 0.12f, 0.35f, 0.50f, 0.35f });
		}
		for (int gy = 1; gy < 3; ++gy) {
			DrawOSRect({ mapStartX, mapStartY + mapH * (gy * 0.333f) }, { mapW, 1.0f }, { 0.12f, 0.35f, 0.50f, 0.35f });
		}

		if (activeSatLayer_ == 0) {
			// TERRAIN レイヤー
			tr->Print("HackGen", "[LAYER 0: TOPOGRAPHY / ELEVATION]", mapStartX + mapW * 0.5f, mapStartY + 28.0f, 11.0f, { 0.35f, 0.85f, 0.95f, 0.90f }, { 0.5f, 0.5f });
			tr->Print("HackGen", "ELEV: 240m - 1200m | SLOPE: 12.4 DEG", mapStartX + mapW * 0.5f, mapStartY + 50.0f, 10.5f, { 0.25f, 0.75f, 0.80f, 0.85f }, { 0.5f, 0.5f });
			tr->Print("HackGen", "NO OBSTACLE IN GLIDESLOPE PATH", mapStartX + mapW * 0.5f, mapStartY + 72.0f, 10.5f, { 0.40f, 1.0f, 0.70f, 0.90f }, { 0.5f, 0.5f });
		} else if (activeSatLayer_ == 1) {
			// RADAR レイヤー
			float sweepAngle = animTimer_ * 2.2f;
			float scx = mapStartX + mapW * 0.5f;
			float scy = mapStartY + mapH * 0.5f;
			float sLen = 42.0f;
			float endX = scx + sLen * std::cos(sweepAngle);
			float endY = scy + sLen * std::sin(sweepAngle);
			DrawOSRect({ scx, scy }, { 2.0f, 2.0f }, { 0.40f, 1.0f, 0.80f, 1.0f });
			DrawOSRect({ (scx + endX) * 0.5f - 15.0f, (scy + endY) * 0.5f }, { 30.0f, 1.0f }, { 0.20f, 0.90f, 1.0f, 0.75f });
			tr->Print("HackGen", "[LAYER 1: RADAR APERTURE SCAN]", mapStartX + mapW * 0.5f, mapStartY + 24.0f, 11.0f, { 0.35f, 1.0f, 0.70f, 0.90f }, { 0.5f, 0.5f });
			tr->Print("HackGen", "TARGET BOGEY ECHO DETECTED (x4)", mapStartX + mapW * 0.5f, mapStartY + 84.0f, 10.5f, { 1.0f, 0.85f, 0.25f, 0.95f }, { 0.5f, 0.5f });
		} else {
			// THREAT レイヤー
			float cx = mapStartX + mapW * 0.5f;
			float cyM = mapStartY + mapH * 0.5f;
			DrawOSBorder({ cx - 44.0f, cyM - 34.0f }, { 88.0f, 68.0f }, { 1.0f, 0.25f, 0.20f, 0.80f }, 1.2f);
			tr->Print("HackGen", "[LAYER 2: SAM ENGAGEMENT ENVELOPE]", mapStartX + mapW * 0.5f, mapStartY + 24.0f, 11.0f, { 1.0f, 0.40f, 0.35f, 0.95f }, { 0.5f, 0.5f });
			tr->Print("HackGen", "TOR-M1 COVERAGE: 12KM RADIUS", mapStartX + mapW * 0.5f, mapStartY + 84.0f, 10.5f, { 1.0f, 0.30f, 0.25f, 0.95f }, { 0.5f, 0.5f });
		}
		cy += 124.0f;

		tr->Print("HackGen", "SAT OVERLAY LAYER (CLICK TO SWITCH):", curPos.x + 16.0f, cy + 2.0f, 10.5f, { 0.30f, 0.80f, 0.90f, 0.85f });
		cy += 18.0f;

		const char* satLabels[] = { "[TERRAIN]", "[RADAR]", "[THREAT]" };
		for (int c = 0; c < 3; ++c) {
			float bx = curPos.x + 14.0f + c * 74.0f;
			bool isCur = (activeSatLayer_ == c);
			bool isHov = isBtnHov(bx, cy, 68.0f, 22.0f);
			Vector4 bBg = isCur ? Vector4{ 0.08f, 0.30f, 0.40f, 0.90f }
			            : (isHov ? Vector4{ 0.04f, 0.18f, 0.25f, 0.85f } : Vector4{ 0.02f, 0.08f, 0.12f, 0.70f });
			Vector4 bBorder = isCur ? Vector4{ 0.40f, 0.95f, 1.0f, 1.0f }
			                : (isHov ? Vector4{ 0.25f, 0.75f, 0.90f, 0.90f } : Vector4{ 0.15f, 0.45f, 0.60f, 0.65f });
			DrawOSPanel({ bx, cy }, { 68.0f, 22.0f }, bBg, bBorder, 1.0f);
			Vector4 textCol = isCur ? Vector4{ 0.90f, 1.0f, 1.0f, 1.0f }
			                : (isHov ? Vector4{ 0.75f, 0.95f, 1.0f, 1.0f } : Vector4{ 0.35f, 0.80f, 0.90f, 0.85f });
			tr->Print("HackGen", satLabels[c], bx + 34.0f, cy + 4.0f, 11.0f, textCol, { 0.5f, 0.0f });
		}
	} else if (selIdx == 2) {
		// ==========================================
		// [03] SYSTEM CONFIG: ハードウェアバス診断
		// ==========================================
		tr->Print("HackGen", "HARDWARE : D3D12 AGILITY SDK 1.614", curPos.x + 14.0f, cy, 11.5f, { 0.35f, 0.95f, 0.60f, 0.95f });
		cy += 20.0f;
		tr->Print("HackGen", "RENDERER : NVIDIA GEFORCE RTX (FL 12_1)", curPos.x + 14.0f, cy, 11.0f, { 0.30f, 0.75f, 0.45f, 0.85f });
		cy += 22.0f;

		// 診断詳細パネル
		const float hwW = curSize.x - 28.0f;
		const float hwH = 96.0f;
		DrawOSPanel({ curPos.x + 14.0f, cy }, { hwW, hwH }, { 0.01f, 0.035f, 0.02f, 0.85f }, { 0.20f, 0.65f, 0.35f, 0.65f }, 1.0f);

		if (activeSysDetailTab_ == 0) {
			tr->Print("HackGen", "COMMAND QUEUE  : DIRECT (1) / COPY (1)", curPos.x + 22.0f, cy + 10.0f, 10.5f, { 0.35f, 0.90f, 0.55f, 0.90f });
			tr->Print("HackGen", "SWAPCHAIN      : DXGI_FORMAT_R8G8B8A8", curPos.x + 22.0f, cy + 28.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "BUFFER COUNT   : 2 (DOUBLE BUFFERED)", curPos.x + 22.0f, cy + 46.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "VSYNC REFRESH  : 60 HZ LOCKED", curPos.x + 22.0f, cy + 64.0f, 10.5f, { 0.40f, 1.0f, 0.65f, 0.95f });
		} else if (activeSysDetailTab_ == 1) {
			tr->Print("HackGen", "CBV_SRV_UAV    : 142 / 2048 DESCRIPTORS", curPos.x + 22.0f, cy + 10.0f, 10.5f, { 0.35f, 0.90f, 0.55f, 0.90f });
			tr->Print("HackGen", "RTV DESCRIPTORS: 3 / 64 ALLOCATED", curPos.x + 22.0f, cy + 28.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "DSV DESCRIPTORS: 1 / 16 ALLOCATED", curPos.x + 22.0f, cy + 46.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "VRAM DEDICATED : 2.4 GB / 8.0 GB IN USE", curPos.x + 22.0f, cy + 64.0f, 10.5f, { 0.40f, 1.0f, 0.65f, 0.95f });
		} else {
			tr->Print("HackGen", "SHADER MODEL   : SM 6.6 DXIL BYTECODE", curPos.x + 22.0f, cy + 10.0f, 10.5f, { 0.35f, 0.90f, 0.55f, 0.90f });
			tr->Print("HackGen", "ROOT SIGNATURE : ROOT 1.1 COMPACT", curPos.x + 22.0f, cy + 28.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "PIPELINE STATE : PSO CACHED (42 SHADERS)", curPos.x + 22.0f, cy + 46.0f, 10.5f, { 0.30f, 0.80f, 0.50f, 0.85f });
			tr->Print("HackGen", "GEOMETRY SHADER: ENABLED & COMPILED", curPos.x + 22.0f, cy + 64.0f, 10.5f, { 0.40f, 1.0f, 0.65f, 0.95f });
		}
		cy += 108.0f;

		tr->Print("HackGen", "DIAGNOSTIC TAB (CLICK TO SWITCH):", curPos.x + 16.0f, cy + 2.0f, 10.5f, { 0.35f, 0.75f, 0.50f, 0.85f });
		cy += 18.0f;

		const char* tabLabels[] = { "[D3D12]", "[HEAP]", "[SHADER]" };
		for (int t = 0; t < 3; ++t) {
			float bx = curPos.x + 14.0f + t * 74.0f;
			bool isCur = (activeSysDetailTab_ == t);
			bool isHov = isBtnHov(bx, cy, 68.0f, 22.0f);
			Vector4 bBg = isCur ? Vector4{ 0.12f, 0.45f, 0.22f, 0.90f }
			            : (isHov ? Vector4{ 0.06f, 0.25f, 0.12f, 0.85f } : Vector4{ 0.02f, 0.10f, 0.05f, 0.70f });
			Vector4 bBorder = isCur ? Vector4{ 0.55f, 1.0f, 0.75f, 1.0f }
			                : (isHov ? Vector4{ 0.40f, 0.90f, 0.60f, 0.90f } : Vector4{ 0.18f, 0.50f, 0.28f, 0.65f });
			DrawOSPanel({ bx, cy }, { 68.0f, 22.0f }, bBg, bBorder, 1.0f);
			Vector4 textCol = isCur ? Vector4{ 1.0f, 0.95f, 0.30f, 1.0f }
			                : (isHov ? Vector4{ 0.85f, 1.0f, 0.90f, 1.0f } : Vector4{ 0.35f, 0.85f, 0.55f, 0.85f });
			tr->Print("HackGen", tabLabels[t], bx + 34.0f, cy + 4.0f, 11.0f, textCol, { 0.5f, 0.0f });
		}
	} else {
		// ==========================================
		// [04] ABORT / EXIT: 電源管理
		// ==========================================
		tr->Print("HackGen", "POWER : DAWN TERMINAL POWER MGMT", curPos.x + 14.0f, cy, 11.5f, { 1.0f, 0.65f, 0.25f, 0.95f });
		cy += 20.0f;
		tr->Print("HackGen", "UPTIME: 00:14:28 | THERMAL: 42.0 ℃", curPos.x + 14.0f, cy, 11.0f, { 0.80f, 0.60f, 0.25f, 0.85f });
		cy += 22.0f;

		DrawOSPanel({ curPos.x + 14.0f, cy }, { curSize.x - 28.0f, 96.0f }, { 0.03f, 0.015f, 0.01f, 0.85f }, { 0.65f, 0.35f, 0.15f, 0.65f }, 1.0f);
		tr->Print("HackGen", "STANDBY CONSUMPTION : 12.4 W", curPos.x + 22.0f, cy + 12.0f, 10.5f, { 0.90f, 0.70f, 0.25f, 0.90f });
		tr->Print("HackGen", "CURRENT BUS PROFILE : FLIGHT_STATION_A", curPos.x + 22.0f, cy + 32.0f, 10.5f, { 0.85f, 0.65f, 0.20f, 0.85f });
		tr->Print("HackGen", "VOLTAGE REGULATOR   : 12.02V STABLE", curPos.x + 22.0f, cy + 52.0f, 10.5f, { 0.85f, 0.65f, 0.20f, 0.85f });
		tr->Print("HackGen", "READY FOR SAFE SHUTDOWN COMMAND", curPos.x + 22.0f, cy + 72.0f, 10.5f, { 1.0f, 0.80f, 0.30f, 0.95f });
		cy += 108.0f;

		tr->Print("HackGen", "ACTION (CLICK TO EXECUTE):", curPos.x + 16.0f, cy + 2.0f, 10.5f, { 0.75f, 0.55f, 0.20f, 0.85f });
		cy += 18.0f;
		const char* pwrLabels[] = { "[LOGOUT]", "[HALT]", "[REBOOT]" };
		for (int p = 0; p < 3; ++p) {
			float bx = curPos.x + 14.0f + p * 74.0f;
			bool isHov = isBtnHov(bx, cy, 68.0f, 22.0f);
			Vector4 bBg = isHov ? Vector4{ 0.25f, 0.10f, 0.05f, 0.85f } : Vector4{ 0.10f, 0.05f, 0.02f, 0.70f };
			Vector4 bBorder = isHov ? Vector4{ 1.0f, 0.55f, 0.25f, 0.95f } : Vector4{ 0.65f, 0.35f, 0.15f, 0.70f };
			DrawOSPanel({ bx, cy }, { 68.0f, 22.0f }, bBg, bBorder, 1.0f);
			Vector4 textCol = isHov ? Vector4{ 1.0f, 0.95f, 0.50f, 1.0f } : Vector4{ 1.0f, 0.75f, 0.30f, 0.85f };
			tr->Print("HackGen", pwrLabels[p], bx + 34.0f, cy + 4.0f, 11.0f, textCol, { 0.5f, 0.0f });
		}
	}

	// 連動切り替え演出（走査線スキャン＆同期プロンプト）
	if (switchRatio > 0.0f) {
		float scanY = curPos.y + win.titleBarHeight + (1.0f - switchRatio) * (curSize.y - win.titleBarHeight);
		DrawOSRect({ curPos.x + 2.0f, scanY }, { curSize.x - 4.0f, 2.0f }, { 0.40f, 1.0f, 0.80f, 0.85f * switchRatio });
		tr->Print("HackGen", ">> SYNCING DATA STREAM...", curPos.x + curSize.x * 0.5f, curPos.y + curSize.y * 0.5f, 11.0f, { 1.0f, 1.0f, 1.0f, 0.95f * switchRatio }, { 0.5f, 0.5f });
	}

	if (isActive) {
		// 四隅コーナーブラケット強調
		const float cLen = 14.0f;
		const float cThick = 2.0f;
		const Vector4 cCol = { 0.85f, 1.0f, 0.95f, 1.0f };
		DrawOSRect(curPos, { cLen, cThick }, cCol);
		DrawOSRect(curPos, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cLen, curPos.y }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cThick, curPos.y }, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x, curPos.y + curSize.y - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x, curPos.y + curSize.y - cLen }, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cLen, curPos.y + curSize.y - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cThick, curPos.y + curSize.y - cLen }, { cThick, cLen }, cCol);
	} else {
		// 非アクティブウィンドウの減光ディムオーバーレイ
		DrawOSRect(curPos, curSize, { 0.0f, 0.015f, 0.008f, 0.35f });
	}
}

void TitleScene::DrawOSWindowTelemetry(const OSWindow& win) {
	if (win.animScale <= 0.01f) { return; } // 完全最小化中は描画スキップ
	TextRenderer* tr = TextRenderer::GetInstance();

	// 最小化・展開トランジション座標計算（項目3）
	float s = win.animScale;
	float smoothS = s * s * (3.0f - 2.0f * s);
	const float tabTargetX = 120.0f + 148.0f * 2.0f + 70.0f;
	const float tabTargetY = 688.0f + 16.0f;
	auto lerpVal = [](float a, float b, float t) { return a + (b - a) * t; };
	Vector2 curPos = {
		lerpVal(tabTargetX - (win.size.x * 0.5f) * smoothS, win.pos.x, smoothS),
		lerpVal(tabTargetY - (win.size.y * 0.5f) * smoothS, win.pos.y, smoothS)
	};
	Vector2 curSize = { win.size.x * smoothS, win.size.y * smoothS };

	bool isActive = (activeWindowId_ == win.id);
	Vector4 borderCol = isActive ? Vector4{ 0.45f, 1.0f, 0.70f, 0.98f * smoothS } : Vector4{ 0.10f, 0.30f, 0.15f, 0.40f * smoothS };
	Vector4 titleBarBg = isActive ? Vector4{ 0.06f, 0.25f, 0.12f, 0.98f } : Vector4{ 0.015f, 0.05f, 0.025f, 0.85f };

	// 連動切り替え演出（切り替え直後のサイバー発光）
	float switchRatio = (kSubWindowSwitchDuration > 0.0f) ? (subWindowSwitchTimer_ / kSubWindowSwitchDuration) : 0.0f;
	if (switchRatio > 0.0f) {
		borderCol.x = (std::min)(1.0f, borderCol.x + 0.50f * switchRatio);
		borderCol.y = (std::min)(1.0f, borderCol.y + 0.30f * switchRatio);
		borderCol.z = (std::min)(1.0f, borderCol.z + 0.45f * switchRatio);
	}

	// 1. ウィンドウ本体パネル（背後ウィンドウのテキスト・スプライトを100%遮蔽する完全不透明プレート）
	DrawOSPanel(curPos, curSize, { 0.01f, 0.04f, 0.02f, 1.0f }, borderCol, isActive ? 2.0f : 1.0f);

	if (smoothS < 0.40f) { return; } // アニメーション極小時は枠のみ描画

	// 2. タイトルバー
	DrawOSRect(curPos, { curSize.x, win.titleBarHeight * smoothS }, titleBarBg);
	DrawOSRect({ curPos.x, curPos.y + win.titleBarHeight * smoothS - 1.0f }, { curSize.x, 1.0f }, borderCol);
	if (isActive) {
		DrawOSRect(curPos, { curSize.x, 2.0f }, { 0.70f, 1.0f, 0.85f, 1.0f }); // 上端ネオンアクセントライン
		DrawOSRect({ curPos.x + 8.0f, curPos.y + 9.0f }, { 6.0f, 6.0f }, { 0.40f, 1.0f, 0.70f, 1.0f }); // 高輝度LED
		tr->Print("HackGen", "[FOCUS] " + win.title, curPos.x + 18.0f, curPos.y + 6.0f, 11.5f * smoothS, { 0.85f, 1.0f, 0.92f, 1.0f });
	} else {
		DrawOSRect({ curPos.x + 8.0f, curPos.y + 9.0f }, { 6.0f, 6.0f }, { 0.12f, 0.30f, 0.16f, 0.45f }); // 消灯LED
		tr->Print("HackGen", "[IDLE]  " + win.title, curPos.x + 18.0f, curPos.y + 6.0f, 11.5f * smoothS, { 0.20f, 0.48f, 0.28f, 0.60f });
	}

	const float closeX = curPos.x + curSize.x - 24.0f;
	const float maxmX  = closeX - 22.0f;
	const float miniX  = maxmX - 22.0f;
	const float btnY   = curPos.y + 5.0f;

	tr->Print("HackGen", "[_]", miniX, btnY + 1.0f, 10.5f, { 0.40f, 0.80f, 0.55f, 0.85f });
	tr->Print("HackGen", "[口]", maxmX, btnY + 1.0f, 10.5f, { 0.40f, 0.80f, 0.55f, 0.85f });
	tr->Print("HackGen", "[X]", closeX, btnY + 1.0f, 10.5f, { 0.95f, 0.35f, 0.30f, 0.95f });

	// ホバー判定ヘルパーラムダ
	auto isBtnHov = [&](float bx, float by, float bw, float bh) {
		return (customCursorPos_.x >= bx && customCursorPos_.x <= bx + bw &&
		        customCursorPos_.y >= by && customCursorPos_.y <= by + bh);
	};

	// 3. ウィンドウ内コンテンツ（中央メニューのカーソル選択に連動したインタラクティブ表示）
	int selIdx = selectionManager_.GetSelectedIndex();
	float cy = curPos.y + win.titleBarHeight + 10.0f;
	const float gW = curSize.x - 28.0f;

	if (selIdx == 0) {
		// ==========================================
		// [01] EXECUTE SORTIE: 機体ステータス (MiG-21)
		// ==========================================
		tr->Print("HackGen", "AIRFRAME : MiG-21 [FISHBED-J]", curPos.x + 14.0f, cy, 12.0f, { 0.40f, 1.0f, 0.65f, 0.95f });
		cy += 20.0f;
		tr->Print("HackGen", "ENGINES  : TUMANSKY R-25-300 x1", curPos.x + 14.0f, cy, 11.0f, { 0.30f, 0.75f, 0.45f, 0.85f });
		cy += 22.0f;

		// 推力・ノズル開度リアルタイム動的変動（テスト時はアフターバーナー急上昇）
		float thrustRatio = 0.94f + 0.04f * std::sin(animTimer_ * 2.8f) + 0.015f * std::cos(animTimer_ * 5.2f);
		if (isEngineTesting_) {
			thrustRatio = 1.085f + 0.02f * std::sin(animTimer_ * 14.0f);
		}
		tr->Print("HackGen", "THRUST OUTPUT :", curPos.x + 14.0f, cy, 11.0f, isEngineTesting_ ? Vector4{ 1.0f, 0.65f, 0.20f, 0.98f } : Vector4{ 0.35f, 0.85f, 0.55f, 0.90f });
		char thrustBuf[32];
		if (isEngineTesting_) {
			snprintf(thrustBuf, sizeof(thrustBuf), "%5.1f %% [A/B]", thrustRatio * 100.0f);
		} else {
			snprintf(thrustBuf, sizeof(thrustBuf), "%5.1f %%", thrustRatio * 100.0f);
		}
		tr->Print("HackGen", thrustBuf, curPos.x + curSize.x - 84.0f, cy, 11.0f, isEngineTesting_ ? Vector4{ 1.0f, 0.35f, 0.20f, 1.0f } : Vector4{ 1.0f, 0.90f, 0.25f, 0.95f });
		cy += 15.0f;

		DrawOSRect({ curPos.x + 14.0f, cy }, { gW, 7.0f }, { 0.03f, 0.10f, 0.05f, 0.80f });
		Vector4 gaugeCol = isEngineTesting_ ? Vector4{ 1.0f, 0.45f, 0.15f, 0.95f } : Vector4{ 0.25f, 0.95f, 0.50f, 0.90f };
		float fillW = (std::min)(gW, gW * (thrustRatio / 1.10f));
		DrawOSRect({ curPos.x + 14.0f, cy }, { fillW, 7.0f }, gaugeCol);
		DrawOSBorder({ curPos.x + 14.0f, cy }, { gW, 7.0f }, { 0.18f, 0.60f, 0.30f, 0.75f }, 1.0f);
		cy += 18.0f;

		// EGT排気温度のリアルタイム微動（テスト時は885℃赤字警告）
		float egtTemp = 641.0f + 4.5f * std::sin(animTimer_ * 1.8f) + 1.8f * std::cos(animTimer_ * 4.3f);
		if (isEngineTesting_) {
			egtTemp = 885.0f + 14.0f * std::sin(animTimer_ * 8.0f);
		}
		char egtBuf[48];
		if (isEngineTesting_) {
			snprintf(egtBuf, sizeof(egtBuf), "EGT (TEMP)    : %3.0f ℃   [OVERHEAT!]", egtTemp);
		} else {
			snprintf(egtBuf, sizeof(egtBuf), "EGT (TEMP)    : %3.0f ℃   [NOMINAL]", egtTemp);
		}
		tr->Print("HackGen", egtBuf, curPos.x + 14.0f, cy, 10.5f, isEngineTesting_ ? Vector4{ 1.0f, 0.30f, 0.20f, 1.0f } : Vector4{ 0.30f, 0.85f, 0.45f, 0.85f });
		cy += 16.0f;

		// 油圧系統（テスト時はMAX FLOW 24.8MPa）
		float hydPress = isEngineTesting_ ? 24.8f : (21.0f + 0.22f * std::sin(animTimer_ * 2.5f));
		char hydBuf[48];
		snprintf(hydBuf, sizeof(hydBuf), "HYDRAULIC SYS : %4.1f MPa [%s]", hydPress, isEngineTesting_ ? "MAX FLOW" : "ONLINE");
		tr->Print("HackGen", hydBuf, curPos.x + 14.0f, cy, 10.5f, { 0.30f, 0.85f, 0.45f, 0.85f });
		cy += 16.0f;

		// ノズル開度（テスト時は100%全開）
		float nozPos = isEngineTesting_ ? 100.0f : (78.5f + 2.8f * std::cos(animTimer_ * 1.4f));
		char nozBuf[48];
		snprintf(nozBuf, sizeof(nozBuf), "NOZZLE POS    : %4.1f %%   [%s]", nozPos, isEngineTesting_ ? "MAX DIVERG" : "ACTIVE");
		tr->Print("HackGen", nozBuf, curPos.x + 14.0f, cy, 10.5f, isEngineTesting_ ? Vector4{ 1.0f, 0.70f, 0.20f, 0.95f } : Vector4{ 0.35f, 0.95f, 0.60f, 0.90f });
		cy += 20.0f;

		tr->Print("HackGen", "AVIONICS SIGNAL SPECTRUM:", curPos.x + 14.0f, cy, 10.5f, { 0.25f, 0.70f, 0.40f, 0.80f });
		cy += 14.0f;

		const int numBars = 14;
		float barWidth = (gW - (numBars - 1) * 3.0f) / numBars;
		for (int b = 0; b < numBars; ++b) {
			float barNoise = 0.35f + 0.50f * std::sin(animTimer_ * (3.0f + b * 0.7f) + b * 0.8f);
			if (isEngineTesting_) {
				barNoise = 0.55f + 0.40f * std::sin(animTimer_ * (6.0f + b * 1.2f) + b * 1.1f);
			}
			barNoise = std::clamp(barNoise, 0.10f, 1.0f);
			float barH = 22.0f * barNoise;
			float bx = curPos.x + 14.0f + b * (barWidth + 3.0f);
			float by = cy + (22.0f - barH);

			Vector4 bCol = (b >= 11 || isEngineTesting_) ? Vector4{ 1.0f, 0.70f, 0.20f, 0.90f } : Vector4{ 0.25f, 0.95f, 0.55f, 0.85f };
			DrawOSRect({ bx, by }, { barWidth, barH }, bCol);
		}
		cy += 28.0f;

		if (defconLevel_ == 1) {
			DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 24.0f }, { 0.25f, 0.05f, 0.05f, 0.90f }, { 1.0f, 0.25f, 0.20f, 0.95f }, 1.0f);
			tr->Print("HackGen", "DEFCON 1 // IMMEDIATE SCRAMBLE HOT", curPos.x + curSize.x * 0.5f, cy + 5.0f, 11.5f, { 1.0f, 0.30f, 0.25f, 0.98f }, { 0.5f, 0.0f });
		} else {
			DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 24.0f }, { 0.05f, 0.15f, 0.07f, 0.85f }, { 0.35f, 0.95f, 0.55f, 0.85f }, 1.0f);
			tr->Print("HackGen", "DEFCON 2 // SCRAMBLE AUTHORIZED", curPos.x + curSize.x * 0.5f, cy + 5.0f, 11.5f, { 1.0f, 0.90f, 0.25f, 0.98f }, { 0.5f, 0.0f });
		}
	} else if (selIdx == 1) {
		// ==========================================
		// [02] MISSION EDITOR: 敵戦力分析データ
		// ==========================================
		tr->Print("HackGen", "THEATER INTEL : CONFLICT ZONE 07", curPos.x + 14.0f, cy, 12.0f, { 1.0f, 0.45f, 0.35f, 0.98f });
		cy += 20.0f;
		tr->Print("HackGen", "THREAT LEVEL  : CRITICAL [4 BOGEYS]", curPos.x + 14.0f, cy, 11.0f, { 1.0f, 0.85f, 0.25f, 0.90f });
		cy += 22.0f;

		// 脅威度メーター 1: 空対空脅威度 (84%)
		float a2aRatio = 0.84f + 0.03f * std::sin(animTimer_ * 3.5f);
		tr->Print("HackGen", "AIR-TO-AIR THREAT :", curPos.x + 14.0f, cy, 10.5f, { 1.0f, 0.40f, 0.35f, 0.90f });
		tr->Print("HackGen", "84 % [CRITICAL]", curPos.x + curSize.x - 100.0f, cy, 10.5f, { 1.0f, 0.30f, 0.25f, 0.98f });
		cy += 14.0f;
		DrawOSRect({ curPos.x + 14.0f, cy }, { gW, 6.0f }, { 0.12f, 0.03f, 0.03f, 0.80f });
		DrawOSRect({ curPos.x + 14.0f, cy }, { gW * a2aRatio, 6.0f }, { 0.98f, 0.30f, 0.25f, 0.95f });
		DrawOSBorder({ curPos.x + 14.0f, cy }, { gW, 6.0f }, { 0.65f, 0.20f, 0.15f, 0.75f }, 1.0f);
		cy += 16.0f;

		// 脅威度メーター 2: 地対空ミサイル脅威度 (52%)
		float samRatio = 0.52f + 0.02f * std::cos(animTimer_ * 2.2f);
		tr->Print("HackGen", "SAM / AAA DANGER  :", curPos.x + 14.0f, cy, 10.5f, { 1.0f, 0.75f, 0.30f, 0.90f });
		tr->Print("HackGen", "52 % [MODERATE]", curPos.x + curSize.x - 100.0f, cy, 10.5f, { 1.0f, 0.80f, 0.25f, 0.95f });
		cy += 14.0f;
		DrawOSRect({ curPos.x + 14.0f, cy }, { gW, 6.0f }, { 0.10f, 0.06f, 0.02f, 0.80f });
		DrawOSRect({ curPos.x + 14.0f, cy }, { gW * samRatio, 6.0f }, { 1.0f, 0.75f, 0.20f, 0.95f });
		DrawOSBorder({ curPos.x + 14.0f, cy }, { gW, 6.0f }, { 0.65f, 0.45f, 0.15f, 0.75f }, 1.0f);
		cy += 16.0f;

		// 敵戦力探知リスト（選択中ターゲットにハイライト枠）
		const char* intelList[] = {
			"BANDIT [A] : Su-27 FLANKER x2 [CAP]",
			"BANDIT [B] : MiG-29 FULCRUM x4 [INT]",
			"SAM SITE   : 9K331 TOR-M1 [ACTIVE]"
		};
		for (int t = 0; t < 3; ++t) {
			if (activeIntelTarget_ == t) {
				DrawOSPanel({ curPos.x + 10.0f, cy - 1.0f }, { gW + 8.0f, 17.0f }, { 0.30f, 0.10f, 0.05f, 0.60f }, { 1.0f, 0.55f, 0.25f, 0.85f }, 1.0f);
			}
			Vector4 tCol = (activeIntelTarget_ == t) ? Vector4{ 1.0f, 0.95f, 0.40f, 1.0f } : Vector4{ 0.95f, 0.55f, 0.35f, 0.85f };
			tr->Print("HackGen", intelList[t], curPos.x + 14.0f, cy, 10.5f, tCol);
			cy += 16.0f;
		}
		cy += 4.0f;

		tr->Print("HackGen", "ECM RADAR JAMMING SPECTRUM:", curPos.x + 14.0f, cy, 10.5f, { 0.90f, 0.40f, 0.30f, 0.80f });
		cy += 14.0f;

		const int numBars = 14;
		float barWidth = (gW - (numBars - 1) * 3.0f) / numBars;
		for (int b = 0; b < numBars; ++b) {
			float barNoise = 0.30f + 0.55f * std::sin(animTimer_ * (4.2f + b * 1.1f) + b * 0.9f);
			barNoise = std::clamp(barNoise, 0.10f, 1.0f);
			float barH = 22.0f * barNoise;
			float bx = curPos.x + 14.0f + b * (barWidth + 3.0f);
			float by = cy + (22.0f - barH);

			Vector4 bCol = (b >= 9) ? Vector4{ 1.0f, 0.30f, 0.25f, 0.95f } : Vector4{ 1.0f, 0.70f, 0.20f, 0.85f };
			DrawOSRect({ bx, by }, { barWidth, barH }, bCol);
		}
		cy += 28.0f;

		DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 24.0f }, { 0.18f, 0.05f, 0.05f, 0.85f }, { 1.0f, 0.35f, 0.30f, 0.85f }, 1.0f);
		tr->Print("HackGen", "ADVISORY // HIGH-G EVASION REQUIRED", curPos.x + curSize.x * 0.5f, cy + 5.0f, 11.5f, { 1.0f, 0.30f, 0.25f, 0.98f }, { 0.5f, 0.0f });
	} else if (selIdx == 2) {
		// ==========================================
		// [03] SYSTEM CONFIG: オーディオ＆入力マトリクス
		// ==========================================
		tr->Print("HackGen", "AUDIO BUS : XAUDIO2 7.1 / MF ENGINE", curPos.x + 14.0f, cy, 12.0f, { 0.35f, 0.95f, 0.60f, 0.95f });
		cy += 20.0f;
		tr->Print("HackGen", "VOLUME    : MASTER 100% | BGM 80%", curPos.x + 14.0f, cy, 11.0f, { 0.30f, 0.80f, 0.50f, 0.85f });
		cy += 22.0f;

		tr->Print("HackGen", "CONTROLLER INPUT MATRIX:", curPos.x + 14.0f, cy, 11.0f, { 0.35f, 0.90f, 0.55f, 0.90f });
		cy += 16.0f;
		DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 56.0f }, { 0.01f, 0.04f, 0.02f, 0.80f }, { 0.20f, 0.60f, 0.35f, 0.65f }, 1.0f);
		tr->Print("HackGen", "PRIMARY  : XINPUT GAMEPAD / FLIGHT STICK", curPos.x + 22.0f, cy + 7.0f, 10.5f, { 0.40f, 0.95f, 0.60f, 0.90f });
		tr->Print("HackGen", "FALLBACK : DIRECTINPUT KB / MOUSE COMBO", curPos.x + 22.0f, cy + 23.0f, 10.5f, { 0.30f, 0.75f, 0.45f, 0.85f });
		tr->Print("HackGen", "CURVES   : PITCH 1.0 | ROLL 1.0 | YAW 1.0", curPos.x + 22.0f, cy + 39.0f, 10.5f, { 0.35f, 0.85f, 0.50f, 0.85f });
		cy += 64.0f;

		tr->Print("HackGen", "AUDIO EQUALIZER SPECTRUM:", curPos.x + 14.0f, cy, 10.5f, { 0.25f, 0.70f, 0.40f, 0.80f });
		cy += 14.0f;

		const int numBars = 14;
		float barWidth = (gW - (numBars - 1) * 3.0f) / numBars;
		for (int b = 0; b < numBars; ++b) {
			float barNoise = 0.25f + 0.60f * std::sin(animTimer_ * (2.4f + b * 0.8f) + b * 0.5f);
			barNoise = std::clamp(barNoise, 0.10f, 1.0f);
			float barH = 22.0f * barNoise;
			float bx = curPos.x + 14.0f + b * (barWidth + 3.0f);
			float by = cy + (22.0f - barH);

			Vector4 bCol = (b >= 11) ? Vector4{ 0.30f, 1.0f, 0.80f, 0.90f } : Vector4{ 0.25f, 0.95f, 0.55f, 0.85f };
			DrawOSRect({ bx, by }, { barWidth, barH }, bCol);
		}
		cy += 28.0f;

		DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 24.0f }, { 0.05f, 0.15f, 0.07f, 0.85f }, { 0.35f, 0.95f, 0.55f, 0.85f }, 1.0f);
		tr->Print("HackGen", "STATUS // HARDWARE BUS INITIALIZED", curPos.x + curSize.x * 0.5f, cy + 5.0f, 11.5f, { 0.40f, 1.0f, 0.65f, 0.98f }, { 0.5f, 0.0f });
	} else {
		// ==========================================
		// [04] ABORT / EXIT: セキュリティセッション終了
		// ==========================================
		tr->Print("HackGen", "SECURITY : SESSION TERMINATION LOG", curPos.x + 14.0f, cy, 12.0f, { 1.0f, 0.70f, 0.25f, 0.95f });
		cy += 20.0f;
		tr->Print("HackGen", "CLEARANCE: LEVEL 5 [OPERATOR AUTH]", curPos.x + 14.0f, cy, 11.0f, { 0.80f, 0.65f, 0.30f, 0.85f });
		cy += 22.0f;

		DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 76.0f }, { 0.03f, 0.02f, 0.01f, 0.80f }, { 0.65f, 0.40f, 0.15f, 0.65f }, 1.0f);
		tr->Print("HackGen", "SESSION CIPHER : AES-256 GCM [ONLINE]", curPos.x + 22.0f, cy + 8.0f, 11.0f, { 0.95f, 0.75f, 0.30f, 0.90f });
		tr->Print("HackGen", "TELEMETRY LOGS : BUFFER ARCHIVED (100%)", curPos.x + 22.0f, cy + 28.0f, 11.0f, { 0.85f, 0.65f, 0.25f, 0.85f });
		tr->Print("HackGen", "NETWORK STATUS : FLUSHING PACKET QUEUE", curPos.x + 22.0f, cy + 48.0f, 11.0f, { 0.85f, 0.65f, 0.25f, 0.85f });
		cy += 84.0f;

		tr->Print("HackGen", "ACTIVITY SPECTRUM:", curPos.x + 14.0f, cy, 10.5f, { 0.70f, 0.50f, 0.20f, 0.80f });
		cy += 14.0f;

		const int numBars = 14;
		float barWidth = (gW - (numBars - 1) * 3.0f) / numBars;
		for (int b = 0; b < numBars; ++b) {
			float barNoise = 0.15f + 0.35f * std::sin(animTimer_ * (1.5f + b * 0.5f) + b * 0.4f);
			barNoise = std::clamp(barNoise, 0.05f, 1.0f);
			float barH = 20.0f * barNoise;
			float bx = curPos.x + 14.0f + b * (barWidth + 3.0f);
			float by = cy + (20.0f - barH);

			DrawOSRect({ bx, by }, { barWidth, barH }, { 0.85f, 0.55f, 0.20f, 0.75f });
		}
		cy += 26.0f;

		DrawOSPanel({ curPos.x + 14.0f, cy }, { gW, 24.0f }, { 0.15f, 0.06f, 0.02f, 0.85f }, { 0.85f, 0.45f, 0.18f, 0.85f }, 1.0f);
		tr->Print("HackGen", "READY // CONFIRM SHUTDOWN IN MAIN TERM", curPos.x + curSize.x * 0.5f, cy + 5.0f, 11.5f, { 1.0f, 0.75f, 0.30f, 0.98f }, { 0.5f, 0.0f });
	}

	// 4. 下部インタラクティブ・アクションボタン群（クリックで実際に動作・実行）
	const float actBtnY = curPos.y + win.titleBarHeight + 276.0f;
	const float actBtnW = 104.0f;
	const float actBtnH = 24.0f;

	struct ActionBtnDef {
		std::string label;
		bool isActive;
		Vector4 normBg;
		Vector4 normBorder;
		Vector4 actBg;
		Vector4 actBorder;
		Vector4 textCol;
	};
	std::vector<ActionBtnDef> actBtns;

	if (selIdx == 0) {
		std::string b0Label = isEngineTesting_ ? "[TESTING...]" : "[ENG TEST]";
		actBtns.push_back({
		b0Label,
		isEngineTesting_,
		{ 0.05f, 0.18f, 0.08f, 0.85f }, { 0.25f, 0.85f, 0.45f, 0.80f },
		{ 0.35f, 0.15f, 0.05f, 0.95f }, { 1.0f, 0.55f, 0.20f, 1.0f },
		isEngineTesting_ ? Vector4{ 1.0f, 0.90f, 0.30f, 1.0f } : Vector4{ 0.40f, 1.0f, 0.65f, 0.95f }
		});
		std::string b1Label = (defconLevel_ == 1) ? "[DEFCON 1]" : "[DEFCON 2]";
		bool isDef1 = (defconLevel_ == 1);
		actBtns.push_back({
		b1Label,
		isDef1,
		{ 0.15f, 0.12f, 0.03f, 0.85f }, { 0.85f, 0.70f, 0.20f, 0.80f },
		{ 0.30f, 0.05f, 0.05f, 0.95f }, { 1.0f, 0.25f, 0.20f, 1.0f },
		isDef1 ? Vector4{ 1.0f, 0.35f, 0.30f, 1.0f } : Vector4{ 1.0f, 0.85f, 0.25f, 0.95f }
		});
		actBtns.push_back({
		"[SORTIE >>]",
		false,
		{ 0.06f, 0.22f, 0.12f, 0.90f }, { 0.35f, 1.0f, 0.65f, 0.90f },
		{ 0.12f, 0.45f, 0.25f, 0.98f }, { 0.60f, 1.0f, 0.80f, 1.0f },
		{ 0.45f, 1.0f, 0.70f, 1.0f }
		});
	} else if (selIdx == 1) {
		actBtns.push_back({
		"[TGT: Su-27]",
		(activeIntelTarget_ == 0),
		{ 0.15f, 0.06f, 0.04f, 0.85f }, { 0.85f, 0.40f, 0.25f, 0.80f },
		{ 0.35f, 0.10f, 0.06f, 0.95f }, { 1.0f, 0.50f, 0.30f, 1.0f },
		{ 1.0f, 0.80f, 0.30f, 0.95f }
		});
		actBtns.push_back({
		"[TGT: MiG-29]",
		(activeIntelTarget_ == 1),
		{ 0.15f, 0.06f, 0.04f, 0.85f }, { 0.85f, 0.40f, 0.25f, 0.80f },
		{ 0.35f, 0.10f, 0.06f, 0.95f }, { 1.0f, 0.50f, 0.30f, 1.0f },
		{ 1.0f, 0.80f, 0.30f, 0.95f }
		});
		actBtns.push_back({
		"[EDIT MISS >>]",
		false,
		{ 0.20f, 0.08f, 0.05f, 0.90f }, { 1.0f, 0.55f, 0.30f, 0.90f },
		{ 0.40f, 0.15f, 0.08f, 0.98f }, { 1.0f, 0.75f, 0.45f, 1.0f },
		{ 1.0f, 0.70f, 0.35f, 1.0f }
		});
	} else if (selIdx == 2) {
		actBtns.push_back({
		"[MUTE BGM]",
		false,
		{ 0.04f, 0.12f, 0.08f, 0.85f }, { 0.25f, 0.75f, 0.45f, 0.80f },
		{ 0.08f, 0.25f, 0.15f, 0.95f }, { 0.45f, 0.95f, 0.65f, 1.0f },
		{ 0.35f, 0.90f, 0.55f, 0.90f }
		});
		actBtns.push_back({
		"[RE-SCAN]",
		false,
		{ 0.04f, 0.12f, 0.08f, 0.85f }, { 0.25f, 0.75f, 0.45f, 0.80f },
		{ 0.08f, 0.25f, 0.15f, 0.95f }, { 0.45f, 0.95f, 0.65f, 1.0f },
		{ 0.35f, 0.90f, 0.55f, 0.90f }
		});
		actBtns.push_back({
		"[CALIBRATE]",
		false,
		{ 0.04f, 0.12f, 0.08f, 0.85f }, { 0.25f, 0.75f, 0.45f, 0.80f },
		{ 0.08f, 0.25f, 0.15f, 0.95f }, { 0.45f, 0.95f, 0.65f, 1.0f },
		{ 0.35f, 0.90f, 0.55f, 0.90f }
		});
	} else {
		actBtns.push_back({
		"[CANCEL]",
		false,
		{ 0.12f, 0.08f, 0.03f, 0.85f }, { 0.65f, 0.45f, 0.20f, 0.80f },
		{ 0.25f, 0.15f, 0.05f, 0.95f }, { 0.85f, 0.60f, 0.25f, 1.0f },
		{ 0.85f, 0.65f, 0.30f, 0.90f }
		});
		actBtns.push_back({
		"[SUSPEND]",
		false,
		{ 0.12f, 0.08f, 0.03f, 0.85f }, { 0.65f, 0.45f, 0.20f, 0.80f },
		{ 0.25f, 0.15f, 0.05f, 0.95f }, { 0.85f, 0.60f, 0.25f, 1.0f },
		{ 0.85f, 0.65f, 0.30f, 0.90f }
		});
		actBtns.push_back({
		"[EXIT >>]",
		false,
		{ 0.25f, 0.06f, 0.05f, 0.90f }, { 0.95f, 0.35f, 0.25f, 0.90f },
		{ 0.45f, 0.10f, 0.08f, 0.98f }, { 1.0f, 0.50f, 0.35f, 1.0f },
		{ 1.0f, 0.45f, 0.35f, 1.0f }
		});
	}

	for (size_t b = 0; b < actBtns.size() && b < 3; ++b) {
		float bx = curPos.x + 14.0f + static_cast<float>(b) * 112.0f;
		bool isHov = isBtnHov(bx, actBtnY, actBtnW, actBtnH);
		const auto& def = actBtns[b];

		Vector4 bBg = def.isActive ? def.actBg : (isHov ? Vector4{ (std::min)(1.0f, def.normBg.x * 2.0f), (std::min)(1.0f, def.normBg.y * 2.0f), (std::min)(1.0f, def.normBg.z * 2.0f), 0.95f } : def.normBg);
		Vector4 bBorder = def.isActive ? def.actBorder : (isHov ? Vector4{ 1.0f, 1.0f, 1.0f, 0.98f } : def.normBorder);
		DrawOSPanel({ bx, actBtnY }, { actBtnW, actBtnH }, bBg, bBorder, (def.isActive || isHov) ? 1.5f : 1.0f);

		Vector4 tCol = isHov ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : def.textCol;
		tr->Print("HackGen", def.label, bx + actBtnW * 0.5f, actBtnY + 5.0f, 10.5f, tCol, { 0.5f, 0.0f });
	}

	// 連動切り替え演出（走査線スキャン＆同期プロンプト）
	if (switchRatio > 0.0f) {
		float scanY = curPos.y + win.titleBarHeight + (1.0f - switchRatio) * (curSize.y - win.titleBarHeight);
		DrawOSRect({ curPos.x + 2.0f, scanY }, { curSize.x - 4.0f, 2.0f }, { 0.40f, 1.0f, 0.80f, 0.85f * switchRatio });
		tr->Print("HackGen", ">> RE-CONFIGURING METRICS...", curPos.x + curSize.x * 0.5f, curPos.y + curSize.y * 0.5f, 11.0f, { 1.0f, 1.0f, 1.0f, 0.95f * switchRatio }, { 0.5f, 0.5f });
	}

	if (isActive) {
		// 四隅コーナーブラケット強調
		const float cLen = 14.0f;
		const float cThick = 2.0f;
		const Vector4 cCol = { 0.85f, 1.0f, 0.95f, 1.0f };
		DrawOSRect(curPos, { cLen, cThick }, cCol);
		DrawOSRect(curPos, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cLen, curPos.y }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cThick, curPos.y }, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x, curPos.y + curSize.y - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x, curPos.y + curSize.y - cLen }, { cThick, cLen }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cLen, curPos.y + curSize.y - cThick }, { cLen, cThick }, cCol);
		DrawOSRect({ curPos.x + curSize.x - cThick, curPos.y + curSize.y - cLen }, { cThick, cLen }, cCol);
	} else {
		// 非アクティブウィンドウの減光ディムオーバーレイ
		DrawOSRect(curPos, curSize, { 0.0f, 0.015f, 0.008f, 0.35f });
	}
}

void TitleScene::DrawDesktopIcons() {
	// 右側のデスクトップアイコンは非表示
}

void TitleScene::DrawCustomCursor() {
	float mx = customCursorPos_.x;
	float my = customCursorPos_.y;
	TextRenderer* tr = TextRenderer::GetInstance();

	size_t curIdx = 0;
	auto drawCursorRect = [&](const Vector2& pos, const Vector2& size, const MyMath::Vector4& color) {
		if (curIdx >= cursorSprites_.size()) { return; }
		auto& sp = cursorSprites_[curIdx++];
		sp->SetPosition(pos);
		sp->SetSize(size);
		sp->SetColor(color);
		sp->Update();
		sp->Draw();
	};

	// 1. クリック波紋エフェクト（クリック地点から放射状に広がるサイバーパルス）
	if (cursorRippleTimer_ > 0.0f) {
		float rp = 1.0f - (cursorRippleTimer_ / 0.30f);
		float rRad = 4.0f + 24.0f * rp;
		float rAlpha = (1.0f - rp) * 0.90f;
		Vector4 ringCol = { 0.35f, 1.0f, 0.65f, rAlpha };
		// リング（外枠4本）
		drawCursorRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y - rRad }, { rRad * 2.0f, 1.5f }, ringCol);
		drawCursorRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y + rRad - 1.5f }, { rRad * 2.0f, 1.5f }, ringCol);
		drawCursorRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y - rRad }, { 1.5f, rRad * 2.0f }, ringCol);
		drawCursorRect({ cursorRipplePos_.x + rRad - 1.5f, cursorRipplePos_.y - rRad }, { 1.5f, rRad * 2.0f }, ringCol);
	}

	// 2. 項目5: ターミナルの色に合わせた緑色のカスタムカーソル（鋭角タクティカル矢印ポインター）
	// 外周黒アウトライン ＋ ネオングリーン本体
	const MyMath::Vector4 kBlackEdge = { 0.0f, 0.0f, 0.0f, 0.95f };
	const MyMath::Vector4 kGreenBody = { 0.22f, 1.0f, 0.55f, 0.98f };
	const MyMath::Vector4 kGreenCore = { 0.85f, 1.0f, 0.90f, 1.00f };

	// 矢印の各ライン（縦軸・斜め軸・返し・尾部）
	// 黒シャドウ・縁
	drawCursorRect({ mx, my }, { 2.0f, 18.0f }, kBlackEdge);
	drawCursorRect({ mx + 1.0f, my + 1.0f }, { 13.0f, 2.0f }, kBlackEdge);
	drawCursorRect({ mx + 2.0f, my + 2.0f }, { 12.0f, 12.0f }, kBlackEdge);

	// 緑本体
	drawCursorRect({ mx + 1.0f, my + 1.0f }, { 2.0f, 15.0f }, kGreenBody);
	drawCursorRect({ mx + 2.0f, my + 2.0f }, { 10.0f, 2.0f }, kGreenBody);
	drawCursorRect({ mx + 3.0f, my + 3.0f }, { 8.0f, 8.0f }, kGreenBody);
	drawCursorRect({ mx + 1.0f, my + 1.0f }, { 2.0f, 4.0f }, kGreenCore); // 先端ハイライト

	// 3. 先端のマイクロクロスヘア（照準レティクル）
	Vector4 reticleCol = { 0.35f, 1.0f, 0.65f, 0.85f };
	drawCursorRect({ mx - 7.0f, my }, { 5.0f, 1.0f }, reticleCol);
	drawCursorRect({ mx + 5.0f, my }, { 5.0f, 1.0f }, reticleCol);
	drawCursorRect({ mx, my - 7.0f }, { 1.0f, 5.0f }, reticleCol);
	drawCursorRect({ mx, my + 5.0f }, { 1.0f, 5.0f }, reticleCol);

	// 4. カーソル右下の微小な座標HUD
	if (mx >= 0.0f && mx <= kScreenWidth && my >= 0.0f && my <= kScreenHeight) {
		char locBuf[32];
		snprintf(locBuf, sizeof(locBuf), "[%d,%d]", static_cast<int>(mx), static_cast<int>(my));
		tr->Print("HackGen", locBuf, mx + 16.0f, my + 14.0f, 9.5f, { 0.35f, 0.95f, 0.55f, 0.80f });
	}
}

void TitleScene::DrawOSDesktop() {
	TextRenderer* tr = TextRenderer::GetInstance();
	osSpritePoolIdx_ = 0;

	// OSデスクトップ同期立ち上がりアニメーション進行度（0.0: ブート直後, 1.0: 立ち上がり完了）
	float enterProg = 1.0f;
	if (menuEnterTimer_ > 0.0f) {
		enterProg = std::clamp(1.0f - (menuEnterTimer_ / kMenuEnterDuration), 0.0f, 1.0f);
	}
	// 滑らかなイーズアウト進行度 (Ease-Out Sine: 最初素早く、最後しっとり定着)
	float smoothProg = (enterProg >= 1.0f) ? 1.0f : std::sin(enterProg * 1.5707963f);

	// 1. デスクトップ上部ヘッダー（OSウォーターマーク & 戦闘連続性ログ：指摘4）
	{
		float hAlpha = smoothProg;
		tr->Print("HackGen", "DAWN_OS TACTICAL DEFENSE MAINFRAME [v4.12.0]", 24.0f, 18.0f, 12.0f, { 0.30f, 0.85f, 0.50f, 0.70f * hAlpha });
		tr->Print("HackGen", "CLEARANCE: LEVEL 5 // SECURE DESKTOP ENVIRONMENT", kScreenWidth - 380.0f, 18.0f, 11.5f, { 0.25f, 0.70f, 0.40f, 0.60f * hAlpha });

		if (!previousSortieLog_.empty()) {
			tr->Print("HackGen", previousSortieLog_.c_str(), 24.0f, 36.0f, 11.0f, { previousSortieColor_.x, previousSortieColor_.y, previousSortieColor_.z, previousSortieColor_.w * hAlpha });
		}
	}
	tr->Flush(); // ヘッダーテキストを先に描画

	// 2. ウィンドウ描画（全ウィンドウが時間差なく完全に同時にアニメーション展開！）
	for (int winId : windowZOrder_) {
		if (winId >= 0 && winId < static_cast<int>(osWindows_.size())) {
			auto& win = osWindows_[winId];
			if (winId == 0) {
				if (win.isOpen && !win.isMinimized && win.animScale >= 0.01f) {
					DrawMenuCard();
					tr->Flush(); // メインターミナルのテキストを描画
				}
			} else if (winId == 1) {
				if (win.isOpen && !win.isMinimized && win.animScale >= 0.01f) {
					DrawOSWindowCam(win);
					tr->Flush(); // サブウィンドウ1のテキストを描画
				}
			} else if (winId == 2) {
				if (win.isOpen && !win.isMinimized && win.animScale >= 0.01f) {
					DrawOSWindowTelemetry(win);
					tr->Flush(); // サブウィンドウ2のテキストを描画
				}
			}
		}
	}

	// 3. デスクトップアイコン（同時にフェードイン）
	DrawDesktopIcons();
	tr->Flush();

	// 4. タスクバー & スタートメニュー（同時にフェードイン）
	DrawOSTaskbar();
	tr->Flush();

	// 5. ターミナルの色に合わせた緑色のカスタムカーソル（最前面: 同時に表示）
	DrawCustomCursor();
	tr->Flush();

	// 起動時の短い立ち上がりHUDインジケーター（画面中央下に一瞬表示してフェード）
	if (menuEnterTimer_ > 0.0f) {
		float hudAlpha = (1.0f - enterProg);
		tr->Print("HackGen", "[ DAWN-OS ALL SYSTEMS ONLINE ]", kScreenWidth * 0.5f, kScreenHeight - 65.0f, 13.0f, { 0.35f, 1.0f, 0.65f, 0.85f * hudAlpha }, { 0.5f, 0.5f });
		tr->Flush();
	}
}
