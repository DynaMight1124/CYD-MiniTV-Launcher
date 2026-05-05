# CYD Mini TV Launcher

A minimalist, high-performance application launcher for the ESP32 **Cheap Yellow Display (CYD)** family. This project allows you to switch between different applications (such as MiniTV, NES emulators, or Galagino) stored on an SD card without ever needing to reflash via USB.

## Credits & Acknowledgments
Special thanks to [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) for providing the foundational logic and the custom bootloader. This project utilizes their specialised ESP32 platform fork to enable seamless "Cold Boot to Menu" functionality.

---

## 📺 What this project achieves
The CYD Mini TV Launcher solves the "4MB Flash Limit" by treating your SD card as a hard drive. It allows you to:
*   **Store unlimited apps:** Keep a library of `.bin` files on your SD card.
*   **Flash in seconds:** Selecting an app writes it to the internal OTA partition and reboots instantly.
*   **Automatic Returns:** Simply power the device off and on to return to the Launcher menu from any app.
*   **Hybrid Control:** Navigate using the built-in button or a connected MCP23017-based I2C game controller.

---

## 🚀 User Guide

### 1. Preparation
1.  Format a microSD card to **FAT32**.
2.  Create a folder named `apps` at the root of the SD card.
3.  Place your compiled application binaries (e.g., `MiniTV.bin`, `NES.bin`) inside the `/apps` folder.

### 2. Initial Setup
The first time you use the launcher, you must flash the combined binary to your device using a tool like **ESP-Flasher**.
*   **For CYD2USB (2 USB ports):** Flash `MiniTV-Launcher-cyd2usb.bin`
*   **For Original CYD (1 USB port):** Flash `MiniTV-Launcher-cyd.bin`

### 3. How to use the Menu
*   **Short Press (Button):** Cycle through your list of apps.
*   **Long Press (Button):** Install and boot the selected app.
*   **Controller Up/Down:** Scroll through the list.
*   **Controller 'A' Button:** Select and boot the app.
*   **Autoboot:** If an app was previously used, a 5-second countdown will appear. Do nothing to boot the app, or press a button to stay in the menu.

---

## 🛠️ Developer Guide

### 🔧 Adjusting the Project
The project is built on PlatformIO and can be easily customised in `src/config.h`:
*   **`NAV_BUTTON`**: Change the physical input pin (currently GPIO 3).
*   **`ENABLE_AUTOBOOT`**: Toggle the 5-second countdown on or off.
*   **`APP_SORT_METHOD`**: Switch between `SORT_ALPHABETICAL` (case-insensitive) or `SORT_NEWEST_FIRST`.
*   **`SCREEN_INVERSION`**: Fix colors for different screen manufacturers.

### 📊 Partition Scheme (4MB Flash)
The launcher uses a custom partition table designed to maximise space for large applications while keeping the launcher safe in its own "Test" partition.

| Name | Type | Offset | Size | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **nvs** | data | `0x9000` | 20KB | Settings & Last App memory |
| **otadata** | data | `0xE000` | 8KB | OTA boot selection data |
| **test** | app | `0x10000` | **768KB** | The Launcher itself |
| **ota_0** | app | `0xD0000` | **3.125MB** | Your flashed apps |
| **spiffs** | data | `0x3F0000` | 64KB | Extra storage for apps |

**Note:** Any app you wish to launch must be compiled to fit within a **3.125MB** window.

### 📦 Build System
The project includes a custom Python script (`support_files/merge.py`) that automatically bundles the bootloader, partition table, and firmware into a single flashable `.bin` file every time you run `pio run`.

### 🎮 I2C Controller (MCP23017)
The I2C controller is configured for:
*   **SDA:** GPIO 22
*   **SCL:** GPIO 27
*   **Default Address:** `0x20`
*   **Pin Mapping:** Up (2), Down (3), A (4).
