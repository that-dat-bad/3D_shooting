#pragma once
#include <cstdint>

/// @brief ゲームのクリア結果データ。シーン間でstatic共有する。
struct GameResult {
	int enemiesDestroyed = 0;		///< 撃破した敵の数
	float clearTimeSeconds = 0.0f;	///< クリアまでの経過時間（秒）
	float playerHPRemaining = 0.0f;	///< 残りHP
	float playerMaxHP = 100.0f;		///< 最大HP
	bool missionCleared = false;	///< ミッションクリアしたか

	/// @brief 結果をリセットする
	void Reset() {
		enemiesDestroyed = 0;
		clearTimeSeconds = 0.0f;
		playerHPRemaining = 0.0f;
		playerMaxHP = 100.0f;
		missionCleared = false;
	}

	/// @brief シーン間の受け渡し用グローバルインスタンス
	static GameResult& GetInstance() {
		static GameResult instance;
		return instance;
	}
};
