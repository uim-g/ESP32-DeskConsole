# ESP32 DeskConsole

English · [简体中文](README.zh-CN.md)

DeskConsole is a desktop console for the ESP32-S3 and a 320×240 ILI9341 SPI display. Its home screen shows the time, date, current weather and temperature for Semenyih, Malaysia. Four physical buttons open apps and navigate the interface.

- **F1 Reaction** — five two-row lamp housings, random lights-out, false starts, timeouts, Normal and Debug modes. The last result, personal best, and total completed rounds are saved in flash.
- **Weather** — current conditions and a three-day forecast from [Open-Meteo](https://open-meteo.com/en/docs).
- **Wi-Fi** — connection state, SSID, IP, signal strength and manual reconnect.
- **Device** — chip temperature, uptime, free heap, PSRAM and flash capacity.

Page changes animate the active button labels at the screen edges. Each app has its own accent color.

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
