// =====================================================================
//  [Arduino Nano] MPU6050 neck tilt -> send to Mega 2560
//  Part 1, written by 방성태 (comments translated to English, logic unchanged)
//  Upload this sketch to the NANO, not the Mega.
// ---------------------------------------------------------------------
//  What it does
//    1) Read accel/gyro from the MPU6050
//    2) Compute absolute neck angle with a complementary filter
//    3) Subtract the fixed good-posture angle (BASE_ANGLE_DEG), send to Mega
//    * Bad-posture decision (per-mode limit, 3 s hold) is done on the Mega
//      (smart_seat/neck_receiver.ino)
//
//  Format sent to the Mega (one line each)
//    "A:12.3\n" : 12.3 deg forward of good posture (forward = +)
//    "E\n"      : MPU connection error (failed at startup or disconnected during use)
//
//  Wiring
//    MPU6050  VCC->Nano 5V, GND->Nano GND, SDA->Nano A4, SCL->Nano A5
//    Nano<->Mega  Nano TX(1)->Mega RX1(19), Nano GND - Mega GND, Nano 5V <- Mega 5V
//    * Unplug the TX wire (pin 1) while uploading to the Nano
//
//  Sensor mounting: back of the neck, X-axis arrow pointing up (to the head),
//                   chip face pointing backward
// =====================================================================
#include <Wire.h>
#include <math.h>

// ---------------------------------------------------------------------
// Settings (edit here if needed)
// ---------------------------------------------------------------------
const float         BASE_ANGLE_DEG         = 10.0;   // absolute neck angle in good posture (placeholder)
const long          SERIAL_BAUD            = 115200; // must match Mega Serial1 (NECK_SERIAL_BAUD)
const float         COMPLEMENTARY_ALPHA    = 0.96;   // how much to trust the gyro
const unsigned long MPU_SAMPLE_INTERVAL_MS = 20;     // sensor read interval
const unsigned long SEND_INTERVAL_MS       = 50;     // send-to-Mega interval
const unsigned long RETRY_INTERVAL_MS      = 2000;   // retry interval when connection fails
const int           MPU_FAIL_LIMIT         = 10;     // [added] this many failed reads in a row => disconnected
const unsigned long WIRE_TIMEOUT_US        = 3000;   // [added] max wait for one I2C transfer
const int           GYRO_BIAS_SAMPLES      = 200;    // samples for gyro bias (~0.4 s)
const float         GYRO_SIGN              = 1.0;    // set -1.0 if angle jumps the wrong way when leaning
const int           PIN_STATUS_LED         = 13;     // Nano built-in LED (on while measuring gyro bias)

// ---------------------------------------------------------------------
// MPU6050 fixed values
// ---------------------------------------------------------------------
const uint8_t MPU_ADDR    = 0x68;    // AD0 pin to GND = 0x68, to 3.3V = 0x69
const float   ACCEL_SCALE = 16384.0; // +-2g range: 1g = 16384
const float   GYRO_SCALE  = 131.0;   // +-250 deg/s range: 1 deg/s = 131

// ---------------------------------------------------------------------
// Variables
// ---------------------------------------------------------------------
float neckAngleDeg = 0.0;            // neck angle relative to good posture (sent to Mega)

int16_t rawAx, rawAy, rawAz;         // raw accel
int16_t rawGx, rawGy, rawGz;         // raw gyro

bool  mpuReady         = false;      // sensor connected?
int   mpuFailCount     = 0;          // [added] failed reads in a row
float gyroBiasY        = 0.0;        // gyro offset when standing still
float filteredAngleDeg = 0.0;        // current absolute angle from the filter

unsigned long lastSampleMs = 0;
unsigned long lastSendMs   = 0;
unsigned long lastRetryMs  = 0;
unsigned long lastMicros   = 0;

// ---------------------------------------------------------------------
// [1] I2C helpers
// ---------------------------------------------------------------------
void mpuWriteReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

int16_t read16() {
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  return (int16_t)(((uint16_t)hi << 8) | lo);
}

bool readMpuRaw() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);                                    // start at ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)14) != 14) return false;

  rawAx = read16();
  rawAy = read16();
  rawAz = read16();
  read16();                                            // temperature (unused)
  rawGx = read16();
  rawGy = read16();
  rawGz = read16();
  return true;
}

// ---------------------------------------------------------------------
// [2] Absolute angle from accel only (direction of gravity)
// ---------------------------------------------------------------------
float calcAccelAngleDeg() {
  float ax = rawAx / ACCEL_SCALE;
  float az = rawAz / ACCEL_SCALE;
  return atan2(az, ax) * 180.0 / PI;
}

// ---------------------------------------------------------------------
// [3] Sensor init + gyro bias measurement
//     (posture does not matter; the sensor just needs to be still briefly)
// ---------------------------------------------------------------------
bool initMpu() {
  mpuWriteReg(0x6B, 0x00);  // wake up (leave sleep mode)
  mpuWriteReg(0x1A, 0x03);  // internal low-pass filter ~44 Hz
  mpuWriteReg(0x1B, 0x00);  // gyro +-250 deg/s
  mpuWriteReg(0x1C, 0x00);  // accel +-2g
  delay(100);               // let the sensor settle (init only; Nano does nothing else)

  digitalWrite(PIN_STATUS_LED, HIGH);

  float sumGy = 0.0;
  int count = 0, tries = 0;
  while (count < GYRO_BIAS_SAMPLES && tries < GYRO_BIAS_SAMPLES * 2) {
    tries++;
    if (readMpuRaw()) {
      sumGy += rawGy;
      count++;
    }
  }

  digitalWrite(PIN_STATUS_LED, LOW);
  if (count == 0) return false;                        // no response from sensor

  gyroBiasY        = sumGy / count;
  filteredAngleDeg = calcAccelAngleDeg();              // start filter at current accel angle
  lastMicros       = micros();
  mpuFailCount     = 0;                                // [added] reset fail count
  return true;
}

// ---------------------------------------------------------------------
// [4] Update neck angle
// ---------------------------------------------------------------------
void updateMpu() {
  unsigned long nowMs = millis();
  if (nowMs - lastSampleMs < MPU_SAMPLE_INTERVAL_MS) return;
  lastSampleMs = nowMs;

  // [changed] count failed reads in a row; over the limit => treat as disconnected
  if (!readMpuRaw()) {
    mpuFailCount++;
    if (mpuFailCount >= MPU_FAIL_LIMIT) {
      mpuReady    = false;                             // -> send E + go to reconnect retries
      lastRetryMs = nowMs;                             // count retry interval from the disconnect
    }
    return;
  }
  mpuFailCount = 0;                                    // any success resets the count

  unsigned long nowUs = micros();
  float dt = (nowUs - lastMicros) / 1000000.0;
  lastMicros = nowUs;

  float gyroRate   = (rawGy - gyroBiasY) / GYRO_SCALE * GYRO_SIGN;
  float accelAngle = calcAccelAngleDeg();

  filteredAngleDeg = COMPLEMENTARY_ALPHA * (filteredAngleDeg + gyroRate * dt)
                   + (1.0 - COMPLEMENTARY_ALPHA) * accelAngle;

  neckAngleDeg = filteredAngleDeg - BASE_ANGLE_DEG;
}

// ---------------------------------------------------------------------
// [5] Send to Mega
// ---------------------------------------------------------------------
void sendToMega() {
  unsigned long nowMs = millis();
  if (nowMs - lastSendMs < SEND_INTERVAL_MS) return;
  lastSendMs = nowMs;

  if (mpuReady) {
    Serial.print("A:");
    Serial.println(neckAngleDeg, 1);                   // e.g. A:12.3
  } else {
    Serial.println("E");
  }
}

// ---------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(PIN_STATUS_LED, OUTPUT);
  Wire.begin();
  Wire.setWireTimeout(WIRE_TIMEOUT_US, true);          // [added] don't hang on I2C if a wire comes loose
  mpuReady = initMpu();
  lastRetryMs = millis();
}

void loop() {
  if (!mpuReady) {
    // On connection failure / disconnect, retry at a fixed interval
    unsigned long nowMs = millis();
    if (nowMs - lastRetryMs >= RETRY_INTERVAL_MS) {
      lastRetryMs = nowMs;
      mpuReady = initMpu();
    }
  } else {
    updateMpu();
  }

  sendToMega();
}
