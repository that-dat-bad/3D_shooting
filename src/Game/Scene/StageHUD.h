#pragma once
#include <memory>
#include <string>
#include "../../engine/base/Math/MyMath.h"
#include "../../engine/Graphics/Sprite/Sprite.h"
#include "../../engine/Graphics/Sprite/SpriteCommon.h"

using namespace MyMath;

/// @brief StageSceneのHUD表示用データ構造体
struct StageHUDData {
	float remainingTime = 300.0f;
	int currentAmmo = 500;
	int maxAmmo = 500;
	bool isFiring = false;

	Vector3 aircraftPosition{};
	Vector3 aircraftForward{};
	Vector3 mouseAimTargetDirection{};
	bool mouseAimEnabled = true;

	Matrix4x4 viewProjectionMatrix{};
	float screenWidth = 1280.0f;
	float screenHeight = 720.0f;
};

/// @brief 戦闘ステージ用HUD管理・描画クラス
class StageHUD {
public:
	StageHUD() = default;
	~StageHUD() = default;

	/// @brief 初期化処理
	/// @param spriteCommon SpriteCommonポインタ
	void Initialize(SpriteCommon* spriteCommon);

	/// @brief 更新処理
	/// @param dt 経過時間（秒）
	/// @param data HUD描画に必要なゲームデータ
	void Update(float dt, const StageHUDData& data);

	/// @brief 2D UIパスでの描画処理
	void Draw();

private:
	// --- 2D描画ヘルパー ---
	void DrawLine(const Vector2& p1, const Vector2& p2, float thickness, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);
	void DrawRect(const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& anchor = { 0.0f, 0.0f }, BlendMode blendMode = BlendMode::kNormal);
	void DrawBorderedPanel(const Vector2& pos, const Vector2& size, const Vector4& bgColor, const Vector4& borderColor, float borderWidth = 1.5f);
	void DrawRing(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);
	void DrawDot(const Vector2& center, float radius, const Vector4& color, BlendMode blendMode = BlendMode::kNormal);

	// 3Dワールド座標から2Dスクリーン座標への投影
	bool ProjectToScreen(const Vector3& worldPos, const Matrix4x4& vpMat, float screenW, float screenH, Vector2& outScreen) const;

	// --- 各HUD要素の描画 ---
	void DrawTimer();
	void DrawAmmo();
	void DrawReticles();

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// 再利用スプライト
	std::unique_ptr<Sprite> rectSprite_;
	std::unique_ptr<Sprite> ringSprite_;
	std::unique_ptr<Sprite> dotSprite_;

	// キャッシュデータ
	StageHUDData hudData_{};
	float animTimer_ = 0.0f;
	float firePunchTimer_ = 0.0f;
};
