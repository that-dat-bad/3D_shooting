#pragma once
// プレイヤー
class Player {
public:
	// 初期化
	void Initialize();

	// 更新
	void Update();

	// 描画
	void Draw();

	// 終了処理
	void Finalize();

	//--アクセッサ--
	//-getter-


	//-setter-

private:
	// ユーザー入力によるスロットル値
	float currentThrottle_; 
	// 現在の推力
	float currentThrust_;
	// 現在の質量
	float currentMass_;
	// 残燃料
	float currentFuel_; 
};
