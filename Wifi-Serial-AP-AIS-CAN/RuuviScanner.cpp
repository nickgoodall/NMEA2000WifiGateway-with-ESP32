#include "RuuviScanner.h"
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <Preferences.h>
#include <math.h>

tRuuviSensor     ruuviSensors[RUUVI_MAX_SENSORS];
SemaphoreHandle_t ruuviMutex = nullptr;

static NimBLEScan* pBLEScan = nullptr;

// Find existing slot by MAC, or claim the first free one.
// Must be called with ruuviMutex held.
static int findOrAlloc(const char* mac) {
  for (int i = 0; i < RUUVI_MAX_SENSORS; i++)
    if (ruuviSensors[i].active && strcmp(ruuviSensors[i].mac, mac) == 0) return i;
  for (int i = 0; i < RUUVI_MAX_SENSORS; i++) {
    if (!ruuviSensors[i].active) {
      ruuviSensors[i].active = true;
      strlcpy(ruuviSensors[i].mac, mac, sizeof(ruuviSensors[i].mac));
      // Default label (may be overwritten by saved NVS label on next restart,
      // but we don't know the slot until first scan so set a placeholder now)
      if (ruuviSensors[i].label[0] == '\0')
        snprintf(ruuviSensors[i].label, sizeof(ruuviSensors[i].label), "Sensor %d", i + 1);
      return i;
    }
  }
  return -1;  // all 4 slots full with different MACs
}

// Data Format 5 (RAWv2) — manufacturer payload starts at d[0]=0x99,0x04 then d[2]=0x05
static void parseDF5(const uint8_t* d, size_t len, const char* mac, int16_t rssi) {
  if (len < 26) return;
  if (xSemaphoreTake(ruuviMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
  int slot = findOrAlloc(mac);
  if (slot < 0) { xSemaphoreGive(ruuviMutex); return; }

  tRuuviSensor& s = ruuviSensors[slot];
  s.lastSeen = millis();
  s.rssi     = rssi;

  // Temperature: bytes [3..4] int16 × 0.005 °C, 0x8000 = invalid
  int16_t rt = (int16_t)((uint16_t)d[3] << 8 | d[4]);
  s.temperature = (rt == (int16_t)0x8000) ? NAN : rt * 0.005f;

  // Humidity: bytes [5..6] uint16 × 0.0025 %RH, 65535 = invalid
  uint16_t rh = (uint16_t)d[5] << 8 | d[6];
  s.humidity = (rh == 65535u) ? NAN : rh * 0.0025f;

  // Pressure: bytes [7..8] uint16 (+50000 Pa) / 100 = hPa, 65535 = invalid
  uint16_t rp = (uint16_t)d[7] << 8 | d[8];
  s.pressure = (rp == 65535u) ? NAN : (rp + 50000.0f) / 100.0f;

  // Power info: bytes [15..16] — top 11 bits = battery offset (+1600 mV), 2047 = invalid
  uint16_t pw = (uint16_t)d[15] << 8 | d[16];
  uint16_t br = pw >> 5;
  s.batteryMv = (br == 2047u) ? 0u : (uint16_t)(br + 1600u);

  // Acceleration: bytes [9..14] int16 mG, 0x8000 = invalid
  int16_t ax = (int16_t)((uint16_t)d[9]  << 8 | d[10]);
  int16_t ay = (int16_t)((uint16_t)d[11] << 8 | d[12]);
  int16_t az = (int16_t)((uint16_t)d[13] << 8 | d[14]);
  if (ax != (int16_t)0x8000) s.accelX = ax;
  if (ay != (int16_t)0x8000) s.accelY = ay;
  if (az != (int16_t)0x8000) s.accelZ = az;

  // Movement counter: byte [17], 255 = error
  if (d[17] != 255u) s.movementCounter = d[17];

  // Heel/pitch via EMA — smooths wave-induced acceleration noise.
  // Assumes sensor mounted face-up with Y axis pointing to starboard, X pointing forward.
  // Adjust labels in UI if mounted differently.
  if (ax != (int16_t)0x8000 && ay != (int16_t)0x8000 && az != (int16_t)0x8000) {
    float newHeel  = atan2f((float)ay, (float)az) * (180.0f / M_PI);
    float newPitch = atan2f((float)ax, (float)az) * (180.0f / M_PI);
    if (isnan(s.heel)) { s.heel = newHeel; s.pitch = newPitch; }
    else { s.heel = s.heel * 0.75f + newHeel * 0.25f; s.pitch = s.pitch * 0.75f + newPitch * 0.25f; }
  }

  xSemaphoreGive(ruuviMutex);
}

// Data Format 3 (RAWv1) — manufacturer payload: d[0]=0x99,0x04, d[2]=0x03
static void parseDF3(const uint8_t* d, size_t len, const char* mac, int16_t rssi) {
  if (len < 16) return;
  if (xSemaphoreTake(ruuviMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
  int slot = findOrAlloc(mac);
  if (slot < 0) { xSemaphoreGive(ruuviMutex); return; }

  tRuuviSensor& s = ruuviSensors[slot];
  s.lastSeen = millis();
  s.rssi     = rssi;

  s.humidity    = d[3] * 0.5f;
  s.temperature = (float)(int8_t)d[4] + d[5] * 0.01f;
  uint16_t rp   = (uint16_t)d[6] << 8 | d[7];
  s.pressure    = (rp + 50000.0f) / 100.0f;
  s.batteryMv   = (uint16_t)d[14] << 8 | d[15];

  xSemaphoreGive(ruuviMutex);
}

class RuuviCallback : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* dev) override {
    if (!dev->haveManufacturerData()) return;
    std::string mfr = dev->getManufacturerData();
    if (mfr.size() < 3) return;
    const uint8_t* d = reinterpret_cast<const uint8_t*>(mfr.data());
    if (d[0] != 0x99 || d[1] != 0x04) return;  // not Ruuvi (company ID 0x0499)
    std::string macStr = dev->getAddress().toString();
    int16_t rssi = dev->getRSSI();
    if (d[2] == 0x05) parseDF5(d, mfr.size(), macStr.c_str(), rssi);
    else if (d[2] == 0x03) parseDF3(d, mfr.size(), macStr.c_str(), rssi);
  }
};

static RuuviCallback ruuviCb;

void ruuviInit() {
  ruuviMutex = xSemaphoreCreateMutex();

  memset(ruuviSensors, 0, sizeof(ruuviSensors));
  for (int i = 0; i < RUUVI_MAX_SENSORS; i++) {
    ruuviSensors[i].temperature = NAN;
    ruuviSensors[i].humidity    = NAN;
    ruuviSensors[i].pressure    = NAN;
    ruuviSensors[i].heel        = NAN;
    ruuviSensors[i].pitch       = NAN;
    snprintf(ruuviSensors[i].label, sizeof(ruuviSensors[i].label), "Sensor %d", i + 1);
  }
  ruuviLoadLabels();

  NimBLEDevice::init("");
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(&ruuviCb, true);  // true = want duplicates (continuous updates)
  pBLEScan->setActiveScan(false);   // passive — no SCAN_REQ, minimises radio contention with WiFi
  pBLEScan->setInterval(1280);      // ms — scan slot period
  pBLEScan->setWindow(100);         // ms — listen window (~8% duty cycle, leaves WiFi headroom)
  pBLEScan->start(0, nullptr);      // 0 = scan indefinitely
  Serial.println("Ruuvi BLE scanner started (passive)");
}

void ruuviLoadLabels() {
  Preferences prefs;
  prefs.begin("ruuvi_cfg", true);
  for (int i = 0; i < RUUVI_MAX_SENSORS; i++) {
    char mk[8], lk[8];
    snprintf(mk, sizeof(mk), "mac%d", i);
    snprintf(lk, sizeof(lk), "lbl%d", i);
    String mac = prefs.getString(mk, "");
    String lbl = prefs.getString(lk, "");
    if (mac.length() > 0) {
      ruuviSensors[i].active = true;
      strlcpy(ruuviSensors[i].mac, mac.c_str(), sizeof(ruuviSensors[i].mac));
    }
    if (lbl.length() > 0)
      strlcpy(ruuviSensors[i].label, lbl.c_str(), sizeof(ruuviSensors[i].label));
  }
  prefs.end();
}

void ruuviSaveLabel(int slot, const char* lbl) {
  if (slot < 0 || slot >= RUUVI_MAX_SENSORS) return;
  strlcpy(ruuviSensors[slot].label, lbl, sizeof(ruuviSensors[slot].label));
  Preferences prefs;
  prefs.begin("ruuvi_cfg", false);
  char mk[8], lk[8];
  snprintf(mk, sizeof(mk), "mac%d", slot);
  snprintf(lk, sizeof(lk), "lbl%d", slot);
  if (ruuviSensors[slot].active) prefs.putString(mk, ruuviSensors[slot].mac);
  prefs.putString(lk, lbl);
  prefs.end();
}

bool ruuviIsStale(int slot) {
  if (!ruuviSensors[slot].active) return true;
  return (millis() - ruuviSensors[slot].lastSeen) > RUUVI_TIMEOUT_MS;
}

