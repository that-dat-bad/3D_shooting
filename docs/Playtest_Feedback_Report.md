# テストプレイ アンケート分析・反省・改善提案レポート

## 1. 総評（エグゼクティブサマリー）

テストプレイで得られた全14件のフィードバック（良かった点／改善・気になった点）を分析した結果、本作の**「核となる世界観」「マウスによる直感的なフライト体験」「段階的なチュートリアル導入」**はテスターから非常に高い評価を得ていることが確認できた。

特に、画面下部のガイド表示やストーリーと連動したチュートリアル構成（[TutorialMissionDirector.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Mission/TutorialMissionDirector.cpp)）、Gフォースによるレッドアウト／ブラックアウト演出（[FlightModel.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/FlightModel.cpp) / [StageScene_Gameplay.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Gameplay.cpp)）、および全体のUIデザイン統一感は強い魅力として機能している。

一方で、3D空間のフライトシューティング特有の**「ターゲット位置・距離感の把握」「空間内での自機位置の認識」「マウス大移動時の機体挙動とリアルさ・遊びやすさのバランス」「白文字や細かいUIの視認性」「敵のエリア外離脱による進行停滞」**といったプレイアビリティ面の課題が明確に浮かび上がった。また、会話スキップ機能や武装バリエーション・カスタマイズ要素など、快適性とリプレイ性を高める施策も求められている。

本レポートでは、アンケート原文の整理からコードベース上の要因分析、優先度付きの改善アクションプランまでをまとめる。

---

## 2. 回答データ一覧（原文整理）

> ※ 画像右端で文字が切れている回答については、`（※画像右端で途切れ）` として原文のまま記録し、誤字と思われる箇所には注記を付している。

| # | 良かった点（原文） | 改善・気になった点（原文） |
| :-: | :--- | :--- |
| 1 | 機体の基本動作が細かく設計されていたり、攻撃位置も分かりやすくなっていてとても良かった。 | 初めに別の方向などを向いてしまった際などに敵の位置が分かりずらく感じたので、目標を見るカメラ演（※画像右端で途切れ） |
| 2 | 下のウインドウで何をすればいいかを明記していてわかりやすい。 | 1つ1つクリアするのが遅いと敵がエリア外に行ってどうしようもない状態になることが多かった。 |
| 3 | チュートリアルが丁寧で分かりやすくてよかったです | 白い文字が空と被って少し見づらいなと思いました |
| 4 | 航空機を傾けて操作するシューティングゲームが楽しい。 | さらに色んな武装を使ってみたいなとかんじた。 |
| 5 | チュートリアルが序盤にあるので操作方法が分かりやすかったです | ストーリーなのか操作の話なのかが分かりずらいところがありました |
| 6 | ストーリーと段階に分けられた説明のチュートリアルがわかりやすかった。 | 旋回するときのブラーが見づらく、今現在どのあたりにいるのかの指標になるオブジェクトがなかったの（※画像右端で途切れ） |
| 7 | ゲームの導入部分で操作などもわかりやすく組まれていてよかった | マウスを大きく動かした際の戦闘機の挙動に関して1回転するのがやや難ありな感じがした |
| 8 | UIやチュートリアル演出の完成度が高くていいと思いました | 操作性が難しいのと、ターゲットの位置がわかりづらくてチュートリアルクリアに時間がかかりました |
| 9 | 世界観がしっかりと固まっているのは良いと思う。 | カスタムとかもなく何度も遊びたくなるような感じはなかった。 |
| 10 | マウスで機体を操作するため、直感的で遊びやすかったです。また、丁寧なチュートリアルが実装されて（※画像右端で途切れ） | チュートリアルの会話をスキップできても良いなと感じました。 |
| 11 | レッドアウト・ブラックアウトは斬新だった | 四店切り替えやターゲット注視があるといいですリアルさとゲーム性のバランスは調整した方がいい（※「四店」は「視点」の誤記と解釈） |
| 12 | ステージが広く自由に飛べてとても楽しかったです | タイトルのUIが細かすぎて少し見にくく感じました |
| 13 | UIが均等に世界観にあっているのがいいと思いました。 | UIがどの何を指しているのかわからないのが気になりました |
| 14 | 自然な導入でチュートリアルについてや目的についても説明できていた。操作感が難しい分なれてくると（※画像右端で途切れ） | 対象物が小さく狙いをつけづらく感じた。もう少し的となるものを大きくするか狙いをつけるための補助（※画像右端で途切れ） |

---

## 3. 良かった点の詳細分析（強みとして維持・伸ばすべき要素）

### 3.1 丁寧なチュートリアル設計と導入フロー（最多評価）
* **該当フィードバック**: #2, #3, #5, #6, #7, #8, #10, #14
* **評価ポイント**:
  * 序盤に丁寧なチュートリアルが組み込まれており、ストーリーの流れに沿って段階的に基本操作・目的を学べる構成が14名中8名以上から高く評価された。
  * 特に [TutorialMissionDirector.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Mission/TutorialMissionDirector.h) / [TutorialMissionDirector.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Mission/TutorialMissionDirector.cpp) における `HalcyonPhase`（基本操縦 → スロットル → 峡谷ゲート通過 → 監視所異変 → 偏差射撃ドローン → 西側機実戦 → 帰投境界）の段階設計と、`DrawTutorialGuideHUD` による画面最下部の「今何をすべきか＋進捗バー」の明示が非常に効果的であった。
  * また、偏差射撃のリードマーカー（未来位置予測の照準円）によって「攻撃位置がわかりやすい」という手応えも生んでいる。

### 3.2 フライト体験・マウス操作と独自演出
* **該当フィードバック**: #1, #4, #10, #11, #12, #14
* **評価ポイント**:
  * 機体を傾けて（バンク＆プル）旋回するフライトシューティングとしての気持ちよさ、マウスで直感的に機首を向けられる操作体系（[MouseAimController.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/MouseAimController.cpp) / [FlightInstructor.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/FlightInstructor.h)）、広い空を自由に飛べる開放感が高く評価された。
  * **レッドアウト・ブラックアウト演出**（[FlightModel.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/FlightModel.h) の `GetBlackoutFactor()` / `GetRedoutFactor()` と [StageScene_Gameplay.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Gameplay.cpp#L1314-L1327) のビネット表現）は「斬新」と強い印象を残しており、作品の個性として確立できている。
  * 「操作感が難しい分、慣れてくると楽しい」という声もあり、物理挙動の作り込み自体は好意的に受け止められている。

### 3.3 統一された世界観とUIデザイン
* **該当フィードバック**: #8, #9, #13
* **評価ポイント**:
  * レトロOS／ミリタリーターミナルを基調とした世界観（[TitleScene_Menu.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/TitleScene_Menu.cpp) / [TitleScene_OSDesktop.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/TitleScene_OSDesktop.cpp) / [StageHUD.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageHUD.cpp)）がゲーム全体で統一されており、「完成度が高い」「UIが世界観に合っている」と高く評価された。

---

## 4. 反省点・課題の深掘り分析（原因とプレイヤー体験への影響）

### 4.1 ターゲット把握と索敵・カメラの課題
* **該当フィードバック**: #1, #8, #11, #14
* **具体的な現象**:
  * ゲーム開始直後や旋回中に別の方向を向いてしまうと、敵・目標がどちらにいるのか瞬時に把握しづらい。
  * ターゲットの機体やドローンが小さく感じられ、距離がある状態で照準を合わせて撃ち落とすのに時間がかかる。
  * ターゲット注視カメラや視点切り替え（コックピット／後方視点など）が欲しかったという要望が出ている。
* **コードベース上の背景・要因**:
  * [PlayerCamera.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Camera/PlayerCamera.h) / [PlayerCamera.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Camera/PlayerCamera.cpp) には `DIK_C` キーによるフリービュー（`freeViewActive_`）は実装されているが、**選択中の敵／目標を自動で画面内に捉え続ける「ターゲット注視（ロックオンカメラ）」**や、**視点切り替えモード（FPS/TPS切り替え等）**が存在しない。
  * [StageScene_Gameplay.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Gameplay.cpp#L981-L995) の通常敵マーカー描画では `projectToScreen(...)`（画面内の場合のみ描画）が使われており、画面外の敵を端へクランプ表示する `StageHUD::ProjectToScreenClamped` が使われていない箇所がある。
  * [TutorialMissionDirector.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Mission/TutorialMissionDirector.cpp#L2598-L2635) では `ProjectToScreenClamped` を用いているものの、画面外クランプ時は小さな四角枠（`kTargetBoxHalfSize = 15.0f`）と距離数値のみが表示されるため、激しい空戦中に直感的な「矢印」として視認しづらい。

### 4.2 敵AIのエリア外離脱問題（進行不能・テンポ悪化）
* **該当フィードバック**: #2
* **具体的な現象**:
  * 「1つ1つクリアするのが遅いと敵がエリア外に行ってどうしようもない状態になることが多かった」という重大なストレス要因が報告されている。
* **コードベース上の背景・要因**:
  * [EnemyBehaviorNodes.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Enemy/BehaviorTree/EnemyBehaviorNodes.cpp) を確認すると、`RetreatAction::Tick` では `safeDistanceMeters_`（初期値 `1400.0f`）に達するまでプレイヤーから離れる方向（`awayVector`）へ `SetThrottleInput(1.0f)`（全力加速）で離脱を続けるが、**作戦空域（マップ中心や境界半径）を超えないための境界判定・反転帰還ロジックが存在しない**。
  * また、`WaitPatrolAction` でもウェイポイント未設定時はそのまま `cruiseDirection_` へ直進し続けるため、プレイヤーが別方向へ飛んだり撃破に時間がかかったりすると、敵機が遠方・エリア外へ飛び去ってしまうリスクが高い。

### 4.3 空間認識（自機位置・姿勢）と大入力時のロール挙動・ブラー
* **該当フィードバック**: #6, #7, #11
* **具体的な現象**:
  * 旋回時のブラーが見づらく、今自分が空間のどのあたりを飛んでいるのかの指標（ランドマーク・オブジェクト）が乏しい。
  * マウスを大きく動かして1回転（急旋回・反転・ループ等）しようとした際の戦闘機の挙動に引っかかりや違和感がある。
  * リアルな物理挙動ゆえに、初心者にはやや難しく感じられ、「リアルさとゲーム性のバランス」の調整が求められている。
* **コードベース上の背景・要因**:
  * **大入力時のロール挙動**: [MouseAimController.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/MouseAimController.cpp) では `UpdateTargetDirection` において 1フレームあたりの入力角度を `kMaxStepAngle = 0.85f` でクランプし、また `CalculateSteeringInput` において `phiTurn = std::atan2(localYaw, localPitch)` と水平復帰角 `phiLevel` を `wTurn`（`angleError` `0.04〜0.20 rad`）でブレンドし、`rollMultiplier = 1.0f + wTurn * 1.5f`（最大2.5倍）を掛けている。大きくマウスを振って真後ろや下方向（`localPitch < 0`）を指定した際、目標ロール角の急変や最短経路の反転（`±π` 跨ぎ）、および高速旋回中のピッチ・ロール連動によって「1回転する動きがぎこちない／制御しづらい」挙動になりやすい。
  * **ブラーと視覚的ノイズ**: [StageScene_Gameplay.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Gameplay.cpp#L1285-L1294) では時速500km/h超（`speed > 140.0f`）で常時 `PostEffectType::kRadialBlur` が適用されており、Gフォースのビネット演出と重なると旋回中の敵や地形の視認性が低下しやすい。
  * **空間指標の不足**: [StageScene_Rendering.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Rendering.cpp#L8-L70) の平坦地面では均一なグリッドが続くため、空中で方位や現在地を直感的に把握する目印（雲層、ランドマーク建築物、高度・方位を示すHUDピッチラダー／コンパスなど）が不足している。

### 4.4 UI・テキストの視認性と情報整理
* **該当フィードバック**: #3, #5, #12, #13
* **具体的な現象**:
  1. **白文字が背景（空・雲）と被って見づらい**
  2. **「ストーリーの会話」と「操作説明」の区別がつきづらい**
  3. **タイトル画面のUIが細かすぎて見にくい**
  4. **HUD・計器UIが「何の数値を指しているのか」わからない**
* **コードベース上の背景・要因**:
  * **白文字の視認性**: [RadioCommunicator.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/UI/RadioCommunicator.h#L100)（`kDialogueBodyColor = { 0.97f, 0.98f, 1.00f, 1.00f }`）および [InfoUI.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/UI/InfoUI.h#L33)（`kTextColor = { 0.97f, 0.98f, 1.00f, 1.00f }`）ともに、明るい白文字テキストをウィンドウ枠なしの背景上に2層ドロップシャドウのみで描画しているため、明るい空・白っぽいスカイボックスと重なるとコントラストが低下する。
  * **ストーリーと操作説明の混在**: [TutorialMissionDirector.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Mission/TutorialMissionDirector.h) では、世界観の会話も操作指示も同じ無線システム（[RadioCommunicator](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/UI/RadioCommunicator.h)）上で流れており、文字色や装飾が統一されているため「今は物語を聞く場面なのか、操作のルール説明なのか」を瞬時に読み分けにくい。
  * **計器UIの意味不明瞭さ**: [InfoUI.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/UI/InfoUI.h#L90-L109) で表示される `THR`, `SPD`, `ALT`, `RALT`, `AMMO` といった英語略称は、フライトシム未経験者には「どれが速度でどれが対地高度なのか」「画面上のどの数値が何を表しているのか」が直感的に伝わりにくい。
  * **タイトルUIの細かさ**: [TitleScene_Menu.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/TitleScene_Menu.cpp) や [TitleScene_OSDesktop.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/TitleScene_OSDesktop.cpp) のOS風ウィンドウ表現は雰囲気に優れる一方、14px前後の小フォントや密な装飾パネルが多く、初見プレイヤーには情報過多・文字が小さく感じられる箇所がある。

### 4.5 快適性（スキップ機能）とリプレイ性・武装バリエーション
* **該当フィードバック**: #4, #9, #10
* **具体的な現象**:
  * チュートリアルの会話をスキップできないため、リトライ時やテンポよく進めたいプレイヤーに待ち時間が発生する。
  * 武装が機銃中心で単調になりやすく、機体や兵装のカスタマイズ要素がないため「もう一度遊びたい」というリプレイ動機が弱い。
* **コードベース上の背景・要因**:
  * [RadioCommunicator.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/UI/RadioCommunicator.h#L177) には `SkipCurrent()` が用意されているものの、現状の呼び出し箇所は [StageScene_Gameplay.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Gameplay.cpp#L1115-L1117) の ImGui デバッグボタンのみであり、通常のゲームプレイ中のキー／クリック操作による会話スキップやチュートリアルスキップ導線が未開放である。
  * 武装システムは [IPayload.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/Payload/IPayload.h) として抽象化されているものの、実装クラスが [Gunpod.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/Payload/Gunpod.h)（ガンポッド）のみとなっており、ミサイル・ロケット・特殊兵装の切り替えや出撃前のカスタム画面が未実装。

---

## 5. 改善アクションプラン（優先度別・実装指針付き）

### 優先度：高（ゲーム成立性・ストレス解消に直結）

#### 1. 敵AIのエリア外離脱防止と復帰ロジックの追加
* **対象**: [EnemyBehaviorNodes.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Enemy/BehaviorTree/EnemyBehaviorNodes.cpp) / [Enemy.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Enemy/Enemy.cpp)
* **改善内容**:
  * ステージ中心からの最大許容半径（例：作戦空域半径 `R_max`）を定義し、敵が境界の一定割合（例：`0.85 * R_max`）を超えた場合は最優先でエリア中心へ旋回帰還する `ReturnToCombatArea` 判定をルートセレクタ最上位に組み込む。
  * `RetreatAction` 中であっても、「プレイヤーから離れすぎて一定距離（例：`1800m`）を超えたら再交戦へ切り替える」あるいは「プレイヤー側を軸に大きく周回する」ように変更し、延々と遠くへ逃げ続ける現象をなくす。
  * チュートリアルおよびミッション中は、プレイヤーが一定時間ターゲットに近づけない場合にガイド矢印または距離補正を行う仕組みを検討する。

#### 2. ターゲット注視カメラ＆画面外ターゲットインジケーターの強化
* **対象**: [PlayerCamera.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Camera/PlayerCamera.cpp) / [StageHUD.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageHUD.cpp) / [StageScene_Gameplay.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Gameplay.cpp)
* **改善内容**:
  * **ターゲット注視カメラ（例：右クリック長押し、または `F` / `Tab` 等）**: 現在のターゲット方向にカメラ視線を滑らかに向け（`PlayerCamera` で機体位置を画面下寄りに収めつつターゲット方向へ `LookAt` 補間する）、見失った敵をボタン1つで視認できるようにする。
  * **画面外矢印インジケーターの統一適用**: [StageScene_Gameplay.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Gameplay.cpp#L981) の敵マーカー描画にも `StageHUD::ProjectToScreenClamped` を適用し、さらに画面外にクランプされた際は**進行方向を指す三角形の矢印（Chevron）**＋大きく見やすい距離表示にする。

#### 3. 白文字UI・HUDの視認性改善（空との被り対策）
* **対象**: [RadioCommunicator.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/UI/RadioCommunicator.h) / [InfoUI.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/UI/InfoUI.h)
* **改善内容**:
  * 空や雲（高輝度背景）の上でも確実に判読できるよう、**半透明ダークプレート（角丸またはタクティカル枠、`alpha: 0.45〜0.65`）をテキスト背面に敷く**か、ドロップシャドウの不透明度・オフセット・アウトライン（4方向黒フチ）を強化する。
  * 無線字幕や計器テキストの色調も純白一辺倒ではなく、背景プレートとコントラストの取れるカラー設計に調整する。

#### 4. 射撃の当てやすさ補助（ヒット判定・エイムアシスト・偏差表示の視認性）
* **対象**: [TutorialMissionDirector.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Mission/TutorialMissionDirector.h) / [BulletManager.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Bullet/BulletManager.cpp) / [Enemy.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Enemy/Enemy.cpp)
* **改善内容**:
  * 対象（ドローンや敵機）の視覚スケールまたは弾丸のヒット判定半径をやや大きめに調整し、「当たっていそうなのに当たらない」ストレスを軽減する。
  * リードマーカー（予測射撃円）に照準が重なった際に色変化・ロックオン音のフィードバックを出し、引き金を引くタイミングを直感化する。
  * リアルさとゲーム性のバランス改善として、レティクル近傍でのわずかな弾道補正（ソフトエイムアシスト）の導入を検討する。

---

### 優先度：中（操作快適性・UX向上）

#### 5. 「ストーリー会話」と「操作指示」の視覚的分離 ＋ 会話スキップ実装
* **対象**: [RadioCommunicator.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/UI/RadioCommunicator.cpp) / [TutorialMissionDirector.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Mission/TutorialMissionDirector.cpp) / [StageScene_Gameplay.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Gameplay.cpp)
* **改善内容**:
  * **会話と操作説明の分離**: 無線セリフ（ストーリー・雰囲気）は上部中央の無線字幕のままにし、操作ルールやキー説明は下部ガイドウィンドウ（または専用の色分け・`[操作ガイド]` タグ・アイコン付きトースト）に限定して表示する。重要なキー名・操作ワードはハイライト色（黄色やシアン等）で強調する。
  * **スキップ機能の実装**: 既存の `RadioCommunicator::SkipCurrent()` をプレイヤー入力（例：`Space` / `Enter` 短押しで現在のセリフ送り、長押しでチュートリアルフェーズや導入会話の一括スキップ）に接続し、画面隅に `[SPACE] SKIP` ガイドを表示する。

#### 6. マウス大移動時のロール挙動調整 ＆ 旋回ブラー軽減
* **対象**: [MouseAimController.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/MouseAimController.cpp) / [StageScene_Gameplay.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Gameplay.cpp)
* **改善内容**:
  * `MouseAimController::CalculateSteeringInput` において、マウスを大きく動かした際（特に目標が機体背面側や真下にある状態）のロール目標角 `phiDesired` の変化に角速度リミッター／スムージングを入れ、急激なロール反転や1回転時の引っかかり感を緩和する。また、1回転（バレルロール／ループ）を意図した操作向けにキーボードでの補助ロール（`A`/`D` や `Q`/`E`）との併用時の競合を見直す。
  * [StageScene_Gameplay.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Gameplay.cpp#L1285-L1294) の `kRadialBlur` 強度を下げ、旋回中やハイスピード時でも視界中央のターゲットがボケて見づらくならないように中心マスクを強くする（または設定でオン／オフ可能にする）。

#### 7. 空間の指標オブジェクト追加 ＆ 計器UIのラベル明記
* **対象**: [EnvironmentManager.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Environment/EnvironmentManager.cpp) / [StageScene_Rendering.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageScene_Rendering.cpp) / [InfoUI.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/UI/InfoUI.cpp) / [StageHUD.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/StageHUD.cpp)
* **改善内容**:
  * **空間指標の強化**: 空域内に高度層ごとの雲・空域境界ビーコン・特徴的な地形ランドマークを配置するほか、HUD上に**コンパス（方位バー：N/E/S/W）**および**簡易ミニマップ／レーダー**を追加し、「自分が今どこにいてどちらを向いているか」を一目で把握できるようにする。
  * **UIの意味づけ**: [InfoUI](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/UI/InfoUI.h) の `THR` / `SPD` / `ALT` / `RALT` / `AMMO` に対し、日本語併記（例：`SPD 速度` / `ALT 高度` / `AMMO 残弾`）またはアイコン＋単位の整理を行い、どの数値が何を指しているか即座に理解できるようにする。

#### 8. タイトル画面UIのフォントサイズ・視認性見直し
* **対象**: [TitleScene_Menu.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/TitleScene_Menu.cpp) / [TitleScene_OSDesktop.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/TitleScene_OSDesktop.cpp)
* **改善内容**:
  * レトロOSの世界観は活かしつつ、主要メニュー項目・ミッション名・操作ガイドの文字サイズ（最低16〜20px以上）と行間を拡大し、装飾用のサブ情報と操作ボタンの視覚的優先度（階層）をはっきり分ける。

---

### 優先度：低〜中長期（やり込み・コンテンツ拡張）

#### 9. 視点切り替え機能（コックピット／後方近接／遠景）の追加
* **対象**: [PlayerCamera.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Camera/PlayerCamera.h) / [PlayerCamera.cpp](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Camera/PlayerCamera.cpp)
* **改善内容**:
  * `V` キー等で「三人称標準」「三人称（機首寄り／HUD重視）」「一人称（コックピット／機首カメラ）」をトグルできるようにする。一人称視点は照準が合わせやすくなるため、ターゲットの狙いづらさ解消にも直結する。

#### 10. 武装バリエーションと機体カスタム要素の実装
* **対象**: [IPayload.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/Payload/IPayload.h) / [AircraftConfig.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/AircraftConfig.h) / [TitleScene.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/Scene/TitleScene.h)
* **改善内容**:
  * 既存の `IPayload` 設計を活かし、ガンポッド（[Gunpod.h](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/Payload/Gunpod.h)）に加えて**誘導ミサイル（標準／マルチロック）**、**無誘導ロケット弾**、**高連射／高威力機関砲**などの複数兵装を追加する。
  * 出撃前画面で機体カラー・兵装ロードアウト・チューニング（旋回重視／速度重視など [AircraftConfig](file:///c:/Users/gamer/source/repos/25B_GE_CG/src/Game/FlightModel/AircraftConfig.h) パラメータの切り替え）やミッション評価ランク（S/A/B/C）を導入し、繰り返し遊びたくなるリプレイ性を構築する。
