# ⌚ Smartwatch Firmware  
**Waveshare ESP32-S3 Touch AMOLED 2.06"**

Firmware for the **Waveshare ESP32-S3-Touch-AMOLED-2.06** smartwatch development board.  
Designed for experimentation, UI development, and wearable prototyping using **ESP-IDF** and **LVGL**.

📘 Official board documentation:  
https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06

<p float="left">
  <img src="ui_assets/s3watch_0.jpg" width="280"/>
  <img src="ui_assets/s3watch_1.jpg" width="280"/>
</p>

---

## Overview

The **ESP32-S3-Touch-AMOLED-2.06** is a watch-shaped development board built around the **ESP32-S3R8** MCU.  
It integrates an AMOLED touch display, sensors, RTC, power management, audio codec, and SD card support, making it ideal for smartwatch and wearable firmware development.

This repository provides a functional smartwatch firmware with a modern UI and extensible architecture.

---

## ✅ Current Features

- [x] Battery status & charging information  
- [x] Screen brightness control  
- [x] Bluetooth (BLE) support  
- [x] Time & date configuration  
- [x] Multiple watch face styles  
- [x] Change background wallpaper from SD card  
- [x] Touch-based UI powered by LVGL  
- [x] RTC-backed time keeping  

---

## 🚧 Planned Features

- [ ] MP3 audio playback from SD card  
- [ ] MP4 video playback from SD card  
- [ ] PDF reader from SD card (experimental / TBD)  
- [ ] Mini games (simple touch-based games)  
- [ ] Improved power-saving modes  
- [ ] Additional watch face themes  

---

## Hardware Highlights

- **MCU:** ESP32-S3R8 (Dual-core Xtensa LX7 @ up to 240 MHz)  
- **Display:** 2.06" AMOLED, 410 × 502, capacitive touch  
- **Wireless:** Wi-Fi 2.4 GHz + Bluetooth 5 (BLE)  
- **Memory:**  
  - 512 KB SRAM  
  - 384 KB ROM  
  - 8 MB PSRAM  
  - 32 MB external Flash  
- **Sensors:** QMI8658 6-axis IMU  
- **RTC:** PCF85063 (battery-backed)  
- **Power Management:** AXP2101  
- **Storage:** TF (microSD) card slot  
- **Connectivity:** USB Type-C, I2C, UART  

---

## Software Stack

- **Framework:** ESP-IDF (v5.x recommended)  
- **UI Library:** LVGL  
- **Board Support:** `esp32_s3_touch_amoled_2_06` BSP  

Dependencies are automatically handled via **ESP-IDF Component Manager**.

---

## Building & Flashing (ESP-IDF + VS Code)

This project is intended to be built using **Visual Studio Code** with the official **ESP-IDF extension**.

### Basic Steps

1. Install **Visual Studio Code**
2. Install the **ESP-IDF Extension**
3. Open this repository folder
4. Set target to `esp32s3`
5. Build the project
6. Flash to the device via USB

---

## 🔥 Flashing the Firmware

To flash, erase, or recover the firmware properly, **follow the official Waveshare flashing guide**:

📘 **Firmware Flashing & Erasing Guide**  
https://www.waveshare.com/wiki/Flash_Firmware_Flashing_and_Erasing

This guide covers:
- Boot mode usage
- Flash erase
- UART flashing
- Common recovery scenarios

---

## Repository Purpose

This project aims to:
- Provide a clean smartwatch firmware baseline  
- Demonstrate LVGL on AMOLED displays  
- Enable experimentation with wearable features  
- Serve as a foundation for custom smartwatch applications  

---

## License

This project is provided for educational and development purposes.  
Refer to individual component licenses (ESP-IDF, LVGL) for details.
