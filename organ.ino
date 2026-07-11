// --- 楽器: オルガン+トランペット (organ v2 / 旧insttest) ---
// ボード: Arduino UNO R4 WiFi → Processing(FlogEnsemble.pde)へシリアル出力
//
// 役割:
//   ・親機からUDPで START|BPM:n / STOP / BPM:n を受信して演奏する
//   ・50Hz(20ms周期)で "周波数,トランペット振幅,オルガン振幅,BPM" のCSVを
//     シリアル送信し、FlogEnsemble.pde が波形合成・再生する
//   ・停止時は "END,0.0,0.0,0" を送って待機表示に戻す
//
// シリアルからのテスト操作（PC単体での動作確認用）:
//   'S'      : 演奏開始
//   'b120\n' : BPMを120に直接設定

#include "config.h"
#include "InstClient.h"
#include "MelodyCore.h"
#include "Voices.h"

InstClient   inst;
MelodyCore   core;
TrumpetVoice trumpet;
OrganVoice   organ;

unsigned long lastStartMs  = 0;   // START重複判定用
unsigned long lastSerialMs = 0;   // 50Hz送信の管理

// 毎ステップの頭（休符は freq=0）
void onNote(float freq, int gateMs) {
  (void)gateMs;
  if (freq > 0.0f) {
    organ.play();
    trumpet.stopNote();   // トランペットは現行実装では未使用（オルガン単声）
  } else {
    organ.stopNote();
    trumpet.stopNote();
  }
}

// 音長の85%経過で消音（音の切れ目を作る）
void onGateOff() {
  organ.stopNote();
  trumpet.stopNote();
}

void startPlayback(int startBpm) {
  core.start(startBpm);
  Serial.println("START");
}

void stopPlayback() {
  core.stop();
  organ.stopNote();
  trumpet.stopNote();
  Serial.println("STOP");
  Serial.println("END,0.0,0.0,0");   // FlogEnsemble側を待機状態に戻す
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

// 50Hzで演奏状態をFlogEnsemble.pdeへ送る
void sendSerialStatus() {
  unsigned long now = millis();
  if (now - lastSerialMs < 20) {
    return;
  }
  lastSerialMs = now;

  if (core.playing) {
    Serial.print(core.currentFreq());
    Serial.print(",");
    Serial.print(trumpet.amp(), 4);
    Serial.print(",");
    Serial.print(organ.amp(), 4);
    Serial.print(",");
    Serial.println(core.bpm);
  } else {
    Serial.print("0.0,0.0,0.0,");
    Serial.println(core.bpm);
  }
}

void setup() {
  core.onNote    = onNote;
  core.onGateOff = onGateOff;

  inst.begin();
  Serial.println("organ (inst3) v2 ready");
}

void loop() {
  if (!inst.isReady()) {
    inst.updateConnection();
  } else {
    // 親機のUDP指令を処理
    char message[64];
    if (inst.receiveCommand(message, sizeof(message))) {
      handleCommand(message);
    }
  }

  // 演奏・エンベロープ・シリアル送信・テスト操作はREADY前でも動かす。
  // READY待ちで止めると、単体テスト（'S'やFlogEnsembleのクリック）が
  // 全員そろうまで一切効かなくなるため
  core.update(millis());
  trumpet.update();
  organ.update();
  sendSerialStatus();

  // シリアルからのテスト操作
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'S') {
      startPlayback(-1);
    } else if (c == 'b' || c == 'B') {
      String bpmStr = Serial.readStringUntil('\n');
      int bpmVal = bpmStr.toInt();
      if (bpmVal > 0) core.setBpm(bpmVal);
    }
  }
}
