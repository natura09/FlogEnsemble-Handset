// Organ.ino (Arduino用オルガン状態計算)

static float organTargetAmp = 0.0f;
static float organCurrentAmp = 0.0f;

extern float lerp(float start, float end, float amt);

void initOrgan() {
  organTargetAmp = 0.0f;
  organCurrentAmp = 0.0f;
}

void organPlay(float frequency) {
  organTargetAmp = 0.25f; 
}

void organStop() {
  organTargetAmp = 0.0f;
}

void organUpdate() {
  if (organTargetAmp > 0) {
    organCurrentAmp = lerp(organCurrentAmp, organTargetAmp, 0.20f);
  } else {
    organCurrentAmp = lerp(organCurrentAmp, organTargetAmp, 0.22f);
  }
}

float getOrganAmp() {
  return organCurrentAmp;
}