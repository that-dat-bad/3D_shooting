#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "../../engine/base/Math/Vector3.h"

enum class TitleEntryReason {
	InitialBoot,          // 通常の初回起動
	RebootFromGameOver,   // ゲームオーバーからの復旧
	ReturnFromClear       // クリアからの復帰
};

enum class GameOverReason {
	Destroyed,   // 撃墜・爆発
	TimeOver     // 時間切れ・作戦離脱
};

struct TrajectorySample {
	Vector3 pos{};
	Vector3 forward{ 0.0f, 0.0f, 1.0f };
	Vector3 up{ 0.0f, 1.0f, 0.0f };
	float timeSec = 0.0f;
};

// ゲームのクリア結果データ。シーン間でstatic共有する。
struct GameResult {
	int enemiesDestroyed = 0;		// 撃破した敵の数
	float clearTimeSeconds = 0.0f;	// クリアまでの経過時間（秒）
	float playerHPRemaining = 0.0f;	// 残りHP
	float playerMaxHP = 100.0f;		// 最大HP
	bool missionCleared = false;	// ミッションクリアしたか
	bool isRetry = false;           // リトライ出撃か
	bool isHalcyonDaysTutorial = false; // Operation Halcyon Days チュートリアルクリアか
	bool isClearAutoDemo = false;       // クリア画面自動デモ再生フラグ
	bool hideClearTerrain = false;      // クリア画面の背景地形を非表示にするフラグ
	GameOverReason gameOverReason = GameOverReason::Destroyed; // 失敗要因
	TitleEntryReason titleEntryReason = TitleEntryReason::InitialBoot; // タイトル画面への突入理由
	std::vector<TrajectorySample> flightTrajectory; // ステージ中のプレイヤー飛行軌跡
	std::string terrainPath = "Resources/terrains/demo2/chunks_info.json"; // 使用中の地形パス

	// 結果をリセットする
	void Reset() {
		enemiesDestroyed = 0;
		clearTimeSeconds = 0.0f;
		playerHPRemaining = 0.0f;
		playerMaxHP = 100.0f;
		missionCleared = false;
		isRetry = false;
		isHalcyonDaysTutorial = false;
		isClearAutoDemo = false;
		hideClearTerrain = false;
		gameOverReason = GameOverReason::Destroyed;
		titleEntryReason = TitleEntryReason::InitialBoot;
		flightTrajectory.clear();
	}

	// シーン間の受け渡し用グローバルインスタンス
	static GameResult& GetInstance() {
		static GameResult instance;
		return instance;
	}
};
