#pragma once
#include "../base/Math/MyMath.h"
#include "UIAnimationCore.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cmath>

using namespace MyMath;

/// @brief 全てのUI要素（文字・図形パネル・画像・ボタン）の共通基底クラス。
///        アニメーション補間（Tween）、ループモーション（Pulse/Float/Shake）、
///        ラムダ式によるCustom条件判定を共通で提供する。
class UIElement {
public:
	virtual ~UIElement() = default;

	/// @brief 毎フレームの更新処理
	virtual void Update() = 0;

	/// @brief 描画処理
	virtual void Draw() = 0;

	// ========================================================
	// 共通トランスフォーム・プロパティ
	// ========================================================

	void SetPosition(const Vector2& pos) { position_ = pos; }
	Vector2 GetPosition() const { return position_; }

	void SetSize(const Vector2& size) { size_ = size; }
	Vector2 GetSize() const { return size_; }

	void SetAnchorPoint(const Vector2& anchor) { anchorPoint_ = anchor; }
	Vector2 GetAnchorPoint() const { return anchorPoint_; }

	void SetVisible(bool visible) { isVisible_ = visible; }
	bool IsVisible() const { return isVisible_; }

	void SetActive(bool active) { isActive_ = active; }
	bool IsActive() const { return isActive_; }

	void SetColor(const Vector4& color) {
		color_ = color;
		// Normalスタイルのカラーも同期
		styles_["Normal"].color = color;
		currentColor_ = color;
	}
	Vector4 GetColor() const { return color_; }

	// ========================================================
	// 状態（State）& 条件判定（Custom Lambda）
	// ========================================================

	/// @brief Custom条件式（ラムダ）を登録する
	/// @param stateName 条件を満たした時に遷移する状態名（例: "LowHP", "Warning", "Complete"）
	/// @param condition 判定ラムダ式（true を返すと該当状態に切り替わる）
	void SetCondition(const std::string& stateName, std::function<bool()> condition) {
		// 既存の同名条件があれば更新、なければ追加
		for (auto& pair : customConditions_) {
			if (pair.first == stateName) {
				pair.second = condition;
				return;
			}
		}
		customConditions_.emplace_back(stateName, condition);
		if (!styles_.contains(stateName)) {
			// デフォルトスタイルを作成
			styles_[stateName] = styles_["Normal"];
		}
	}

	/// @brief 全てのCustom条件をクリアする
	void ClearConditions() {
		customConditions_.clear();
	}

	/// @brief 強制的に現在の状態を設定する（"Normal", "Hovered", "Selected", "Pressed" など）
	void SetState(const std::string& stateName) {
		currentState_ = stateName;
	}
	const std::string& GetCurrentState() const { return currentState_; }

	// ========================================================
	// スタイル設定（各状態ごとの色・スケール・オフセット・モーション）
	// ========================================================

	void SetStyle(const std::string& stateName, const UIStateStyle& style) {
		styles_[stateName] = style;
	}

	UIStateStyle GetStyle(const std::string& stateName) const {
		auto it = styles_.find(stateName);
		if (it != styles_.end()) {
			return it->second;
		}
		// 見つからない場合は Normal をフォールバック
		auto itNorm = styles_.find("Normal");
		if (itNorm != styles_.end()) {
			return itNorm->second;
		}
		return UIStateStyle{};
	}

	std::unordered_map<std::string, UIStateStyle>& GetStyles() { return styles_; }
	const std::unordered_map<std::string, UIStateStyle>& GetStyles() const { return styles_; }

	// ========================================================
	// 描画用計算結果（アニメーション・モーション適用後）
	// ========================================================

	Vector2 GetRenderPosition() const {
		return { position_.x + currentOffset_.x + loopOffset_.x, position_.y + currentOffset_.y + loopOffset_.y };
	}

	float GetRenderScale() const {
		return currentScale_ * loopScale_;
	}

	Vector2 GetRenderSize() const {
		float scale = GetRenderScale();
		return { size_.x * scale, size_.y * scale };
	}

	Vector4 GetRenderColor() const {
		return { currentColor_.x, currentColor_.y, currentColor_.z, currentColor_.w * loopAlpha_ };
	}

	/// @brief プレビュー用の強制状態を設定（エディタ用）
	void SetPreviewStateOverride(const std::string& stateName) {
		previewStateOverride_ = stateName;
	}
	void ClearPreviewStateOverride() {
		previewStateOverride_.clear();
	}

protected:
	/// @brief 毎フレーム呼び出すアニメーション更新処理（派生クラスの Update() 内で実行）
	void UpdateAnimation(float dt = 1.0f / 60.0f) {
		animTimer_ += dt;

		// 1. 状態の自動判定（Custom条件ラムダ式を優先順に評価）
		if (!previewStateOverride_.empty()) {
			currentState_ = previewStateOverride_;
		} else {
			bool matchedCustom = false;
			for (const auto& [stateName, conditionFunc] : customConditions_) {
				if (conditionFunc && conditionFunc()) {
					currentState_ = stateName;
					matchedCustom = true;
					break;
				}
			}
			if (!matchedCustom) {
				// 汎用状態の維持（UIButton等で Hovered / Selected がセットされていない場合は Normal）
				if (currentState_ != "Hovered" && currentState_ != "Selected" && currentState_ != "Pressed") {
					currentState_ = "Normal";
				}
			}
		}

		// 2. 現在の目標スタイルを取得
		UIStateStyle targetStyle = GetStyle(currentState_);

		// 3. スムーズな遷移補間（Tween / Exponential Lerp）
		float t = 1.0f - std::exp(-transitionSpeed_ * dt);
		currentScale_ += (targetStyle.scale - currentScale_) * t;
		currentOffset_.x += (targetStyle.offset.x - currentOffset_.x) * t;
		currentOffset_.y += (targetStyle.offset.y - currentOffset_.y) * t;

		currentColor_.x += (targetStyle.color.x - currentColor_.x) * t;
		currentColor_.y += (targetStyle.color.y - currentColor_.y) * t;
		currentColor_.z += (targetStyle.color.z - currentColor_.z) * t;
		currentColor_.w += (targetStyle.color.w - currentColor_.w) * t;

		// 4. ループモーション計算
		loopOffset_ = { 0.0f, 0.0f };
		loopScale_ = 1.0f;
		loopAlpha_ = 1.0f;

		switch (targetStyle.loopMotion) {
		case UILoopMotion::Pulse: {
			float wave = std::sin(animTimer_ * targetStyle.motionSpeed * 3.14159265f);
			loopScale_ = 1.0f + wave * targetStyle.motionIntensity * 0.1f;
			loopAlpha_ = 1.0f + wave * targetStyle.motionIntensity * 0.1f;
			break;
		}
		case UILoopMotion::Floating: {
			float wave = std::sin(animTimer_ * targetStyle.motionSpeed * 3.14159265f);
			loopOffset_.y = wave * targetStyle.motionIntensity * 4.0f;
			break;
		}
		case UILoopMotion::Shake: {
			// 擬似乱数シェイク（時間ベース）
			float freq = targetStyle.motionSpeed * 20.0f;
			float r1 = std::sin(animTimer_ * freq) * std::cos(animTimer_ * freq * 1.3f);
			float r2 = std::cos(animTimer_ * freq * 1.7f) * std::sin(animTimer_ * freq * 0.9f);
			loopOffset_.x = r1 * targetStyle.motionIntensity * 2.0f;
			loopOffset_.y = r2 * targetStyle.motionIntensity * 2.0f;
			break;
		}
		case UILoopMotion::None:
		default:
			break;
		}
	}

protected:
	Vector2 position_ = { 0.0f, 0.0f };
	Vector2 size_ = { 100.0f, 50.0f };
	Vector2 anchorPoint_ = { 0.0f, 0.0f };
	bool isVisible_ = true;
	bool isActive_ = true;
	Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 状態・スタイル管理
	std::string currentState_ = "Normal";
	std::string previewStateOverride_ = "";
	std::unordered_map<std::string, UIStateStyle> styles_ = {
		{ "Normal",   UIStateStyle{ { 1.0f, 1.0f, 1.0f, 1.0f }, 1.00f, { 0.0f,  0.0f }, UILoopMotion::None, 3.0f, 1.0f } },
		{ "Hovered",  UIStateStyle{ { 1.0f, 0.9f, 0.4f, 1.0f }, 1.15f, { 0.0f, -4.0f }, UILoopMotion::Pulse, 3.0f, 0.5f } },
		{ "Selected", UIStateStyle{ { 0.9f, 0.95f, 1.0f, 1.0f }, 1.10f, { 0.0f, -2.0f }, UILoopMotion::Pulse, 2.5f, 0.4f } },
		{ "Pressed",  UIStateStyle{ { 0.8f, 0.8f, 0.8f, 1.0f }, 0.95f, { 0.0f,  2.0f }, UILoopMotion::None, 1.0f, 1.0f } },
	};

	// Custom条件判定ラムダリスト (順次評価)
	std::vector<std::pair<std::string, std::function<bool()>>> customConditions_;

	// 実行時アニメーション状態
	float animTimer_ = 0.0f;
	float transitionSpeed_ = 14.0f; // 状態遷移の補間速度

	float currentScale_ = 1.0f;
	Vector2 currentOffset_ = { 0.0f, 0.0f };
	Vector4 currentColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };

	Vector2 loopOffset_ = { 0.0f, 0.0f };
	float loopScale_ = 1.0f;
	float loopAlpha_ = 1.0f;
};
