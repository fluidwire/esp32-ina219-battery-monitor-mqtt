/*
 * ESP32 INA219 Battery / Solar Monitor -> MQTT
 * --------------------------------------------
 * Reads bus voltage and current from an INA219 high-side sensor,
 * integrates current over time (coulomb counting) to track mAh and mWh
 * drawn or harvested, estimates a rough state of charge from voltage,
 * and publishes a JSON payload to MQTT every 10 seconds.
 *
 * Sign convention: positive current = discharging (load draws from the
 * battery), negative = charging (solar/charger feeds the battery) when the
 * INA219 is wired in series on the battery's positive lead as shown below.
 *
 * Required libraries (Library Manager):
 *   - "Adafruit INA219" by Adafruit  (pulls in "Adafruit BusIO")
 *   - "PubSubClient"    by Nick O'Leary
 *
 * Wiring (INA219 breakout, I2C):
 *   INA219 VCC -> ESP32 3V3
 *   INA219 GND -> ESP32 GND  (also common ground with the battery -)
 *   INA219 SDA -> ESP32 GPIO21
 *   INA219 SCL -> ESP32 GPIO22
 *   Battery +  -> INA219 Vin+
 *   INA219 Vin- -> Load / charger +
 *   Battery -  -> Load / charger -
 *
 * Setup:
 *   1. Copy config.h.example to config.h and fill in WiFi, broker, capacity.
 *   2. Flash, open Serial Monitor at 115200.
 *
 * Sample payload:
 *   {"bus_v":3.91,"current_ma":212.4,"power_mw":830.5,"soc_pct":71,
 *    "used_mah":12.35,"net_mwh":47.2,"uptime_s":210}
 *
 * (c) 2026 Fluidwire. Released under the MIT License.
 * https://github.com/fluidwire
 */

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <PubSubClient.h>
#include "config.h"

const uint32_t SAMPLE_INTERVAL_MS  = 1000UL;
const uint32_t PUBLISH_INTERVAL_MS = 10000UL;

WiFiClient      wifiClient;
PubSubClient    mqtt(wifiClient);
Adafruit_INA219 ina219;   // default I2C address 0x40

float    usedMah   = 0.0f;   // net mAh drawn since boot (negative = net charge)
float    netMwh    = 0.0f;
uint32_t lastSample  = 0;
uint32_t lastPublish = 0;

void connectWiFi() {
  Serial.print(F("[wifi] connecting to "));
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.print(F("[wifi] IP: "));
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  while (!mqtt.connected()) {
    Serial.print(F("[mqtt] connecting... "));
    bool ok = strlen(MQTT_USER) > 0
      ? mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)
      : mqtt.connect(MQTT_CLIENT_ID);
    if (ok) {
      Serial.println(F("ok"));
    } else {
      Serial.print(F("failed, state="));
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

// Rough linear SoC from resting voltage. Real Li-ion curves are non-linear
// and shift under load; treat this as an indicator, not a fuel gauge.
int estimateSoc(float busV) {
  float pct = (busV - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0f;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (int)(pct + 0.5f);
}

void sampleAndIntegrate(float &busV, float &currentMa, float &powerMw) {
  busV      = ina219.getBusVoltage_V();
  currentMa = ina219.getCurrent_mA();
  powerMw   = busV * currentMa;

  uint32_t now = millis();
  float hours = (now - lastSample) / 3600000.0f;
  lastSample = now;
  usedMah += currentMa * hours;
  netMwh  += powerMw * hours;
}

void publishReading(float busV, float currentMa, float powerMw) {
  char buf[200];
  snprintf(buf, sizeof(buf),
           "{\"bus_v\":%.2f,\"current_ma\":%.1f,\"power_mw\":%.1f,\"soc_pct\":%d,"
           "\"used_mah\":%.2f,\"net_mwh\":%.1f,\"uptime_s\":%lu}",
           busV, currentMa, powerMw, estimateSoc(busV), usedMah, netMwh,
           (unsigned long)(millis() / 1000UL));
  bool ok = mqtt.publish(MQTT_TOPIC, buf);
  Serial.print(F("[mqtt] "));
  Serial.print(ok ? F("published ") : F("FAILED "));
  Serial.println(buf);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(21, 22);
  if (!ina219.begin()) {
    Serial.println(F("[ina219] not found - check wiring and I2C address (0x40)"));
    while (true) delay(1000);
  }
  // Pick the calibration range that matches your battery and load:
  //   setCalibration_32V_2A()  : up to 32V, 2A, 0.8mA resolution (default)
  //   setCalibration_32V_1A()  : up to 32V, 1A, better resolution
  //   setCalibration_16V_400mA(): up to 16V, 400mA, best for small Li-ion loads
  ina219.setCalibration_32V_2A();
  Serial.println(F("[ina219] ready"));

  connectWiFi();
  lastSample = millis();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  static float busV = 0, currentMa = 0, powerMw = 0;
  if (millis() - lastSample >= SAMPLE_INTERVAL_MS) {
    sampleAndIntegrate(busV, currentMa, powerMw);
  }
  if (millis() - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = millis();
    publishReading(busV, currentMa, powerMw);
  }
  delay(10);
}
