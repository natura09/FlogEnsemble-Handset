#include "Instfunc.h"
#include "config.h"

/*
 * InstClient: 音楽演奏ロジック（Melody/Organ/Trumpet）完全合体版 (修正版)
 */

InstClass inst;

// Melody.ino 等にある外部関数・変数をこのファイルでも使うための宣言(extern)
extern int bpm;
extern boolean isPlaying;
extern int currentIndex;
extern unsigned long nextNoteTime;
extern unsigned long noteStartMillis;
extern unsigned long stopNoteTime; // ★外部変数の参照を正しく修正
extern void initTrumpet();
extern void initOrgan();
extern void playNextNote();
extern void trumpetUpdate();
extern void organUpdate();
extern void trumpetStop();
extern void organStop();
extern void recalculateTempo();
extern float getTrumpetAmp();
extern float getOrganAmp();
extern const float melody[];
extern const int melodyLength;

// Processingへ状態を送るタイマー
unsigned long lastSerialTime = 0;
// 親機との時刻同期用
unsigned long syncTime = 0; 
// 内部状態管理
boolean musicInitialized = false;

void setup() {
    inst.Starts(); // シリアル、Wi-Fi、UDPの初期化
    
    Serial.println("=========================================");
    Serial.println("InstClient: Music & Sync Integrated Mode");
    Serial.println("Waiting for Connection & START signal...");
    Serial.println("=========================================");
}

void loop() {
    // ----------------------------------------------------
    // ステップ1: まずはTCP接続と親機への登録を行う
    // ----------------------------------------------------
    if (inst.ready < 1){
        inst.connection();   
        isPlaying = false;
        musicInitialized = false;
    } 
    
    // ----------------------------------------------------
    // ステップ2: 接続完了(ready=1)。親機からのUDP演奏開始指示(START)を待つ
    // ----------------------------------------------------
    else if (inst.ready == 1) {
        inst.recieveStart(); 
        
        // 親機からSTARTを受信して ready が 2 に変化した瞬間
        if (inst.ready == 2) {
            syncTime = millis();
            Serial.println("▶▶ 親機よりSTART信号を受信！演奏を開始します。");
            
            // 音楽データの初期化
            initTrumpet();
            initOrgan();
            currentIndex = 0;
            isPlaying = true;
            musicInitialized = true;
            playNextNote(); // 最初の音を鳴らす
        }
        
        // 待機中もProcessingへ「0」のデータを送り、BPM状態を伝える
        if (millis() - lastSerialTime >= 10) {
            lastSerialTime = millis();
            Serial.print("0.0,0.0,0.0,");
            Serial.println(bpm);
        }
    }
    
    // ----------------------------------------------------
    // ステップ3: 演奏中(ready=2)。親機からのテンポ監視と演奏処理
    // ----------------------------------------------------
    else {
        // ① 親機からのテンポ変更指示（LEVEL）をリアルタイムにチェック
        int level = inst.recieveLevel();
        if (level == 99) { // 時刻同期信号
            syncTime = millis();
            Serial.println(">>> SYNC Received. Local clock reset.");
        } 
        else if (level > 0) {
            // 受信したレベルに応じてBPMを書き換え、音符の長さを再計算
            int prevBpm = bpm;
            if (level == 1) bpm = 70;  // 遅い
            if (level == 2) bpm = 90;  // 普通
            if (level == 3) bpm = 110; // 速い
            
            if (bpm != prevBpm) {
                Serial.print("⏱ 親機指示によりBPM変更: "); 
                Serial.print(prevBpm); Serial.print(" -> "); Serial.println(bpm);
                recalculateTempo(); // Melody.inoの破綻なきテンポ再計算関数を呼び出す
            }
        }

        // ② あなたが作った本来のMelody.inoの演奏・ADSR更新メイン処理
        if (isPlaying) {
            trumpetUpdate();
            organUpdate();
            
            // 音符の切り替えタイミングの管理
            if (millis() >= nextNoteTime) {
                playNextNote();
            } else if (millis() >= stopNoteTime) { // ★記述ミスを修正
                // 音符の終了直前の消音処理
                float currentFreq = melody[currentIndex - 1];
                if (currentFreq != 0.0) {
                    trumpetStop();
                    organStop();
                }
            }
            
            // 曲が最後まで終わった時の判定
            if (!isPlaying) {
                Serial.println("END"); // Processingへ演奏終了(自動保存等)を通知
                inst.ready = 1;        // 次の親機の演奏開始指示を待つ状態(ready=1)に戻る
                musicInitialized = false;
            }
        }

        // ③ Processingへ楽器の「リアルタイム状態データ」をシリアル高速送信（10msごと）
        if (millis() - lastSerialTime >= 10) {
            lastSerialTime = millis();
            
            if (isPlaying && currentIndex > 0) {
                float currentFreq = melody[currentIndex - 1];
                if (millis() >= stopNoteTime) { // ★記述ミスを修正
                    currentFreq = 0.0; // 消音時間帯は周波数を0にして送る
                }
                Serial.print(currentFreq, 2);
                Serial.print(",");
                Serial.print(getTrumpetAmp(), 4);
                Serial.print(",");
                Serial.print(getOrganAmp(), 4);
                Serial.print(",");
                Serial.println(bpm);
            } else {
                // 演奏が完全に終わった時
                Serial.print("0.0,0.0,0.0,");
                Serial.println(bpm);
            }
        }
    }
}