#ifndef CONFIG_H
#define CONFIG_H

// ================= 共通設定（全スケッチで同一に保つこと） =================
#define WIFI_SSID "H1-SyncAP"
#define WIFI_PASS "sync2026"
#define BAUD 115200

#define SERVER_IP "192.168.4.1"
#define UDP_PORT 9001

// テンポ関連。DEFAULT_BPM は親機の拍クロックと全楽器の初期値で共通。
// ここが食い違うと輪唱の開始間隔と演奏テンポがズレる。
#define DEFAULT_BPM 90
#define MIN_BPM 40
#define MAX_BPM 240

// ================= 楽器共通設定（4楽器で同一に保つこと） =================
#define INITIAL_CONNECT_DELAY_MS 1500
#define CONNECTION_RETRY_DELAY_MS 250        // 未登録時の HELLO 再送間隔
#define REGISTERED_HELLO_RETRY_DELAY_MS 2000 // 登録後(READY待ち)の HELLO 再送間隔

// 親機はSTARTを連続送信してくるため、この時間内の重複STARTは無視する。
// この時間を過ぎた演奏中のSTARTは「親機の復旧再開始」なので受け入れる
#define START_DEDUP_MS 2000

// ================= この楽器の設定 =================
// トランペット（Processing: trumpet.pde と接続。giro/violinと同じ単独構成）
#define MYNAME "inst2"

#endif
