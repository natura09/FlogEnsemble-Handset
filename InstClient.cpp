#include "InstClient.h"

InstClient::InstClient()
    : registered(false), ready(false), connectionReady(false),
      bootAt(0), lastHelloMs(0) {
}

void InstClient::begin() {
    Serial.begin(BAUD);
    // PC未接続（スタンドアロン運用）でも起動できるよう待ちは最大3秒
    unsigned long ts = millis();
    while (!Serial && millis() - ts < 3000);

    Serial.print(MYNAME);
    Serial.println(" 起動");

    Serial.print("親機APに接続中: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi接続成功");
    serverIP.fromString(SERVER_IP);

    udp.begin(UDP_PORT);
    Serial.println("UDPポート待機中");

    bootAt = millis();
}

bool InstClient::isReady() {
    return ready;
}

void InstClient::updateConnection() {
    if (ready) {
        return;
    }

    // 起動直後はAP側の準備を待つ
    if (!connectionReady) {
        if (millis() - bootAt < INITIAL_CONNECT_DELAY_MS) {
            return;
        }
        connectionReady = true;
    }

    unsigned long now = millis();
    unsigned long retryDelay = registered ?
        REGISTERED_HELLO_RETRY_DELAY_MS : CONNECTION_RETRY_DELAY_MS;
    if (now - lastHelloMs >= retryDelay) {
        sendHello();
        lastHelloMs = now;
    }

    char buf[64];
    receiveCommand(buf, sizeof(buf));  // WELCOME/READYは内部で処理される
}

void InstClient::sendHello() {
    udp.beginPacket(serverIP, UDP_PORT);
    udp.print("HELLO|");
    udp.print(MYNAME);
    udp.endPacket();

    if (!registered) {
        Serial.print("UDP HELLO送信: ");
        Serial.println(MYNAME);
    }
}

bool InstClient::receiveCommand(char* buffer, size_t bufferSize) {
    int packetSize = udp.parsePacket();
    if (!packetSize || bufferSize == 0) {
        return false;
    }

    int len = udp.read(buffer, bufferSize - 1);
    if (len <= 0) {
        buffer[0] = 0;
        return false;
    }
    buffer[len] = 0;

    // 末尾の改行・空白を落とす
    while (len > 0 &&
           (buffer[len - 1] == '\r' || buffer[len - 1] == '\n' ||
            buffer[len - 1] == ' ')) {
        buffer[--len] = 0;
    }
    if (len == 0) {
        return false;
    }

    // 登録応答はここで吸収する（親機はHELLO再送のたびに返してくる）
    if (strcmp(buffer, "WELCOME") == 0) {
        if (!registered) {
            registered = true;
            Serial.println("Server registered this device (WELCOME).");
        }
        return false;
    }
    if (strcmp(buffer, "READY") == 0) {
        registered = true;
        if (!ready) {
            ready = true;
            Serial.println("全員の準備が整いました (READY受信)");
        }
        return false;
    }

    return true;
}
