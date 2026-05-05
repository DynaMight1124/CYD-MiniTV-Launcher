#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- App Settings ---
#define APP_PATH "/apps"         // Folder on SD card for .bin files
#define NAV_BUTTON 3             // GPIO Pin for the navigation button
#define LONG_PRESS_MS 1000       // Hold for 1 second to flash
#define DEBOUNCE_MS 50           // Simple debounce

// --- Autoboot Settings ---
#define ENABLE_AUTOBOOT true     // Set to false to always stay in the launcher menu on boot
#define AUTOBOOT_DELAY_SEC 5     // Number of seconds to wait before autobooting the last app

// --- Controller Settings (MCP23017) ---
#define ENABLE_I2C_CONTROLLER true
#define I2C_SDA 22               // CYD CN1 Header Pin 1
#define I2C_SCL 27               // CYD CN1 Header Pin 2
#define MCP_ADDR 0x20            // Default address for MCP23017
#define MCP_BTN_UP 2             // Pin 2 for UP
#define MCP_BTN_DOWN 3           // Pin 3 for DOWN
#define MCP_BTN_A 4              // Pin 4 for A

// --- Sorting Settings ---
#define SORT_ALPHABETICAL 1
#define SORT_NEWEST_FIRST 0
#define APP_SORT_METHOD SORT_ALPHABETICAL // SORT_NEWEST_FIRST to show recent files first or SORT_ALPHABETICAL for alphabetical.

// --- Hardware Profiles ---
#if defined(BOARD_CYD2USB) || defined(BOARD_CYD)
  // Display
  #define TFT_BRIGHTNESS 128 // Can be set between 0 and 255
  #define TFT_BL 21
  #define TFT_SCK 14
  #define TFT_MOSI 13
  #define TFT_MISO 12
  #define TFT_CS 15
  #define TFT_DC 2
  #define TFT_RST -1
  #define SCREEN_ROTATION 1      // Change to 3 to flip 180 degrees.
  
  #ifdef BOARD_CYD2USB
    #define SCREEN_INVERSION true // CYD2USB
  #else
    #define SCREEN_INVERSION false // Original CYD
  #endif
  
  // SD Card (VSPI)
  #define SD_SCK 18
  #define SD_MOSI 23
  #define SD_MISO 19
  #define SD_CS 5
#endif

#endif
