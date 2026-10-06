#include "UIPanel.h"
#include "../Sprite/SpriteCommon.h"
#include "../System/TextureManager.h"

void UIPanel::Initialize(SpriteCommon* spriteCommon) {
	spriteCommon_ = spriteCommon;

	// white1x1.png をパネル背景に使用（色はSetColorで制御）
	backgroundSprite_ = std::make_unique<Sprite>();
	backgroundSprite_->Initialize(spriteCommon_, "assets/textures/white1x1.png");
}

void UIPanel::Update() {
	if (!isVisible_ || !isActive_) return;

	UpdateAnimation(1.0f / 60.0f);

	backgroundSprite_->SetPosition(GetRenderPosition());
	backgroundSprite_->SetSize(GetRenderSize());
	backgroundSprite_->SetColor(GetRenderColor());
	backgroundSprite_->SetAnchorPoint(anchorPoint_);
	backgroundSprite_->Update();
}

void UIPanel::Draw() {
	if (!isVisible_) return;

	backgroundSprite_->Draw();
}
