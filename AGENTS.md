# pianoTest - AGENTS.md

Codexへの永続的なコンテキスト。全タスクでこの内容を前提に作業すること。

---

## プロジェクト基本情報

- 言語：C++ / openFrameworks 0.12.1
- 環境：MSYS2 / MinGW64

パス（MSYS2内）：
~/openframeworks/of_v0.12.1_msys2_mingw64_release/apps/myApps/pianoTest

ビルド：
make -j4

起動：
./bin/pianoTest.exe

---

## アーキテクチャ方針（変更禁止）

- StepUIData と StepData は完全独立。同期させない
- Step.midi_notes に Group 変更を反映させない
- Sequencer.h の STEPS = 96 は変更禁止
- マルチスレッド安全性を保証（std::atomic 使用箇所を破壊しない）

---

## UI仕様（固定）

- 解像度：1920x1080 固定
- フォント：JetBrains Mono のみ（Regular, 16/14/12px）
- カラーパレット：
  colCyan_   = #6BE4FF
  colPink_   = #FF6B9D
  colYellow_ = #E1FF00
  colBg_     = #1A1A1A
- 色はすべて既存定数を使用。ハードコード禁止

---

## 現在の実装状態

### ステップグリッド（96ステップ）

- 16x6 配置、全ステップ シアン円で統一
- 1番・96番の黄色スマイルは削除済み
- 各ステップ円の下に個別メニュー用ハイフン（cy + r + 4px）
- グループタグ（Grp1 等）は元の位置のまま

### ステップ詳細パネル

- 左メニュー：PIANO ROLL / PARAMETER の独立トグル
- 左メニュー描画：drawMainMonitorChrome()
- ステップ詳細本体描画：drawPanel()
- 状態は panelShowPiano_ / panelShowParameter_ で保持
- 初期状態：両方ON、ステップ切り替え後も状態保持
- ミニピアノ：シアンのラインのみ、黒鍵盤は白鍵盤の60%高さ、左詰め
- ON/OFF/SKIP：元より10%小さい、右詰め配置
- パラメータバー7本：VEL/GAT/PRB/GRD/RPT/GLD/STR
- ロックアイコン：丸ではなく小さいロックアイコン
- STR に lockStrum 追加済み
- ALL LOCK に lockStrum を含む

### コード選択パネル

- CHORDテキスト部分のみクリック判定
- パネル内：. DELETE / . PANEL OFF / コード候補
- DELETE・PANEL OFF はピンク文字・背景なし
- 選択中コード：黄色文字・背景なし

### ボイスグループ / Sound Set

- Grp は再生側の単位、Sound Set は音色モジュールの単位
- Grp と Sound Set の番号が一致する必要はない
- ただし複数 Grp が同じ Sound Set を共有するのは禁止
- Sound Set 未設定時は、空いている番号を自然順で割り当てる
- Sound Set はクリック / ホイールで切り替え可能
- 使用中の Sound Set は切り替え候補から除外する
- StepUIData / StepData / Step.midi_notes には Sound Set 変更を同期しない

### Sound Set 音色仕様

- OSC1 / SUB1 / SUB2 / OSC2 を1ページ目に表示
- FILTER / ENV / LFO / REVERB を2ページ目に表示
- OSC1 の初期波形は SAW 100
- OSC1 LEVEL の初期値は 50
- OSC2 / SUB1 / SUB2 の初期 LEVEL は 0
- 全体 VOL の初期値は 50%
- SUB1 は OSC1 の音設定に追従する
- SUB2 は OSC2 の音設定に追従する
- 追従は StepData ではなく Sound Set の音色パラメーターとして扱う
- 波形値 SAW / SQR / TRI / SIN は合計最大 100
- 波形の実音ミックスは非ゼロ値を正規化して扱う
- 例：SAW 30、他0なら実音は SAW 100%
- 例：SAW 50、SQR 25なら残り25を TRI / SIN で分けられる

### 音声処理アーキテクチャ

- OscVoice は Sound Set のパラメーター保持用
- AudioVoiceState は Grp 再生 voice ごとのDSP状態保持用
- 周波数 / gate / ENV状態 / oscillator位相 / filter / LFO / reverb状態は Grp voice 側に置く
- audioOut では Grp voice が割り当て Sound Set のパラメーターを参照して鳴る
- Sound Set の共有禁止により、実質的に各 Grp が独立した音色モジュールを持つ

### REVERB

- 現状は maximilian の専用リバーブではなく自前リバーブ
- comb / allpass の内部バッファは AudioVoiceState 側に置く
- つまり reverb の残響状態は Grp voice ごとに独立
- room / damp / wet は Sound Set ごとのパラメーター
- stereo reverb として左右で異なる delay length を使う
- dry は中央、wet 成分で左右の広がりを作る

---

## StepUIData 追加済みメンバ

bool lockStrum = false;

---

## 既知のwarning（放置OK）

- unused variable 'globalStep'
- unused variable 'scy3'
- misleading-indentation
- maximilian addon 側の reorder warning

---

## 禁止事項

- 新規外部ライブラリ追加
- JetBrains Mono 以外のフォント
- 色のハードコード
- StepUIData / StepData の同期
- STEPS = 96 の変更
- 既存関数シグネチャ変更
- マルチスレッド安全性の破壊

---

## 完了条件（全タスク共通）

- make -j4 成功
- ./bin/pianoTest.exe 起動
- git diff で意図した変更のみ
- 新規依存関係なし
- 既存 warning 数が増えていない
