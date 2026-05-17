#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define RUUVI_MAX_SENSORS 4
#define RUUVI_TIMEOUT_MS  30000UL   // mark stale if no advertisement for 30 s

struct tRuuviSensor {
  bool          active;           // slot occupied
  char          mac[18];          // "AA:BB:CC:DD:EE:FF"
  char          label[32];        // user-defined label
  unsigned long lastSeen;         // millis() at last advertisement
  float         temperature;      // °C  (NAN = invalid)
  float         humidity;         // %RH (NAN = invalid)
  float         pressure;         // hPa (NAN = invalid)
  uint16_t      batteryMv;        // mV  (0 = unknown)
  int16_t       rssi;             // dBm
  int16_t       accelX, accelY, accelZ;  // mG, raw (0x8000 = invalid)
  uint8_t       movementCounter;  // increments on motion, wraps 0-254
  float         heel;             // degrees, EMA-filtered (NAN until first accel reading)
  float         pitch;            // degrees, EMA-filtered (NAN until first accel reading)
};

extern tRuuviSensor     ruuviSensors[RUUVI_MAX_SENSORS];
extern SemaphoreHandle_t ruuviMutex;

void ruuviInit();                               // call once in setup()
void ruuviLoadLabels();                         // load NVS labels into slots
void ruuviSaveLabel(int slot, const char* lbl); // persist one label
bool ruuviIsStale(int slot);                    // >30 s since last seen
