#include "t1000e_sensors.h"

#include <Arduino.h>
#include <Wire.h>

#define HEATER_NTC_BX 4250   // thermistor coefficient B
#define HEATER_NTC_RP 8250   // ohm, series resistance to thermistor
#define HEATER_NTC_KA 273.15 // 25 Celsius at Kelvin
#define NTC_REF_VCC   3300   // mV, max voltage of 3V3 sensor rail
#define LIGHT_REF_VCC 2400   //

// QMA6100P I2C address and registers
#define QMA6100P_I2C_ADDR   0x12
#define QMA6100P_REG_XOUT_L 0x00   // X output low byte
#define QMA6100P_REG_XOUT_H 0x01   // X output high byte (start address for reading all 6 bytes)
#define QMA6100P_REG_YOUT_L 0x02   // Y output low byte
#define QMA6100P_REG_YOUT_H 0x03   // Y output high byte
#define QMA6100P_REG_ZOUT_L 0x04   // Z output low byte
#define QMA6100P_REG_ZOUT_H 0x05   // Z output high byte
#define QMA6100P_REG_CHIP_ID 0x00  // Chip ID register (same as XOUT_L but different context)
#define QMA6100P_REG_CTRL1  0x0B   // Control register 1
#define QMA6100P_REG_CTRL7  0x32   // Control register 7 (data read mode)

static unsigned int ntc_res2[136] = {
  113347, 107565, 102116, 96978, 92132, 87559, 83242, 79166, 75316, 71677, 68237, 64991, 61919, 59011,
  56258,  53650,  51178,  48835, 46613, 44506, 42506, 40600, 38791, 37073, 35442, 33892, 32420, 31020,
  29689,  28423,  27219,  26076, 24988, 23951, 22963, 22021, 21123, 20267, 19450, 18670, 17926, 17214,
  16534,  15886,  15266,  14674, 14108, 13566, 13049, 12554, 12081, 11628, 11195, 10780, 10382, 10000,
  9634,   9284,   8947,   8624,  8315,  8018,  7734,  7461,  7199,  6948,  6707,  6475,  6253,  6039,
  5834,   5636,   5445,   5262,  5086,  4917,  4754,  4597,  4446,  4301,  4161,  4026,  3896,  3771,
  3651,   3535,   3423,   3315,  3211,  3111,  3014,  2922,  2834,  2748,  2666,  2586,  2509,  2435,
  2364,   2294,   2228,   2163,  2100,  2040,  1981,  1925,  1870,  1817,  1766,  1716,  1669,  1622,
  1578,   1535,   1493,   1452,  1413,  1375,  1338,  1303,  1268,  1234,  1202,  1170,  1139,  1110,
  1081,   1053,   1026,   999,   974,   949,   925,   902,   880,   858,
};

static int8_t ntc_temp2[136] = {
  -30, -29, -28, -27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11,
  -10, -9,  -8,  -7,  -6,  -5,  -4,  -3,  -2,  -1,  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,
  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,
  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105,
};

static float get_heater_temperature(unsigned int vcc_volt, unsigned int ntc_volt) {
  int i = 0;
  float Vout = 0, Rt = 0, temp = 0;
  Vout = ntc_volt;

  Rt = (HEATER_NTC_RP * vcc_volt) / Vout - HEATER_NTC_RP;

  for (i = 0; i < 136; i++) {
    if (Rt >= ntc_res2[i]) {
      break;
    }
  }

  temp = ntc_temp2[i - 1] + 1 * (ntc_res2[i - 1] - Rt) / (float)(ntc_res2[i - 1] - ntc_res2[i]);

  temp = (temp * 100 + 5) / 100;
  return temp;
}

static int get_light_lv(unsigned int light_volt) {
  float Vout = 0, Vin = 0, Rt = 0, temp = 0;
  unsigned int light_level = 0;

  // Seeed's firmware maps the photocell reading to a 0-100 % range rather than lux.
  if (light_volt <= 80) {
    light_level = 0;
    return light_level;
  } else if (light_volt >= 2480) {
    light_level = 100;
    return light_level;
  }
  Vout = light_volt;
  light_level = 100 * (Vout - 80) / LIGHT_REF_VCC;

  return light_level;
}

float t1000e_get_temperature(void) {
  unsigned int ntc_v, vcc_v;

  digitalWrite(PIN_3V3_EN, HIGH);
  digitalWrite(SENSOR_EN, HIGH);
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(12);
  delay(10);
  unsigned int rail_v = (1000.0 * (analogRead(BATTERY_PIN) * ADC_MULTIPLIER * AREF_VOLTAGE)) / 4096;
  vcc_v = (rail_v > NTC_REF_VCC) ? NTC_REF_VCC : rail_v;
  ntc_v = (1000.0 * AREF_VOLTAGE * analogRead(TEMP_SENSOR)) / 4096;
  digitalWrite(PIN_3V3_EN, LOW);
  digitalWrite(SENSOR_EN, LOW);

  return get_heater_temperature(vcc_v, ntc_v);
}

uint32_t t1000e_get_light(void) {
  int lux = 0;
  unsigned int lux_v = 0;

  digitalWrite(PIN_3V3_EN, HIGH);
  digitalWrite(SENSOR_EN, HIGH);
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(12);
  delay(10);
  lux_v = 1000 * analogRead(LUX_SENSOR) * AREF_VOLTAGE / 4096;
  lux = get_light_lv(lux_v);
  digitalWrite(SENSOR_EN, LOW);
  digitalWrite(PIN_3V3_EN, LOW);

  return lux;
}

// Initialize QMA6100P accelerometer
void t1000e_init_accel() {
  Serial.println("[Accel] Initializing QMA6100P...");
  
  // Ensure Wire is initialized
  Wire.begin();
  delay(10);
  
  // Enable accelerometer power
  #ifdef PIN_3V3_ACC_EN
    pinMode(PIN_3V3_ACC_EN, OUTPUT);
    digitalWrite(PIN_3V3_ACC_EN, HIGH);
    delay(100);  // Wait for power stabilization
  #endif
  
  // Try to read register 0x00 to verify I2C communication and check data
  Wire.beginTransmission(QMA6100P_I2C_ADDR);
  Wire.write(0x00);
  int result = Wire.endTransmission();
  Serial.printf("[Accel] I2C select register 0x00: %d\n", result);
  
  if (result == 0) {
    delay(10);
    if (Wire.requestFrom(QMA6100P_I2C_ADDR, 6) == 6) {
      uint8_t data[6];
      for (int i = 0; i < 6; i++) {
        data[i] = Wire.read();
      }
      Serial.printf("[Accel] Initial read bytes: %02X %02X %02X %02X %02X %02X\n", 
                    data[0], data[1], data[2], data[3], data[4], data[5]);
    }
  }
  
  // Try wake up: set PWR_MGMT_1 (0x11) bit 7 to 1
  Wire.beginTransmission(QMA6100P_I2C_ADDR);
  Wire.write(0x11);  // PWR_MGMT_1
  Wire.write(0x80);  // Wake up
  result = Wire.endTransmission();
  Serial.printf("[Accel] Wake-up write result: %d\n", result);
  delay(100);
  
  Serial.println("[Accel] Init complete");
}

// Read accelerometer data from QMA6100P
// Data format: 6 bytes starting from register 0x00
// Read accelerometer data - most basic, reliable approach
bool t1000e_read_accel(int8_t& x, int8_t& y, int8_t& z) {
  x = 0;
  y = 0;
  z = 0;
  
  // Ensure power is on
  #ifdef PIN_3V3_ACC_EN
    digitalWrite(PIN_3V3_ACC_EN, HIGH);
  #endif
  
  // Step 1: Send register address 0x01 (skip 0x00 status byte, start from XOUT_H)
  Wire.beginTransmission(QMA6100P_I2C_ADDR);
  Wire.write(0x01);  // Start from register 0x01 (XOUT_H), skip status byte at 0x00
  int tx_result = Wire.endTransmission(true);  // STOP condition
  
  if (tx_result != 0) {
    Serial.printf("[Accel] TX error: %d\n", tx_result);
    return false;
  }
  
  // Step 2: Wait for register pointer to be ready
  delayMicroseconds(200);
  
  // Step 3: Read 6 bytes of data (XOUT_H, XOUT_L, YOUT_H, YOUT_L, ZOUT_H, ZOUT_L)
  int bytes_read = Wire.requestFrom(QMA6100P_I2C_ADDR, (uint8_t)6, (uint8_t)true);
  
  if (bytes_read != 6) {
    Serial.printf("[Accel] RX failed: got %d/6\n", bytes_read);
    return false;
  }
  
  uint8_t data[6];
  for (int i = 0; i < 6; i++) {
    data[i] = Wire.read();
  }
  
  Serial.printf("[Accel] Raw 6: %02X %02X %02X %02X %02X %02X -> ", 
                data[0], data[1], data[2], data[3], data[4], data[5]);
  
  // Parse as 16-bit big-endian, shift right by 2 for 8-bit result
  // data[0-1] = X, data[2-3] = Y, data[4-5] = Z
  int16_t raw_x = ((int16_t)data[0] << 8) | data[1];
  int16_t raw_y = ((int16_t)data[2] << 8) | data[3];
  int16_t raw_z = ((int16_t)data[4] << 8) | data[5];
  
  x = (int8_t)(raw_x >> 2);
  y = (int8_t)(raw_y >> 2);
  z = (int8_t)(raw_z >> 2);
  
  Serial.printf("x=%d y=%d z=%d\n", (int)x, (int)y, (int)z);
  
  return true;
}

// Check if device is face-down in dark: light < threshold AND device is inverted (face-down)
// Returns true if device should be muted (face-down AND dark)
bool t1000e_is_face_down_in_dark(uint32_t light_threshold_lux) {
  // Check light level first (cheaper than I2C read)
  uint32_t light = t1000e_get_light();
  
  Serial.printf("[FlipMute] Light:%lu lux (threshold:%lu) ", light, light_threshold_lux);
  
  // Dark enough check first
  if (light >= light_threshold_lux) {
    Serial.printf("-> NOT DARK ENOUGH (light >= %lu), ALLOWING SOUND\n", light_threshold_lux);
    return false;  // Not dark enough
  }
  
  Serial.printf("-> DARK ENOUGH (light < %lu), checking accel...\n", light_threshold_lux);
  
  // Now check accelerometer orientation
  int8_t x, y, z;
  bool accel_ok = t1000e_read_accel(x, y, z);
  
  if (!accel_ok) {
    Serial.println("[FlipMute] Failed to read accelerometer, ALLOWING SOUND");
    return false;  // Failed to read accelerometer
  }
  
  // Face-down detection based on Z-axis
  // Measured values (desktop):
  // - Face-down:    z=64 (lowest - only this should trigger mute)
  // - Face-left:    z=80
  // - Face-right:   z=78
  // - Face-up:      z=94 (highest - should allow sound)
  // Z-axis represents vertical orientation: z<70 means face-down
  
  const int8_t Z_FACE_DOWN_THRESHOLD = 70;  // Only z<70 = face-down, z>=70 = other orientations
  
  Serial.printf("[FlipMute] Z-axis: %d | threshold: %d | ", (int)z, Z_FACE_DOWN_THRESHOLD);
  
  if (z < Z_FACE_DOWN_THRESHOLD) {
    Serial.printf("*** DEVICE FACE-DOWN (z=%d < %d) -> MUTING SOUND ***\n", (int)z, Z_FACE_DOWN_THRESHOLD);
    return true;
  }
  
  Serial.printf("Device NOT face-down (z=%d >= %d) -> ALLOWING SOUND\n", (int)z, Z_FACE_DOWN_THRESHOLD);
  return false;
}
