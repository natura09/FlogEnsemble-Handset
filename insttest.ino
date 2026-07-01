#include "Instfunc.h"
#include "config.h"

/*
 * InstClient 通信遅延テスト用スケッチ (音源入りファイル統合版)
 */

InstClass inst;
WiFiUDP ackUdp;
unsigned long bootAt = 0;
bool connectionReady = false;
unsigned long lastHelloMs = 0;

// ==========================================
// ★ Melody.ino 側の関数を外部参照(extern)
// ==========================================
extern void initTrumpet();
extern void initOrgan();
extern void startMelody();
extern void changeBpmByLevel(int level);
extern void updateMelodyLoop();
extern void stopMelody(); // ← 追加

bool parseCommandWithSeq(char *message, char *&command, unsigned int &seq) {
    char *sep = strrchr(message, '|');
    if (sep == nullptr || sep == message || sep[1] == 0) {
        return false;
    }
    *sep = 0;
    command = message;
    seq = (unsigned int)atoi(sep + 1);
    return seq > 0 && command[0] != 0;
}

void sendAck(const char *command, unsigned int seq) {
    IPAddress syncIP;
    syncIP.fromString(SERVER_IP);
    ackUdp.beginPacket(syncIP, ACK_PORT);
    ackUdp.print("A|i|");
    ackUdp.print(command);
    ackUdp.print("|");
    ackUdp.print(seq);
    ackUdp.endPacket();
}

void sendHello() {
    IPAddress syncIP;
    syncIP.fromString(SERVER_IP);
    ackUdp.beginPacket(syncIP, ACK_PORT);
    ackUdp.print("HELLO|");
    ackUdp.print(myname);
    ackUdp.endPacket();
}

void handleCommand(char *message) {
    if (message[0] == 0) return;

    // ★ WELCOME / READY（UDP登録用）
    if (strcmp(message, "WELCOME") == 0) {
        inst.isRegistered = true;
        Serial.println("Server registered this device (WELCOME).");
        return;
    }
    if (strcmp(message, "READY") == 0) {
        inst.isRegistered = true;
        inst.ready = 1;
        Serial.println("全員の準備が整いました (READY受信)");
        return;
    }

    // ★ START（演奏開始）
    if (strcmp(message, "START") == 0) {
        inst.ready = 2;
        Serial.println("START");           // ガイド指定のログ形式
        startMelody();
        return;
    }

    // ★ STOP（演奏停止）
    if (strcmp(message, "STOP") == 0) {
        inst.ready = 1;
        inst.currentLevel = DEFAULT_LEVEL;
        Serial.println("STOP");            // ガイド指定のログ形式
        // 必要なら演奏停止処理をここに追加
        return;
    }

    // ★ LEVEL:1〜3（速度変更）
    if (strncmp(message, "LEVEL:", 6) == 0 && message[6] != 0) {
        inst.currentLevel = atoi(message + 6);
        Serial.print("LEVEL:");
        Serial.println(inst.currentLevel);  // ガイド指定のログ形式
        changeBpmByLevel(inst.currentLevel);
        return;
    }

    if (strcmp(message, "STOP") == 0) {
    inst.ready = 1;
    inst.currentLevel = DEFAULT_LEVEL;
    Serial.println("STOP");
    stopMelody(); // ← これで isPlaying = false になる
    return;
    }
}

void setup() {
    inst.Starts();
    bootAt = millis();
    
    // ★音源（トランペット・オルガン）の初期化処理を呼び出し
    initTrumpet();
    initOrgan();
    
    Serial.println("InstClient: Delay Test Ready (With Melody)");
}

void loop() {
    inst.connection();

    char message[32];
    if (inst.recieveCommandFast(message, sizeof(message))) {
        Serial.print("[DEBUG] UDP受信: ");
        Serial.println(message);
        handleCommand(message);
    }

    // ★音源のエンベロープ状態更新とProcessingへのシリアル送信を毎ループ実行
    updateMelodyLoop();
    
    delay(10); // 元のMelody.inoのループ間隔・ウェイトを維持
}