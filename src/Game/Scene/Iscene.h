#pragma once
#include"Input.h"

/// <summary>
/// シーンの種類を定義する列挙型
/// </summary>
enum SCENE {
	TITLE,
	STAGE,
	CLEAR,
	GAMEOVER,
	RESULT = GAMEOVER, // 互換性エイリアス
	DEBUG,
	MISSION_EDITOR,
};

/// <summary>
/// 全てのシーンの基底となるインターフェースクラス
/// </summary>
class IScene {
protected:
	static int sceneID;

public:
	/// <summary>初期化処理</summary>
	virtual void Initialize() = 0;
	/// <summary>毎フレームの更新処理</summary>
	virtual void Update() = 0;
	/// <summary>描画処理</summary>
	virtual void Draw() = 0;
	/// <summary>2D UI描画処理</summary>
	virtual void DrawUI() {}
	/// <summary>重いリソースの事前非同期/バックグラウンド生成（ゲーム状態はアクティブ化しない）</summary>
	virtual void PrewarmAssets() {}
	/// <summary>終了処理</summary>
	virtual void Finalize() = 0;
	
	virtual ~IScene();

	/// <summary>キー入力のトリガー判定（便利関数）</summary>
	static bool IsKeyTriggered(BYTE keyNumber);
	/// <summary>キー入力のプレス判定（便利関数）</summary>
	static bool IsKeyPressed(BYTE keyNumber);

	/// <summary>
	/// 次のシーンを設定する
	/// </summary>
	/// <param name="id">遷移先のシーンID</param>
	static void SetSceneID(int id);

	/// <summary>
	/// 現在のシーンIDを取得する
	/// </summary>
	int GetSceneID();
};
