import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;

Serial myPort;
Minim minim;
AudioOutput out;

// ※ giro.pde / violin.pde / trumpet.pde と同じ構成。
//   organ.ino から "周波数(Hz),発音時間(ms)" を1行ずつ受信し、
//   ここで音色生成・再生のみを行う（ビートパターンはArduino側が持つ）。
//
//   音色は旧FlogEnsemble.pde のオルガン部（3倍音の加算合成、
//   基音1.0倍・下オクターブ0.5倍・上5度2.0倍、重み 1.00/0.70/0.40）を
//   1音符ずつ鳴らす形に移植した。トランペットと違い、アタックは滑らかにしている。

void setup() {
  size(800, 400);
  minim = new Minim(this);
  out = minim.getLineOut(Minim.MONO, 1024);

  // --- シリアル通信の初期化(giro.pde / violin.pde / trumpet.pde と同じ方式) ---
  String portName = "/dev/cu.usbmodemXXXXXXXX"; // ★オルガン用Arduinoのポートに合わせる
  myPort = new Serial(this, portName, 115200);
  myPort.clear();
  myPort.bufferUntil('\n');

  println("=== Organ (Arduino-driven) ===");
}

void draw() {
  background(20, 25, 22); // 落ち着いた深緑(教会オルガンの木目イメージ)

  // --- 音の波形(時間領域)を描画 ---
  stroke(150, 200, 170); // オルガンパイプの銀green
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
  text("Instrument: Organ (Arduino-driven)", 20, 45);
}

// --- シリアル受信イベント ---
// Arduino(organ.ino)から "周波数,発音時間(ms)" を1行ずつ受け取って発音する。
// violin.pde / giro.pde / trumpet.pde の serialEvent と同じ作り。
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
        out.playNote(0, durationSec, new OrganInstrument(freq));
        println("Played: " + freq + " Hz, Dur: " + durationSec + "s");
      }
    }
  }
}

// ==========================================
// オルガン: 3倍音の加算合成 + 滑らかなエンベロープ
// 倍音構成は旧FlogEnsemble.pdeのオルガン部と同じ配分。
// ==========================================
class OrganInstrument implements Instrument {
  Summer mix;
  Oscil o1, o2, o3;
  ADSR env;

  OrganInstrument(float freq) {
    mix = new Summer();

    o1 = new Oscil(freq * 1.0f, 1.00f, Waves.SINE);   // 基音
    o2 = new Oscil(freq * 0.5f, 0.70f, Waves.SINE);   // 下オクターブ
    o3 = new Oscil(freq * 2.0f, 0.40f, Waves.SINE);   // 上5度(倍音)

    o1.patch(mix);
    o2.patch(mix);
    o3.patch(mix);

    // トランペットと違い、アタックを滑らかにして素直に立ち上がる
    env = new ADSR(0.5f, 0.08f, 0.15f, 0.85f, 0.20f);
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
