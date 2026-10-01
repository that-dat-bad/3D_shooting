# War Thunder風 ミッションエディタ 設計ドキュメント

> 作成日: 2026-09-24
> 対象プロジェクト: 25B_GE_CG（DirectX フライトゲームエンジン）

---

## 1. 現状分析

### 既存のミッションエディタ機能

現在 `StageScene::DrawMissionEditor()` に ImGui ベースの基本エディタが実装済み:

- **ミッションファイル管理**: JSON 保存/読み込み/新規作成 (`Resources/missions/`)
- **ミッション基本情報**: ミッション名・説明・プレイヤー初期HP・初期位置
- **空中敵の配置**: 位置・HP・AIタイプ(ChaseAttack/CruiseEvade)・モデルパス
- **地上目標の配置**: Turret(対空砲台) / Structure(施設) / PatrolVehicle(巡回車両)
- **地上目標の詳細設定**: 射程・旋回速度・弾速・バースト数・パトロール距離等
- **Apply & Restart**: 編集内容の即時反映

### 関連ファイル構成

| ファイル | 役割 |
|---|---|
| `src/Game/System/MissionManager.h/cpp` | ミッションデータの管理、JSON シリアライズ |
| `src/Game/Scene/StageScene.h/cpp` | ゲーム本編シーン、エディタUI描画 |
| `src/Game/Enemy/Enemy.h/cpp` | 空中敵クラス |
| `src/Game/Enemy/GroundEnemy.h/cpp` | 地上敵クラス |
| `src/Game/Enemy/EnemyManager.h/cpp` | 敵の一括管理 |
| `src/Game/Environment/EnvironmentManager.h/cpp` | 環境管理（太陽・時刻） |
| `src/Game/Environment/Sun.h/cpp` | 太陽光源 |
| `src/Game/FlightModel/AircraftConfig.h/cpp` | 機体設定ファイル(.cfg) |

### データフロー

```
MissionManager (JSON) → MissionData → StageScene::Initialize()
                                        ├── FlightModel (プレイヤー初期化)
                                        ├── EnemyManager (敵の一括生成)
                                        └── EnvironmentManager (環境初期化)
```

---

## 2. 拡張計画 全体像

```
Phase 1: ミッション目標 & トリガーシステム   ← 最重要
Phase 2: ウェイポイント & AI経路システム
Phase 3: 天候・時刻・環境設定
Phase 4: 2Dマップビュー（トップダウンエディタ）
```

各フェーズは独立して動作し、段階的に実装・テスト可能。

---

## 3. Phase 1: ミッション目標 & トリガーシステム

### 3.1 概要

War Thunder のミッションで最も重要な「勝利条件」「イベントトリガー」「動的敵出現」を実現する。
現状はハードコードされた「全敵殲滅 → クリア」「HP0 or 時間切れ → 失敗」のみ。
これを汎用的な目標・トリガー・アクション のフレームワークに置き換える。

### 3.2 データ構造

#### MissionObjective（ミッション目標）

```cpp
enum class ObjectiveType {
    DestroyAll,      // 全敵を殲滅
    DestroyCount,    // N体以上の敵を撃破
    DestroyTarget,   // 特定のユニットを撃破
    ReachArea,       // 指定エリアに到達
    Survive,         // 制限時間まで生存
    EscortProtect,   // 護衛対象の生存
};

enum class ObjectiveStatus {
    Inactive,   // 未開始
    Active,     // 進行中
    Completed,  // 達成
    Failed,     // 失敗
};

struct MissionObjective {
    std::string name;
    std::string description;
    ObjectiveType type;
    ObjectiveStatus status = ObjectiveStatus::Active;
    bool isPrimary = true;       // true: 主目標, false: 副目標
    bool isHidden = false;       // true: HUDに表示しない（隠し目標）

    // --- 各目標タイプのパラメータ ---
    int requiredKills = 0;       // DestroyCount用
    int targetIndex = -1;        // DestroyTarget / EscortProtect用
    Vector3 areaCenter;          // ReachArea用
    float areaRadius = 100.0f;   // ReachArea用
    float duration = 300.0f;     // Survive用 (秒)
    float minHP = 0.0f;          // EscortProtect用 (0以上で生存)
};
```

#### MissionTrigger（トリガー条件）

```cpp
enum class TriggerCondition {
    OnMissionStart,     // ミッション開始時
    OnTimer,            // 経過時間で発火
    OnAreaEnter,        // プレイヤーがエリアに進入
    OnAreaLeave,        // プレイヤーがエリアから離脱
    OnEnemyDestroyed,   // 特定の敵が撃破された
    OnKillCount,        // 撃破数が閾値に達した
    OnPlayerDamaged,    // プレイヤーのHPが閾値以下
    OnObjectiveComplete,// 特定の目標が達成された
};

enum class TriggerActionType {
    SpawnEnemies,       // 追加の敵を出現させる
    ShowMessage,        // 画面にメッセージ表示
    SetObjective,       // 新しい目標を追加/変更
    ChangeWeather,      // 天候を変化させる (Phase 3で実装)
    PlaySound,          // サウンド再生
    MissionComplete,    // ミッション成功
    MissionFail,        // ミッション失敗
    ActivateTrigger,    // 別のトリガーを有効化
};

struct TriggerAction {
    TriggerActionType type;
    // --- パラメータ (使用するフィールドはactionTypeで決まる) ---
    std::string message;
    float messageDuration = 3.0f;
    int objectiveIndex = -1;
    std::vector<EnemySpawnData> spawnEnemies;
    std::vector<GroundEnemySpawnData> spawnGroundEnemies;
    int activateTriggerIndex = -1;
};

struct MissionTrigger {
    std::string name;
    TriggerCondition condition;
    bool isEnabled = true;       // 有効/無効
    bool isFired = false;        // 発火済みフラグ
    bool isRepeatable = false;   // 繰り返し発火可能か

    // --- 条件パラメータ ---
    float timerSeconds = 0.0f;         // OnTimer用
    Vector3 areaCenter;                // OnAreaEnter/Leave用
    float areaRadius = 100.0f;         // OnAreaEnter/Leave用
    int enemyIndex = -1;               // OnEnemyDestroyed用
    int killCount = 0;                 // OnKillCount用
    float hpThreshold = 50.0f;         // OnPlayerDamaged用
    int objectiveIndex = -1;           // OnObjectiveComplete用

    // --- 発火時のアクション（複数可能） ---
    std::vector<TriggerAction> actions;
};
```

### 3.3 MissionData への統合

```cpp
struct MissionData {
    // --- 既存 ---
    std::string name;
    std::string description;
    float playerHP;
    Vector3 playerPosition;
    std::vector<EnemySpawnData> enemies;
    std::vector<GroundEnemySpawnData> groundEnemies;

    // --- Phase 1 追加 ---
    float timeLimit = 300.0f;   // ミッション制限時間（0 = 無制限）
    std::vector<MissionObjective> objectives;
    std::vector<MissionTrigger> triggers;
};
```

### 3.4 ランタイム評価ロジック

```
MissionManager::EvaluateObjectives(gameState) {
    for each objective:
        switch (objective.type):
            DestroyAll   → enemyManager.IsAllDestroyed()
            DestroyCount → enemyManager.GetDestroyedCount() >= requiredKills
            ReachArea    → distance(player, areaCenter) <= areaRadius
            Survive      → elapsedTime >= duration
            ...
        if all primary objectives completed → Mission Clear
        if any primary objective failed → Mission Fail
}

MissionManager::EvaluateTriggers(gameState) {
    for each trigger (enabled && !fired):
        if condition is met:
            for each action:
                execute action
            trigger.isFired = true
}
```

### 3.5 ImGui エディタUI

DrawMissionEditor() に以下のセクションを追加:

- **[Mission Objectives]** セクション
  - 目標のリスト表示（主目標/副目標で色分け）
  - 目標の追加・削除・編集
  - 各目標タイプに応じたパラメータ入力

- **[Mission Triggers]** セクション
  - トリガーのリスト表示
  - 条件の選択・パラメータ入力
  - アクションの追加・編集（複数アクション対応）

### 3.6 新規ファイル

| ファイル | 内容 |
|---|---|
| `src/Game/System/MissionObjective.h` | ObjectiveType, MissionObjective, JSON変換 |
| `src/Game/System/MissionTrigger.h` | TriggerCondition, TriggerAction, MissionTrigger, JSON変換 |

### 3.7 変更ファイル

| ファイル | 変更内容 |
|---|---|
| `src/Game/System/MissionManager.h` | MissionData に objectives/triggers 追加 |
| `src/Game/System/MissionManager.cpp` | Evaluate 関数追加 |
| `src/Game/Scene/StageScene.h` | 評価用メンバ変数追加 |
| `src/Game/Scene/StageScene.cpp` | 目標/トリガー評価のUpdate統合 + エディタUI拡張 |

---

## 4. Phase 2: ウェイポイント & AI経路システム

### 4.1 概要

敵AIに「ウェイポイント追従」モードを追加し、エディタ上で巡回ルートを設定可能にする。

### 4.2 データ構造

```cpp
enum class WaypointAction {
    Continue,     // 次のウェイポイントへ
    Orbit,        // この地点を旋回
    Attack,       // 攻撃モードに遷移
    Land,         // 着陸
    Patrol,       // パトロール（ループ）
};

struct Waypoint {
    Vector3 position;
    float speed = 200.0f;       // 目標速度 (km/h)
    float altitude = 100.0f;    // 目標高度 (m)
    WaypointAction action = WaypointAction::Continue;
};

struct WaypointPath {
    std::string name;
    std::vector<Waypoint> waypoints;
    bool isLoop = false;        // ループ巡回
};
```

### 4.3 敵AIへの統合

- `AIType` に `FollowWaypoint` を追加
- `EnemySpawnData` に `waypointPathIndex` を追加
- `Enemy::UpdateAI()` にウェイポイント追従ロジック追加
- 地上車両 (`PatrolVehicle`) にもウェイポイント経路を適用可能に

### 4.4 新規ファイル

| ファイル | 内容 |
|---|---|
| `src/Game/System/Waypoint.h` | Waypoint, WaypointPath, JSON変換 |

---

## 5. Phase 3: 天候・時刻・環境設定

### 5.1 概要

ミッションごとに天候・時刻・環境パラメータを設定可能にする。

### 5.2 データ構造

```cpp
enum class WeatherType {
    Clear,       // 晴天
    Cloudy,      // 曇り
    Overcast,    // 厚い雲
    Rain,        // 雨
    Storm,       // 嵐
    Fog,         // 霧
};

struct EnvironmentConfig {
    // 時刻
    float timeOfDay = 12.0f;        // 0.0〜24.0
    bool dynamicTimeEnabled = false;
    float timeScale = 1.0f;

    // 天候
    WeatherType weather = WeatherType::Clear;
    float cloudDensity = 0.3f;
    float cloudAltitude = 2000.0f;
    float visibility = 10000.0f;    // 視程 (m)

    // 風
    Vector3 windDirection = { 1.0f, 0.0f, 0.0f };
    float windSpeed = 0.0f;

    // マップ・スカイボックス
    std::string mapName = "default";
    std::string skyboxTexture = "cedar_bridge_sunset_1_2k.dds";
};
```

### 5.3 既存システムとの連携

- `EnvironmentManager::SetTimeOfDay()` に時刻設定を反映
- `Sun::SetDirection()` / `SetColor()` / `SetIntensity()` を時刻に連動
- フライトモデルに風の影響を追加 (横風・追い風)
- ポストエフェクトに視程制限 (フォグ) を追加

---

## 6. Phase 4: 2Dマップビュー（トップダウンエディタ）

### 6.1 概要

ImGuiの `ImDrawList` を使用し、ミッションの全体配置を俯瞰できる2Dマップビューを実装。

### 6.2 機能一覧

- **ユニットアイコン描画**: プレイヤー(三角)・空中敵(菱形)・地上目標(四角)
- **ドラッグ＆ドロップ**: マウスでユニットを直接移動
- **ウェイポイント経路描画**: 矢印付きの線でルートを可視化
- **トリガーエリア描画**: 半透明の円/矩形でエリアを表示
- **ズーム/パン**: マウスホイールでズーム、ドラッグでスクロール
- **グリッド表示**: 100m / 500m / 1km 単位の格子線
- **選択ハイライト**: 選択中のユニットをアウトライン強調
- **コンテキストメニュー**: 右クリックで追加/削除/プロパティ

### 6.3 新規ファイル

| ファイル | 内容 |
|---|---|
| `src/Game/System/MissionMapView.h` | マップビュークラス宣言 |
| `src/Game/System/MissionMapView.cpp` | ImDrawListベースの描画ロジック |

### 6.4 座標変換

```
ワールド座標 (X, Z) → マップ座標 (screenX, screenY)
  screenX = (worldX - cameraX) * zoom + canvasCenterX
  screenY = (worldZ - cameraZ) * zoom + canvasCenterY
  (Y軸は高度なので2Dマップでは省略、ツールチップで表示)
```

---

## 7. 実装チェックリスト

### Phase 1: ミッション目標 & トリガーシステム
- [x] `MissionObjective.h` — 目標データ型 + JSON変換 ✅
- [x] `MissionTrigger.h` — トリガー・アクションデータ型 + JSON変換 ✅
- [x] `MissionManager.h` — MissionData に objectives / triggers / timeLimit 追加 ✅
- [x] `MissionManager.cpp` — EvaluateObjectives() / EvaluateTriggers() 実装 ✅
- [x] `MissionManager.cpp` — CreateDefaultMission() にデフォルト目標追加 ✅
- [x] `StageScene.h` — ランタイム評価用メンバ変数追加 ✅
- [x] `StageScene.cpp` — Update() に目標・トリガー評価統合 ✅
- [x] `StageScene.cpp` — DrawMissionEditor() に目標・トリガー編集UI追加 ✅
- [x] `StageScene.cpp` — DrawMissionObjectivesHUD() / DrawMissionMessages() 追加 ✅
- [x] vcxproj / filters にファイル登録 ✅
- [x] コンパイル通過確認 ✅
- [ ] JSON 保存/読み込みテスト（実行時テスト）

### Phase 2: ウェイポイント & AI経路システム
- [ ] `Waypoint.h` — データ構造 + JSON変換
- [ ] `MissionData` に waypointPaths 追加
- [ ] `AIType::FollowWaypoint` 追加
- [ ] `Enemy::UpdateAI()` ウェイポイント追従ロジック
- [ ] 地上車両のウェイポイント対応
- [ ] エディタUI

### Phase 3: 天候・時刻・環境設定
- [ ] `EnvironmentConfig` データ構造 + JSON変換
- [ ] `MissionData` に environmentConfig 追加
- [ ] `EnvironmentManager` の拡張
- [ ] フライトモデルへの風影響追加
- [ ] エディタUI

### Phase 4: 2Dマップビュー
- [ ] `MissionMapView.h/cpp` — ImDrawList ベース描画
- [ ] ユニットアイコン描画
- [ ] ドラッグ＆ドロップ移動
- [ ] ウェイポイント経路 / トリガーエリア可視化
- [ ] ズーム・パン・グリッド
- [ ] コンテキストメニュー

---

## 8. 参考: War Thunder ミッションエディタの主要概念

| WT概念 | 本プロジェクトでの対応 |
|---|---|
| Mission Properties | MissionData (name, description, timeLimit) |
| Unit (air/ground) | EnemySpawnData / GroundEnemySpawnData |
| Waypoint | WaypointPath + Waypoint |
| Mission Objective | MissionObjective |
| Trigger | MissionTrigger + TriggerAction |
| Area (zone) | areaCenter + areaRadius (円形ゾーン) |
| Weather | EnvironmentConfig |
| Map | mapName + skyboxTexture |
