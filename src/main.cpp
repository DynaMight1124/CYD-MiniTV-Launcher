#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <Update.h>
#include <vector>
#include <Preferences.h>
#include <algorithm>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "esp_ota_ops.h"
#include "config.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

// Explicitly use HSPI for Display
SPIClass displaySPI(HSPI);
Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO, &displaySPI);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, TFT_RST, SCREEN_ROTATION, false /* IPS */);

Adafruit_MCP23X17 mcp;
bool mcpActive = false;

std::vector<String> appFiles;
int selectedIndex = 0;
unsigned long buttonPressTime = 0;
bool buttonActive = false;
bool sdMounted = false;
Preferences prefs;
String lastApp = "";

// Helper for MCP button states
bool isMcpPressed(uint8_t pin) {
    if (!mcpActive) return false;
    return (mcp.digitalRead(pin) == LOW);
}

void listApps() {
    struct AppFile {
        String name;
        time_t time;
    };
    std::vector<AppFile> files;
    
    appFiles.clear();
    if (!sdMounted) return;

    File root = SD.open(APP_PATH);
    if (!root || !root.isDirectory()) {
        Serial.println("Apps folder not found on SD!");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = String(file.name());
            if (name.endsWith(".bin")) {
                files.push_back({name, file.getLastWrite()}); 
            }
        }
        file = root.openNextFile();
    }
    root.close();

    Serial.printf("Sorting %d apps (Method: %d)\n", (int)files.size(), APP_SORT_METHOD);

    std::sort(files.begin(), files.end(), [](const AppFile &a, const AppFile &b) {
        if (APP_SORT_METHOD == SORT_NEWEST_FIRST) {
            return a.time > b.time;
        } else {
            return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
        }
    });

    for (const auto &f : files) {
        appFiles.push_back(f.name);
        Serial.printf(" - %s\n", f.name.c_str());
    }
}

void drawUI() {
    gfx->fillScreen(BLACK);
    
    // Center title "CYD Mini TV"
    gfx->setTextColor(WHITE);
    gfx->setTextSize(2);
    String title = "CYD Mini TV";
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor((320 - w) / 2, 10);
    gfx->println(title);
    
    gfx->drawLine(0, 35, 320, 35, GREEN);

    if (!sdMounted) {
        gfx->setCursor(10, 60);
        gfx->setTextColor(RED);
        gfx->println("SD Card NOT found!");
        gfx->setCursor(10, 85);
        gfx->setTextColor(WHITE);
        gfx->setTextSize(1);
        gfx->println("Please insert SD and reboot");
        return;
    }

    if (appFiles.empty()) {
        gfx->setCursor(10, 60);
        gfx->println("No .bin files found");
        return;
    }

    for (int i = 0; i < (int)appFiles.size(); i++) {
        int y = 60 + (i * 30);
        if (i == selectedIndex) {
            gfx->fillRect(5, y - 5, 310, 25, BLUE);
            gfx->setTextColor(WHITE);
        } else {
            gfx->setTextColor(LIGHTGREY);
        }
        gfx->setCursor(15, y);
        
        String displayName = appFiles[i];
        if (displayName.endsWith(".bin")) {
            displayName = displayName.substring(0, displayName.length() - 4);
        }
        
        int maxChars = (appFiles[i] == lastApp) ? 19 : 23;
        if (displayName.length() > maxChars) {
            displayName = displayName.substring(0, maxChars - 3) + "...";
        }
        
        gfx->print(displayName);
        if (appFiles[i] == lastApp) {
            gfx->setTextColor(GREEN);
            gfx->print(" (*)");
        }
        gfx->println();
    }
}

void drawProgressBar(int progress, int total) {
    int width = 280;
    int height = 20;
    int x = 20;
    int y = 140;
    
    float percentage = (float)progress / total;
    int fillWidth = (int)(width * percentage);
    
    gfx->drawRect(x, y, width, height, WHITE);
    gfx->fillRect(x + 2, y + 2, fillWidth - 4, height - 4, GREEN);
    
    gfx->fillRect(x, y + height + 5, width, 20, BLACK);
    char pctStr[10];
    snprintf(pctStr, sizeof(pctStr), "%d%%", (int)(percentage * 100));
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(pctStr, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor((320 - w) / 2, y + height + 5);
    gfx->setTextColor(WHITE);
    gfx->print(pctStr);
}

void flashApp(String fileName) {
    const esp_partition_t* update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (!update_partition) return;

    if (fileName == lastApp) {
        gfx->fillScreen(BLACK);
        gfx->setTextColor(GREEN);
        gfx->setTextSize(2);
        String msg = "Booting App...";
        int16_t x1, y1;
        uint16_t w, h;
        gfx->getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((320 - w) / 2, 60);
        gfx->println(msg);
        
        gfx->setTextSize(3);
        String dispName = fileName;
        if (dispName.endsWith(".bin")) dispName = dispName.substring(0, dispName.length() - 4);
        gfx->getTextBounds(dispName, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((320 - w) / 2, 100);
        gfx->println(dispName);
        
        esp_ota_set_boot_partition(update_partition);
        delay(1500);
        ESP.restart();
        return;
    }

    gfx->fillScreen(BLACK);
    gfx->setTextColor(CYAN);
    gfx->setTextSize(2);
    String flashMsg = "Flashing...";
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(flashMsg, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor((320 - w) / 2, 60);
    gfx->println(flashMsg);
    
    gfx->setTextSize(2);
    String waitMsg = "Please wait...";
    gfx->getTextBounds(waitMsg, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor((320 - w) / 2, 95);
    gfx->println(waitMsg);

    gfx->setTextSize(1);
    String dangerMsg = "Do not power off.";
    gfx->getTextBounds(dangerMsg, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor((320 - w) / 2, 120);
    gfx->println(dangerMsg);
    
    String fullPath = String(APP_PATH) + "/" + fileName;
    File file = SD.open(fullPath);
    if (!file) return;

    size_t fileSize = file.size();

    if (Update.begin(fileSize, U_FLASH)) {
        int written = 0;
        uint8_t buf[1024];
        int bytesRead;
        int lastPercent = -1;
        while (written < fileSize) {
            bytesRead = file.read(buf, sizeof(buf));
            if (bytesRead == 0) break;
            
            written += Update.write(buf, bytesRead);
            
            int percent = (written * 100) / fileSize;
            if (percent != lastPercent) {
                lastPercent = percent;
                drawProgressBar(written, fileSize);
                Serial.printf("Progress: %d%%\n", percent);
            }
        }
        
        if (written == fileSize && Update.end()) {
            Serial.println("Success! Setting boot partition and rebooting...");
            
            prefs.begin("launcher", false);
            prefs.putString("last_app", fileName);
            prefs.end();
            
            esp_ota_set_boot_partition(update_partition);
            delay(1000);
            ESP.restart();
        } else {
            Serial.printf("Update Failed! Error: %d\n", Update.getError());
        }
    } else {
        Serial.printf("Update.begin Failed! Error: %d\n", Update.getError());
    }

    file.close();
    drawUI();
}

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Serial.begin(115200);
    delay(500);
    pinMode(NAV_BUTTON, INPUT_PULLUP); 
    
    prefs.begin("launcher", false);
    lastApp = prefs.getString("last_app", "");
    prefs.end();

    displaySPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
    gfx->begin();
    gfx->setRotation(SCREEN_ROTATION);
    gfx->invertDisplay(SCREEN_INVERSION);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Init MCP23017 if enabled
    #if ENABLE_I2C_CONTROLLER
    Wire.begin(I2C_SDA, I2C_SCL);
    if (mcp.begin_I2C(MCP_ADDR)) {
        mcp.pinMode(MCP_BTN_UP, INPUT_PULLUP);
        mcp.pinMode(MCP_BTN_DOWN, INPUT_PULLUP);
        mcp.pinMode(MCP_BTN_A, INPUT_PULLUP);
        mcpActive = true;
        Serial.println("MCP23017 Initialized.");
    } else {
        Serial.println("MCP23017 Not Found!");
    }
    #endif

    if (ENABLE_AUTOBOOT && lastApp != "") {
        gfx->fillScreen(BLACK);
        gfx->setTextColor(WHITE);
        gfx->setTextSize(2);
        String autoMsg = "Autobooting:";
        int16_t x1, y1;
        uint16_t w, h;
        gfx->getTextBounds(autoMsg, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((320 - w) / 2, 40);
        gfx->println(autoMsg);

        gfx->setTextColor(CYAN);
        gfx->setTextSize(3);
        String dispApp = lastApp;
        if (dispApp.endsWith(".bin")) dispApp = dispApp.substring(0, dispApp.length() - 4);
        gfx->getTextBounds(dispApp, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((320 - w) / 2, 80);
        gfx->println(dispApp);
        
        gfx->setTextColor(YELLOW);
        gfx->setTextSize(1);
        String cancelMsg = "Press button to enter menu";
        gfx->getTextBounds(cancelMsg, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((320 - w) / 2, 130);
        gfx->println(cancelMsg);

        bool cancelled = false;
        for (int i = AUTOBOOT_DELAY_SEC; i > 0; i--) {
            String countStr = String(i);
            gfx->fillRect(110, 160, 100, 40, BLACK);
            gfx->setTextSize(4);
            gfx->setTextColor(WHITE);
            gfx->getTextBounds(countStr, 0, 0, &x1, &y1, &w, &h);
            gfx->setCursor((320 - w) / 2, 160);
            gfx->print(countStr);
            for (int j = 0; j < 100; j++) {
                if (digitalRead(NAV_BUTTON) == LOW || isMcpPressed(MCP_BTN_A)) { 
                    cancelled = true; 
                    break; 
                }
                delay(10);
            }
            if (cancelled) break;
        }

        if (!cancelled) {
            const esp_partition_t* ota_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
            if (ota_part) { esp_ota_set_boot_partition(ota_part); ESP.restart(); }
        }
    }
    
    while(digitalRead(NAV_BUTTON) == LOW || isMcpPressed(MCP_BTN_A)) { delay(10); }
    delay(100);

    const esp_partition_t* factory_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (factory_part) esp_ota_set_boot_partition(factory_part);

    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (SD.begin(SD_CS)) sdMounted = true;
    else sdMounted = false;

    listApps();
    drawUI();
}

void loop() {
    bool isPressed = (digitalRead(NAV_BUTTON) == LOW);
    bool upPressed = isMcpPressed(MCP_BTN_UP);
    bool downPressed = isMcpPressed(MCP_BTN_DOWN);
    bool aPressed = isMcpPressed(MCP_BTN_A);

    if (mcpActive && !appFiles.empty()) {
        static unsigned long lastMove = 0;
        static bool lastAPressed = false;

        if (aPressed && !lastAPressed) {
            flashApp(appFiles[selectedIndex]);
            lastAPressed = true;
        } else if (!aPressed) {
            lastAPressed = false;
        }

        if (millis() - lastMove > 250) { 
            if (upPressed) {
                selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : appFiles.size() - 1;
                drawUI();
                lastMove = millis();
            } else if (downPressed) {
                selectedIndex = (selectedIndex + 1) % appFiles.size();
                drawUI();
                lastMove = millis();
            }
        }
    }

    if (isPressed && !buttonActive) {
        buttonActive = true;
        buttonPressTime = millis();
    } else if (!isPressed && buttonActive) {
        buttonActive = false;
        unsigned long duration = millis() - buttonPressTime;
        if (duration >= LONG_PRESS_MS) {
            if (!appFiles.empty()) flashApp(appFiles[selectedIndex]);
        } else if (duration >= DEBOUNCE_MS) {
            if (!appFiles.empty()) {
                selectedIndex = (selectedIndex + 1) % appFiles.size();
                drawUI();
            }
        }
    }
}
