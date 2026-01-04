#pragma once
#include <stdint.h>

// Light and temperature sensors are on ADC ports
// functions adapted from Seeed examples to get values
// see : https://github.com/Seeed-Studio/Seeed-Tracker-T1000-E-for-LoRaWAN-dev-board

extern uint32_t t1000e_get_light();
extern float t1000e_get_temperature();
extern bool t1000e_read_accel(int8_t& x, int8_t& y, int8_t& z);
extern void t1000e_init_accel();

// FlipMute detection: check if device is face-down in dark
// Returns true if device should be muted (face-down AND dark)
extern bool t1000e_is_face_down_in_dark(uint32_t light_threshold_lux = 3);
