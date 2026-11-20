// ============================================================================
// smart_seat.ino - main sketch
// Defines the shared global variables (declared in common.h), holds the main
// logic (updatePostureState / updateSeatState) and calls every part's
// setupXXX() / updateXXX(). No delay() anywhere.
//
// Files in this sketch:
//   config.h          shared constants
//   common.h          shared enums + extern variables
//   fayoz_parts.ino   parts 2, 3, 6, 7, 8 (Fayoz)
//   loadcell.ino      part 4 (황현지)
//   oled_timer.ino    part 5
//   neck_receiver.ino part 1, Mega side (receives the angle from the Nano)
//
// Part 1 itself runs on a separate Arduino Nano: ../neck_nano/neck_nano.ino
// ============================================================================

#include <Wire.h>
#include "config.h"
#include "common.h"

// ---- Shared global variables (defined exactly once, here) ----
PostureState  postureState         = POSTURE_UNKNOWN;
SeatState     seatState            = SEAT_AVAILABLE;
UseMode       useMode              = MODE_LAPTOP;
bool          isPersonPresent      = false;
bool          isAway               = false;
float         neckAngleDeg         = 0.0;
bool          neckBad              = false;
bool          leanBad              = false;
float         loadLeftG            = 0.0;
float         loadRightG           = 0.0;
float         ultrasonicDistanceCm = 0.0;
bool          irDetected           = false;
unsigned long focusTimeSec         = 0;
unsigned long awayRemainSec        = 0;
unsigned long seatRemainSec        = 0;

// ---- Part functions (defined in the other .ino files) ----
void setupFayoz();      void updateFayoz();      void updateLed();   // 2,3,6,7,8
void setupLoadcell();   void updateLoadcell();                       // 4
void setupOledTimer();  void updateOledTimer();                      // 5
void setupMpu();       void updateMpu();                            // 1 (Mega side)

// ----------------------------------------------------------------------------
// SERIAL_TEST_MODE = 1 : test without the load cells / MPU connected.
//   Type in the Serial Monitor (115200 baud):
//     p = toggle person present   n = toggle neckBad   l = toggle leanBad
//   (load cells and the Nano link are skipped; buttons, LEDs, OLED and
//    the main logic still run for real)
// SERIAL_TEST_MODE = 0 : normal operation with real sensors.
// ----------------------------------------------------------------------------
#define SERIAL_TEST_MODE 0

// Print a status line every second (set to 0 to turn off)
#define DEBUG_STATUS_PRINT 1

// ============================================================================
// Main logic
// ============================================================================

// Combine part 1 (neck) and part 4 (presence/lean) into postureState.
// Only place that writes postureState.
void updatePostureState() {
  if (!isPersonPresent)  postureState = POSTURE_UNKNOWN;
  else if (neckBad)      postureState = POSTURE_BAD_NECK;
  else if (leanBad)      postureState = POSTURE_BAD_LEAN;
  else                   postureState = POSTURE_GOOD;
}

// Seat state machine. Only place that writes seatState.
//
// The away button TOGGLES isAway (part 8), so every change of isAway is
// treated as one press. Main never writes isAway (it belongs to part 8).
//
//   AVAILABLE -> OCCUPIED  : isPersonPresent
//   OCCUPIED  -> AWAY      : away button pressed
//   OCCUPIED  -> AVAILABLE : no person for AWAY_NO_PERSON_TIMEOUT_SEC
//   AWAY      -> OCCUPIED  : person sits down again (after having left),
//                            or away button pressed again (cancel)
//   AWAY      -> AVAILABLE : away time over (awayRemainSec == 0, from part 5)
//
// Why "after having left": the person presses the away button while still
// sitting, so isPersonPresent is still true for a moment. Without this check
// the seat would jump straight back to OCCUPIED.
static bool          prevIsAway       = false;
static bool          leftDuringAway   = false;  // has the person stood up since going away?
static unsigned long noPersonSinceMs  = 0;
static bool          noPersonTiming   = false;

void updateSeatState() {
  unsigned long now = millis();
  bool awayPressed = (isAway != prevIsAway);
  prevIsAway = isAway;

  switch (seatState) {
    case SEAT_AVAILABLE:
      if (isPersonPresent) {
        seatState = SEAT_OCCUPIED;
        noPersonTiming = false;
      }
      break;

    case SEAT_OCCUPIED:
      if (awayPressed) {
        seatState = SEAT_AWAY;
        leftDuringAway = false;
        noPersonTiming = false;
        break;
      }
      if (isPersonPresent) {
        noPersonTiming = false;
      } else {
        if (!noPersonTiming) {
          noPersonTiming  = true;
          noPersonSinceMs = now;
        }
        if (now - noPersonSinceMs >= AWAY_NO_PERSON_TIMEOUT_SEC * 1000UL) {
          seatState = SEAT_AVAILABLE;
          noPersonTiming = false;
        }
      }
      break;

    case SEAT_AWAY:
      if (!isPersonPresent) leftDuringAway = true;

      if (awayPressed || (leftDuringAway && isPersonPresent)) {
        seatState = SEAT_OCCUPIED;       // came back / cancelled away
      } else if (awayRemainSec == 0) {
        seatState = SEAT_AVAILABLE;      // did not come back in time
      }
      break;
  }
}

// ============================================================================
#if SERIAL_TEST_MODE
void updateTestInput() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case 'p': isPersonPresent = !isPersonPresent; break;
    case 'n': neckBad = !neckBad;                 break;
    case 'l': leanBad = !leanBad;                 break;
    default: return;   // ignore newlines etc.
  }
  Serial.print(F("[TEST] present=")); Serial.print(isPersonPresent);
  Serial.print(F(" neckBad="));      Serial.print(neckBad);
  Serial.print(F(" leanBad="));      Serial.println(leanBad);
}
#endif

#if DEBUG_STATUS_PRINT
static unsigned long lastStatusMs = 0;
void printStatus() {
  unsigned long now = millis();
  if (now - lastStatusMs < 1000UL) return;
  lastStatusMs = now;
  Serial.print(F("seat="));     Serial.print((int)seatState);
  Serial.print(F(" posture=")); Serial.print((int)postureState);
  Serial.print(F(" mode="));    Serial.print((int)useMode);
  Serial.print(F(" neck="));    Serial.print(neckAngleDeg);
  Serial.print(F(" away="));    Serial.print(isAway);
  Serial.print(F(" L="));       Serial.print(loadLeftG);
  Serial.print(F(" R="));       Serial.print(loadRightG);
  Serial.print(F(" US="));      Serial.print(ultrasonicDistanceCm);
  Serial.print(F(" IR="));      Serial.print(irDetected);
  Serial.print(F(" focus="));   Serial.print(focusTimeSec);
  Serial.print(F(" seatLeft="));Serial.print(seatRemainSec);
  Serial.print(F(" awayLeft="));Serial.println(awayRemainSec);
}
#endif

// ============================================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();

#if !SERIAL_TEST_MODE
  setupMpu();           // Serial1 link to the Nano
  setupLoadcell();      // chair must be empty at power-on (tare)
#endif
  setupFayoz();
  setupOledTimer();
}

void loop() {
  // 1) Sensors / inputs
#if SERIAL_TEST_MODE
  updateTestInput();
#else
  updateMpu();
  updateLoadcell();
#endif
  updateFayoz();        // ultrasonic, IR, mode button, away button

  // 2) Main logic
  updatePostureState();
  updateSeatState();

  // 3) Outputs (after main logic so they show this loop's state)
  updateLed();
  updateOledTimer();

#if DEBUG_STATUS_PRINT
  printStatus();
#endif
}
