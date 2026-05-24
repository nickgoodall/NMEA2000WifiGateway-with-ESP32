/*
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

// Version 1.3, 04.08.2020, AK-Homberger

#define ESP32_CAN_TX_PIN GPIO_NUM_5  // Set CAN TX port to 5 (Caution!!! Pin 2 before)
#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to 4

#define INTERNAL_LED 2

#include "esp_mac.h"
#include <Arduino.h>
#include <NMEA2000_CAN.h>  // This will automatically choose right CAN library and create suitable NMEA2000 object
#include <Seasmart.h>
#include <N2kMessages.h>
#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <OneButton.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "N2kDataToNMEA0183.h"
#include "List.h"
#include "index_html.h"
#include "sw_js.h"
#include "leaflet_js.h"
#include "settings_html.h"
#include "BoatData.h"
#include "LocalSensors.h"
#include "RuuviScanner.h"

// ── Simulation mode ──────────────────────────────────────────────────────────
// Set MOCK_MODE in MockData.h:
//   0 = full real    — hardware only, no mock data used
//   1 = partial mock — mock fills in until each sensor group initialises (default)
//   2 = full mock    — all values from MockData.h constants (demos / UI dev)
#include "MockData.h"


#define ENABLE_DEBUG_LOG 1 // Debug log, set to 1 to enable AIS forward on USB-Serial / 2 for ADC voltage to support calibration
#define UDP_Forwarding 1   // Set to 1 for forwarding AIS from serial2 to UDP brodcast
#define HighTempAlarm 12   // Alarm level for fridge temperature (higher)
#define LowVoltageAlarm 11 // Alarm level for battery voltage (lower)

#define ADC_Calibration_Value 34.3 // The real value depends on the true resistor values for the ADC input (100K / 27 K)

// WiFi config — loaded from NVS ("wifi_cfg") at boot, editable via /settings
static uint8_t wifiMode    = 0;            // 0=AP only  1=STA only  2=AP+STA
static char    apSsid[33]  = "NMEA2000-GW";
static char    apPass[65]  = "nmea2000";
static char    staSsid[33] = "";
static char    staPass[65] = "";
static bool    staConnected = false;

// Sensor live-data flags — used in MOCK_MODE 1 to switch from mock to real per source.
// Each flag latches true once the sensor provides a valid reading and never resets.
static bool n2kActive     = false;  // set on first NMEA2000 message received
static bool ds18b20Active = false;  // set when DS18B20 returns a valid temperature
static bool adcActive     = false;  // set when battery ADC reads > 5.0 V

// MQTT config — loaded from NVS ("mqtt_cfg") at boot, editable via /settings
static bool mqttEnabled      = false;
static char mqttHost[129]    = "";
static uint16_t mqttPort     = 8883;
static char mqttUser[65]     = "";
static char mqttPass[129]    = "";
static char mqttTopic[65]    = "boat/myyacht";
static bool mqttConnected    = false;

static WiFiClientSecure* mqttTlsClient = nullptr;
static PubSubClient*     mqttClient    = nullptr;

// AP always uses a fixed subnet so the dashboard IP is predictable
IPAddress AP_local_ip(192, 168, 15, 1);
IPAddress AP_gateway(192, 168, 15, 1);
IPAddress AP_subnet(255, 255, 255, 0);

const uint16_t ServerPort = 2222; // Define the port, where server sends data. Use this e.g. on OpenCPN. Use 39150 for Navionis AIS

// UPD broadcast for Navionics, OpenCPN, etc.
char udpAddress[16] = "192.168.15.255"; // AP subnet broadcast; updated to 255.255.255.255 in STA/Both modes
const int udpPort = 2000; // port 2000 lets think Navionics it is an DY WLN10 device

// Create UDP instance
WiFiUDP udp;

// Struct to update BoatData. See BoatData.h for content
tBoatData BoatData;

int NodeAddress;  // To store last Node Address

Preferences preferences;             // Nonvolatile storage on ESP32 - To store LastDeviceAddress

int buzzerPin = 12;   // Buzzer on GPIO 12
int buttonPin = 0;    // Button on GPIO 0 to acknowledge alarm with buzzer
int alarmstate = false; // Alarm state (low voltage/temperature)
int acknowledge = false; // Acknowledge for alarm, button pressed

OneButton button(buttonPin, false); // The OneButton library is used to debounce the acknowledge button

const size_t MaxClients = 10;
bool SendNMEA0183Conversion = true; // Do we send NMEA2000 -> NMEA0183 conversion
bool SendSeaSmart = false; // Do we send NMEA2000 messages in SeaSmart format

WiFiServer server(ServerPort, MaxClients);
WiFiServer json(90);

using tWiFiClientPtr = std::shared_ptr<WiFiClient>;
LinkedList<tWiFiClientPtr> clients;

tN2kDataToNMEA0183 tN2kDataToNMEA0183(&NMEA2000, 0);

// Set the information for other bus devices, which messages we support
const unsigned long TransmitMessages[] PROGMEM = {127489L, // Engine dynamic
                                                  130312L, // Temperature (Ruuvi)
                                                  130313L, // Humidity    (Ruuvi)
                                                  130314L, // Pressure    (Ruuvi)
                                                  0
                                                 };
const unsigned long ReceiveMessages[] PROGMEM = {/*126992L,*/ // System time
      127250L, // Heading
      127258L, // Magnetic variation
      128259UL,// Boat speed
      128267UL,// Depth
      129025UL,// Position
      129026L, // COG and SOG
      129029L, // GNSS
      130306L, // Wind
      128275UL,// Log
      127245UL,// Rudder
      0
    };

// Forward declarations
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg);
void SendNMEA0183Message(const tNMEA0183Msg &NMEA0183Msg);
void loadWifiConfig();
void startWifi();
void factoryResetWifi();
void longPressFactoryReset();
void handle_ruuvi_labels_get();
void handle_ruuvi_labels_set();
void loadMqttConfig();
void mqttReconnect();
void mqttPublish();
void handle_mqtt_config();
void handle_mqtt_save();

// Data wire for teperature (Dallas DS18B20) is plugged into GPIO 13 on the ESP32
#define ONE_WIRE_BUS 13
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

WebServer webserver(80);

#define MiscSendOffset 120
#define SlowDataUpdatePeriod 1000  // Time between CAN Messages sent

// Battery voltage is connected GPIO 34 (Analog ADC1_CH6)
const int ADCpin = 34;

tLocalSensors LocalSensors;

// Task handle for OneWire read (Core 0 on ESP32)
TaskHandle_t Task1;

// Serial port 2 config (GPIO 16)
const int baudrate = 38400;
const int rs_config = SERIAL_8N1;

// Buffer config

#define MAX_NMEA0183_MESSAGE_SIZE 150 // For AIS
char buff[MAX_NMEA0183_MESSAGE_SIZE];

// NMEA message for AIS receiving and multiplexing
tNMEA0183Msg NMEA0183Msg;
tNMEA0183 NMEA0183;


void debug_log(char* str) {
#if ENABLE_DEBUG_LOG == 1
  Serial.println(str);
#endif
}



void setup() {
pinMode(INTERNAL_LED, OUTPUT);
digitalWrite(INTERNAL_LED, HIGH);

  uint8_t chipid[6];
  uint32_t id = 0;
  int i = 0;
  int wifi_retry = 0;

  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  button.attachClick(clickedIt);
  // Long-press factory reset is handled by a raw digitalRead() timer in loop()
  // rather than OneButton, because the board uses activeLow=false which makes
  // OneButton see the pulled-up pin as permanently pressed.

  // Init USB serial port
  Serial.begin(115200);

  // Init AIS serial port 2
  Serial2.begin(baudrate, rs_config);
  NMEA0183.Begin(&Serial2, 3, baudrate);

  loadWifiConfig();
  startWifi();

  loadMqttConfig();

  // Start Ruuvi BLE scanner (passive, co-exists with WiFi)
  ruuviInit();

  // Start OneWire
  sensors.begin();

  // Start TCP server
  server.begin();

  // Start JSON server
  json.begin();


  // Start Web Server
  webserver.on("/",               HTTP_GET,  Ereignis_Index);
  webserver.on("/ADC.txt",        HTTP_GET,  Ereignis_ADC);
  webserver.on("/data",           HTTP_GET,  handle_data_json);
  webserver.on("/sw.js",          HTTP_GET,  handle_sw_js);
  webserver.on("/leaflet.js",     HTTP_GET,  handle_leaflet_js);
  webserver.on("/settings",       HTTP_GET,  handle_settings);
  webserver.on("/api/status",     HTTP_GET,  handle_api_status);
  webserver.on("/api/wifi/scan",  HTTP_GET,  handle_wifi_scan);
  webserver.on("/api/wifi/save",  HTTP_POST, handle_wifi_save);
  webserver.on("/api/wifi/reset",    HTTP_POST, handle_wifi_reset);
  webserver.on("/api/ruuvi/labels",  HTTP_GET,  handle_ruuvi_labels_get);
  webserver.on("/api/ruuvi/labels",  HTTP_POST, handle_ruuvi_labels_set);
  webserver.on("/api/mqtt/config",   HTTP_GET,  handle_mqtt_config);
  webserver.on("/api/mqtt/save",     HTTP_POST, handle_mqtt_save);
  webserver.onNotFound(handleNotFound);

  webserver.begin();
  Serial.println("HTTP server started");

  // Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega

  NMEA2000.SetN2kCANMsgBufSize(8);
  NMEA2000.SetN2kCANReceiveFrameBufSize(250);
  NMEA2000.SetN2kCANSendFrameBufSize(250);

  esp_efuse_mac_get_default(chipid);
  for (i = 0; i < 6; i++) id += (chipid[i] << (7 * i));

  // Set product information
  NMEA2000.SetProductInformation("1", // Manufacturer's Model serial code
                                 100, // Manufacturer's product code
                                 "NMEA 2000 WiFi Gateway",  // Manufacturer's Model ID
                                 "1.0.2.25 (2019-07-07)",  // Manufacturer's Software version code
                                 "1.0.2.0 (2019-07-07)" // Manufacturer's Model version
                                );
  // Set device information
  NMEA2000.SetDeviceInformation(id, // Unique number. Use e.g. Serial number. Id is generated from MAC-Address
                                130, // Device function=Analog to NMEA 2000 Gateway. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                25, // Device class=Inter/Intranetwork Device. See codes on  http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                2046 // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
                               );

  // If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below

  NMEA2000.SetForwardType(tNMEA2000::fwdt_Text); // Show in clear text. Leave uncommented for default Actisense format.

  preferences.begin("nvs", false);                          // Open nonvolatile storage (nvs)
  NodeAddress = preferences.getInt("LastNodeAddress", 32);  // Read stored last NodeAddress, default 32
  preferences.end();

  Serial.printf("NodeAddress=%d\n", NodeAddress);

  NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, NodeAddress);

  NMEA2000.ExtendTransmitMessages(TransmitMessages);
  NMEA2000.ExtendReceiveMessages(ReceiveMessages);
  NMEA2000.AttachMsgHandler(&tN2kDataToNMEA0183); // NMEA 2000 -> NMEA 0183 conversion
  NMEA2000.SetMsgHandler(HandleNMEA2000Msg); // Also send all NMEA2000 messages in SeaSmart format

  tN2kDataToNMEA0183.SetSendNMEA0183MessageCallback(SendNMEA0183Message);

  NMEA2000.Open();

  // Create task for core 0, loop() runs on core 1
 
  xTaskCreatePinnedToCore(
    GetTemperature, /* Function to implement the task */
    "Task1", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &Task1,  /* Task handle. */
    0); /* Core where the task should run */
  delay(200);
      digitalWrite(INTERNAL_LED, LOW);
}

// This task runs isolated on core 0 because sensors.requestTemperatures() is slow and blocking for about 750 ms
void GetTemperature( void * parameter) {
  for (;;) {
    digitalWrite(INTERNAL_LED, HIGH);
    sensors.requestTemperatures();
    vTaskDelay(100);
    float tmp = sensors.getTempCByIndex(0);
    if (tmp != DEVICE_DISCONNECTED_C) {
      LocalSensors.FridgeTemp = tmp;
      ds18b20Active = true;
    }
    vTaskDelay(250);
    digitalWrite(INTERNAL_LED, LOW);
  }
}

//*****************************************************************************

void Ereignis_Index()    // Wenn "http://<ip address>/" aufgerufen wurde
{
  webserver.send_P(200, "text/html", indexHTML);
}

void Ereignis_ADC()     // Wenn "http://<ip address>/ADC.txt" aufgerufen wurde
{

  webserver.sendHeader("Cache-Control", "no-cache");  // Sehr wichtig !!!!!!!!!!!!!!!!!!!
  webserver.send(200, "text/plain", String(LocalSensors.FridgeTemp));
}

void handle_sw_js()
{
  webserver.sendHeader("Cache-Control", "no-cache");
  webserver.send(200, "application/javascript", swJS);
}

void handle_leaflet_js()
{
  webserver.sendHeader("Cache-Control", "public, max-age=604800"); // 1 week
  webserver.sendHeader("Content-Encoding", "gzip");
  webserver.send_P(200, "application/javascript", (const char*)leafletJS, leafletJSLen);
}

void handleNotFound()
{
  webserver.send(404, "text/plain", "File Not Found\n\n");
}

// ── WiFi configuration ────────────────────────────────────────────────────────

void loadWifiConfig() {
  preferences.begin("wifi_cfg", true);
  wifiMode = preferences.getUChar("mode", 0);
  String s;
  s = preferences.getString("ap_ssid", "NMEA2000-GW"); strlcpy(apSsid,  s.c_str(), sizeof(apSsid));
  s = preferences.getString("ap_pass", "nmea2000");    strlcpy(apPass,  s.c_str(), sizeof(apPass));
  s = preferences.getString("sta_ssid", "");           strlcpy(staSsid, s.c_str(), sizeof(staSsid));
  s = preferences.getString("sta_pass", "");           strlcpy(staPass, s.c_str(), sizeof(staPass));
  preferences.end();
  Serial.printf("WiFi config: mode=%d ap=%s sta=%s\n", wifiMode, apSsid, staSsid);
}

void startWifi() {
  WiFi.disconnect(true);
  delay(100);

  bool doAP  = (wifiMode == 0 || wifiMode == 2);
  bool doSTA = (wifiMode == 1 || wifiMode == 2);

  if      (doAP && doSTA) WiFi.mode(WIFI_AP_STA);
  else if (doSTA)         WiFi.mode(WIFI_STA);
  else                    WiFi.mode(WIFI_AP);

  if (doAP) {
    if (strlen(apPass) >= 8)
      WiFi.softAP(apSsid, apPass);
    else
      WiFi.softAP(apSsid);           // open network
    delay(100);
    WiFi.softAPConfig(AP_local_ip, AP_gateway, AP_subnet);
    Serial.printf("AP started: %s  IP: %s\n", apSsid, WiFi.softAPIP().toString().c_str());
    strlcpy(udpAddress, "192.168.15.255", sizeof(udpAddress));
  }

  if (doSTA && strlen(staSsid) > 0) {
    WiFi.begin(staSsid, staPass);
    int retries = 0;
    Serial.printf("Connecting to %s", staSsid);
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500); Serial.print("."); retries++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      staConnected = true;
      Serial.printf("STA connected  IP: %s\n", WiFi.localIP().toString().c_str());
      strlcpy(udpAddress, "255.255.255.255", sizeof(udpAddress));
    } else {
      Serial.println("STA connection failed");
      if (wifiMode == 1) {
        // STA-only but no connection — fall back to AP so user can reconfigure
        WiFi.mode(WIFI_AP);
        if (strlen(apPass) >= 8) WiFi.softAP(apSsid, apPass); else WiFi.softAP(apSsid);
        delay(100);
        WiFi.softAPConfig(AP_local_ip, AP_gateway, AP_subnet);
        Serial.printf("Fallback AP: %s\n", WiFi.softAPIP().toString().c_str());
        strlcpy(udpAddress, "192.168.15.255", sizeof(udpAddress));
      }
    }
  }

  if (MDNS.begin("nmea2000")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS: nmea2000.local");
  }
}

void factoryResetWifi() {
  Serial.println("Factory reset: clearing wifi_cfg");
  preferences.begin("wifi_cfg", false);
  preferences.clear();
  preferences.end();
}

void longPressFactoryReset() {
  factoryResetWifi();
  // Three short buzzes to confirm
  for (int i = 0; i < 3; i++) {
    digitalWrite(buzzerPin, HIGH); delay(120);
    digitalWrite(buzzerPin, LOW);  delay(120);
  }
  delay(200);
  ESP.restart();
}

// ── Settings web handlers ─────────────────────────────────────────────────────

void handle_settings() {
  webserver.send_P(200, "text/html", settingsHTML);
}

void handle_api_status() {
  JsonDocument doc;
  doc["mode"]          = wifiMode;
  doc["ap_ssid"]       = apSsid;
  doc["ap_ip"]         = WiFi.softAPIP().toString();
  doc["sta_ssid"]      = staSsid;
  doc["sta_connected"] = staConnected;
  doc["sta_ip"]        = WiFi.localIP().toString();
  String out; serializeJson(doc, out);
  webserver.sendHeader("Cache-Control", "no-cache");
  webserver.sendHeader("Access-Control-Allow-Origin", "*");
  webserver.send(200, "application/json", out);
}

void handle_wifi_scan() {
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray nets = doc["networks"].to<JsonArray>();
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject obj = nets.add<JsonObject>();
    obj["ssid"]   = WiFi.SSID(i);
    obj["rssi"]   = WiFi.RSSI(i);
    obj["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
  }
  WiFi.scanDelete();
  String out; serializeJson(doc, out);
  webserver.sendHeader("Cache-Control", "no-cache");
  webserver.sendHeader("Access-Control-Allow-Origin", "*");
  webserver.send(200, "application/json", out);
}

void handle_wifi_save() {
  if (!webserver.hasArg("plain")) {
    webserver.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, webserver.arg("plain"))) {
    webserver.send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
    return;
  }
  uint8_t mode = doc["mode"] | 0;
  if (mode > 2) mode = 0;

  preferences.begin("wifi_cfg", false);
  preferences.putUChar("mode", mode);
  // Only overwrite credentials when the user supplied a non-empty value so
  // leaving the password field blank preserves the existing stored password.
  String v;
  v = doc["ap_ssid"] | ""; if (v.length() > 0) preferences.putString("ap_ssid", v);
  v = doc["ap_pass"]  | ""; if (v.length() > 0) preferences.putString("ap_pass",  v);
  v = doc["sta_ssid"] | ""; preferences.putString("sta_ssid", v); // allow clearing
  v = doc["sta_pass"]  | ""; if (v.length() > 0) preferences.putString("sta_pass",  v);
  preferences.end();

  webserver.sendHeader("Cache-Control", "no-cache");
  webserver.sendHeader("Access-Control-Allow-Origin", "*");
  webserver.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  ESP.restart();
}

void handle_wifi_reset() {
  factoryResetWifi();
  webserver.sendHeader("Cache-Control", "no-cache");
  webserver.sendHeader("Access-Control-Allow-Origin", "*");
  webserver.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  ESP.restart();
}

// ── Ruuvi label handlers ──────────────────────────────────────────────────────

void handle_ruuvi_labels_get() {
  JsonDocument doc;
  JsonArray arr = doc["sensors"].to<JsonArray>();
  tRuuviSensor snap[RUUVI_MAX_SENSORS];
  if (ruuviMutex && xSemaphoreTake(ruuviMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    memcpy(snap, ruuviSensors, sizeof(snap));
    xSemaphoreGive(ruuviMutex);
  } else {
    memcpy(snap, ruuviSensors, sizeof(snap));
  }
  for (int i = 0; i < RUUVI_MAX_SENSORS; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["slot"]   = i;
    o["active"] = snap[i].active;
    o["mac"]    = snap[i].mac;
    o["label"]  = snap[i].label;
    o["stale"]  = ruuviIsStale(i);
  }
  String out; serializeJson(doc, out);
  webserver.sendHeader("Cache-Control", "no-cache");
  webserver.sendHeader("Access-Control-Allow-Origin", "*");
  webserver.send(200, "application/json", out);
}

void handle_ruuvi_labels_set() {
  if (!webserver.hasArg("plain")) {
    webserver.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, webserver.arg("plain"))) {
    webserver.send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
    return;
  }
  for (JsonObject o : doc["sensors"].as<JsonArray>()) {
    int slot = o["slot"] | -1;
    const char* lbl = o["label"] | "";
    if (slot >= 0 && slot < RUUVI_MAX_SENSORS && strlen(lbl) > 0)
      ruuviSaveLabel(slot, lbl);
  }
  webserver.sendHeader("Cache-Control", "no-cache");
  webserver.sendHeader("Access-Control-Allow-Origin", "*");
  webserver.send(200, "application/json", "{\"ok\":true}");
}


// ── MQTT ─────────────────────────────────────────────────────────────────────

void loadMqttConfig() {
  Preferences prefs;
  prefs.begin("mqtt_cfg", true);
  mqttEnabled = prefs.getBool("enabled", false);
  mqttPort    = prefs.getUShort("port", 8883);
  String s;
  s = prefs.getString("host",  ""); strlcpy(mqttHost,  s.c_str(), sizeof(mqttHost));
  s = prefs.getString("user",  ""); strlcpy(mqttUser,  s.c_str(), sizeof(mqttUser));
  s = prefs.getString("pass",  ""); strlcpy(mqttPass,  s.c_str(), sizeof(mqttPass));
  s = prefs.getString("topic", "boat/myyacht"); strlcpy(mqttTopic, s.c_str(), sizeof(mqttTopic));
  prefs.end();
  Serial.printf("MQTT config: enabled=%d host=%s\n", mqttEnabled, mqttHost);
}

void mqttReconnect() {
  if (strlen(mqttHost) == 0) return;
  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt < 30000) return;  // back-off: retry every 30 s
  lastAttempt = millis();

  // Lazy-create so TLS context only allocates heap when MQTT is actually enabled
  if (!mqttTlsClient) mqttTlsClient = new WiFiClientSecure();
  if (!mqttClient)    mqttClient    = new PubSubClient(*mqttTlsClient);

  mqttTlsClient->setInsecure();  // encrypt but skip cert verification
  mqttClient->setServer(mqttHost, mqttPort);
  mqttClient->setKeepAlive(60);
  mqttClient->setSocketTimeout(10);

  char clientId[32];
  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(clientId, sizeof(clientId), "nmea-gw-%02X%02X", mac[4], mac[5]);

  Serial.printf("MQTT connecting to %s as %s\n", mqttHost, clientId);
  if (mqttClient->connect(clientId, mqttUser, mqttPass)) {
    mqttConnected = true;
    Serial.println("MQTT connected");
    char statusTopic[96];
    snprintf(statusTopic, sizeof(statusTopic), "%s/status", mqttTopic);
    mqttClient->publish(statusTopic, "online", true);
  } else {
    mqttConnected = false;
    Serial.printf("MQTT failed, rc=%d\n", mqttClient->state());
  }
}

void mqttPublish() {
  if (!mqttClient || !mqttClient->connected()) return;

  // Build full telemetry JSON — same fields as /data plus Ruuvi array
  JsonDocument doc;

  bool useMockN2k    = (MOCK_MODE == 2) || (MOCK_MODE == 1 && !n2kActive);
  bool useMockFridge = (MOCK_MODE == 2) || (MOCK_MODE == 1 && !ds18b20Active);
  bool useMockAdc    = (MOCK_MODE == 2) || (MOCK_MODE == 1 && !adcActive);

  const char* dataSource =
    (MOCK_MODE == 0)                               ? "live"    :
    (MOCK_MODE == 2)                               ? "mock"    :
    (!useMockN2k && !useMockFridge && !useMockAdc) ? "live"    :
    (useMockN2k  && useMockFridge  && useMockAdc)  ? "mock"    : "partial";
  doc["dataSource"]       = dataSource;

  doc["Latitude"]         = useMockN2k ? MOCK_LATITUDE    : BoatData.Latitude;
  doc["Longitude"]        = useMockN2k ? MOCK_LONGITUDE   : BoatData.Longitude;
  doc["Heading"]          = useMockN2k ? MOCK_HEADING     : BoatData.Heading;
  doc["COG"]              = useMockN2k ? MOCK_COG         : BoatData.COG;
  doc["SOG"]              = useMockN2k ? MOCK_SOG         : BoatData.SOG;
  doc["STW"]              = useMockN2k ? MOCK_STW         : BoatData.STW;
  doc["AWS"]              = useMockN2k ? MOCK_AWS         : BoatData.AWS;
  doc["TWS"]              = useMockN2k ? MOCK_TWS         : BoatData.TWS;
  doc["AWA"]              = useMockN2k ? MOCK_AWA         : BoatData.AWA;
  doc["TWA"]              = useMockN2k ? MOCK_TWA         : BoatData.TWA;
  doc["TWD"]              = useMockN2k ? MOCK_TWD         : BoatData.TWD;
  doc["WaterDepth"]       = useMockN2k ? MOCK_WATER_DEPTH : BoatData.WaterDepth;
  doc["WaterTemperature"] = useMockN2k ? MOCK_WATER_TEMP  : BoatData.WaterTemperature;

  doc["FridgeTemperature"] = useMockFridge ? MOCK_FRIDGE_TEMP     : LocalSensors.FridgeTemp;
  doc["BatteryVoltage"]    = useMockAdc    ? MOCK_BATTERY_VOLTAGE : LocalSensors.BatteryVoltage;
  doc["timestamp"]         = millis() / 1000;  // seconds since boot

  // Ruuvi sensors
  {
    tRuuviSensor snap[RUUVI_MAX_SENSORS];
    if (ruuviMutex && xSemaphoreTake(ruuviMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      memcpy(snap, ruuviSensors, sizeof(snap));
      xSemaphoreGive(ruuviMutex);
    } else {
      memcpy(snap, ruuviSensors, sizeof(snap));
    }
    JsonArray arr = doc["ruuvi"].to<JsonArray>();
    for (int i = 0; i < RUUVI_MAX_SENSORS; i++) {
      bool stale = ruuviIsStale(i);
      if (!snap[i].active || stale) continue;
      JsonObject sr = arr.add<JsonObject>();
      sr["label"] = snap[i].label;
      sr["mac"]   = snap[i].mac;
      if (!isnan(snap[i].temperature)) sr["temp"] = snap[i].temperature;
      if (!isnan(snap[i].humidity))    sr["hum"]  = snap[i].humidity;
      if (!isnan(snap[i].pressure))    sr["pres"] = snap[i].pressure;
      if (snap[i].batteryMv > 0)       sr["batt"] = snap[i].batteryMv;
      sr["rssi"]     = snap[i].rssi;
      sr["movement"] = snap[i].movementCounter;
      if (!isnan(snap[i].heel))  sr["heel"]  = snap[i].heel;
      if (!isnan(snap[i].pitch)) sr["pitch"] = snap[i].pitch;
    }
  }

  char pubTopic[96];
  snprintf(pubTopic, sizeof(pubTopic), "%s/telemetry", mqttTopic);

  String payload;
  serializeJson(doc, payload);

  mqttClient->setBufferSize(1024);
  if (!mqttClient->publish(pubTopic, payload.c_str())) {
    Serial.println("MQTT publish failed (payload too large?)");
  }
}

void handle_mqtt_config() {
  JsonDocument doc;
  doc["enabled"]   = mqttEnabled;
  doc["host"]      = mqttHost;
  doc["port"]      = mqttPort;
  doc["user"]      = mqttUser;
  doc["topic"]     = mqttTopic;
  doc["connected"] = mqttConnected && mqttClient && mqttClient->connected();
  String out; serializeJson(doc, out);
  webserver.sendHeader("Cache-Control", "no-cache");
  webserver.sendHeader("Access-Control-Allow-Origin", "*");
  webserver.send(200, "application/json", out);
}

void handle_mqtt_save() {
  if (!webserver.hasArg("plain")) {
    webserver.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, webserver.arg("plain"))) {
    webserver.send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
    return;
  }
  mqttEnabled = doc["enabled"] | false;
  mqttPort    = doc["port"]    | 8883;

  Preferences prefs;
  prefs.begin("mqtt_cfg", false);
  prefs.putBool("enabled", mqttEnabled);
  prefs.putUShort("port", mqttPort);
  String v;
  v = doc["host"]  | ""; if (v.length() > 0) { strlcpy(mqttHost,  v.c_str(), sizeof(mqttHost));  prefs.putString("host",  v); }
  v = doc["user"]  | ""; if (v.length() > 0) { strlcpy(mqttUser,  v.c_str(), sizeof(mqttUser));  prefs.putString("user",  v); }
  v = doc["pass"]  | ""; if (v.length() > 0) { strlcpy(mqttPass,  v.c_str(), sizeof(mqttPass));  prefs.putString("pass",  v); }
  v = doc["topic"] | ""; if (v.length() > 0) { strlcpy(mqttTopic, v.c_str(), sizeof(mqttTopic)); prefs.putString("topic", v); }
  prefs.end();

  // Disconnect so mqttReconnect() picks up the new credentials on next loop tick
  if (mqttClient) mqttClient->disconnect();
  mqttConnected = false;

  webserver.sendHeader("Cache-Control", "no-cache");
  webserver.sendHeader("Access-Control-Allow-Origin", "*");
  webserver.send(200, "application/json", "{\"ok\":true}");
}


//*****************************************************************************
void SendBufToClients(const char *buf) {
  for (auto it = clients.begin() ; it != clients.end(); it++) {
    if ( (*it) != NULL && (*it)->connected() ) {
      (*it)->println(buf);
    }
  }
}

#define MAX_NMEA2000_MESSAGE_SEASMART_SIZE 500
//*****************************************************************************
//NMEA 2000 message handler
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg) {
  n2kActive = true;  // latch: bus is live

  if ( !SendSeaSmart ) return;

  char buf[MAX_NMEA2000_MESSAGE_SEASMART_SIZE];
  if ( N2kToSeasmart(N2kMsg, millis(), buf, MAX_NMEA2000_MESSAGE_SEASMART_SIZE) == 0 ) return;
  SendBufToClients(buf);
}


//*****************************************************************************
void SendNMEA0183Message(const tNMEA0183Msg &NMEA0183Msg) {
  if ( !SendNMEA0183Conversion ) return;

  char buf[MAX_NMEA0183_MESSAGE_SIZE];
  if ( !NMEA0183Msg.GetMessage(buf, MAX_NMEA0183_MESSAGE_SIZE) ) return;
  SendBufToClients(buf);
}


bool IsTimeToUpdate(unsigned long NextUpdate) {
  return (NextUpdate < millis());
}
unsigned long InitNextUpdate(unsigned long Period, unsigned long Offset = 0) {
  return millis() + Period + Offset;
}

void SetNextUpdate(unsigned long &NextUpdate, unsigned long Period) {
  while ( NextUpdate < millis() ) NextUpdate += Period;
}


void SendN2kEngine() {
  static unsigned long SlowDataUpdated = InitNextUpdate(SlowDataUpdatePeriod, MiscSendOffset);
  tN2kMsg N2kMsg;

  if ( IsTimeToUpdate(SlowDataUpdated) ) {
    SetNextUpdate(SlowDataUpdated, SlowDataUpdatePeriod);

    SetN2kEngineDynamicParam(N2kMsg, 0, N2kDoubleNA, N2kDoubleNA, CToKelvin(LocalSensors.FridgeTemp), LocalSensors.BatteryVoltage, N2kDoubleNA, N2kDoubleNA, N2kDoubleNA, N2kDoubleNA, N2kInt8NA, N2kInt8NA, true);
    NMEA2000.SendMsg(N2kMsg);
  }
}


String addNMEAChecksum(const String &sentence) {
  byte checksum = 0;
  for (size_t i = 0; i < sentence.length(); i++) checksum ^= (byte)sentence[i];
  char buf[8];
  sprintf(buf, "*%02X\r\n", checksum);
  return "$" + sentence + String(buf);
}

#if MOCK_MODE > 0
void SendHardcodedNMEA() {
  // Partial mode: once the NMEA2000 bus is live, real data flows — stop injecting mock.
  if (MOCK_MODE == 1 && n2kActive) return;

  static unsigned long nextSend = 0;
  if (millis() < nextSend) return;
  nextSend = millis() + 1000;

  // Mirror mock nav values into BoatData so the /data endpoint and UDP broadcast stay in sync.
  BoatData.Latitude         = MOCK_LATITUDE;
  BoatData.Longitude        = MOCK_LONGITUDE;
  BoatData.Heading          = MOCK_HEADING;
  BoatData.COG              = MOCK_COG;
  BoatData.SOG              = MOCK_SOG;
  BoatData.STW              = MOCK_STW;
  BoatData.TWD              = MOCK_TWD;
  BoatData.TWS              = MOCK_TWS;
  BoatData.TWA              = MOCK_TWA;
  BoatData.AWS              = MOCK_AWS;
  BoatData.AWA              = MOCK_AWA;
  BoatData.WaterDepth       = MOCK_WATER_DEPTH;
  BoatData.WaterTemperature = MOCK_WATER_TEMP;
  BoatData.RudderPosition   = MOCK_RUDDER_POSITION;
  BoatData.TripLog          = MOCK_TRIP_LOG;
  BoatData.Log              = MOCK_TOTAL_LOG;
  BoatData.GPSTime          = MOCK_GPS_TIME;
  BoatData.Variation        = MOCK_VARIATION;

  // Build NMEA sentences from the same constants so UDP output stays in sync
  int    latD = (int)fabs(MOCK_LATITUDE);
  float  latM = (fabs(MOCK_LATITUDE) - latD) * 60.0f;
  int    lonD = (int)fabs(MOCK_LONGITUDE);
  float  lonM = (fabs(MOCK_LONGITUDE) - lonD) * 60.0f;
  char   latS[10], lonS[11];
  snprintf(latS, sizeof(latS), "%02d%05.2f", latD, latM);
  snprintf(lonS, sizeof(lonS), "%03d%05.2f", lonD, lonM);
  char   latH = MOCK_LATITUDE  >= 0 ? 'N' : 'S';
  char   lonH = MOCK_LONGITUDE >= 0 ? 'E' : 'W';

  // Fridge XDR: use mock value unless DS18B20 has already provided a real reading
  float fridgeForNMEA = (MOCK_MODE == 2 || !ds18b20Active) ? MOCK_FRIDGE_TEMP : LocalSensors.FridgeTemp;

  char rmcBuf[80], vtgBuf[60], mwvRBuf[40], mwvTBuf[40], dbtBuf[50], xdrBuf[40];
  snprintf(rmcBuf,  sizeof(rmcBuf),  "GPRMC,120000.00,A,%s,%c,%s,%c,%.1f,%.1f,140526,,",
           latS, latH, lonS, lonH, (float)MOCK_SOG, (float)MOCK_COG);
  snprintf(vtgBuf,  sizeof(vtgBuf),  "GPVTG,%.1f,T,,M,%.1f,N,%.2f,K,A",
           (float)MOCK_COG, (float)MOCK_SOG, (float)(MOCK_SOG * 1.852));
  snprintf(mwvRBuf, sizeof(mwvRBuf), "WIMWV,%.1f,R,%.1f,N,A", (float)MOCK_AWA, (float)MOCK_AWS);
  snprintf(mwvTBuf, sizeof(mwvTBuf), "WIMWV,%.1f,T,%.1f,N,A", (float)MOCK_TWA, (float)MOCK_TWS);
  snprintf(dbtBuf,  sizeof(dbtBuf),  "SDDBT,%.2f,f,%.2f,M,%.2f,F",
           (float)(MOCK_WATER_DEPTH * 3.28084), (float)MOCK_WATER_DEPTH,
           (float)(MOCK_WATER_DEPTH * 0.546807));
  snprintf(xdrBuf,  sizeof(xdrBuf),  "IIXDR,C,%.2f,C,FRIDGE", fridgeForNMEA);

  udp.beginPacket(udpAddress, udpPort);
  udp.print(addNMEAChecksum(String(rmcBuf)));
  udp.print(addNMEAChecksum(String(vtgBuf)));
  udp.print(addNMEAChecksum(String(mwvRBuf)));
  udp.print(addNMEAChecksum(String(mwvTBuf)));
  udp.print(addNMEAChecksum(String(dbtBuf)));
  udp.print(addNMEAChecksum(String(xdrBuf)));
  udp.endPacket();
}
#endif // MOCK_MODE > 0

//*****************************************************************************
void AddClient(WiFiClient &client) {
  Serial.println("New Client.");
  clients.push_back(tWiFiClientPtr(new WiFiClient(client)));
}

//*****************************************************************************
void StopClient(LinkedList<tWiFiClientPtr>::iterator &it) {
  Serial.println("Client Disconnected.");
  (*it)->stop();
  it = clients.erase(it);
}

//*****************************************************************************
void CheckConnections() {
  WiFiClient client = server.available();   // listen for incoming clients

  if ( client ) AddClient(client);

  for (auto it = clients.begin(); it != clients.end(); it++) {
    if ( (*it) != NULL ) {
      if ( !(*it)->connected() ) {
        StopClient(it);
      } else {
        if ( (*it)->available() ) {
          char c = (*it)->read();
          if ( c == 0x03 ) StopClient(it); // Close connection by ctrl-c
        }
      }
    } else {
      it = clients.erase(it); // Should have been erased by StopClient
    }
  }
}

// ReadVoltage is used to improve the linearity of the ESP32 ADC see: https://github.com/G6EJD/ESP32-ADC-Accuracy-Improvement-function

double ReadVoltage(byte pin) {
  double reading = analogRead(pin); // Reference voltage is 3v3 so maximum reading is 3v3 = 4095 in range 0 to 4095
  if (reading < 1 || reading > 4095) return 0;
  // return -0.000000000009824 * pow(reading,3) + 0.000000016557283 * pow(reading,2) + 0.000854596860691 * reading + 0.065440348345433;
  return (-0.000000000000016 * pow(reading, 4) + 0.000000000118171 * pow(reading, 3) - 0.000000301211691 * pow(reading, 2) + 0.001109019271794 * reading + 0.034143524634089) * 1000;
} // Added an improved polynomial, use either, comment out as required


void clickedIt() {  // Button was pressed
  acknowledge = !acknowledge;
}



void handle_json() {

  WiFiClient client = json.available();

  // Do we have a client?
  if (!client) return;

  // Serial.println(F("New client"));

  // Read the request (we ignore the content in this example)
  while (client.available()) client.read();

  // Allocate JsonBuffer
  JsonDocument root;

  root["Latitude"] = BoatData.Latitude;
  root["Longitude"] = BoatData.Longitude;
  root["Heading"] = BoatData.Heading;
  root["COG"] = BoatData.COG;
  root["SOG"] = BoatData.SOG;
  root["STW"] = BoatData.STW;
  root["AWS"] = BoatData.AWS;
  root["TWS"] = BoatData.TWS;
  root["MaxAws"] = BoatData.MaxAws;
  root["MaxTws"] = BoatData.MaxTws;
  root["AWA"] = BoatData.AWA;
  root["TWA"] = BoatData.TWA;
  root["TWD"] = BoatData.TWD;
  root["TripLog"] = BoatData.TripLog;
  root["Log"] = BoatData.Log;
  root["RudderPosition"] = BoatData.RudderPosition;
  root["WaterTemperature"] = BoatData.WaterTemperature;
  root["WaterDepth"] = BoatData.WaterDepth;
  root["Variation"] = BoatData.Variation;
  root["Altitude"] = BoatData.Altitude;
  root["GPSTime"] = BoatData.GPSTime;
  root["DaysSince1970"] = BoatData.DaysSince1970;
  root["FridgeTemperature"] = LocalSensors.FridgeTemp;
  root["BatteryVoltage"]    = LocalSensors.BatteryVoltage;


  //Serial.print(F("Sending: "));
  //serializeJson(root, Serial);
  //Serial.println();

  // Write response headers
  client.println("HTTP/1.0 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  // Write JSON document
  serializeJsonPretty(root, client);

  // Disconnect
  client.stop();
}


// JSON endpoint on the main WebServer (port 80, route /data).
// In mock mode: reads directly from MockData.h constants so floating CAN GPIO
// pins (noise on the TWAI controller) cannot overwrite the values.
// In live mode: reads from BoatData (populated by tN2kDataToNMEA0183) and
// LocalSensors (on-board DS18B20 + ADC).  Local sensors always use real hardware.
void handle_data_json() {
  JsonDocument root;

  // Determine which data source to use for each sensor group
  bool useMockN2k    = (MOCK_MODE == 2) || (MOCK_MODE == 1 && !n2kActive);
  bool useMockFridge = (MOCK_MODE == 2) || (MOCK_MODE == 1 && !ds18b20Active);
  bool useMockAdc    = (MOCK_MODE == 2) || (MOCK_MODE == 1 && !adcActive);

  root["Latitude"]         = useMockN2k ? MOCK_LATITUDE        : BoatData.Latitude;
  root["Longitude"]        = useMockN2k ? MOCK_LONGITUDE       : BoatData.Longitude;
  root["Heading"]          = useMockN2k ? MOCK_HEADING         : BoatData.Heading;
  root["COG"]              = useMockN2k ? MOCK_COG             : BoatData.COG;
  root["SOG"]              = useMockN2k ? MOCK_SOG             : BoatData.SOG;
  root["STW"]              = useMockN2k ? MOCK_STW             : BoatData.STW;
  root["AWS"]              = useMockN2k ? MOCK_AWS             : BoatData.AWS;
  root["TWS"]              = useMockN2k ? MOCK_TWS             : BoatData.TWS;
  root["AWA"]              = useMockN2k ? MOCK_AWA             : BoatData.AWA;
  root["TWA"]              = useMockN2k ? MOCK_TWA             : BoatData.TWA;
  root["TWD"]              = useMockN2k ? MOCK_TWD             : BoatData.TWD;
  root["MaxAws"]           = useMockN2k ? MOCK_AWS             : BoatData.MaxAws;
  root["MaxTws"]           = useMockN2k ? MOCK_TWS             : BoatData.MaxTws;
  root["TripLog"]          = useMockN2k ? MOCK_TRIP_LOG        : BoatData.TripLog;
  root["Log"]              = useMockN2k ? MOCK_TOTAL_LOG       : BoatData.Log;
  root["RudderPosition"]   = useMockN2k ? MOCK_RUDDER_POSITION : BoatData.RudderPosition;
  root["WaterTemperature"] = useMockN2k ? MOCK_WATER_TEMP      : BoatData.WaterTemperature;
  root["WaterDepth"]       = useMockN2k ? MOCK_WATER_DEPTH     : BoatData.WaterDepth;
  root["Variation"]        = useMockN2k ? MOCK_VARIATION       : BoatData.Variation;
  root["Altitude"]         = useMockN2k ? 0.0                  : BoatData.Altitude;
  root["GPSTime"]          = useMockN2k ? MOCK_GPS_TIME        : BoatData.GPSTime;
  root["DaysSince1970"]    = useMockN2k ? 0.0                  : (double)BoatData.DaysSince1970;

  root["gpsValid"]   = useMockN2k ? 1 : (fabs(BoatData.Latitude) > 0.001 || fabs(BoatData.Longitude) > 0.001) ? 1 : 0;
  root["windValid"]  = useMockN2k ? 1 : (BoatData.AWS > 0.01 || BoatData.TWS > 0.01) ? 1 : 0;
  root["depthValid"] = useMockN2k ? 1 : (BoatData.WaterDepth > 0.01) ? 1 : 0;
  root["localValid"] = 1;

  const char* dataSource =
    (MOCK_MODE == 0)                               ? "live"    :
    (MOCK_MODE == 2)                               ? "mock"    :
    (!useMockN2k && !useMockFridge && !useMockAdc) ? "live"    :
    (useMockN2k  && useMockFridge  && useMockAdc)  ? "mock"    : "partial";
  root["dataSource"] = dataSource;

  root["FridgeTemperature"] = useMockFridge ? MOCK_FRIDGE_TEMP     : LocalSensors.FridgeTemp;
  root["BatteryVoltage"]    = useMockAdc    ? MOCK_BATTERY_VOLTAGE : LocalSensors.BatteryVoltage;

  // Ruuvi BLE sensors — snapshot under mutex
  {
    tRuuviSensor snap[RUUVI_MAX_SENSORS];
    if (ruuviMutex && xSemaphoreTake(ruuviMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      memcpy(snap, ruuviSensors, sizeof(snap));
      xSemaphoreGive(ruuviMutex);
    } else {
      memcpy(snap, ruuviSensors, sizeof(snap));
    }
    JsonArray arr = root["ruuvi"].to<JsonArray>();
    for (int i = 0; i < RUUVI_MAX_SENSORS; i++) {
      JsonObject sr = arr.add<JsonObject>();
      bool stale = ruuviIsStale(i);
      sr["active"] = snap[i].active;
      sr["stale"]  = stale;
      sr["label"]  = snap[i].label;
      sr["mac"]    = snap[i].mac;
      if (snap[i].active && !stale) {
        if (!isnan(snap[i].temperature)) sr["temp"] = snap[i].temperature;
        if (!isnan(snap[i].humidity))    sr["hum"]  = snap[i].humidity;
        if (!isnan(snap[i].pressure))    sr["pres"] = snap[i].pressure;
        if (snap[i].batteryMv > 0)       sr["batt"] = snap[i].batteryMv;
        sr["rssi"]     = snap[i].rssi;
        sr["movement"] = snap[i].movementCounter;
        if (!isnan(snap[i].heel))  sr["heel"]  = snap[i].heel;
        if (!isnan(snap[i].pitch))  sr["pitch"] = snap[i].pitch;
      }
    }
  }

  String output;
  serializeJson(root, output);
  webserver.sendHeader("Cache-Control", "no-cache");
  webserver.sendHeader("Access-Control-Allow-Origin", "*");
  webserver.send(200, "application/json", output);
}


void loop() {
digitalWrite(INTERNAL_LED, HIGH);

  unsigned int size;
  int wifi_retry;

  webserver.handleClient();
  handle_json();

  if (NMEA0183.GetMessage(NMEA0183Msg)) {  // Get AIS NMEA sentences from serial2

    SendNMEA0183Message(NMEA0183Msg);      // Send to TCP clients

    NMEA0183Msg.GetMessage(buff, MAX_NMEA0183_MESSAGE_SIZE); // send to buffer

#if ENABLE_DEBUG_LOG == 1
    Serial.println(buff);
#endif

#if UDP_Forwarding == 1
    size = strlen(buff);
    udp.beginPacket(udpAddress, udpPort);  // Send to UDP
    udp.write((byte*)buff, size);
    udp.endPacket();
#endif
  }

  LocalSensors.BatteryVoltage = ((LocalSensors.BatteryVoltage * 15) + (ReadVoltage(ADCpin) * ADC_Calibration_Value / 4096)) / 16; // low-pass filter to eliminate ADC spikes
  if (LocalSensors.BatteryVoltage > 5.0) adcActive = true;

  SendN2kEngine();
  // Send Ruuvi environmental data to NMEA2000 bus every 10 s
  {
    static unsigned long lastRuuviN2k = 0;
    if (millis() - lastRuuviN2k >= 10000) {
      lastRuuviN2k = millis();
      tRuuviSensor snap[RUUVI_MAX_SENSORS];
      if (ruuviMutex && xSemaphoreTake(ruuviMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        memcpy(snap, ruuviSensors, sizeof(snap));
        xSemaphoreGive(ruuviMutex);
        tN2kMsg msg;
        for (int i = 0; i < RUUVI_MAX_SENSORS; i++) {
          if (!snap[i].active || ruuviIsStale(i)) continue;
          uint8_t inst = 10 + i;
          if (!isnan(snap[i].temperature)) {
            SetN2kTemperature(msg, 0xFF, inst, N2kts_MainCabinTemperature, CToKelvin(snap[i].temperature));
            NMEA2000.SendMsg(msg);
          }
          if (!isnan(snap[i].humidity)) {
            SetN2kHumidity(msg, 0xFF, inst, N2khs_InsideHumidity, snap[i].humidity);
            NMEA2000.SendMsg(msg);
          }
          if (!isnan(snap[i].pressure)) {
            SetN2kPressure(msg, 0xFF, inst, N2kps_Atmospheric, snap[i].pressure * 100.0f);
            NMEA2000.SendMsg(msg);
          }
        }
      }
    }
  }
#if MOCK_MODE > 0
  SendHardcodedNMEA();
#endif
  CheckConnections();
  NMEA2000.ParseMessages();

  int SourceAddress = NMEA2000.GetN2kSource();
  if (SourceAddress != NodeAddress) { // Save potentially changed Source Address to NVS memory
    NodeAddress = SourceAddress;      // Set new Node Address (to save only once)
    preferences.begin("nvs", false);
    preferences.putInt("LastNodeAddress", SourceAddress);
    preferences.end();
    Serial.printf("Address Change: New Address=%d\n", SourceAddress);
  }

  tN2kDataToNMEA0183.Update(&BoatData);

  // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
  if ( Serial.available() ) {
    Serial.read();
  }

  // Factory reset: physical button held LOW for 5 seconds.
  // Read directly because OneButton is configured activeLow=false (inverted),
  // so OneButton's long-press API would misfire on the pulled-up idle state.
  {
    static unsigned long btnHeldMs = 0;
    static bool btnActive = false;
    if (digitalRead(buttonPin) == LOW) {
      if (!btnActive) { btnHeldMs = millis(); btnActive = true; }
      else if (millis() - btnHeldMs >= 5000) { longPressFactoryReset(); }
    } else {
      btnActive = false;
    }
  }

  // Alarm handling
  button.tick();

#if ENABLE_DEBUG_LOG == 2
  Serial.print("Voltage:"); Serial.println(LocalSensors.BatteryVoltage);
  Serial.print("Temperature: "); Serial.println(LocalSensors.FridgeTemp);
  Serial.println("");
#endif

  alarmstate = false;
  if (LocalSensors.FridgeTemp > HighTempAlarm || LocalSensors.BatteryVoltage < LowVoltageAlarm) {
    alarmstate = true;
  }
  if (alarmstate == true && acknowledge == false) {
    digitalWrite(buzzerPin, HIGH);
  } else {
    digitalWrite(buzzerPin, LOW);
  }

  // MQTT — keep connection alive and publish telemetry every 10 s
  if (mqttEnabled && (wifiMode == 1 || wifiMode == 2) && staConnected) {
    if (!mqttClient || !mqttClient->connected()) mqttReconnect();
    if (mqttClient && mqttClient->connected()) {
      mqttClient->loop();
      static unsigned long lastMqttPub = 0;
      if (millis() - lastMqttPub >= 60000) { lastMqttPub = millis(); mqttPublish(); }
    }
  }

  // Monitor STA connection and attempt reconnect if lost
  if ((wifiMode == 1 || wifiMode == 2) && strlen(staSsid) > 0) {
    staConnected = (WiFi.status() == WL_CONNECTED);
    if (!staConnected) {
      wifi_retry = 0;
      while (WiFi.status() != WL_CONNECTED && wifi_retry < 5) {
        wifi_retry++;
        Serial.println("WiFi STA reconnecting...");
        WiFi.disconnect();
        WiFi.begin(staSsid, staPass);
        delay(500);
      }
      staConnected = (WiFi.status() == WL_CONNECTED);
    }
  }
  delay(250);
  digitalWrite(INTERNAL_LED, LOW);
}
