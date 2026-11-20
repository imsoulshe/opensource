// config.h - shared configuration constants (from the team spec)
// Change values here only; never hard-code them inside module code.
#ifndef CONFIG_H
#define CONFIG_H

// ---- Pin numbers (update all at once after wiring is final) ----
const int PIN_ULTRASONIC_TRIG = 22;
const int PIN_ULTRASONIC_ECHO = 23;
const int PIN_IR              = 24;
const int PIN_LED_RED         = 25;
const int PIN_LED_YELLOW      = 26;
const int PIN_LED_BLUE        = 27;
const int PIN_BTN_MODE        = 28;
const int PIN_BTN_AWAY        = 29;
const int PIN_LOADCELL_L_DT   = 30;
const int PIN_LOADCELL_L_SCK  = 31;
const int PIN_LOADCELL_R_DT   = 32;
const int PIN_LOADCELL_R_SCK  = 33;

// ---- MPU6050 ----
const float NECK_ANGLE_LIMIT_LAPTOP_DEG = 30.0;   // bad-posture limit in laptop mode
const float NECK_ANGLE_LIMIT_BOOK_DEG   = 45.0;   // bad-posture limit in book mode
const float COMPLEMENTARY_ALPHA         = 0.96;   // gyro/accel complementary filter ratio
const unsigned long BAD_POSTURE_HOLD_MS = 3000;   // must persist this long to count as bad
const int MPU_SAMPLE_INTERVAL_MS        = 20;

// ---- Load cells ----
const float PRESENCE_WEIGHT_MIN_G = 20000;        // above this => person present
const float LR_DIFF_LIMIT_G       = 5000;         // L/R difference above this => leaning
const unsigned long LEAN_HOLD_MS  = 3000;
const float LOADCELL_CAL_FACTOR_L = 1.0;
const float LOADCELL_CAL_FACTOR_R = 1.0;

// ---- Ultrasonic / IR ----
const float ULTRASONIC_NEAR_CM = 50.0;
const int   SENSOR_DEBOUNCE_MS = 500;
const unsigned long ULTRASONIC_SAMPLE_INTERVAL_MS = 100;    // how often to measure (ms) (added by part 2)
const unsigned long ULTRASONIC_TIMEOUT_US         = 25000;  // echo wait limit (~4 m)    (added by part 2)
const bool          IR_ACTIVE_LOW                 = true;   // most IR modules output LOW on obstacle (part 3)

// ---- Timers ----
const unsigned long SEAT_TOTAL_SEC = 4UL * 60 * 60;   // max seat usage time
const unsigned long AWAY_MAX_SEC   = 30UL * 60;       // allowed away time
const unsigned long AWAY_NO_PERSON_TIMEOUT_SEC = 10;  // no person this long => seat freed

// ---- OLED / LED / Buttons ----
const int OLED_UPDATE_INTERVAL_MS = 500;
const int LED_BRIGHTNESS          = 255;
const int BTN_DEBOUNCE_MS         = 50;

// ---- OLED hardware (added by part 5; SSD1306 128x64 over I2C) ----
// On Arduino Mega the I2C pins are SDA=20, SCL=21 (shared with the MPU6050).
const int     OLED_WIDTH    = 128;
const int     OLED_HEIGHT   = 64;
const uint8_t OLED_I2C_ADDR = 0x3C;   // some modules use 0x3D
const int     OLED_RESET_PIN = -1;    // -1 = module has no reset pin / shares Arduino reset

#endif
