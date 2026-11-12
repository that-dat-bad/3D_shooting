#pragma once
#include "Sun.h"
#include "../../engine/Graphics/Camera/Camera.h"

class EnvironmentManager {
public:
	void Initialize();

	// 環境全体の更新（時間の進行、太陽の角度更新、ブルームの適用など）
	void Update(Camera* camera, float deltaTime);

	// 環境構成要素（太陽など）の描画
	void Draw(Camera* camera);

	// 太陽クラスの参照取得
	Sun& GetSun() { return sun_; }
	const Sun& GetSun() const { return sun_; }

	// 時間取得
	float GetTimeOfDay() const { return timeOfDay_; }
	void SetTimeOfDay(float time) { timeOfDay_ = time; }

private:
	Sun sun_;
	float timeOfDay_ = 0.0f; // 0.0 〜 24.0 の時間スケール (仮実装)
};
