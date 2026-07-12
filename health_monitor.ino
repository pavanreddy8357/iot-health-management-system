/*
  IoT-Driven Health Management System
  ---------------------------------------
  Board:    ESP32
  Sensors:  MAX30102 (heart rate + SpO2, I2C), DS18B20 (body temperature, 1-Wire)
  Cloud:    ThingSpeak channel (HTTP upload) — swap in any REST endpoint as needed

  Wiring:
    MAX30102     -> SDA: GPIO21, SCL: GPIO22, VCC: 3.3V, GND: GND
    DS18B20      -> Data: GPIO4 (with 4.7k pull-up resistor to 3.3V), VCC: 3.3V, GND: GND

  Libraries required (Arduino Library Manager):
    - MAX30105 library (SparkFun) -- also covers MAX30102
    - OneWire
    - DallasTemperature
    - WiFi (bundled with ESP32 board package)
    - ThingSpeak (MathWorks)
*/

#include <Wire.h>
#include <WiFi.h>
#include "ThingSpeak.h"
#include "MAX30105.h"
#include "heartRate.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------- User configuration ----------
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

unsigned long THINGSPEAK_CHANNEL_ID = 0;          // <-- put your channel ID here
const char*   THINGSPEAK_WRITE_API_KEY = "YOUR_WRITE_API_KEY";

#define ONE_WIRE_BUS 4
const unsigned long UPLOAD_INTERVAL_MS = 20000;
// -----------------------------------------

MAX30105 particleSensor;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
WiFiClient client;

const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

unsigned long lastUpload = 0;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nConnected. IP: " + WiFi.localIP().toString());
}

void setup() {
  Serial.begin(115200);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found. Check wiring.");
    while (1) delay(1000);
  }
  particleSensor.setup();                     // default sensor settings
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);

  tempSensor.begin();

  connectWiFi();
  ThingSpeak.begin(client);
}

void loop() {
  long irValue = particleSensor.getIR();

  // --- Heart-rate detection (finger presence check via IR value) ---
  if (checkForBeat(irValue)) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60.0 / (delta / 1000.0);

    if (beatsPerMinute > 20 && beatsPerMinute < 255) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;
      beatAvg = 0;
      for (byte i = 0; i < RATE_SIZE; i++) beatAvg += rates[i];
      beatAvg /= RATE_SIZE;
    }
  }

  bool fingerDetected = irValue >= 50000;

  // --- Body temperature ---
  tempSensor.requestTemperatures();
  float bodyTempC = tempSensor.getTempCByIndex(0);

  if (fingerDetected) {
    Serial.printf("IR:%ld  BPM:%.1f  AvgBPM:%d  Temp:%.1fC\n",
                  irValue, beatsPerMinute, beatAvg, bodyTempC);
  } else {
    Serial.println("No finger detected on sensor.");
  }

  if (millis() - lastUpload >= UPLOAD_INTERVAL_MS) {
    if (WiFi.status() == WL_CONNECTED && THINGSPEAK_CHANNEL_ID != 0 && fingerDetected) {
      ThingSpeak.setField(1, beatAvg);
      ThingSpeak.setField(2, bodyTempC);
      // Note: true SpO2 requires the red+IR ratio-of-ratios algorithm;
      // this build reports heart rate + temperature. See README for SpO2 extension.
      int httpCode = ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_WRITE_API_KEY);
      Serial.println(httpCode == 200 ? "Cloud update OK" : "Cloud update failed: " + String(httpCode));
    }
    lastUpload = millis();
  }

  delay(20);  // MAX30102 needs frequent polling for accurate beat detection
}
