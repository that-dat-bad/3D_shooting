#include "PseudoOS.h"
#include "../Sprite/SpriteCommon.h"
#include "../Text/TextRenderer.h"
#include "../../base/WinApp.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "../../io/Input.h"
#include "../../base/Math/Easing.h"

static constexpr float kScreenWidth = static_cast<float>(WinApp::kClientWidth);
static constexpr float kScreenHeight = static_cast<float>(WinApp::kClientHeight);

PseudoOS::PseudoOS() {
	ApplyThemePalette();
}

PseudoOS::~PseudoOS() {
}

void PseudoOS::Initialize(SpriteCommon* spriteCommon, Theme theme) {
	spriteCommon_ = spriteCommon;
	currentTheme_ = theme;
	ApplyThemePalette();

	// スプライトプールの初期化（512枚：プール枯渇防止）
	if (spritePool_.empty()) {
		spritePool_.clear();
		for (int i = 0; i < 512; ++i) {
			auto sp = std::make_unique<Sprite>();
			sp->Initialize(spriteCommon, "assets/textures/white1x1.png");
			sp->SetAnchorPoint({ 0.0f, 0.0f });
			sp->SetPosition({ 0.0f, 0.0f });
			sp->SetSize({ 1.0f, 1.0f });
			sp->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			sp->Update();
			spritePool_.push_back(std::move(sp));
		}
	}
	spritePoolIdx_ = 0;

	// 専用カーソルスプライトの初期化（24個：プール枯渇と完全分離）
	if (cursorSprites_.empty()) {
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
	}
}

void PseudoOS::SetTheme(Theme theme) {
	currentTheme_ = theme;
	ApplyThemePalette();
}

void PseudoOS::ApplyThemePalette() {
	switch (currentTheme_) {
	case Theme::KernelPanicRed:
		colPrimary_        = { 1.00f, 0.35f, 0.30f, 1.00f };
		colSecondary_      = { 0.60f, 0.18f, 0.15f, 0.85f };
		colHighlight_      = { 1.00f, 0.90f, 0.35f, 1.00f };
		colPanelBg_        = { 0.05f, 0.01f, 0.01f, 0.98f };
		colTaskbarBg_      = { 0.04f, 0.01f, 0.01f, 0.98f };
		colBorderActive_   = { 1.00f, 0.40f, 0.35f, 0.98f };
		colBorderInactive_ = { 0.35f, 0.10f, 0.08f, 0.50f };
		colLedActive_      = { 1.00f, 0.25f, 0.20f, 1.00f };
		break;

	case Theme::TacticalCyan:
		colPrimary_        = { 0.25f, 0.85f, 1.00f, 1.00f };
		colSecondary_      = { 0.10f, 0.45f, 0.60f, 0.85f };
		colHighlight_      = { 0.85f, 0.98f, 1.00f, 1.00f };
		colPanelBg_        = { 0.01f, 0.03f, 0.06f, 0.98f };
		colTaskbarBg_      = { 0.01f, 0.03f, 0.05f, 0.98f };
		colBorderActive_   = { 0.35f, 0.90f, 1.00f, 0.98f };
		colBorderInactive_ = { 0.10f, 0.28f, 0.40f, 0.50f };
		colLedActive_      = { 0.30f, 0.95f, 1.00f, 1.00f };
		break;

	case Theme::TerminalGreen:
	default:
		colPrimary_        = { 0.35f, 1.00f, 0.60f, 1.00f };
		colSecondary_      = { 0.15f, 0.55f, 0.30f, 0.85f };
		colHighlight_      = { 0.90f, 1.00f, 0.95f, 1.00f };
		colPanelBg_        = { 0.01f, 0.04f, 0.02f, 0.98f };
		colTaskbarBg_      = { 0.01f, 0.04f, 0.02f, 0.98f };
		colBorderActive_   = { 0.45f, 1.00f, 0.70f, 0.98f };
		colBorderInactive_ = { 0.12f, 0.35f, 0.20f, 0.50f };
		colLedActive_      = { 0.40f, 1.00f, 0.70f, 1.00f };
		break;
	}
}

void PseudoOS::AddWindow(const Window& window) {
	windows_.push_back(window);
	windowZOrder_.push_back(window.id);
	if (windows_.size() == 1) {
		activeWindowId_ = window.id;
	}
}

void PseudoOS::ClearWindows() {
	windows_.clear();
	windowZOrder_.clear();
	activeWindowId_ = 0;
}

PseudoOS::Window* PseudoOS::GetWindow(int id) {
	for (auto& w : windows_) {
		if (w.id == id) return &w;
	}
	return nullptr;
}

const PseudoOS::Window* PseudoOS::GetWindow(int id) const {
	for (const auto& w : windows_) {
		if (w.id == id) return &w;
	}
	return nullptr;
}

void PseudoOS::BringWindowToFront(int id) {
	activeWindowId_ = id;
	auto it = std::find(windowZOrder_.begin(), windowZOrder_.end(), id);
	if (it != windowZOrder_.end()) {
		windowZOrder_.erase(it);
	}
	windowZOrder_.push_back(id);
}

bool PseudoOS::IsCursorInsideWindow(int windowId) const {
	const Window* win = GetWindow(windowId);
	if (!win || !win->isOpen || win->isMinimized) return false;
	return (cursorPos_.x >= win->pos.x && cursorPos_.x <= win->pos.x + win->size.x &&
	        cursorPos_.y >= win->pos.y && cursorPos_.y <= win->pos.y + win->size.y);
}

void PseudoOS::DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color) {
	if (spritePoolIdx_ >= spritePool_.size()) {
		auto sp = std::make_unique<Sprite>();
		sp->Initialize(spriteCommon_ ? spriteCommon_ : SpriteCommon::GetInstance(), "assets/textures/white1x1.png");
		sp->SetAnchorPoint({ 0.0f, 0.0f });
		spritePool_.push_back(std::move(sp));
	}
	auto& sp = spritePool_[spritePoolIdx_++];
	sp->SetPosition(pos);
	sp->SetSize(size);
	sp->SetColor(color);
	sp->Update();
	sp->Draw();
}

void PseudoOS::DrawBorder(const Vector2& pos, const Vector2& size, const Vector4& color, float thick) {
	DrawRect(pos, { size.x, thick }, color);
	DrawRect({ pos.x, pos.y + size.y - thick }, { size.x, thick }, color);
	DrawRect(pos, { thick, size.y }, color);
	DrawRect({ pos.x + size.x - thick, pos.y }, { thick, size.y }, color);
}

void PseudoOS::DrawPanel(const Vector2& pos, const Vector2& size, const Vector4& bgCol, const Vector4& borderCol, float thick) {
	DrawRect(pos, size, bgCol);
	DrawBorder(pos, size, borderCol, thick);
}

void PseudoOS::DrawSpectrumBars(const Vector2& pos, float width, float height, int barCount, float animSpeed, const Vector4& color) {
	if (barCount <= 0 || width <= 0.0f) return;
	float gap = 3.0f;
	float barWidth = (width - (barCount - 1) * gap) / barCount;
	for (int b = 0; b < barCount; ++b) {
		float noise = 0.25f + 0.65f * std::sin(animTimer_ * (animSpeed + b * 0.7f) + b * 0.9f);
		noise = (std::clamp)(noise, 0.08f, 1.0f);
		float barH = height * noise;
		float bx = pos.x + b * (barWidth + gap);
		float by = pos.y + (height - barH);
		DrawRect({ bx, by }, { barWidth, barH }, color);
	}
}

void PseudoOS::DrawGauge(const Vector2& pos, const Vector2& size, float ratio, const Vector4& fillColor, const Vector4& borderCol) {
	DrawRect(pos, size, { 0.02f, 0.06f, 0.04f, 0.85f });
	float r = (std::clamp)(ratio, 0.0f, 1.0f);
	if (r > 0.0f) {
		DrawRect(pos, { size.x * r, size.y }, fillColor);
	}
	DrawBorder(pos, size, borderCol, 1.0f);
}

void PseudoOS::Update(float dt) {
	Input* input = Input::GetInstance();
	if (input) {
		Vector2 mousePos = input->GetVirtualMousePosition(kScreenWidth, kScreenHeight);
		bool isLDown = input->PushMouse(0);
		bool isLTrig = input->TriggerMouse(0);
		Update(dt, mousePos, isLDown, isLTrig);
	} else {
		Update(dt, { 640.0f, 360.0f }, false, false);
	}
}

void PseudoOS::Update(float dt, const Vector2& mousePos, bool isLDown, bool isLTrig) {
	animTimer_ += dt;
	cursorPos_ = mousePos;

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

	// 1. ウィンドウ開閉アニメーション / 最小化・復元アニメーション (Tween)
	if (windowAnimState_ == WindowAnimState::Opening) {
		windowAnimTimer_ += dt;
		float t = Easing::Clamp01(windowAnimTimer_ / (std::max)(0.001f, windowAnimDuration_));
		float scale = Easing::EaseOutCubic(t);
		for (auto& win : windows_) {
			if (win.isOpen && !win.isMinimized) {
				win.animScale = scale;
			}
		}
		if (t >= 1.0f) {
			windowAnimState_ = WindowAnimState::None;
		}
	} else if (windowAnimState_ == WindowAnimState::Closing) {
		windowAnimTimer_ += dt;
		float t = Easing::Clamp01(windowAnimTimer_ / (std::max)(0.001f, windowAnimDuration_));
		float scale = (1.0f - t) * (1.0f - t);
		for (auto& win : windows_) {
			win.animScale = scale;
		}
		if (t >= 1.0f) {
			windowAnimState_ = WindowAnimState::None;
		}
	} else {
		for (auto& win : windows_) {
			float targetScale = (win.isMinimized || !win.isOpen) ? 0.0f : 1.0f;
			float speed = 12.0f;
			win.animScale += (targetScale - win.animScale) * (1.0f - std::exp(-speed * dt));
			if (std::abs(win.animScale - targetScale) < 0.005f) {
				win.animScale = targetScale;
			}
		}
	}

	// 2. ウィンドウドラッグ移動更新
	for (auto& win : windows_) {
		if (win.isDragging) {
			if (isLDown) {
				win.pos = { mx - win.dragOffset.x, my - win.dragOffset.y };
				// 画面外脱出の防止
				win.pos.x = (std::clamp)(win.pos.x, -win.size.x + 80.0f, kScreenWidth - 80.0f);
				win.pos.y = (std::clamp)(win.pos.y, 0.0f, tbY - win.titleBarHeight);
			} else {
				win.isDragging = false;
			}
		}
	}

	// 3. クリック判定（タスクバー・スタートメニュー・ウィンドウ操作）
	if (isLTrig) {
		// A. タスクバー上の操作判定
		if (isTaskbarVisible_ && my >= tbY) {
			// スタートボタン [⯈ DAWN_OS] (8px 〜 112px)
			if (mx >= 8.0f && mx <= 114.0f && my >= tbY + 2.0f && my <= tbY + 30.0f) {
				isStartMenuOpen_ = !isStartMenuOpen_;
				return;
			}

			// タスクバータブ（ウィンドウ復元/最小化トグル）
			float tabX = 120.0f;
			for (auto& win : windows_) {
				float tabW = 142.0f;
				float tabH = 26.0f;
				float tabY = tbY + 3.0f;

				if (mx >= tabX && mx <= tabX + tabW && my >= tabY && my <= tabY + tabH) {
					if (win.isMinimized || !win.isOpen) {
						win.isOpen = true;
						win.isMinimized = false;
						BringWindowToFront(win.id);
					} else if (activeWindowId_ == win.id) {
						win.isMinimized = true;
					} else {
						BringWindowToFront(win.id);
					}
					isStartMenuOpen_ = false;
					return;
				}
				tabX += tabW + 6.0f;
			}
		}

		// B. スタートメニュー内部のクリック判定
		if (isStartMenuOpen_) {
			const float smX = 8.0f;
			const float smY = tbY - 188.0f;
			const float smW = 250.0f;
			const float smH = 184.0f;

			if (mx >= smX && mx <= smX + smW && my >= smY && my <= smY + smH) {
				float itemY = smY + 34.0f;
				for (size_t i = 0; i < startMenuItems_.size(); ++i) {
					if (startMenuItems_[i].isSeparator) {
						itemY += 6.0f;
					}
					if (mx >= smX + 4.0f && mx <= smX + smW - 4.0f && my >= itemY - 2.0f && my < itemY + 22.0f) {
						if (startMenuItems_[i].onClick) {
							startMenuItems_[i].onClick();
						}
						isStartMenuOpen_ = false;
						return;
					}
					itemY += 26.0f;
				}
			} else {
				isStartMenuOpen_ = false;
			}
		}

		// C. ウィンドウのクリック判定（手前優先：逆順探索）
		for (auto it = windowZOrder_.rbegin(); it != windowZOrder_.rend(); ++it) {
			int winId = *it;
			Window* winPtr = GetWindow(winId);
			if (!winPtr || !winPtr->isOpen || winPtr->isMinimized) continue;
			Window& win = *winPtr;

			// タイトルバー右上のボタン判定 [_] [口] [X]
			float closeX = win.pos.x + win.size.x - 24.0f;
			float maxmX  = closeX - 22.0f;
			float miniX  = maxmX - 22.0f;
			float btnY   = win.pos.y + 4.0f;
			float btnW   = 20.0f;
			float btnH   = win.titleBarHeight - 8.0f;

			if (my >= btnY && my <= btnY + btnH) {
				if (mx >= closeX && mx <= closeX + btnW) {
					// [X] 閉じる
					win.isOpen = false;
					break;
				} else if (mx >= maxmX && mx <= maxmX + btnW) {
					// [口] 定位置リセット
					win.pos = win.normalPos;
					BringWindowToFront(win.id);
					break;
				} else if (mx >= miniX && mx <= miniX + btnW) {
					// [_] 最小化
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

			// ウィンドウ内部クリックで最前面化
			if (mx >= win.pos.x && mx <= win.pos.x + win.size.x &&
				my >= win.pos.y && my <= win.pos.y + win.size.y) {
				BringWindowToFront(win.id);
				break;
			}
		}
	}
}

void PseudoOS::Draw() {
	TextRenderer* tr = TextRenderer::GetInstance();
	spritePoolIdx_ = 0;

	// 1. デスクトップ上部ヘッダー（OSウォーターマーク & クリアランス）
	tr->Print("HackGen", osName_, 24.0f, 18.0f, 12.0f, colSecondary_);
	tr->Print("HackGen", securityClearance_, kScreenWidth - 380.0f, 18.0f, 11.5f, colSecondary_);
	tr->Flush();

	// 2. 全ウィンドウ描画（ZOrder順：奥から手前へ）
	for (int winId : windowZOrder_) {
		Window* win = GetWindow(winId);
		if (!win || !win->isOpen || win->animScale < 0.01f) continue;
		DrawWindowFrame(*win);
		tr->Flush();
	}

	// 3. タスクバー & スタートメニュー
	if (isTaskbarVisible_) {
		DrawTaskbar();
		tr->Flush();
	}

	// 4. カスタムサイバーマウスカーソル（最前面）
	if (isCursorVisible_) {
		DrawCustomCursor();
		tr->Flush();
	}
}

void PseudoOS::DrawWindowFrame(Window& win) {
	TextRenderer* tr = TextRenderer::GetInstance();
	float s = win.animScale;
	float smoothS = s * s * (3.0f - 2.0f * s);

	const float tabTargetX = 120.0f + 148.0f * static_cast<float>(win.id) + 70.0f;
	const float tabTargetY = 688.0f + 16.0f;
	auto lerpVal = [](float a, float b, float t) { return a + (b - a) * t; };
	Vector2 curPos = {
		lerpVal(tabTargetX - (win.size.x * 0.5f) * smoothS, win.pos.x, smoothS),
		lerpVal(tabTargetY - (win.size.y * 0.5f) * smoothS, win.pos.y, smoothS)
	};
	Vector2 curSize = { win.size.x * smoothS, win.size.y * smoothS };

	bool isActive = (activeWindowId_ == win.id);
	Vector4 borderCol = isActive ? colBorderActive_ : colBorderInactive_;
	borderCol.w *= smoothS;

	Vector4 titleBarBg = isActive ? Vector4{ colBorderActive_.x * 0.15f, colBorderActive_.y * 0.15f, colBorderActive_.z * 0.15f, 0.98f }
	                              : Vector4{ colPanelBg_.x * 1.5f, colPanelBg_.y * 1.5f, colPanelBg_.z * 1.5f, 0.85f };

	// 0. ウィンドウ背面の多層ドロップシャドウ（項目9: デスクトップ空間としての明確な奥行き・立体感）
	const float shadowOffset1 = 6.0f * smoothS;
	const float shadowOffset2 = 12.0f * smoothS;
	DrawRect({ curPos.x + shadowOffset1, curPos.y + shadowOffset1 }, curSize, { 0.0f, 0.0f, 0.0f, 0.40f * smoothS });
	DrawRect({ curPos.x + shadowOffset2, curPos.y + shadowOffset2 }, curSize, { 0.0f, 0.0f, 0.0f, 0.18f * smoothS });

	// 1. ウィンドウ本体パネル（背後を完全遮蔽するソリッドバックプレート）
	DrawPanel(curPos, curSize, colPanelBg_, borderCol, isActive ? 2.0f : 1.0f);

	if (smoothS < 0.35f) { return; } // アニメーション極小時は枠のみ

	// 2. タイトルバー
	float tbH = win.titleBarHeight * smoothS;
	DrawRect(curPos, { curSize.x, tbH }, titleBarBg);
	DrawRect({ curPos.x, curPos.y + tbH - 1.0f }, { curSize.x, 1.0f }, borderCol);

	if (isActive) {
		DrawRect(curPos, { curSize.x, 2.0f }, colHighlight_); // 上端ネオンアクセントライン
		DrawRect({ curPos.x + 8.0f, curPos.y + 9.0f }, { 6.0f, 6.0f }, colLedActive_); // 高輝度LED
		tr->Print("HackGen", "[FOCUS] " + win.title, curPos.x + 18.0f, curPos.y + 6.0f, 11.5f * smoothS, colHighlight_);
	} else {
		DrawRect({ curPos.x + 8.0f, curPos.y + 9.0f }, { 6.0f, 6.0f }, { colSecondary_.x * 0.5f, colSecondary_.y * 0.5f, colSecondary_.z * 0.5f, 0.45f });
		tr->Print("HackGen", "[IDLE]  " + win.title, curPos.x + 18.0f, curPos.y + 6.0f, 11.5f * smoothS, colSecondary_);
	}

	// 3. タイトルバー右上のボタン [_] [口] [X]
	const float btnY = curPos.y + 5.0f;
	const float closeX = curPos.x + curSize.x - 24.0f;
	const float maxmX  = closeX - 22.0f;
	const float miniX  = maxmX - 22.0f;

	tr->Print("HackGen", "[_]", miniX, btnY + 1.0f, 10.5f, colSecondary_);
	tr->Print("HackGen", "[口]", maxmX, btnY + 1.0f, 10.5f, colSecondary_);
	tr->Print("HackGen", "[X]", closeX, btnY + 1.0f, 10.5f, { 1.0f, 0.35f, 0.30f, 0.95f });

	// 4. ウィンドウ個別コンテンツ描画
	if (win.onDrawContent) {
		win.onDrawContent(win, this);
	}

	// 5. 四隅コーナーブラケット（アクティブ時強調）
	if (isActive) {
		const float cLen = 14.0f;
		const float cThick = 2.0f;
		DrawRect(curPos, { cLen, cThick }, colHighlight_);
		DrawRect(curPos, { cThick, cLen }, colHighlight_);
		DrawRect({ curPos.x + curSize.x - cLen, curPos.y }, { cLen, cThick }, colHighlight_);
		DrawRect({ curPos.x + curSize.x - cThick, curPos.y }, { cThick, cLen }, colHighlight_);
		DrawRect({ curPos.x, curPos.y + curSize.y - cThick }, { cLen, cThick }, colHighlight_);
		DrawRect({ curPos.x, curPos.y + curSize.y - cLen }, { cThick, cLen }, colHighlight_);
		DrawRect({ curPos.x + curSize.x - cLen, curPos.y + curSize.y - cThick }, { cLen, cThick }, colHighlight_);
		DrawRect({ curPos.x + curSize.x - cThick, curPos.y + curSize.y - cLen }, { cThick, cLen }, colHighlight_);
	} else {
		// 非アクティブウィンドウの減光ディム
		DrawRect(curPos, curSize, { 0.0f, 0.0f, 0.0f, 0.30f });
	}
}

void PseudoOS::DrawTaskbar() {
	TextRenderer* tr = TextRenderer::GetInstance();
	const float tbY = 688.0f;
	const float tbH = 32.0f;

	// 1. タスクバー背景 & 上部境界線
	DrawRect({ 0.0f, tbY }, { kScreenWidth, tbH }, colTaskbarBg_);
	DrawRect({ 0.0f, tbY }, { kScreenWidth, 1.5f }, colBorderActive_);

	// 2. スタートボタン [ ⯈ DAWN_OS ]
	bool smHover = (cursorPos_.x >= 8.0f && cursorPos_.x <= 114.0f && cursorPos_.y >= 690.0f && cursorPos_.y <= 718.0f);
	Vector4 startBg = (isStartMenuOpen_ || smHover) ? Vector4{ colBorderActive_.x * 0.25f, colBorderActive_.y * 0.25f, colBorderActive_.z * 0.25f, 0.92f }
	                                                : Vector4{ colBorderActive_.x * 0.08f, colBorderActive_.y * 0.08f, colBorderActive_.z * 0.08f, 0.90f };
	Vector4 startLine = (isStartMenuOpen_ || smHover) ? colHighlight_ : colBorderActive_;
	DrawPanel({ 8.0f, tbY + 3.0f }, { 104.0f, 26.0f }, startBg, startLine, 1.5f);
	tr->Print("HackGen", "[⯈ DAWN_OS]", 14.0f, tbY + 8.0f, 12.0f, colPrimary_);

	// 3. 全ウィンドウのタスクバータブ常時表示
	float tabX = 120.0f;
	for (auto& win : windows_) {
		float tabW = 142.0f;
		float tabH = 24.0f;
		float tabY = tbY + 4.0f;

		bool isAct = (activeWindowId_ == win.id && !win.isMinimized && win.isOpen);
		bool isMin = (win.isMinimized || !win.isOpen);
		bool isHov = (cursorPos_.x >= tabX && cursorPos_.x <= tabX + tabW && cursorPos_.y >= tabY && cursorPos_.y <= tabY + tabH);

		Vector4 tBg = isAct ? Vector4{ colBorderActive_.x * 0.20f, colBorderActive_.y * 0.20f, colBorderActive_.z * 0.20f, 0.95f }
		            : (isMin ? Vector4{ 0.04f, 0.08f, 0.05f, 0.85f }
		            : (isHov ? Vector4{ colBorderActive_.x * 0.12f, colBorderActive_.y * 0.12f, colBorderActive_.z * 0.12f, 0.85f } : Vector4{ 0.02f, 0.06f, 0.03f, 0.80f }));

		Vector4 tBorder = isAct ? colHighlight_
		                : (isMin ? Vector4{ 0.70f, 0.60f, 0.18f, 0.80f }
		                : (isHov ? colPrimary_ : colSecondary_));

		DrawPanel({ tabX, tabY }, { tabW, tabH }, tBg, tBorder, isAct ? 1.5f : 1.0f);
		if (isAct) {
			DrawRect({ tabX, tabY }, { tabW, 2.0f }, colHighlight_); // アクティブタブ上端ネオンライン
		}

		// ステータスLED（最小化中はアンバー点滅）
		bool amberBlink = (std::fmod(animTimer_, 0.6f) < 0.35f);
		Vector4 dotCol = isAct ? colLedActive_
		               : (isMin ? (amberBlink ? Vector4{ 1.0f, 0.80f, 0.20f, 1.0f } : Vector4{ 0.45f, 0.35f, 0.08f, 0.6f })
		                        : Vector4{ colSecondary_.x, colSecondary_.y, colSecondary_.z, 0.8f });
		DrawRect({ tabX + 7.0f, tabY + 8.0f }, { 6.0f, 6.0f }, dotCol);

		std::string tabLabel = win.title;
		if (tabLabel.length() > 14) {
			tabLabel = tabLabel.substr(0, 14) + "..";
		}
		if (isMin) {
			tabLabel = "[_MIN] " + tabLabel;
		}

		Vector4 textCol = isAct ? colHighlight_ : (isMin ? Vector4{ 0.95f, 0.85f, 0.35f, 0.90f } : (isHov ? colHighlight_ : colSecondary_));
		tr->Print("HackGen", tabLabel, tabX + 18.0f, tabY + 6.0f, 10.5f, textCol);

		tabX += tabW + 6.0f;
	}

	// 4. システムトレイ
	float cpuNoise = 14.0f + 5.0f * std::sin(animTimer_ * 1.5f) + 3.0f * std::sin(animTimer_ * 3.7f);
	float memNoise = 38.0f + 2.0f * std::sin(animTimer_ * 0.7f);
	char sysBuf[64];
	snprintf(sysBuf, sizeof(sysBuf), "CPU: %d%% | MEM: %d%%", static_cast<int>(cpuNoise), static_cast<int>(memNoise));
	tr->Print("HackGen", sysBuf, kScreenWidth - 290.0f, tbY + 9.0f, 11.5f, colSecondary_);

	bool netBlink = (std::fmod(animTimer_, 0.8f) < 0.65f);
	Vector4 netLedCol = netBlink ? colLedActive_ : Vector4{ colSecondary_.x * 0.4f, colSecondary_.y * 0.4f, colSecondary_.z * 0.4f, 0.40f };
	DrawRect({ kScreenWidth - 152.0f, tbY + 12.0f }, { 6.0f, 6.0f }, netLedCol);
	tr->Print("HackGen", "NET: 10G", kScreenWidth - 140.0f, tbY + 9.0f, 11.0f, colSecondary_);

	char timeBuf[32];
	int totalSec = static_cast<int>(animTimer_);
	int hrs = (totalSec / 3600) % 24;
	int mins = (totalSec / 60) % 60;
	int secs = totalSec % 60;
	snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d UTC", (16 + hrs) % 24, mins, secs);
	tr->Print("HackGen", timeBuf, kScreenWidth - 82.0f, tbY + 9.0f, 11.5f, colPrimary_);

	// 5. スタートメニューポップアップ
	if (isStartMenuOpen_) {
		const float smX = 8.0f;
		const float smY = tbY - 188.0f;
		const float smW = 250.0f;
		const float smH = 184.0f;

		DrawPanel({ smX, smY }, { smW, smH }, colPanelBg_, colBorderActive_, 1.5f);
		DrawRect({ smX, smY }, { smW, 26.0f }, Vector4{ colBorderActive_.x * 0.15f, colBorderActive_.y * 0.15f, colBorderActive_.z * 0.15f, 0.98f });
		tr->Print("HackGen", startMenuTitle_, smX + 10.0f, smY + 6.0f, 11.5f, colPrimary_);

		float itemY = smY + 34.0f;
		for (size_t i = 0; i < startMenuItems_.size(); ++i) {
			if (startMenuItems_[i].isSeparator) {
				DrawRect({ smX + 8.0f, itemY - 2.0f }, { smW - 16.0f, 1.0f }, colSecondary_);
				itemY += 6.0f;
			}
			bool itemHov = (cursorPos_.x >= smX && cursorPos_.x <= smX + smW &&
			                cursorPos_.y >= itemY - 4.0f && cursorPos_.y < itemY + 22.0f);
			if (itemHov) {
				DrawRect({ smX + 4.0f, itemY - 2.0f }, { smW - 8.0f, 22.0f }, Vector4{ colBorderActive_.x * 0.25f, colBorderActive_.y * 0.25f, colBorderActive_.z * 0.25f, 0.85f });
			}
			Vector4 itemCol = itemHov ? colHighlight_ : colPrimary_;
			tr->Print("HackGen", startMenuItems_[i].label, smX + 12.0f, itemY + 2.0f, 11.5f, itemCol);
			itemY += 26.0f;
		}
	}
}

void PseudoOS::DrawCustomCursor() {
	float mx = cursorPos_.x;
	float my = cursorPos_.y;
	TextRenderer* tr = TextRenderer::GetInstance();

	size_t curIdx = 0;
	auto drawCursorRect = [&](const Vector2& pos, const Vector2& size, const Vector4& color) {
		if (curIdx >= cursorSprites_.size()) { return; }
		auto& sp = cursorSprites_[curIdx++];
		sp->SetPosition(pos);
		sp->SetSize(size);
		sp->SetColor(color);
		sp->Update();
		sp->Draw();
	};

	// 1. クリック波紋エフェクト
	if (cursorRippleTimer_ > 0.0f) {
		float rp = 1.0f - (cursorRippleTimer_ / 0.30f);
		float rRad = 4.0f + 24.0f * rp;
		float rAlpha = (1.0f - rp) * 0.90f;
		Vector4 ringCol = { colPrimary_.x, colPrimary_.y, colPrimary_.z, rAlpha };
		drawCursorRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y - rRad }, { rRad * 2.0f, 1.5f }, ringCol);
		drawCursorRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y + rRad - 1.5f }, { rRad * 2.0f, 1.5f }, ringCol);
		drawCursorRect({ cursorRipplePos_.x - rRad, cursorRipplePos_.y - rRad }, { 1.5f, rRad * 2.0f }, ringCol);
		drawCursorRect({ cursorRipplePos_.x + rRad - 1.5f, cursorRipplePos_.y - rRad }, { 1.5f, rRad * 2.0f }, ringCol);
	}

	// 2. カスタムサイバーカーソル（テーマカラー適応）
	const Vector4 kBlackEdge = { 0.0f, 0.0f, 0.0f, 0.95f };
	const Vector4 kBody = colPrimary_;
	const Vector4 kCore = colHighlight_;

	// 黒縁
	drawCursorRect({ mx, my }, { 2.0f, 18.0f }, kBlackEdge);
	drawCursorRect({ mx + 1.0f, my + 1.0f }, { 13.0f, 2.0f }, kBlackEdge);
	drawCursorRect({ mx + 2.0f, my + 2.0f }, { 12.0f, 12.0f }, kBlackEdge);

	// 本体
	drawCursorRect({ mx + 1.0f, my + 1.0f }, { 2.0f, 15.0f }, kBody);
	drawCursorRect({ mx + 2.0f, my + 2.0f }, { 10.0f, 2.0f }, kBody);
	drawCursorRect({ mx + 3.0f, my + 3.0f }, { 8.0f, 8.0f }, kBody);
	drawCursorRect({ mx + 1.0f, my + 1.0f }, { 2.0f, 4.0f }, kCore);

	// 3. 先端照準レティクル
	Vector4 reticleCol = { colPrimary_.x, colPrimary_.y, colPrimary_.z, 0.85f };
	drawCursorRect({ mx - 7.0f, my }, { 5.0f, 1.0f }, reticleCol);
	drawCursorRect({ mx + 5.0f, my }, { 5.0f, 1.0f }, reticleCol);
	drawCursorRect({ mx, my - 7.0f }, { 1.0f, 5.0f }, reticleCol);
	drawCursorRect({ mx, my + 5.0f }, { 1.0f, 5.0f }, reticleCol);

	// 4. カーソル座標HUD
	if (mx >= 0.0f && mx <= kScreenWidth && my >= 0.0f && my <= kScreenHeight) {
		char locBuf[32];
		snprintf(locBuf, sizeof(locBuf), "[%d,%d]", static_cast<int>(mx), static_cast<int>(my));
		tr->Print("HackGen", locBuf, mx + 16.0f, my + 14.0f, 9.5f, colSecondary_);
	}
}

void PseudoOS::PlayWindowOpenAnimation(float duration) {
	windowAnimState_ = WindowAnimState::Opening;
	windowAnimTimer_ = 0.0f;
	windowAnimDuration_ = duration;
	for (auto& win : windows_) {
		if (win.isOpen && !win.isMinimized) {
			win.animScale = 0.0f;
		}
	}
}

void PseudoOS::PlayWindowCloseAnimation(float duration) {
	windowAnimState_ = WindowAnimState::Closing;
	windowAnimTimer_ = 0.0f;
	windowAnimDuration_ = duration;
}

void PseudoOS::ResetWindowPositions() {
	for (auto& win : windows_) {
		win.pos = win.normalPos;
		win.size = win.normalSize;
		win.isOpen = true;
		win.isMinimized = false;
		win.animScale = 1.0f;
	}
}

void PseudoOS::DrawCRTShutdown(float progress, float screenWidth, float screenHeight) {
	float p = Easing::Clamp01(progress);
	if (p <= 0.0f) return;

	// フェーズ1: 上下から黒帯が迫り、中央のスリットに画面が収束 (0.0〜0.35)
	if (p < 0.35f) {
		float phaseT = p / 0.35f;
		float curH = screenHeight * (1.0f - phaseT);
		float curY = (screenHeight - curH) * 0.5f;

		// 上黒帯
		DrawRect({ 0.0f, 0.0f }, { screenWidth, curY }, { 0.0f, 0.0f, 0.0f, 1.0f });
		// 下黒帯
		DrawRect({ 0.0f, curY + curH }, { screenWidth, screenHeight - (curY + curH) }, { 0.0f, 0.0f, 0.0f, 1.0f });
	} else {
		// フェーズ2: 画面全体が暗転し、中央の白い水平線が左右から縮小して点となって消える (0.35〜1.0)
		float phaseT = (p - 0.35f) / 0.65f;
		phaseT = (std::min)(1.0f, phaseT);

		// 全画面ブラックアウト
		DrawRect({ 0.0f, 0.0f }, { screenWidth, screenHeight }, { 0.0f, 0.0f, 0.0f, 1.0f });

		// 中央水平光線
		float curW = screenWidth * (1.0f - phaseT);
		float curX = (screenWidth - curW) * 0.5f;
		float lineH = (std::max)(1.0f, 4.0f * (1.0f - phaseT));
		float lineY = (screenHeight - lineH) * 0.5f;
		float alpha = 1.0f - phaseT;

		DrawRect({ curX, lineY }, { curW, lineH }, { 1.0f, 1.0f, 1.0f, alpha });
	}
}

void PseudoOS::DrawButtonFlash(const Vector2& pos, const Vector2& size, float timer, float duration, const Vector4& color) {
	if (timer <= 0.0f || duration <= 0.0f) return;
	float alpha = Easing::Clamp01(timer / duration);
	float expand = (1.0f - alpha) * 16.0f;
	Vector4 col = color;
	col.w *= alpha;
	DrawBorder({ pos.x - expand, pos.y - expand }, { size.x + expand * 2.0f, size.y + expand * 2.0f }, col, 2.5f);
}
