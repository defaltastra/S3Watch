
---

# S3Watch-Fork

**Smartwatch firmware for Waveshare ESP32-S3-Touch-AMOLED-2.06**

This is a fork of the original firmware for the Waveshare ESP32-S3-Touch-AMOLED-2.06 smartwatch. **New features in this fork:**

* Experimental **image viewer**
* **MP3 playback** (in progress)
* **Bluetooth support** (coming soon)

More information about the board can be found on the Waveshare Wiki: [ESP32-S3-Touch-AMOLED-2.06 Wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06)

---

## Introduction

The ESP32-S3-Touch-AMOLED-2.06 is a high-performance wearable development board shaped like a smartwatch. It is based on the ESP32-S3R8 microcontroller and integrates:

* 2.06-inch capacitive touch HD AMOLED screen
* Six-axis sensor (IMU)
* RTC (real-time clock)
* Audio codec chip
* Power management modules

With its custom case, it resembles a smartwatch and is designed for prototyping and testing wearable applications.

---

## Features

* ESP32-S3R8 dual-core Xtensa 32-bit LX7 processor, up to 240 MHz
* 2.4 GHz Wi-Fi (802.11 b/g/n) and Bluetooth 5 (BLE)
* 512 KB SRAM, 384 KB ROM, 8 MB PSRAM onboard, and 32 MB external Flash
* Type-C interface for convenience and compatibility

---

## Hardware Description

* **Screen:** 2.06-inch capacitive touch AMOLED, 410×502 resolution, 16.7M colors
* **Driver chips:** CO5300 (AMOLED) and FT3168 (capacitive touch), connected via QSPI/I2C
* **Sensors:** QMI8658 6-axis IMU for motion detection, gestures, and step counting
* **RTC:** PCF85063, powered through AXP2101 for uninterrupted operation
* **Buttons:** PWR and BOOT with customizable functions
* **Battery:** 3.7V MX1.25 lithium, rechargeable
* **Ports:** 1x I2C, 1x UART, 1x USB pad for debugging and external device connection
* **Storage:** TF card slot for extended storage and media playback
* **Power management:** AXP2101 manages multiple voltages, battery charging, and optimizes battery life

The AMOLED screen offers high contrast, wide viewing angles, rich colors, fast response, thin design, lower power consumption, and flexibility.

---

## Building and Flashing (Visual Studio Code + ESP-IDF)

This project targets the ESP32-S3 and works with the ESP-IDF VS Code extension:

1. **Install prerequisites:** Visual Studio Code + ESP-IDF Extension. Let the extension install ESP-IDF 5.x tools or point to an existing setup.
2. **Configure the extension:** Use `ESP-IDF: Configure ESP-IDF extension` from the command palette. Select ESP-IDF directory, Python environment, and tools folder.
3. **Open project folder:** File → Open Folder → repository root (`S3Watch`). The ESP-IDF status bar should appear.
4. **Set target:** ESP-IDF status bar → Set Target → `esp32s3`.
5. **Build project:** Click Build Project in the status bar or run `ESP-IDF: Build Project`.
6. **Select serial port:** Connect the board via USB → ESP-IDF: Select Serial Port.
7. **Flash and monitor:** Use ESP-IDF: Flash (UART) or Flash and Monitor for combined flashing and monitoring.

---

## Dependencies

Managed via ESP-IDF Component Manager (`main/idf_component.yml` and `dependencies.lock`):

* `esp32_s3_touch_amoled_2_06`: Board support package
* `lvgl/lvgl`: Graphics library for UI

Dependencies are automatically fetched during the ESP-IDF build process.

---

