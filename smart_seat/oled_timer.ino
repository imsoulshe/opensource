// ============================================================================
// Part 5: OLED + Timer
// ----------------------------------------------------------------------------
// What this module does
//   1) Timers (1-second tick, millis() based, no delay()):
//        - focusTimeSec  : +1 per second ONLY while seat is OCCUPIED and
//                          postureState == POSTURE_GOOD
//        - seatRemainSec : counts down from SEAT_TOTAL_SEC while the seat is
//                          in use (OCCUPIED or AWAY)
//        - awayRemainSec : counts down from AWAY_MAX_SEC while seat is AWAY
//   2) OLED display (SSD1306 128x64, I2C), redrawn every OLED_UPDATE_INTERVAL_MS
//      and immediately when seatState changes.
//
// Variables WRITTEN here : focusTimeSec, awayRemainSec, seatRemainSec
// Variables READ here    : postureState, seatState, isAway, isPersonPresent
// Constants used         : SEAT_TOTAL_SEC, AWAY_MAX_SEC, OLED_UPDATE_INTERVAL_MS,
//                          OLED_WIDTH, OLED_HEIGHT, OLED_I2C_ADDR, OLED_RESET_PIN
//
// Notes for the main (updateSeatState) owner
//   - While NOT away, awayRemainSec stays at AWAY_MAX_SEC (never 0), so
//     "awayRemainSec == 0" can safely be used for the AWAY -> AVAILABLE rule.
//   - seatRemainSec stops at 0 (seat time over). What happens then is a
//     main-logic decision; this module only shows "TIME OVER".
//
// Libraries (Library Manager): "Adafruit SSD1306" and "Adafruit GFX Library"
// Wiring (Arduino Mega): OLED SDA -> 20, SCL -> 21, VCC -> 5V, GND -> GND
// ============================================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "common.h"

// ---- Module-private state (static = not visible to other parts) ----
static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
static bool          oledReady       = false;           // false if OLED not found
static unsigned long lastTickMs      = 0;               // last 1-second timer tick
static unsigned long lastDrawMs      = 0;               // last OLED redraw
static SeatState     prevSeatState   = SEAT_AVAILABLE;  // to detect state changes

static const unsigned long TICK_MS = 1000;

// Forward declarations (helpers used before their definitions)
static void handleSeatStateChange();
static void tickTimersOneSecond();
static void drawScreen();
static void formatHMS(char *buf, size_t len, unsigned long totalSec);
static void formatMS(char *buf, size_t len, unsigned long totalSec);

// ----------------------------------------------------------------------------
// setupOledTimer(): call once from setup(), after Wire/Serial are ready.
// ----------------------------------------------------------------------------
void setupOledTimer() {
  focusTimeSec  = 0;
  seatRemainSec = SEAT_TOTAL_SEC;
  awayRemainSec = AWAY_MAX_SEC;
  prevSeatState = SEAT_AVAILABLE;

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally.
  // If the OLED is missing we do NOT stop the program (no for(;;) loop):
  // timers keep working and other sensors keep running.
  oledReady = oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  if (!oledReady) {
    Serial.println(F("[OLED] SSD1306 not found - check wiring/address"));
  } else {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println(F("Smart Seat"));
    oled.println(F("Starting..."));
    oled.display();
  }

  unsigned long now = millis();
  lastTickMs = now;
  lastDrawMs = now;
}

// ----------------------------------------------------------------------------
// updateOledTimer(): call every loop(). Never blocks.
// Call it AFTER updateSeatState() so it sees this loop's seatState/postureState.
// ----------------------------------------------------------------------------
void updateOledTimer() {
  unsigned long now = millis();

  // 1) React to seat state changes (start session, enter/leave away, ...)
  bool stateChanged = (seatState != prevSeatState);
  if (stateChanged) {
    handleSeatStateChange();
    prevSeatState = seatState;
  }

  // 2) 1-second timer tick.
  //    "now - lastTickMs" is overflow-safe with unsigned math (millis wraps ~49 days).
  //    "lastTickMs += TICK_MS" (instead of = now) keeps the timer from drifting,
  //    and the while-loop catches up if loop() was ever slow for >1 s.
  while (now - lastTickMs >= TICK_MS) {
    lastTickMs += TICK_MS;
    tickTimersOneSecond();
  }

  // 3) Redraw OLED periodically, or right away when the state changed.
  if (stateChanged || (now - lastDrawMs >= (unsigned long)OLED_UPDATE_INTERVAL_MS)) {
    lastDrawMs = now;
    drawScreen();
  }
}

// ----------------------------------------------------------------------------
// Timer logic
// ----------------------------------------------------------------------------

// Called once when seatState is different from last loop.
static void handleSeatStateChange() {
  // New session: someone sits on a free seat -> reset all timers.
  // (AWAY -> OCCUPIED is the same person returning, so we keep the timers.)
  if (prevSeatState == SEAT_AVAILABLE && seatState == SEAT_OCCUPIED) {
    focusTimeSec  = 0;
    seatRemainSec = SEAT_TOTAL_SEC;
    awayRemainSec = AWAY_MAX_SEC;
  }

  // Going away: start the away countdown from full.
  if (seatState == SEAT_AWAY) {
    awayRemainSec = AWAY_MAX_SEC;
  }

  // Back from away (or seat freed): away timer goes back to full.
  if (prevSeatState == SEAT_AWAY && seatState != SEAT_AWAY) {
    awayRemainSec = AWAY_MAX_SEC;
  }

  // Seat freed: show full seat time for the next user.
  // focusTimeSec is kept until the next session starts.
  if (seatState == SEAT_AVAILABLE) {
    seatRemainSec = SEAT_TOTAL_SEC;
    awayRemainSec = AWAY_MAX_SEC;
  }
}

// Called exactly once per elapsed second.
static void tickTimersOneSecond() {
  switch (seatState) {
    case SEAT_OCCUPIED:
      if (seatRemainSec > 0) seatRemainSec--;
      // Focus time only grows with good posture.
      if (postureState == POSTURE_GOOD) focusTimeSec++;
      break;

    case SEAT_AWAY:
      // Seat is still reserved for this person, so seat time keeps running.
      if (seatRemainSec > 0) seatRemainSec--;
      if (awayRemainSec > 0) awayRemainSec--;
      break;

    case SEAT_AVAILABLE:
    default:
      // Nothing runs while the seat is free.
      break;
  }
}

// ----------------------------------------------------------------------------
// Display
// Layout: 128x64 px. Text size 1 = 6x8 px per char (21 chars x 8 lines),
//         text size 2 = 12x16 px per char (10 chars x 4 lines).
// ----------------------------------------------------------------------------

// Draws a full-width white bar with black text (used as a title bar).
static void drawTitleBar(const __FlashStringHelper *title) {
  oled.fillRect(0, 0, OLED_WIDTH, 10, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setTextSize(1);
  oled.setCursor(2, 1);
  oled.print(title);
  oled.setTextColor(SSD1306_WHITE);
}

static void drawAvailableScreen() {
  drawTitleBar(F("SEAT AVAILABLE"));
  oled.setTextSize(2);
  oled.setCursor(10, 20);
  oled.print(F("FREE SEAT"));
  oled.setTextSize(1);
  oled.setCursor(10, 48);
  oled.print(F("Sit down to start"));
}

static void drawOccupiedScreen() {
  char buf[12];

  drawTitleBar(F("IN USE"));

  // Seat remaining time
  oled.setTextSize(1);
  oled.setCursor(0, 14);
  oled.print(F("Seat left"));
  oled.setCursor(68, 14);
  if (seatRemainSec == 0) {
    oled.print(F("TIME OVER"));
  } else {
    formatHMS(buf, sizeof(buf), seatRemainSec);
    oled.print(buf);
  }

  // Focus time (big, it is the main info while studying)
  oled.setCursor(0, 26);
  oled.print(F("Focus"));
  formatHMS(buf, sizeof(buf), focusTimeSec);
  oled.setTextSize(2);
  oled.setCursor(16, 36);
  oled.print(buf);

  // Posture status line at the bottom (inverted when posture is bad)
  oled.setTextSize(1);
  bool bad = (postureState == POSTURE_BAD_NECK || postureState == POSTURE_BAD_LEAN);
  if (bad) {
    oled.fillRect(0, 54, OLED_WIDTH, 10, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
  }
  oled.setCursor(2, 55);
  if (!isPersonPresent) {
    oled.print(F("Not seated"));
  } else {
    switch (postureState) {
      case POSTURE_GOOD:     oled.print(F("Posture: GOOD"));        break;
      case POSTURE_BAD_NECK: oled.print(F("Fix NECK! (paused)"));   break;
      case POSTURE_BAD_LEAN: oled.print(F("Sit STRAIGHT!(paused)")); break;
      default:               oled.print(F("Posture: --"));          break;
    }
  }
  oled.setTextColor(SSD1306_WHITE);
}

static void drawAwayScreen() {
  char buf[12];

  drawTitleBar(F("AWAY"));

  // Away remaining time (big)
  oled.setTextSize(1);
  oled.setCursor(0, 14);
  oled.print(F("Return within"));
  formatMS(buf, sizeof(buf), awayRemainSec);
  oled.setTextSize(2);
  oled.setCursor(34, 24);
  oled.print(buf);

  // Seat and focus time (small)
  oled.setTextSize(1);
  oled.setCursor(0, 44);
  oled.print(F("Seat left"));
  oled.setCursor(68, 44);
  if (seatRemainSec == 0) {
    oled.print(F("TIME OVER"));
  } else {
    formatHMS(buf, sizeof(buf), seatRemainSec);
    oled.print(buf);
  }

  oled.setCursor(0, 55);
  oled.print(F("Focus"));
  oled.setCursor(68, 55);
  formatHMS(buf, sizeof(buf), focusTimeSec);
  oled.print(buf);
}

static void drawScreen() {
  if (!oledReady) return;   // no OLED -> skip drawing, timers still run

  oled.clearDisplay();
  switch (seatState) {
    case SEAT_OCCUPIED: drawOccupiedScreen();  break;
    case SEAT_AWAY:     drawAwayScreen();      break;
    case SEAT_AVAILABLE:
    default:            drawAvailableScreen(); break;
  }
  oled.display();   // send buffer to the OLED over I2C (a few ms, non-blocking enough)
}

// ----------------------------------------------------------------------------
// Time formatting helpers
// ----------------------------------------------------------------------------

// seconds -> "HH:MM:SS"  (e.g. 3725 -> "01:02:05")
static void formatHMS(char *buf, size_t len, unsigned long totalSec) {
  unsigned long h = totalSec / 3600UL;
  unsigned long m = (totalSec % 3600UL) / 60UL;
  unsigned long s = totalSec % 60UL;
  snprintf(buf, len, "%02lu:%02lu:%02lu", h, m, s);
}

// seconds -> "MM:SS"  (e.g. 1799 -> "29:59"), used for the away countdown
static void formatMS(char *buf, size_t len, unsigned long totalSec) {
  unsigned long m = totalSec / 60UL;
  unsigned long s = totalSec % 60UL;
  snprintf(buf, len, "%02lu:%02lu", m, s);
}
