# 🐱 Jie — ESP32-C3 Virtual Pet

"Jie" is a small virtual pet project based on the ESP32-C3 SuperMini and an OLED display. It features animated cat expressions and interactive functions, bringing a simple virtual pet to life on a tiny screen.
//It's a birthday gift for my best friend.

This repository is published for project demonstration and portfolio purposes.

## ✨ Features

- 🐱 Animated cat expressions
- ❤️ Heart and food-themed graphics
- 👆 Touch sensor interaction
- 🎮 Mini-game features
- 🖥️ OLED display graphics
- 🔌 Compact ESP32-C3 hardware design

*The exact features depend on the functions implemented in the current source code.*

## 🛠️ Hardware

- ESP32-C3 SuperMini
- SSD1306 128 × 64 OLED display (I2C, address `0x3C`)
- TTP223 touch sensor
- Jumper wires

## 🔗 Wiring

| Component | Pin | ESP32-C3 SuperMini |
|---|---|---|
| OLED | SDA | GPIO 6 |
| OLED | SCL | GPIO 7 |
| OLED | VCC | 3.3V or a supported supply voltage |
| OLED | GND | GND |
| TTP223 | OUT | GPIO 4 |
| TTP223 | VCC | 3.3V |
| TTP223 | GND | GND |

Check your specific module's voltage requirements before connecting it.

## 💻 Software Requirements

- Arduino IDE
- ESP32 board support package
- Adafruit SSD1306 library
- Adafruit GFX Library

Install the required libraries through the Arduino IDE Library Manager.

## 🚀 Getting Started

1. Install Arduino IDE.
2. Install the ESP32 board support package.
3. Install the Adafruit SSD1306 and Adafruit GFX libraries.
4. Connect the OLED display and touch sensor according to the wiring table.
5. Open the `.ino` sketch in Arduino IDE.
6. Select the appropriate ESP32-C3 board and COM port.
7. Upload the program and check the OLED display.

## 🎨 Design and Development

The virtual pet graphics and expressions were created through drawing code generated with the assistance of ChatGPT, based on the developer's own requirements.

ChatGPT was also used as an AI-assisted programming tool during development.

## 📚 Third-Party Libraries

This project uses the following third-party libraries:

- **Adafruit SSD1306** — https://github.com/adafruit/Adafruit_SSD1306
- **Adafruit GFX Library** — https://github.com/adafruit/Adafruit-GFX-Library

These libraries are maintained by their respective authors and are subject to their own licenses. Please refer to their official repositories for license terms.

## © Copyright and Usage

Copyright © 2026 Linzhi555. All rights reserved.

This repository is published for project demonstration and portfolio purposes only.

Unless permitted by applicable law or the licenses of third-party components, no permission is granted to copy, modify, redistribute, or commercially use this project's original code and materials without prior written permission.

Third-party components remain subject to their respective licenses.

**No open-source license is granted for the project's original code by this notice.**

## 📌 Disclaimer

This project is provided for educational and demonstration purposes. Hardware configurations and features may vary depending on the specific components and source-code version used.
