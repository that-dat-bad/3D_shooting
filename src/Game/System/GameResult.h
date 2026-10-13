#pragma once
#include <cstdint>

enum class TitleEntryReason {
	InitialBoot,          ///< 通常の初回起動（ドローン視点）
	RebootFromGameOver,   ///< ゲームオーバーからの復旧（SYSTEM REBOOTING... → ConsoleBoot）
	ReturnFromClear       ///< クリアからの復帰（WindowClose → 再認証セキュリティ演出）
};

/// @brief ゲームのクリア結果データ。シーン間でstatic共有する。
struct GameResult {
	int enemiesDestroyed = 0;		///< 撃破した敵の数
	float clearTimeSeconds = 0.0f;	///< クリアまでの経過時間（秒）
	float playerHPRemaining = 0.0f;	///< 残りHP
	float playerMaxHP = 100.0f;		///< 最大HP
	bool missionCleared = false;	///< ミッションクリアしたか
	bool isRetry = false;           ///< リトライ出撃か（trueなら起動シーケンスを高速短縮）
	TitleEntryReason titleEntryReason = TitleEntryReason::InitialBoot; ///< タイトル画面への突入理由

	/// @brief 結果をリセットする
	void Reset() {
		enemiesDestroyed = 0;
		clearTimeSeconds = 0.0f;
		playerHPRemaining = 0.0f;
		playerMaxHP = 100.0f;
		missionCleared = false;
		isRetry = false;
		titleEntryReason = TitleEntryReason::InitialBoot;
	}

	/// @brief シーン間の受け渡し用グローバルインスタンス
	static GameResult& GetInstance() {
		static GameResult instance;
		return instance;
	}
};
