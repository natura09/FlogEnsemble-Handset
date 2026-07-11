// --- 楽器: トランペット (trumpet v3 / giro・violinと同じ構成に書き直し) ---
// ボード: Arduino UNO R4 WiFi → Processing(trumpet.pde)へシリアル出力
//
// 役割:
//   ・親機からUDPで START|BPM:n / STOP / BPM:n を受信して演奏する
//   ・音符ごとに "周波数(Hz),発音時間(ms)" をシリアル送信し、
//     trumpet.pde が音色生成・再生する（休符は送らない）
//   ・オンボードLEDマトリクスの「ドットのカエル」を演奏と同時に動かす
//
// ★これまでの trumpet.ino は organ.ino と組んで FlogEnsemble.pde に
//   CSV(周波数,トランペット振幅,オルガン振幅,BPM)を50Hzで送る構成だったが、
//   giro.ino / violin.ino と同じ「単独のino + 単独のpde」構成に書き直した。
//   Voices.h(TrumpetVoice)はこの構成では使わない。

#include "config.h"
#include "InstClient.h"
#include "MelodyCore.h"
#include "FrogMatrix.h"

InstClient inst;
MelodyCore core;
FrogMatrix frog;

unsigned long lastStartMs = 0;   // START重複判定用

// 毎ステップの頭（休符は freq=0 なので送らない）
void onNote(float freq, int gateMs) {
  if (freq > 0.0f) {
    Serial.print(freq);
    Serial.print(",");
    Serial.println(gateMs);   // "音程(Hz),発音時間(ms)"
  }
}

void startPlayback(int startBpm) {
  core.start(startBpm);
  frog.start();
  Serial.println("START");
}

void stopPlayback() {
  core.stop();
  frog.stop();
  Serial.println("STOP");
}

void handleCommand(const char* msg) {
  if (msg[0] == 0) return;

  if (isTickCommand(msg)) {
    // 毎拍の同期パルス。拍グリッドを合わせ続ける（テンポもここで変わる）
    uint32_t beat, hub; int tbpm;
    if (parseTick(msg, &beat, &tbpm, &hub)) {
      core.onTick(beat, tbpm, hub, millis());
    }

  } else if (isStartCommand(msg)) {
    unsigned long now = millis();
    if (core.playing && now - lastStartMs < START_DEDUP_MS) {
      return;   // 親機の連続送信による重複
    }
    lastStartMs = now;
    startPlayback(parseStartBpm(msg));

  } else if (strcmp(msg, "STOP") == 0) {
    stopPlayback();
  }
}

void setup() {
  core.onNote = onNote;

  inst.begin();
  frog.begin();
  Serial.println("trumpet (inst2) v3 ready");
}

void loop() {
  if (!inst.isReady()) {
    inst.updateConnection();
    return;
  }

  char message[64];
  if (inst.receiveCommand(message, sizeof(message))) {
    handleCommand(message);
  }

  core.update(millis());
  frog.update();
}
