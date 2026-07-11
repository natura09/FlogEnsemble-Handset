import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;

Serial myPort;
Minim minim;
AudioOutput out;

// ※ giro.pde / violin.pde と同じ構成。
//   trumpet.ino から "周波数(Hz),発音時間(ms)" を1行ずつ受信し、
//   ここで音色生成・再生のみを行う（ビートパターンはArduino側が持つ）。
//
//   音色は旧FlogEnsemble.pde のトランペット部（5倍音の加算合成、
//   倍音ごとの重み 0.40/0.25/0.15/0.10/0.05）を1音符ずつ鳴らす形に移植した。

void setup() {
  size(800, 400);
  minim = new Minim(this);
  out = minim.getLineOut(Minim.MONO, 1024);

  // --- シリアル通信の初期化(giro.pde / violin.pde と同じ方式) ---
  String portName = "/dev/cu.usbmodemF412FA9C9E482"; // ★トランペット用Arduinoのポートに合わせる
  myPort = new Serial(this, portName, 115200);
  myPort.clear();
  myPort.bufferUntil('\n');

  println("=== Trumpet (Arduino-driven) ===");
}

void draw() {
  background(25, 20, 35); // 深い紫がかった背景(真鍮の対比)

  // --- 音の波形(時間領域)を描画 ---
  stroke(230, 190, 90); // 真鍮色
  strokeWeight(2);
  noFill();
  beginShape();
  for (int i = 0; i < out.bufferSize(); i++) {
    float x = map(i, 0, out.bufferSize(), 0, width);
    float y = height / 2 + out.mix.get(i) * (height / 2);
    vertex(x, y);
  }
  endShape();
  strokeWeight(1);

  fill(255);
  textSize(16);
  textAlign(LEFT, TOP);
  text("Song: Controlled by Arduino", 20, 20);
  text("Instrument: Trumpet (Arduino-driven)", 20, 45);
}

// --- シリアル受信イベント ---
// Arduino(trumpet.ino)から "周波数,発音時間(ms)" を1行ずつ受け取って発音する。
// violin.pde / giro.pde の serialEvent と同じ作り。
void serialEvent(Serial p) {
  String inString = p.readStringUntil('\n');
  if (inString != null) {
    inString = trim(inString);

    // ★デバッグ用：受信した生データをコンソールに表示
    println("受信データ: " + inString);

    String[] data = split(inString, ',');

    if (data.length >= 2) {
      float freq  = float(data[0]);
      float durMs = float(data[1]);

      if (freq > 0.0 && durMs > 0) {
        float durationSec = durMs / 1000.0;
        out.playNote(0, durationSec, new TrumpetInstrument(freq));
        println("Played: " + freq + " Hz, Dur: " + durationSec + "s");
      }
    }
  }
}

// ==========================================
// トランペット: 5倍音の加算合成 + アタック強調エンベロープ
// 倍音の重みは旧FlogEnsemble.pdeのトランペット部と同じ配分。
// ==========================================
class TrumpetInstrument implements Instrument {
  Summer mix;
  Oscil o1, o2, o3, o4, o5;
  ADSR env;

  TrumpetInstrument(float freq) {
    mix = new Summer();

    o1 = new Oscil(freq * 1.0f, 0.40f, Waves.SINE);
    o2 = new Oscil(freq * 2.0f, 0.25f, Waves.SINE);
    o3 = new Oscil(freq * 3.0f, 0.15f, Waves.SINE);
    o4 = new Oscil(freq * 4.0f, 0.10f, Waves.SINE);
    o5 = new Oscil(freq * 5.0f, 0.05f, Waves.SINE);

    o1.patch(mix);
    o2.patch(mix);
    o3.patch(mix);
    o4.patch(mix);
    o5.patch(mix);

    // アタックが鋭く立ち上がり、サステインを保つ金管らしいエンベロープ
    env = new ADSR(0.75f, 0.03f, 0.08f, 0.7f, 0.12f);
    mix.patch(env);
  }

  void noteOn(float duration) {
    env.noteOn();
    env.patch(out);
  }

  void noteOff() {
    env.noteOff();
    env.unpatchAfterRelease(out);
  }
}
