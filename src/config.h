#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- UI Settings ---
#define MENU_TITLE "CYD Mini TV" // Change this to amend the title in the menu

// --- App Settings ---
#define APP_PATH "/apps"         // Folder on SD card for .bin files
#define NAV_BUTTON 3             // GPIO Pin for the navigation button
#define LONG_PRESS_MS 1000       // Hold for 1 second to flash
#define DEBOUNCE_MS 50           // Simple debounce

// --- Autoboot Settings ---
#define ENABLE_AUTOBOOT true     // Set to false to always stay in the launcher menu on boot
#define AUTOBOOT_DELAY_SEC 5     // Number of seconds to wait before autobooting the last app

// --- Controller Settings (I2C) ---
#define ENABLE_I2C_CONTROLLER true
#define I2C_SDA 22               // CYD CN1 Header Pin 1
#define I2C_SCL 27               // CYD CN1 Header Pin 2
#define IO_EXPANDER_ADDRESS 0x20 // Default address for most expanders

// --- Choose your Controller Chip ---
#define USE_MCP23017             // Uncomment this if using MCP23017
//#define USE_PCF8575            // Uncomment this if using PCF8575

// Common Joystick mappings
#define BTN_UP 2                 // Pin for UP
#define BTN_DOWN 3               // Pin for DOWN
#define BTN_A 4                  // Pin for A (Flash)

// --- Sorting Settings ---
#define SORT_ALPHABETICAL        // Uncomment this to sort A-Z
//#define SORT_NEWEST_FIRST      // Uncomment this to show most recently added files first

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
