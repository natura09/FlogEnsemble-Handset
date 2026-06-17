// Trumpet.ino (Arduino用トランペット状態計算)

static float trumpetTargetAmp = 0.0f;
static float trumpetCurrentAmp = 0.0f;
static float trumpetAttackFactor = 0.0f;

extern float lerp(float start, float end, float amt);

void initTrumpet() {
  trumpetTargetAmp = 0.0f;
  trumpetCurrentAmp = 0.0f;
  trumpetAttackFactor = 0.0f;
}

void trumpetPlay(float frequency) {
  // 核心ロジック：持続音量を抑えつつアタックFactorを4倍にする計算をマイコン側で処理
  trumpetTargetAmp = 0.15f;
  trumpetAttackFactor = 4.0f; 
}

void trumpetStop() {
  trumpetTargetAmp = 0.0f;
}

void trumpetUpdate() {
  if (trumpetTargetAmp > 0) {
    trumpetCurrentAmp = lerp(trumpetCurrentAmp, trumpetTargetAmp, 0.12f);
    trumpetAttackFactor = lerp(trumpetAttackFactor, 1.0f, 0.05f);
  } else {
    trumpetCurrentAmp = lerp(trumpetCurrentAmp, trumpetTargetAmp, 0.15f);
    trumpetAttackFactor = lerp(trumpetAttackFactor, 0.0f, 0.15f);
  }
}

float getTrumpetAmp() {
  return trumpetCurrentAmp * trumpetAttackFactor;
}