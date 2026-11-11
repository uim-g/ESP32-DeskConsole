# ESP32 桌面控制台

[English](README.md) · 简体中文

DeskConsole 使用 ESP32-S3、320×240 ILI9341 SPI 屏幕和四个实体按键。桌面显示时间、日期，以及马来西亚士毛月的当前天气和气温。按键可以打开 App 和切换页面。

- **F1 Reaction**。五组双排灯箱、随机熄灯、抢跑、超时及 Normal / Debug 模式。上一局结果、个人最佳和总局数保存到闪存。
- **天气**。显示 [Open-Meteo](https://open-meteo.com/en/docs) 的当前天气和三天预报。
- **Wi-Fi**。查看连接状态、SSID、IP、信号强度，支持手动重连。
- **设备诊断**。查看芯片温度、运行时长、剩余堆内存、PSRAM 和 Flash 容量。

切页时，侧边可用按键的标签会滑出和滑入。各 App 使用各自的主题色。

## 安装

1. 在 Arduino IDE 2 中安装 **esp32 by Espressif Systems**，选择 **ESP32S3 Dev Module** 和开发板串口。N16R8 的 Flash Size 设为 16 MB。
2. 安装 **Adafruit ILI9341** 及其依赖，以及 **ArduinoJson 7**。
3. 打开 `DeskConsole/DeskConsole.ino`，在本地 `Config.h` 填入 2.4 GHz Wi-Fi 名称和密码。分享代码时保留占位符。
4. 按下表接好屏幕和按钮，再编译上传。

| ILI9341 | ESP32-S3 |
|---|---|
| VCC 和 LED，J1 已短接 | 3V3 |
| GND | GND |
| CS、RESET、DC | GPIO10、GPIO8、GPIO9 |
| MOSI、SCK | GPIO11、GPIO12 |

每个按钮接相应 GPIO 和公共 GND。程序启用内部上拉。

| 按键 | 桌面和 App 导航 | F1 |
|---|---|---|
| 左上 GPIO4 | App 列表／返回 | 返回 |
| 左下 GPIO5 | 打开／执行 | Normal / Debug |
| 右上 GPIO6 | 向上 | 统计 |
| 右下 GPIO7 | 向下 | 开始／反应／清除本局 |

F1 以 TFT 熄灯为信号，成绩会受显示和输入延迟影响。天气客户端目前跳过 HTTPS 证书校验。本次发布归档未重新编译或上板验证。

参见[版本记录](CHANGELOG.md)。
