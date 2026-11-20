// ============================================================
// Part 4: Load cells (HX711) - written by 황현지
// Seat detection from total weight + left/right lean detection.
// Library: "HX711 Arduino Library" (Bogdan Necula) from Library Manager.
//
// Integration changes (vs. the original file):
//   - File renamed from .c to .ino (HX711 is a C++ class; a .c file is
//     compiled as C and cannot use it).
//   - extern declarations replaced by #include "common.h".
//   - abs() -> fabs() for the float difference.
//   - Comments translated to English (same meaning).
// ============================================================

#include <HX711.h>
#include <math.h>
#include "config.h"
#include "common.h"

// Variables WRITTEN here: loadLeftG, loadRightG, isPersonPresent, leanBad

// HX711 objects
static HX711 scaleL;
static HX711 scaleR;

// Internal state (static so it does not clash with other files)
static unsigned long leanStartMs  = 0;      // time when leaning started
static bool          isLeanTiming = false;  // currently timing a lean?

// Exponential moving average (EMA) factor (0.0 ~ 1.0; smaller = smoother but slower)
static const float EMA_ALPHA = 0.3f;

void setupLoadcell() {
  scaleL.begin(PIN_LOADCELL_L_DT, PIN_LOADCELL_L_SCK);
  scaleR.begin(PIN_LOADCELL_R_DT, PIN_LOADCELL_R_SCK);

  scaleL.set_scale(LOADCELL_CAL_FACTOR_L);
  scaleR.set_scale(LOADCELL_CAL_FACTOR_R);

  // Zero at startup (the chair must be EMPTY when powering on).
  // tare() blocks briefly, which is OK because setup() runs only once.
  scaleL.tare();
  scaleR.tare();

  loadLeftG       = 0.0f;
  loadRightG      = 0.0f;
  isPersonPresent = false;
  leanBad         = false;
}

void updateLoadcell() {
  unsigned long now = millis();

  // 1. Non-blocking read: read once only when data is ready, then apply EMA
  if (scaleL.is_ready()) {
    float rawL = scaleL.get_units(1); // 1 = single read, no averaging wait
    if (rawL < 0.0f) rawL = 0.0f;     // ignore tiny negatives from zero drift
    loadLeftG = (EMA_ALPHA * rawL) + ((1.0f - EMA_ALPHA) * loadLeftG);
  }

  if (scaleR.is_ready()) {
    float rawR = scaleR.get_units(1);
    if (rawR < 0.0f) rawR = 0.0f;
    loadRightG = (EMA_ALPHA * rawR) + ((1.0f - EMA_ALPHA) * loadRightG);
  }

  // 2. Seated or not, from total weight
  float totalWeightG = loadLeftG + loadRightG;
  isPersonPresent = (totalWeightG >= PRESENCE_WEIGHT_MIN_G);

  // 3. Left/right lean (only checked while seated)
  if (!isPersonPresent) {
    leanBad      = false;
    isLeanTiming = false;
    leanStartMs  = 0;
    return;
  }

  float weightDiffG = fabs(loadLeftG - loadRightG);

  if (weightDiffG > LR_DIFF_LIMIT_G) {
    // Start timing the moment leaning is first detected
    if (!isLeanTiming) {
      isLeanTiming = true;
      leanStartMs  = now;
    }
    // Leaning for LEAN_HOLD_MS (3000 ms) or longer => bad posture
    if (now - leanStartMs >= LEAN_HOLD_MS) {
      leanBad = true;
    }
  } else {
    // Back to normal: reset timer and state immediately
    isLeanTiming = false;
    leanStartMs  = 0;
    leanBad      = false;
  }
}
