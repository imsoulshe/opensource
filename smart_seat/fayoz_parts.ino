// ============================================================
// Fayoz's parts: 2 (ultrasonic), 3 (IR), 6 (LED), 7 (mode button), 8 (away button)
// Common rules: no delay(), millis() only; pins/times come from config.h.
// Usage: main calls setupFayoz() / updateFayoz().
//
// Integration changes (vs. the original file):
//   - Enums / extern variables now come from common.h (they were declared
//     twice before, which is a compile error once everything is in one sketch).
//   - ULTRASONIC_SAMPLE_INTERVAL_MS, ULTRASONIC_TIMEOUT_US, IR_ACTIVE_LOW moved
//     to config.h. (The old "#ifndef" fallback never worked: #ifndef only sees
//     #define macros, not const variables, so it would have defined them twice.)
//   - updateLed() writes each LED once per loop instead of "all off, then on".
//   - updateLed() is no longer inside updateFayoz(): main calls it AFTER
//     updateSeatState(), so the LED shows this loop's seat state.
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "common.h"

// Variables WRITTEN here: ultrasonicDistanceCm (2), irDetected (3),
//                         useMode (7), isAway (8)
// Variables READ here   : seatState (6)

// ============================================================
// 2. Ultrasonic sensor
// ============================================================
static unsigned long lastUltrasonicMs = 0;

void setupUltrasonic() {
  pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
  pinMode(PIN_ULTRASONIC_ECHO, INPUT);
  digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
}

void updateUltrasonic() {
  unsigned long now = millis();
  if (now - lastUltrasonicMs < ULTRASONIC_SAMPLE_INTERVAL_MS) return;
  lastUltrasonicMs = now;

  // 10-microsecond trigger pulse (delayMicroseconds does not stall millis())
  digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_ULTRASONIC_TRIG, LOW);

  // Echo pulse width; returns 0 on timeout.
  // Note: pulseIn waits up to ULTRASONIC_TIMEOUT_US (25 ms) once every 100 ms.
  unsigned long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, ULTRASONIC_TIMEOUT_US);
  if (duration == 0) return;  // no reading: keep the previous value

  // Speed of sound ~0.0343 cm/us; divide by 2 for the round trip
  ultrasonicDistanceCm = duration * 0.0343f / 2.0f;
}

// ============================================================
// 3. Infrared sensor (with debounce)
// ============================================================
static bool          irRawLast      = false;
static unsigned long irRawChangedMs = 0;

static bool readIrRaw() {
  return IR_ACTIVE_LOW ? (digitalRead(PIN_IR) == LOW) : (digitalRead(PIN_IR) == HIGH);
}

void setupIr() {
  pinMode(PIN_IR, INPUT);
  irRawLast = readIrRaw();
  irDetected = irRawLast;
  irRawChangedMs = millis();
}

void updateIr() {
  bool raw = readIrRaw();
  unsigned long now = millis();

  if (raw != irRawLast) {          // reading changed: restart the stability timer
    irRawLast = raw;
    irRawChangedMs = now;
  }
  // Accept the value only after it stays stable for SENSOR_DEBOUNCE_MS
  if (now - irRawChangedMs >= (unsigned long)SENSOR_DEBOUNCE_MS) {
    irDetected = raw;
  }
}

// ============================================================
// 6. LED (only READS seatState)
// Note: on Arduino Mega, pins 25-27 are NOT PWM pins. analogWrite() on them
// acts as on/off (value >= 128 -> on). For real dimming, move the LEDs to
// PWM pins (2-13 or 44-46) and change the pins in config.h.
// ============================================================
void setupLed() {
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
}

void updateLed() {
  // Each LED is written exactly once, so there is no off/on flicker.
  analogWrite(PIN_LED_BLUE,   seatState == SEAT_AVAILABLE ? LED_BRIGHTNESS : 0);  // blue
  analogWrite(PIN_LED_RED,    seatState == SEAT_OCCUPIED  ? LED_BRIGHTNESS : 0);  // red
  analogWrite(PIN_LED_YELLOW, seatState == SEAT_AWAY      ? LED_BRIGHTNESS : 0);  // yellow
}

// ============================================================
// 7. Mode button (LAPTOP <-> BOOK)
// ============================================================
static bool          modeBtnStable   = HIGH;  // INPUT_PULLUP: released = HIGH
static bool          modeBtnLastRaw  = HIGH;
static unsigned long modeBtnChangeMs = 0;

void setupModeButton() {
  pinMode(PIN_BTN_MODE, INPUT_PULLUP);
}

void updateModeButton() {
  bool raw = digitalRead(PIN_BTN_MODE);
  unsigned long now = millis();

  if (raw != modeBtnLastRaw) {
    modeBtnLastRaw = raw;
    modeBtnChangeMs = now;
  }
  if (now - modeBtnChangeMs >= (unsigned long)BTN_DEBOUNCE_MS && raw != modeBtnStable) {
    modeBtnStable = raw;
    if (modeBtnStable == LOW) {  // act once, on the press edge (HIGH -> LOW)
      useMode = (useMode == MODE_LAPTOP) ? MODE_BOOK : MODE_LAPTOP;
    }
  }
}

// ============================================================
// 8. Away button (toggles isAway)
// Main treats every CHANGE of isAway as one button press (see updateSeatState).
// ============================================================
static bool          awayBtnStable   = HIGH;
static bool          awayBtnLastRaw  = HIGH;
static unsigned long awayBtnChangeMs = 0;

void setupAwayButton() {
  pinMode(PIN_BTN_AWAY, INPUT_PULLUP);
}

void updateAwayButton() {
  bool raw = digitalRead(PIN_BTN_AWAY);
  unsigned long now = millis();

  if (raw != awayBtnLastRaw) {
    awayBtnLastRaw = raw;
    awayBtnChangeMs = now;
  }
  if (now - awayBtnChangeMs >= (unsigned long)BTN_DEBOUNCE_MS && raw != awayBtnStable) {
    awayBtnStable = raw;
    if (awayBtnStable == LOW) {
      isAway = !isAway;  // toggle on press
    }
  }
}

// ============================================================
// Helper functions that bundle everything
// ============================================================
void setupFayoz() {
  setupUltrasonic();
  setupIr();
  setupLed();
  setupModeButton();
  setupAwayButton();
}

// Inputs only. updateLed() is called separately by main, after updateSeatState().
void updateFayoz() {
  updateUltrasonic();
  updateIr();
  updateModeButton();
  updateAwayButton();
}
