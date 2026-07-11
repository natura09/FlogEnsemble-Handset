#ifndef INST_CLIENT_H
#define INST_CLIENT_H

// ============================================================
// InstClient ── 楽器側の通信処理（全楽器共通）
//
//   ★このファイルは organ / violin / giro / servo の4スケッチに
//     同一内容で置く共通ファイル。変更する時は4つ全部に反映すること。
//
//   ・親機(SyncMain)のAPへ接続し、"HELLO|<名前>" でUDP自己登録する
//   ・親機は WELCOME（登録受理）を返し、全員そろうと READY を返す
//   ・READY後は receiveCommand() で START/STOP/BPM を受信する
//     （WELCOME/READYの重複受信はここで吸収する）
// ============================================================

#include <Arduino.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "config.h"

class InstClient {
public:
    InstClient();

    // Serial・WiFi接続・UDP開始
    void begin();

    // READY（全員集合）を受信済みか
    bool isReady();

    // READY前に毎ループ呼ぶ。HELLO再送とWELCOME/READY待ちを行う
    void updateConnection();

    // 親機からの指令を受信する（非ブロッキング）。
    // 指令が入ったら true。WELCOME/READY はここで処理して false を返す
    bool receiveCommand(char* buffer, size_t bufferSize);

private:
    WiFiUDP udp;
    IPAddress serverIP;

    bool registered;
    bool ready;
    bool connectionReady;
    unsigned long bootAt;
    unsigned long lastHelloMs;

    void sendHello();
};

#endif
