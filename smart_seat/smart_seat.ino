// ============================================================================
// smart_seat.ino - main sketch
// Defines the shared global variables (declared in common.h) and calls every
// part's setupXXX() / updateXXX(). No delay() anywhere.
//
// Other parts: add your setupXXX()/updateXXX() calls in the marked places.
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

// Part 5 (OLED + timer) - see oled_timer.ino
void setupOledTimer();
void updateOledTimer();

// ----------------------------------------------------------------------------
// OLED_TEST_MODE = 1 : test part 5 alone, without the other sensors.
//   Type one character in the Serial Monitor (115200 baud):
//     a = seat AVAILABLE      o = seat OCCUPIED     w = seat AWAY
//     g = posture GOOD        n = bad NECK          l = bad LEAN
//     u = posture UNKNOWN (person left)
//   Set to 0 when the real sensor parts are connected.
// ----------------------------------------------------------------------------
#define OLED_TEST_MODE 1

#if OLED_TEST_MODE
void updateTestInput() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case 'a': seatState = SEAT_AVAILABLE; isPersonPresent = false; postureState = POSTURE_UNKNOWN; break;
    case 'o': seatState = SEAT_OCCUPIED;  isPersonPresent = true;  postureState = POSTURE_GOOD;    break;
    case 'w': seatState = SEAT_AWAY;      isPersonPresent = false; postureState = POSTURE_UNKNOWN; break;
    case 'g': isPersonPresent = true;  postureState = POSTURE_GOOD;     break;
    case 'n': isPersonPresent = true;  postureState = POSTURE_BAD_NECK; break;
    case 'l': isPersonPresent = true;  postureState = POSTURE_BAD_LEAN; break;
    case 'u': isPersonPresent = false; postureState = POSTURE_UNKNOWN;  break;
    default: return;   // ignore newlines etc.
  }
  Serial.print(F("seat=")); Serial.print(seatState);
  Serial.print(F(" posture=")); Serial.print(postureState);
  Serial.print(F(" focus=")); Serial.print(focusTimeSec);
  Serial.print(F(" seatLeft=")); Serial.print(seatRemainSec);
  Serial.print(F(" awayLeft=")); Serial.println(awayRemainSec);
}
#endif

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // TODO other parts: setupMpu(); setupUltrasonic(); setupIr(); setupLoadcell();
  //                   setupLed(); setupModeButton(); setupAwayButton();
  setupOledTimer();
}

void loop() {
#if OLED_TEST_MODE
  updateTestInput();
#else
  // 1) Sensors / inputs
  // TODO: updateMpu(); updateUltrasonic(); updateIr(); updateLoadcell();
  //       updateModeButton(); updateAwayButton();

  // 2) Main logic (combine posture, change seatState)
  // TODO: updatePostureState(); updateSeatState();
#endif

  // 3) Outputs - after the main logic so they see this loop's state
  // TODO: updateLed();
  updateOledTimer();
}
