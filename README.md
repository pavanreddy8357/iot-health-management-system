# IoT-Driven Health Management System

An ESP32-based wearable/bedside health monitor that reads heart rate and body
temperature and streams them to a cloud dashboard in real time, for remote
patient tracking.

## Hardware
| Component      | Notes                                             |
|-----------------|----------------------------------------------------|
| ESP32 dev board | Wi-Fi + dual-core for sensor sampling + networking |
| MAX30102        | I2C pulse-oximetry sensor (heart rate, IR/Red LEDs)|
| DS18B20         | 1-Wire digital temperature sensor                  |

## Wiring
```
MAX30102   SDA -> GPIO21   SCL -> GPIO22   VCC -> 3.3V   GND -> GND
DS18B20    Data -> GPIO4 (4.7k pull-up to 3.3V)   VCC -> 3.3V   GND -> GND
```

## Libraries (Arduino Library Manager)
- SparkFun MAX3010x Pulse and Proximity Sensor Library (covers MAX30102)
- OneWire
- DallasTemperature
- ThingSpeak (MathWorks)

## Setup
1. Create a ThingSpeak channel with 2 fields (Heart Rate, Body Temperature).
2. Fill in Wi-Fi credentials and ThingSpeak Channel ID / Write API Key in
   `src/health_monitor.ino`.
3. Flash to the ESP32 and open Serial Monitor at 115200 baud.
4. Place a finger on the MAX30102 sensor — the sketch checks `IR >= 50000` to
   confirm finger presence before trusting the beat-detection output.

## How it works
- Uses a peak-detection algorithm (`checkForBeat`) on the IR signal to estimate
  instantaneous BPM, then averages the last 4 beats for a stable reading.
- Reads body temperature independently over 1-Wire.
- Every 20 seconds, if a finger is detected, both readings are pushed to the
  cloud dashboard.

## Known limitation / next step
This build reports **heart rate + temperature**, which is what the MAX30102's
raw IR channel reliably gives without extra calibration. True **SpO2** requires
implementing the red/IR ratio-of-ratios calculation (or using SparkFun's
`spo2_algorithm.h`, included in their library examples) — worth adding if you
want to fully match "oxygen levels" in the project description. That's flagged
here deliberately rather than silently claimed.

## Possible extensions
- Add the SpO2 ratio-of-ratios algorithm for true blood-oxygen estimation.
- Move to deep-sleep between samples for battery-powered wearable use.
- Add BLE fallback for offline logging when Wi-Fi isn't available.
