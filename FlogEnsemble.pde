// FrogEnsemble.pde (外部ライブラリ不要・Java標準UDP同期機能のみ搭載モデル)

import ddf.minim.*;
import ddf.minim.ugens.*;
import processing.serial.*;

// Java標準のネットワーク機能
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

Minim minim;
AudioOutput out;

// Java標準のUDP受信ソケットとスレッド制御
DatagramSocket socket;
Thread udpThread;

// 5重サイン波加算合成用（トランペット用）
Oscil tOsc1, tOsc2, tOsc3, tOsc4, tOsc5;
// 3重倍音合成用（オルガン用）
Oscil oOsc1, oOsc2, oOsc3;

Serial myPort; 
float serialFreq = 0.0f;
float serialTrumpetAmp = 0.0f;
float serialOrganAmp = 0.0f;
int serialBpm = 90; 

boolean isPlaying = false;
boolean isRunning = true; // UDPループ制御用

void setup() {
  size(500, 320); 
  pixelDensity(1); // 高解像度画面の座標ズレを修正します
  
  // シリアル通信初期化
  printArray(Serial.list());
  String portName = Serial.list()[2]; // 環境に合わせてインデックスを変更してください
  myPort = new Serial(this, portName, 115200);
  myPort.bufferUntil('\n'); 
  
  // 音響合成（Minim）初期化
  minim = new Minim(this);
  out = minim.getLineOut(Minim.STEREO, 1024);
  
  // オシレータの初期設定とパッチ接続
  tOsc1 = new Oscil(0, 0f, Waves.SINE); tOsc1.patch(out);
  tOsc2 = new Oscil(0, 0f, Waves.SINE); tOsc2.patch(out);
  tOsc3 = new Oscil(0, 0f, Waves.SINE); tOsc3.patch(out);
  tOsc4 = new Oscil(0, 0f, Waves.SINE); tOsc4.patch(out);
  tOsc5 = new Oscil(0, 0f, Waves.SINE); tOsc5.patch(out);
  
  oOsc1 = new Oscil(0, 0f, Waves.SINE); oOsc1.patch(out);
  oOsc2 = new Oscil(0, 0f, Waves.SINE); oOsc2.patch(out);
  oOsc3 = new Oscil(0, 0f, Waves.SINE); oOsc3.patch(out);

  // Java標準機能によるUDPポート「9001」の監視スレッド起動
  try {
    socket = new DatagramSocket(9001);
    udpThread = new Thread(new Runnable() {
      public void run() {
        byte[] buffer = new byte[1024];
        while (isRunning) {
          try {
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
            socket.receive(packet); // 親機からのUDPデータを待機（ブロッキング）
            
            String message = new String(packet.getData(), 0, packet.getLength());
            parseUdpMessage(message); // 受信文字列を解析
          } catch (Exception e) {
            // ソケットクローズ時の例外などは無視
          }
        }
      }
    });
    udpThread.start();
    println("● UDPポート 9001 で親機からの同期信号待機を開始しました。");
  } catch (Exception e) {
    println("❌ UDPソケットの初期化に失敗しました: " + e.getMessage());
  }
}

void draw() {
  background(30);
  fill(255);
  textSize(16);
  
  // 各オシレータへの周波数適用
  if (serialFreq > 0) {
    tOsc1.setFrequency(serialFreq * 1.0f);
    tOsc2.setFrequency(serialFreq * 2.0f);
    tOsc3.setFrequency(serialFreq * 3.0f);
    tOsc4.setFrequency(serialFreq * 4.0f);
    tOsc5.setFrequency(serialFreq * 5.0f);
    
    oOsc1.setFrequency(serialFreq * 1.0f);
    oOsc2.setFrequency(serialFreq * 0.5f);
    oOsc3.setFrequency(serialFreq * 2.0f);
  }
  
  // 各オシレータへの振幅（音量）エンベロープ適用
  tOsc1.setAmplitude(serialTrumpetAmp * 0.40f);
  tOsc2.setAmplitude(serialTrumpetAmp * 0.25f);
  tOsc3.setAmplitude(serialTrumpetAmp * 0.15f);
  tOsc4.setAmplitude(serialTrumpetAmp * 0.10f);
  tOsc5.setAmplitude(serialTrumpetAmp * 0.05f);
  
  oOsc1.setAmplitude(serialOrganAmp * 1.00f);
  oOsc2.setAmplitude(serialOrganAmp * 0.70f);
  oOsc3.setAmplitude(serialOrganAmp * 0.40f);

  // 画面上のUI状態テキスト
  if (!isPlaying) {
    text("Status: Sync Waiting...", 20, 35);
    text("Action: Waiting for Parent 'START' via UDP", 20, 65);
  } else {
    text("Status: Playing (Tempo Synchronized)", 20, 35);
    text("Current Freq: " + serialFreq + " Hz", 20, 65);
  }
  
  fill(0, 255, 255);
  text("BPM: " + serialBpm, 380, 35);
  
  textSize(12);
  fill(150);
  text("Network: Wi-Fi[H1-SyncAP]  Port[UDP 9001]", 20, 95);
  
  // リアルタイム波形描画
  stroke(0, 255, 0); 
  strokeWeight(2);
  for (int i = 0; i < out.bufferSize() - 1; i++) {
    float x1 = map(i, 0, out.bufferSize(), 0, width);
    float x2 = map(i+1, 0, out.bufferSize(), 0, width);
    float y1 = 220 + out.left.get(i) * 50;
    float y2 = 220 + out.left.get(i+1) * 50;
    line(x1, y1, x2, y2);
  }
}

// 届いたUDPパケットの内容に応じて処理を分岐
void parseUdpMessage(String message) {
  message = trim(message);
  
  // 1. 親機から "START" コマンドを受信
  if (message.equals("START")) {
    if (!isPlaying) {
      myPort.write('S'); // Arduinoへ演奏開始信号を送信
      isPlaying = true;
      println("▶ [UDP] STARTを受信。演奏を開始します。");
    }
  }
  
  // 2. 親機から テンポ変更コマンド "LEVEL:x" を受信
  else if (message.startsWith("LEVEL:")) {
    String[] parts = split(message, ':');
    if (parts.length == 2) {
      int level = int(parts[1]);
      if (level == 1) myPort.write('1'); // テンポ遅め (BPM 70)
      if (level == 2) myPort.write('2'); // テンポ普通 (BPM 90)
      if (level == 3) myPort.write('3'); // テンポ早め (BPM 110)
      println("⏱ [UDP] LEVEL:" + level + " を受信。テンポを同期しました。");
    }
  }
}

// PC単体での動作確認用（画面クリックでも開始可能）
void mousePressed() {
  if (!isPlaying) {
    myPort.write('S');
    isPlaying = true;
  }
}

// Arduinoからのリアルタイム楽譜データ受信
void serialEvent(Serial p) {
  String inString = p.readStringUntil('\n');
  if (inString != null) {
    inString = trim(inString);
    
    // 曲が最後まで終わった場合
    if (inString.startsWith("END")) {
      isPlaying = false;
      println("● 曲が正常に終了し、待機状態に戻りました。");
      return;
    }
    
    String[] list = split(inString, ',');
    if (list.length == 4) {
      serialFreq = float(list[0]);
      serialTrumpetAmp = float(list[1]);
      serialOrganAmp = float(list[2]);
      serialBpm = int(list[3]);
    }
  }
}

// 終了時のソケット・オーディオクローズ処理
void stop() {
  isRunning = false;
  if (socket != null) {
    socket.close();
  }
  out.close();
  minim.stop();
  super.stop();
}
