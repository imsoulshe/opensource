// ============================================================================
// Part 1 (Mega side): receive neck angle from the Nano, decide neckBad
// ----------------------------------------------------------------------------
// The MPU6050 is read on a separate Arduino Nano (../neck_nano/neck_nano.ino,
// written by 방성태). The Nano sends one line every 50 ms on its TX pin:
//     "A:12.3\n"  -> neck is 12.3 deg forward of the good-posture angle
//     "E\n"       -> Nano cannot talk to the MPU6050
// The Nano only measures; the Mega decides bad posture (mode limit + 3 s hold).
//
// Wiring: Nano TX(1) -> Mega RX1(19), Nano GND - Mega GND.
//
// Variables WRITTEN here: neckAngleDeg, neckBad
// Variables READ here   : useMode
// Constants used        : NECK_ANGLE_LIMIT_LAPTOP_DEG / NECK_ANGLE_LIMIT_BOOK_DEG,
//                         BAD_POSTURE_HOLD_MS, NECK_SERIAL_BAUD, NECK_DATA_TIMEOUT_MS
//
// If no valid data arrives for NECK_DATA_TIMEOUT_MS (Nano off, cable loose,
// or "E"), neckBad is forced to false so a broken sensor never blocks the
// focus timer. The status is printed once on Serial when it changes.
// ============================================================================

#include <stdlib.h>
#include "config.h"
#include "common.h"

#define NECK_SERIAL Serial1   // Mega hardware serial 1: RX1 = pin 19

static char          neckLineBuf[16];          // one incoming line, e.g. "A:-12.3"
static uint8_t       neckLineLen     = 0;
static unsigned long lastValidDataMs = 0;
static bool          neckOnline      = false;
static bool          neckBadTiming   = false;
static unsigned long neckBadStartMs  = 0;

// Parse one complete line. Returns true if it was a valid angle.
static bool parseNeckLine(const char *line) {
  if (line[0] == 'A' && line[1] == ':') {
    char *end;
    float angle = (float)strtod(line + 2, &end);
    if (end == line + 2) return false;   // "A:" with no number
    neckAngleDeg = angle;
    return true;
  }
  return false;                          // "E" or garbage => not valid
}

// Read all bytes that have arrived so far (never waits for more).
static void readNeckSerial(unsigned long now) {
  while (NECK_SERIAL.available() > 0) {
    char c = (char)NECK_SERIAL.read();

    if (c == '\r') continue;
    if (c == '\n') {
      neckLineBuf[neckLineLen] = '\0';
      if (parseNeckLine(neckLineBuf)) {
        lastValidDataMs = now;
      }
      neckLineLen = 0;
      continue;
    }
    if (neckLineLen < sizeof(neckLineBuf) - 1) {
      neckLineBuf[neckLineLen++] = c;
    } else {
      neckLineLen = 0;                   // line too long: drop it
    }
  }
}

void setupMpu() {
  NECK_SERIAL.begin(NECK_SERIAL_BAUD);
  neckAngleDeg    = 0.0f;
  neckBad         = false;
  neckOnline      = false;
  lastValidDataMs = millis();
}

void updateMpu() {
  unsigned long now = millis();
  readNeckSerial(now);

  // Online/offline check
  bool online = (now - lastValidDataMs < NECK_DATA_TIMEOUT_MS);
  if (online != neckOnline) {
    neckOnline = online;
    Serial.println(online ? F("[NECK] Nano data OK") : F("[NECK] no data from Nano"));
  }
  if (!online) {
    neckBad       = false;
    neckBadTiming = false;
    return;
  }

  // Limit depends on mode (looking down at a book is allowed more)
  float limitDeg = (useMode == MODE_BOOK) ? NECK_ANGLE_LIMIT_BOOK_DEG
                                          : NECK_ANGLE_LIMIT_LAPTOP_DEG;

  if (neckAngleDeg > limitDeg) {
    if (!neckBadTiming) {                // start timing when it first goes over
      neckBadTiming  = true;
      neckBadStartMs = now;
    }
    if (now - neckBadStartMs >= BAD_POSTURE_HOLD_MS) {
      neckBad = true;                    // over the limit for 3 s => bad
    }
  } else {
    neckBadTiming = false;               // back under the limit => good right away
    neckBad       = false;
  }
}
