# ESP32 DeskConsole

English · [简体中文](README.zh-CN.md)

DeskConsole is a desktop console for the ESP32-S3 and a 320×240 ILI9341 SPI display. Its home screen shows the time, date, current weather and temperature for Semenyih, Malaysia. Four physical buttons open apps and navigate the interface.

- **F1 Reaction** — five two-row lamp housings, random lights-out, false starts, timeouts, Normal and Debug modes. The last result, personal best, and total completed rounds are saved in flash.
- **Weather** — current conditions and a three-day forecast from [Open-Meteo](https://open-meteo.com/en/docs).
- **Wi-Fi** — connection state, SSID, IP, signal strength and manual reconnect.
- **Device** — chip temperature, uptime, free heap, PSRAM and flash capacity.

Page changes animate the active button labels at the screen edges. Each app has its own accent color.

## UI overview

![DeskConsole v1.1.0 UI concept sheet](docs/ui-v1.1.0.png)

The nine panels illustrate Home, Apps, F1, Statistics, Calibration, Weather Now, Forecast, Wi-Fi, and Device. This is a design illustration with sample values. The firmware uses a vertical app list, side labels aligned to physical buttons, vertically stacked RAW and CAL times, and forecast rain probability.

## Navigation and rendering

- **Home** shows time, date, current conditions and temperature. Press the upper-left button to open Apps.
- **Apps** uses the right buttons to move the selection and the lower-left button to open it. The rounded highlight eases between cards and inverts the text and icon it passes over. The card boundaries clip the moving highlight.
- **Weather** uses either right button to toggle between current conditions and the three-day forecast. Current conditions include humidity, wind, pressure and rain. The forecast includes highs, lows, rain probability, sunrise and sunset.
- **Wi-Fi** shows status, SSID, IP, RSSI and NTP readiness. Press the lower-left button to request reconnection.
- **Device** shows chip temperature, uptime, free heap, free PSRAM and flash capacity. Chip temperature measures the ESP32-S3 itself.
- Page transitions slide out the old side labels, replace the page, then slide in the new labels. Label animation writes only the edge regions. The interface uses Nunito. Its license is included in [Nunito-OFL.txt](DeskConsole/Nunito-OFL.txt).

## F1 controls and calibration

- Press **PLAY** (lower right) to start. Five red lights illuminate in sequence. After a randomized wait, react when they go out. Press PLAY again after a result to reset the round. An early press is a false start and the reaction window is ten seconds.
- Press **MODE** (lower left) between rounds to toggle Normal and Debug. In Normal, **STATS** (upper right) shows the last outcome, best raw time and total rounds. Press **ZERO** twice to clear those records.
- In Debug, the result includes **RAW** and **CAL**. **SET** (upper right) opens calibration. Tap **FIELD** to select a parameter or hold it for about 800 ms to restore 1.00 and 0 ms. The right buttons adjust the multiplier by 0.01 or the offset by 1 ms. Holding either adjustment button repeats after about 420 ms. **SAVE** writes the settings to flash and returns.
- Calibration uses `CAL = max(0, RAW × multiplier + offset)`, with both times and the offset expressed in milliseconds. The multiplier range is 0.50–1.50 and the offset range is −500 to +500 ms. Adjusting calibration does not rewrite saved raw times.
- Completed reactions, false starts and timeouts count toward the total. Exiting mid-round does not.

## Serial Monitor

Use **115200 baud**. For native USB, enable **USB CDC On Boot** and re-upload if the sketch produces no serial output. Open the monitor before pressing RESET to capture startup messages. Logs include button presses, navigation, Wi-Fi and NTP status, weather updates, F1 lights and results, calibration edits and flash saves.

## Setup

1. In Arduino IDE 2, install **esp32 by Espressif Systems**. Select **ESP32S3 Dev Module** and your board's serial port. Set Flash Size to 16 MB for N16R8.
2. Install **Adafruit ILI9341** and its dependencies, plus **ArduinoJson 7**.
3. Open `DeskConsole/DeskConsole.ino`. In your local `Config.h`, fill in your 2.4 GHz Wi-Fi name and password. Keep placeholders in shared copies.
4. Wire the display and buttons as below, then compile and upload.

| ILI9341 | ESP32-S3 |
|---|---|
| VCC and LED with J1 bridged | 3V3 |
| GND | GND |
| CS, RESET, DC | GPIO10, GPIO8, GPIO9 |
| MOSI, SCK | GPIO11, GPIO12 |

Each button connects its GPIO to common GND when pressed. The firmware enables internal pull-ups.

| Button | Home / app navigation | F1 |
|---|---|---|
| Top left · GPIO4 | Apps / Back | Back |
| Bottom left · GPIO5 | Open / Action | Normal / Debug |
| Top right · GPIO6 | Up | Statistics |
| Bottom right · GPIO7 | Down | Play / React / Reset result |

F1 timing uses the TFT lights-out signal and includes hardware-dependent display and input latency. The prototype weather client skips HTTPS certificate verification. This release snapshot has not been recompiled or hardware-tested during packaging.

See [release history](CHANGELOG.md).
