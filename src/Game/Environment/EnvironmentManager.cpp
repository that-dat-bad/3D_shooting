#include "EnvironmentManager.h"
#include <cmath>
#include <algorithm>

void EnvironmentManager::Initialize() {
	timeOfDay_ = 12.0f; // 昼間（12時）から開始
	sun_.Initialize();
}

void EnvironmentManager::Update(Camera* camera, float deltaTime) {
	// 時間の進行 (仮の昼夜サイクル: リアル時間1秒につきゲーム内0.01時間進む)
	timeOfDay_ += deltaTime * 0.01f;
	if (timeOfDay_ > 24.0f) {
		timeOfDay_ = 0.0f;
	}

	// 太陽の回転計算 (6:00日の出 〜 12:00南中 〜 18:00日の入り)
	float angle = ((timeOfDay_ - 6.0f) / 12.0f) * 3.14159265f;
	
	// 太陽光の進行方向ベクトルを計算
	MyMath::Vector3 dir;
	dir.x = std::cos(angle);
	dir.y = -std::sin(angle); // 昼間は下向き (太陽が空にある)
	dir.z = -0.4f;
	
	sun_.SetDirection(MyMath::Normalize(dir));

	// 太陽の高さ（地平線より上か下か）に応じて明るさと色を調整
	float sunHeight = -dir.y;
	if (sunHeight > 0.0f) {
		sun_.SetIntensity(std::clamp(sunHeight * 1.2f, 0.4f, 1.0f)); // 昼間はしっかり照らす

		if (sunHeight < 0.3f) {
			float lerpFactor = sunHeight / 0.3f;
			sun_.SetColor({ 1.0f, 0.7f + 0.25f * lerpFactor, 0.4f + 0.45f * lerpFactor, 1.0f });
		} else {
			sun_.SetColor({ 1.0f, 0.95f, 0.85f, 1.0f });
		}
	} else {
		// 夜間
		sun_.SetIntensity(0.0f);
	}

	// 太陽光設定の適用
	sun_.UpdateLight();

	// 太陽ブルーム（グレア）の動的適用
	sun_.UpdateBloom(camera);
}

void EnvironmentManager::Draw(Camera* camera) {
	// 太陽が地平線上にあるときのみ、太陽の見た目を描画する
	if (sun_.GetIntensity() > 0.0f) {
		sun_.Draw(camera);
	}
}
