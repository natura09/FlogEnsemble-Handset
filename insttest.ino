// Melody.ino (Arduino用音源モジュール - 修正版)

const float NOTE_C4 = 261.63; // ド
const float NOTE_D4 = 293.66; // レ
const float NOTE_E4 = 329.63; // ミ
const float NOTE_F4 = 349.23; // ファ
const float NOTE_G4 = 392.00; // ソ
const float NOTE_A4 = 440.00; // ラ
const float REST    = 0.0;    // 休符

const float melody[] = {
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_E4, NOTE_D4, NOTE_C4, REST,
  NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_G4, NOTE_F4, NOTE_E4, REST,
  NOTE_C4, REST, NOTE_C4, REST, NOTE_C4, REST, NOTE_C4, REST,
  NOTE_C4, NOTE_C4, NOTE_D4, NOTE_D4, NOTE_E4, NOTE_E4, NOTE_F4, NOTE_F4,
  NOTE_E4, NOTE_D4, NOTE_C4, REST              
};
const int melodyLength = sizeof(melody) / sizeof(melody[0]);

const int duration[] = {
  4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 4, 4,
  8, 8, 8, 8, 8, 8, 8, 8,
  4, 4, 4, 4
};
int bpm = 90; // 初期値は「普通(90)」
int currentIndex = 0; 
unsigned long nextNoteTime = 0; 
unsigned long stopNoteTime = 0;
unsigned long noteStartMillis = 0; // 現在の音符が鳴り始めた時刻を記録
float currentNoteDurationMs = 0;   // 現在の音符の総長さ(ms)
bool isPlaying = false;

extern void initTrumpet();
extern void initOrgan();
extern void trumpetPlay(float freq);
extern void trumpetStop();
extern void trumpetUpdate();
extern void organPlay(float freq);
extern void organStop();
extern void organUpdate();
extern float getTrumpetAmp();
extern float getOrganAmp();

float lerp(float start, float end, float amt) {
  return start + amt * (end - start);
}

// 演奏途中でBPMが変わった際、現在鳴っている音符の残りの長さを再計算する関数
void recalculateTempo() {
  if (!isPlaying || currentIndex == 0) return;
  int idx = currentIndex - 1;
  float newDurationMs = (60000.0 / bpm) * (4.0 / duration[idx]);
  
  unsigned long elapsed = millis() - noteStartMillis;
  currentNoteDurationMs = newDurationMs;
  nextNoteTime = noteStartMillis + (unsigned long)currentNoteDurationMs;
  stopNoteTime = noteStartMillis + (unsigned long)(currentNoteDurationMs * 0.85f);
}

void playNextNote() {
  if (currentIndex >= melodyLength) {
    trumpetStop();
    organStop();
    isPlaying = false; 
    return;
  }
  
  float currentFreq = melody[currentIndex];
  noteStartMillis = millis();
  
  currentNoteDurationMs = (60000.0 / bpm) * (4.0 / duration[currentIndex]);
  nextNoteTime = noteStartMillis + (unsigned long)currentNoteDurationMs;
  stopNoteTime = noteStartMillis + (unsigned long)(currentNoteDurationMs * 0.85f);
  if (currentFreq > 0) {
    trumpetPlay(currentFreq);
    organStop();
  } else {
    trumpetStop();
    organStop();
  }
  
  currentIndex++;
}

// ==========================================
// ★ 新しい通信コードと噛み合わせるための関数群
// ==========================================

// 外部から演奏を開始させる関数
void startMelody() {
  if (!isPlaying) {
    currentIndex = 0;
    isPlaying = true;
    nextNoteTime = millis();
    stopNoteTime = millis();
  }
}

// 外部から受信した速度レベル(1〜3)に応じてBPMを変更する関数
void changeBpmByLevel(int level) {
  if (level == 1)      bpm = 70;  // 遅い
  else if (level == 2) bpm = 90;  // 普通
  else if (level == 3) bpm = 110; // 早い
  recalculateTempo();
}

// メインのloopから毎サイクル呼び出され、音の更新とシリアル出力を担う関数
void updateMelodyLoop() {
  if (isPlaying) {
    trumpetUpdate();
    organUpdate();
    if (millis() > stopNoteTime) {
      trumpetStop();
      organStop();
    }
    
    if (millis() > nextNoteTime) {
      playNextNote();
    }

    if (!isPlaying) {
      Serial.println("END,0.0,0.0,0");
    } else {
      Serial.print(currentIndex > 0 ? melody[currentIndex - 1] : 0.0);
      Serial.print(",");
      Serial.print(getTrumpetAmp(), 4);
      Serial.print(",");
      Serial.print(getOrganAmp(), 4);
      Serial.print(",");
      Serial.println(bpm);
    }
  } else {
    // 待機中もProcessingへのシリアル送信を維持
    Serial.print("0.0,0.0,0.0,");
    Serial.println(bpm);
  }
}
