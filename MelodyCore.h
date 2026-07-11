#ifndef MELODY_CORE_H
#define MELODY_CORE_H

// ============================================================
// MelodyCore ── 全楽器共通の楽譜・拍グリッド・スケジューリング（v3）
//
//   ★このファイルは organ / violin / giro / servo / trumpet の5スケッチに
//     同一内容で置く共通ファイル。変更する時は5つ全部に反映すること。
//
//   v3の考え方（きれいな輪唱のための同期）:
//     ・親機が毎拍配る TICK（拍番号・BPM・その拍の親機時刻）に、
//       自分の「拍グリッド時計」を合わせ続ける（ドリフトを溜めない）。
//     ・発音時刻は millis() 自走ではなく「拍グリッド上の位置」から算出する。
//       全楽器が同じグリッドに乗るので、8拍ずれても音符の頭が揃う＝輪唱が濁らない。
//     ・STARTは即時（未来拍の予約はしない）。グリッド未確立で先着したら、
//       TICK到着まで保留し、最寄りの拍にスナップして開始する。
//     ・テンポはTICKで全楽器一斉に変わるので、加速しても輪唱が保たれる。
//     ・leadMs: サーボ等の機械遅延を補償する先行トリガ（グリッドは不変）。
// ============================================================

#include <Arduino.h>
#include "config.h"

// 拍グリッド位相補正の調整値（1回のTICKで詰める量／一気に合わせ直す閾値）
#ifndef TICK_SLEW_MS
#define TICK_SLEW_MS 3
#endif
#ifndef TICK_HARD_RESYNC_MS
#define TICK_HARD_RESYNC_MS 60
#endif

// ---- 楽譜（かえるの合唱・4分の4拍子・36音・合計32拍）----
// 8拍×4声部=32拍でちょうど1周が重なるので、輪唱の継ぎ目がきれいに閉じる。
static const float NOTE_C4 = 261.63f;  // ド
static const float NOTE_D4 = 293.66f;  // レ
static const float NOTE_E4 = 329.63f;  // ミ
static const float NOTE_F4 = 349.23f;  // ファ
static const float NOTE_G4 = 392.00f;  // ソ
static const float NOTE_A4 = 440.00f;  // ラ
static const float REST    = 0.0f;     // 休符

static const float MELODY[] = {
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_E4, NOTE_D4, NOTE_C4, REST,
  NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_G4, NOTE_F4, NOTE_E4, REST,
  NOTE_C4, REST,    NOTE_C4, REST,    NOTE_C4, REST,    NOTE_C4, REST,
  NOTE_C4, NOTE_C4, NOTE_D4, NOTE_D4, NOTE_E4, NOTE_E4, NOTE_F4, NOTE_F4,
  NOTE_E4, NOTE_D4, NOTE_C4, REST
};
static const int DURATION[] = {   // 4=4分音符, 8=8分音符
  4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 4, 4,
  8, 8, 8, 8, 8, 8, 8, 8,
  4, 4, 4, 4
};
static const int MELODY_LENGTH = (int)(sizeof(MELODY) / sizeof(MELODY[0]));

// 発音時間 = 音長 × GATE_RATIO（残りは音の切れ目）
static const float GATE_RATIO = 0.85f;

class MelodyCore {
public:
  // 毎ステップの頭で呼ばれる。休符では freq=0 で呼ばれる。
  // gateMs = 発音維持時間[ms]（音長×GATE_RATIO）
  void (*onNote)(float freq, int gateMs) = nullptr;

  // 発音中の音のゲート終了（音長の85%経過）で呼ばれる（不要なら未設定でよい）
  void (*onGateOff)() = nullptr;

  int  bpm     = DEFAULT_BPM;   // 表示・シリアル出力用（実テンポはグリッドの周期）
  bool playing = false;

  // 機械遅延の補償[ms]。onNote をこの時間だけ早く発火する（グリッドは不変）。サーボ用。
  unsigned long leadMs = 0;

  // ---- 親機からのTICK受信: 拍グリッドを合わせ続ける ----
  //   beatNo   : 拍番号（親機の通し番号。曲ごとに0から）
  //   tickBpm  : その拍のBPM（テンポはこれで全楽器一斉に変わる）
  //   hubMs    : その拍の親機時刻（本実装では未使用。到着時刻で合わせる）
  //   now      : このTICKを受け取ったローカル時刻 millis()
  void onTick(uint32_t beatNo, int tickBpm, unsigned long hubMs, unsigned long now) {
    (void)hubMs;
    float newPeriod = period_;
    if (tickBpm >= MIN_BPM && tickBpm <= MAX_BPM) {
      newPeriod = 60000.0f / (float)tickBpm;
      bpm = tickBpm;
    }

    if (!hasGrid_) {
      // 最初のTICK: その拍が「今」着いたとみなしてアンカーにする
      anchorBeat_   = beatNo;
      anchorLocalMs_ = now;
      anchorFrac_   = 0.0f;
      period_       = newPeriod;
      lastTickBeat_ = beatNo;
      hasGrid_      = true;
      if (pendingStart_) { pendingStart_ = false; applyStart(now); }
      return;
    }

    if ((int32_t)(beatNo - lastTickBeat_) <= 0) return;  // 古い/重複TICKは捨てる
    lastTickBeat_ = beatNo;

    // 今のグリッドで拍 beatNo の時刻を予測し、実際の到着時刻とのズレを測る
    float rel = (float)(int32_t)(beatNo - anchorBeat_) * period_ + anchorFrac_;
    int32_t relWhole = (int32_t)floorf(rel);
    float relFrac = rel - (float)relWhole;
    unsigned long predWhole = anchorLocalMs_ + (unsigned long)relWhole;
    float err = (float)(int32_t)(now - predWhole) - relFrac;

    if (err > (float)TICK_HARD_RESYNC_MS || err < -(float)TICK_HARD_RESYNC_MS) {
      // 大ズレ: 一気に合わせ直す
      anchorBeat_    = beatNo;
      anchorLocalMs_ = now;
      anchorFrac_    = 0.0f;
    } else {
      // 小ズレ: ±TICK_SLEW_MS だけなめらかに詰める
      float adj = err;
      if (adj >  (float)TICK_SLEW_MS) adj =  (float)TICK_SLEW_MS;
      if (adj < -(float)TICK_SLEW_MS) adj = -(float)TICK_SLEW_MS;
      float newRel = rel + adj;
      int32_t w = (int32_t)floorf(newRel);
      anchorBeat_    = beatNo;
      anchorLocalMs_ = anchorLocalMs_ + (unsigned long)w;
      anchorFrac_    = newRel - (float)w;
    }
    period_ = newPeriod;

    if (pendingStart_) { pendingStart_ = false; applyStart(now); }
  }

  // 演奏開始（即時）。startBpm>0 なら開始テンポの初期値として採用。
  // グリッド未確立なら、最初のTICKが来るまで保留する（予約ではなく待機）。
  void start(int startBpm) {
    if (startBpm >= MIN_BPM && startBpm <= MAX_BPM) {
      bpm = startBpm;
      if (!hasGrid_) period_ = 60000.0f / (float)startBpm;
    }
    if (!hasGrid_) {
      pendingStart_ = true;
      playing = false;
      return;
    }
    applyStart(millis());
  }

  void stop() {
    playing = false;
    if (gateOpen_) {
      gateOpen_ = false;
      if (onGateOff) onGateOff();
    }
    pendingStart_ = false;
    // グリッドを破棄: 次の曲は親機の拍0から新しく張り直す
    hasGrid_      = false;
    lastTickBeat_ = 0;
  }

  // テンポ直接設定（PC単体テストの 'b' キー用。通常はTICKでテンポが変わる）
  void setBpm(int newBpm) {
    if (newBpm < MIN_BPM) newBpm = MIN_BPM;
    if (newBpm > MAX_BPM) newBpm = MAX_BPM;
    bpm = newBpm;
    period_ = 60000.0f / (float)newBpm;
  }

  // 毎ループ呼ぶ
  void update(unsigned long now) {
    if (!playing || !hasGrid_) return;

    // ゲート終了（音の切れ目）
    if (gateOpen_ && (int32_t)(now - localMsOfBeat(gateOffBeat_)) >= 0) {
      gateOpen_ = false;
      if (onGateOff) onGateOff();
    }

    // 発音時刻（グリッド上の拍位置）が来たら次の音符を鳴らす。
    // leadMs はサーボの先行駆動ぶんだけ発火を早める（グリッド位置は不変）。
    unsigned long fireAt = localMsOfBeat(nextNoteBeat_) - leadMs;
    if ((int32_t)(now - fireAt) >= 0) {
      fireNextNote(now);
    }
  }

  // 今鳴っている（直前に発火した）音の周波数。休符・未演奏は0
  float currentFreq() const {
    if (!playing || lastIndex_ < 0) return 0.0f;
    return MELODY[lastIndex_];
  }

private:
  // ---- 拍グリッド時計（TICKで規律付け）----
  bool          hasGrid_      = false;
  uint32_t      anchorBeat_   = 0;      // アンカー拍番号
  unsigned long anchorLocalMs_ = 0;     // アンカー拍のローカル時刻（整数ms部）
  float         anchorFrac_   = 0.0f;   // 同・端数部
  float         period_       = 60000.0f / (float)DEFAULT_BPM;  // 1拍[ms]
  uint32_t      lastTickBeat_ = 0;

  // ---- 楽譜の進行 ----
  bool          pendingStart_ = false;  // グリッド待ちの開始保留
  int           index_        = 0;      // 次に鳴らす音符
  int           lastIndex_    = -1;     // 直前に鳴らした音符
  float         nextNoteBeat_ = 0.0f;   // 次の音符の絶対拍位置（グリッド上）
  float         gateOffBeat_  = 0.0f;   // 現在の音のゲート終了の絶対拍位置
  bool          gateOpen_     = false;

  // 拍位置（小数可）→ ローカル時刻[ms]
  unsigned long localMsOfBeat(float beat) const {
    float rel = (beat - (float)anchorBeat_) * period_ + anchorFrac_;
    return anchorLocalMs_ + (unsigned long)(long)lroundf(rel);
  }

  // ローカル時刻 → 拍位置（小数）
  float beatAt(unsigned long now) const {
    float d = (float)(int32_t)(now - anchorLocalMs_) - anchorFrac_;
    return (float)anchorBeat_ + d / period_;
  }

  // 即時開始をグリッドに合わせる: 最寄りの拍から楽譜を鳴らし始める
  void applyStart(unsigned long now) {
    float nowBeat = beatAt(now);
    uint32_t sb = (uint32_t)floorf(nowBeat + 0.5f);   // 最寄りの拍にスナップ
    index_        = 0;
    lastIndex_    = -1;
    nextNoteBeat_ = (float)sb;
    gateOpen_     = false;
    playing       = true;
  }

  void fireNextNote(unsigned long now) {
    int i = index_;
    float f = MELODY[i];
    float durBeats = 4.0f / (float)DURATION[i];   // 4分=1拍, 8分=0.5拍
    float onsetBeat = nextNoteBeat_;

    // 大幅に遅れている（ハードリシンク直後など）: 音は出さず位置だけ進める
    bool tooLate = (int32_t)(now - localMsOfBeat(onsetBeat)) > (int32_t)period_;

    gateOffBeat_ = onsetBeat + durBeats * GATE_RATIO;
    lastIndex_   = i;
    gateOpen_    = (!tooLate && f > 0.0f);

    if (!tooLate && onNote) {
      int gateMs = (int)(durBeats * period_ * GATE_RATIO);
      onNote(f, gateMs);
    }

    // 次の音符へ。曲末は先頭へ戻るが拍位置は進め続ける＝継ぎ目なく無限カノン
    nextNoteBeat_ = onsetBeat + durBeats;
    index_++;
    if (index_ >= MELODY_LENGTH) index_ = 0;
  }
};

// ---- 親機コマンドの解析ヘルパ ----

// "START" または "START|BPM:97" なら true
inline bool isStartCommand(const char* msg) {
  return strncmp(msg, "START", 5) == 0 && (msg[5] == 0 || msg[5] == '|');
}

// "START|BPM:97" から同梱BPMを取り出す。無ければ -1
inline int parseStartBpm(const char* msg) {
  const char* p = strchr(msg, '|');
  if (p == nullptr) return -1;
  if (strncmp(p + 1, "BPM:", 4) != 0) return -1;
  int v = atoi(p + 5);
  return (v > 0) ? v : -1;
}

// "TICK|拍番号|BPM|親機時刻" なら true
inline bool isTickCommand(const char* msg) {
  return strncmp(msg, "TICK|", 5) == 0;
}

// "TICK|<beat>|<bpm>|<hub>" を分解する。成功で true。
inline bool parseTick(const char* msg, uint32_t* beat, int* bpm, uint32_t* hub) {
  const char* p = msg + 5;                       // "TICK|" の後ろ
  char* end = nullptr;
  *beat = (uint32_t)strtoul(p, &end, 10);
  if (end == p || *end != '|') return false;
  p = end + 1;
  *bpm = (int)strtol(p, &end, 10);
  if (end == p || *end != '|') return false;
  p = end + 1;
  *hub = (uint32_t)strtoul(p, &end, 10);
  if (end == p) return false;
  return true;
}

#endif
