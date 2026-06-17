// Melody.ino (子機用・音楽演奏データおよびテンポ計算処理)

const float NOTE_C4 = 261.63; // ド
const float NOTE_D4 = 293.66; // レ
const float NOTE_E4 = 329.63; // ミ
const float NOTE_F4 = 349.23; // ファ
const float NOTE_G4 = 392.00; // ソ
const float NOTE_A4 = 440.00; // ラ
const float REST    = 0.0;    // 休符

// カエルの合唱の楽譜データ（例）
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
float currentNoteDurationMs = 0;   // 現在の音符の長さ(ミリ秒)
boolean isPlaying = false;

// 外部（Organ.ino / Trumpet.ino）の関数を使うための宣言
extern void trumpetPlay(float freq);
extern void organPlay(float freq);
extern void trumpetStop();
extern void organStop();

// 演奏途中でBPMが変わった際、現在鳴っている音符の残りの長さを破綻なく再計算する関数
void recalculateTempo() {
  if (!isPlaying || currentIndex == 0) return;
  
  // 現在鳴らしている音符（currentIndex - 1）の新しいBPM基準での総長さを計算
  int idx = currentIndex - 1;
  float newDurationMs = (60000.0 / bpm) * (4.0 / duration[idx]);
  
  // この音符が始まってからすでに経過した時間を割り出す
  unsigned long elapsed = millis() - noteStartMillis;
  
  // 新しいテンポ基準でタイムラインを上書き
  currentNoteDurationMs = newDurationMs;
  nextNoteTime = noteStartMillis + (unsigned long)currentNoteDurationMs;
  stopNoteTime = noteStartMillis + (unsigned long)(currentNoteDurationMs * 0.85f);
}

// 次の音符を再生する関数
void playNextNote() {
  if (currentIndex >= melodyLength) {
    trumpetStop();
    organStop();
    isPlaying = false; 
    return;            
  }
  
  float currentFreq = melody[currentIndex];
  noteStartMillis = millis(); // 発音開始時刻を記録
  
  // 音符の長さをミリ秒に変換 (4分音符なら 60000 / BPM)
  currentNoteDurationMs = (60000.0 / bpm) * (4.0 / duration[currentIndex]);
  nextNoteTime = noteStartMillis + (unsigned long)currentNoteDurationMs;
  
  // スタッカート（音符の長さの85%が過ぎたら消音フェーズに入る設定）
  stopNoteTime = noteStartMillis + (unsigned long)(currentNoteDurationMs * 0.85f);
  
  if (currentFreq != REST) {
    trumpetPlay(currentFreq);
    organPlay(currentFreq);
  } else {
    trumpetStop();
    organStop();
  }
  
  currentIndex++;
}

// 補助的な線形補間関数
float lerp(float start, float end, float amt) {
  return start + amt * (end - start);
}