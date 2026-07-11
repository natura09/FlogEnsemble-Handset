#ifndef VOICES_H
#define VOICES_H

// オルガン／トランペットの音量エンベロープ計算（organスケッチ専用）。
// 実際の波形合成はProcessing(FlogEnsemble.pde)側で行い、Arduinoは
// 「いまの音量(振幅)」だけを計算してシリアルで送る。

#include <Arduino.h>

inline float voiceLerp(float start, float end, float amt) {
  return start + amt * (end - start);
}

// トランペット：立ち上がりを強調するアタック係数付き。
// ※現行実装ではメロディに割り当てられておらず発音されない（オルガン単声）。
//   パート分けする場合は organ.ino の onNote から play() を呼ぶこと。
struct TrumpetVoice {
  float targetAmp  = 0.0f;
  float currentAmp = 0.0f;
  float attack     = 0.0f;

  void play() {
    targetAmp = 0.15f;
    attack    = 4.0f;
  }
  void stopNote() {
    targetAmp = 0.0f;
  }
  void update() {
    if (targetAmp > 0) {
      currentAmp = voiceLerp(currentAmp, targetAmp, 0.12f);
      attack     = voiceLerp(attack, 1.0f, 0.05f);
    } else {
      currentAmp = voiceLerp(currentAmp, targetAmp, 0.15f);
      attack     = voiceLerp(attack, 0.0f, 0.15f);
    }
  }
  float amp() const {
    return currentAmp * attack;
  }
};

// オルガン：素直な立ち上がり／減衰
struct OrganVoice {
  float targetAmp  = 0.0f;
  float currentAmp = 0.0f;

  void play() {
    targetAmp = 0.25f;
  }
  void stopNote() {
    targetAmp = 0.0f;
  }
  void update() {
    if (targetAmp > 0) {
      currentAmp = voiceLerp(currentAmp, targetAmp, 0.20f);
    } else {
      currentAmp = voiceLerp(currentAmp, targetAmp, 0.22f);
    }
  }
  float amp() const {
    return currentAmp;
  }
};

#endif
