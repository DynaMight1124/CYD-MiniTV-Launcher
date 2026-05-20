#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <Update.h>
#include <vector>
#include <Preferences.h>
#include <algorithm>
#include "esp_ota_ops.h"
#include "config.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

// Hardware Abstraction for Display
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO, HSPI);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, TFT_RST, SCREEN_ROTATION, false /* IPS */);

std::vector<String> appFiles;
int selectedIndex = 0;
int windowStart = 0;
unsigned long buttonPressTime = 0;
bool buttonActive = false;
bool sdMounted = false;
bool expanderActive = false;
Preferences prefs;
String lastApp = "";

// Forward declaration
void drawUI();

// --- Bitbang I2C Implementation ---
void i2c_delay() { delayMicroseconds(5); }
void i2c_start() {
    digitalWrite(I2C_SDA, HIGH); i2c_delay();
    digitalWrite(I2C_SCL, HIGH); i2c_delay();
    digitalWrite(I2C_SDA, LOW);  i2c_delay();
    digitalWrite(I2C_SCL, LOW);  i2c_delay();
}
void i2c_stop() {
    digitalWrite(I2C_SDA, LOW);  i2c_delay();
    digitalWrite(I2C_SCL, HIGH); i2c_delay();
    digitalWrite(I2C_SDA, HIGH); i2c_delay();
}
bool i2c_write(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        digitalWrite(I2C_SDA, (data & 0x80) ? HIGH : LOW);
        data <<= 1; i2c_delay();
        digitalWrite(I2C_SCL, HIGH); i2c_delay();
        digitalWrite(I2C_SCL, LOW); i2c_delay();
    }
    pinMode(I2C_SDA, INPUT_PULLUP);
    digitalWrite(I2C_SCL, HIGH); i2c_delay();
    bool ack = (digitalRead(I2C_SDA) == LOW);
    digitalWrite(I2C_SCL, LOW); i2c_delay();
    pinMode(I2C_SDA, OUTPUT);
    return ack;
}
uint8_t i2c_read(bool ack) {
    uint8_t data = 0; pinMode(I2C_SDA, INPUT_PULLUP);
    for (int i = 0; i < 8; i++) {
        digitalWrite(I2C_SCL, HIGH); i2c_delay();
        data <<= 1; if (digitalRead(I2C_SDA) == HIGH) data |= 1;
        digitalWrite(I2C_SCL, LOW); i2c_delay();
    }
    pinMode(I2C_SDA, OUTPUT);
    digitalWrite(I2C_SDA, ack ? LOW : HIGH); i2c_delay();
    digitalWrite(I2C_SCL, HIGH); i2c_delay();
    digitalWrite(I2C_SCL, LOW); i2c_delay();
    digitalWrite(I2C_SDA, HIGH);
    return data;
}
uint16_t readExpander() {
    if (!expanderActive) return 0xFFFF;
    uint16_t data = 0xFFFF;
    #ifdef USE_MCP23017
        i2c_start();
        if (i2c_write(IO_EXPANDER_ADDRESS << 1)) {
            i2c_write(0x12); i2c_start();
            i2c_write((IO_EXPANDER_ADDRESS << 1) | 1);
            uint8_t low = i2c_read(true); uint8_t high = i2c_read(false);
            data = low | (high << 8);
        }
        i2c_stop();
    #elif defined(USE_PCF8575)
        i2c_start();
        if (i2c_write((IO_EXPANDER_ADDRESS << 1) | 1)) {
            uint8_t low = i2c_read(true); uint8_t high = i2c_read(false);
            data = low | (high << 8);
        }
        i2c_stop();
    #endif
    return data;
}
bool isExpanderPressed(uint8_t pin) { return !(readExpander() & (1 << pin)); }

void updateSelection(int newIndex) {
    if (appFiles.empty()) return;
    if (newIndex < 0) newIndex = appFiles.size() - 1;
    if (newIndex >= (int)appFiles.size()) newIndex = 0;
    selectedIndex = newIndex;
    if (selectedIndex < windowStart) windowStart = selectedIndex;
    else if (selectedIndex >= windowStart + 6) windowStart = selectedIndex - 5;
    drawUI();
}

void listApps() {
    struct AppFile { String name; uint32_t time; };
    std::vector<AppFile> files; appFiles.clear();
    if (!sdMounted) return;
    File root = SD.open(APP_PATH);
    if (!root || !root.isDirectory()) return;
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = String(file.name());
            if (name.endsWith(".bin")) files.push_back({name, (uint32_t)file.getLastWrite()});
        }
        file = root.openNextFile();
    }
    root.close();
    std::sort(files.begin(), files.end(), [](const AppFile &a, const AppFile &b) {
        #ifdef SORT_NEWEST_FIRST
            return a.time > b.time;
        #else
            return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
        #endif
    });
    for (const auto &f : files) appFiles.push_back(f.name);
}

void drawUI() {
    gfx->fillScreen(BLACK);
    gfx->setTextColor(WHITE); gfx->setTextSize(2);
    String title = MENU_TITLE; int16_t x1, y1; uint16_t w, h;
    gfx->getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor((320 - w) / 2, 10); gfx->println(title);
    gfx->drawLine(0, 35, 320, 35, GREEN);
    if (!sdMounted) {
        gfx->setCursor(10, 60); gfx->setTextColor(RED); gfx->println("SD Card NOT found!");
        gfx->setCursor(10, 85); gfx->setTextColor(WHITE); gfx->setTextSize(1);
        gfx->println("Please insert SD and reboot"); return;
    }
    if (appFiles.empty()) { gfx->setCursor(10, 60); gfx->println("No .bin files found"); return; }
    for (int i = windowStart; i < (int)appFiles.size() && i < windowStart + 6; i++) {
        int y = 60 + ((i - windowStart) * 30);
        if (i == selectedIndex) { gfx->fillRect(5, y - 5, 310, 25, BLUE); gfx->setTextColor(WHITE); }
        else { gfx->setTextColor(LIGHTGREY); }
        gfx->setCursor(15, y);
        String disp = appFiles[i]; if (disp.endsWith(".bin")) disp = disp.substring(0, disp.length() - 4);
        int maxChars = (appFiles[i] == lastApp) ? 19 : 23;
        if (disp.length() > maxChars) disp = disp.substring(0, maxChars - 3) + "...";
        gfx->print(disp); if (appFiles[i] == lastApp) { gfx->setTextColor(GREEN); gfx->print(" (*)"); }
        gfx->println();
    }
    if (windowStart > 0) gfx->fillTriangle(310, 45, 305, 52, 315, 52, WHITE);
    if (windowStart + 6 < (int)appFiles.size()) gfx->fillTriangle(310, 235, 305, 228, 315, 228, WHITE);
}

void drawProgressBar(int progress, int total) {
    int width = 280, height = 20, x = 20, y = 140;
    float pct = (float)progress / total;
    gfx->drawRect(x, y, width, height, WHITE);
    gfx->fillRect(x + 2, y + 2, (int)(width * pct) - 4, height - 4, GREEN);
    gfx->fillRect(x, y + height + 5, width, 20, BLACK);
    char pctStr[10]; snprintf(pctStr, sizeof(pctStr), "%d%%", (int)(pct * 100));
    int16_t x1, y1; uint16_t w, h;
    gfx->getTextBounds(pctStr, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor((320 - w) / 2, y + height + 5);
    gfx->setTextColor(WHITE); gfx->print(pctStr);
}

void flashApp(String fileName) {
    const esp_partition_t* update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (!update_partition) return;
    if (fileName == lastApp) {
        gfx->fillScreen(BLACK); gfx->setTextColor(GREEN); gfx->setTextSize(2);
        String msg = "Booting App..."; int16_t x1, y1; uint16_t w, h;
        gfx->getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((320 - w) / 2, 60); gfx->println(msg);
        gfx->setTextSize(3); String dispName = fileName; if (dispName.endsWith(".bin")) dispName = dispName.substring(0, dispName.length() - 4);
        gfx->getTextBounds(dispName, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((320 - w) / 2, 100); gfx->println(dispName);
        esp_ota_set_boot_partition(update_partition); delay(1500); ESP.restart(); return;
    }
    gfx->fillScreen(BLACK); gfx->setTextColor(CYAN); gfx->setTextSize(2);
    String flashMsg = "Flashing..."; int16_t x1, y1; uint16_t w, h;
    gfx->getTextBounds(flashMsg, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor((320 - w) / 2, 60); gfx->println(flashMsg);
    gfx->setTextSize(2); String waitMsg = "Please wait...";
    gfx->getTextBounds(waitMsg, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor((320 - w) / 2, 95); gfx->println(waitMsg);
    gfx->setTextSize(1); String dangerMsg = "Do not power off.";
    gfx->getTextBounds(dangerMsg, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor((320 - w) / 2, 120); gfx->println(dangerMsg);
    String fullPath = String(APP_PATH) + "/" + fileName;
    File file = SD.open(fullPath); if (!file) return;
    size_t fileSize = file.size();
    if (Update.begin(fileSize, U_FLASH)) {
        int written = 0; uint8_t buf[1024]; int bytesRead; int lastPercent = -1;
        while (written < fileSize) {
            bytesRead = file.read(buf, sizeof(buf)); if (bytesRead == 0) break;
            written += Update.write(buf, bytesRead);
            int percent = (written * 100) / fileSize;
            if (percent != lastPercent) { lastPercent = percent; drawProgressBar(written, fileSize); }
        }
        if (written == fileSize && Update.end()) {
            prefs.begin("launcher", false); prefs.putString("last_app", fileName); prefs.end();
            esp_ota_set_boot_partition(update_partition); delay(1000); ESP.restart();
        }
    }
    file.close(); drawUI();
}

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Serial.begin(115200); delay(500);
    pinMode(NAV_BUTTON, INPUT_PULLUP); pinMode(I2C_SDA, OUTPUT); pinMode(I2C_SCL, OUTPUT);
    prefs.begin("launcher", false); lastApp = prefs.getString("last_app", ""); prefs.end();
    gfx->begin(); gfx->setRotation(SCREEN_ROTATION); gfx->invertDisplay(SCREEN_INVERSION);
    pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);

    #if ENABLE_I2C_CONTROLLER
    i2c_start();
    if (i2c_write(IO_EXPANDER_ADDRESS << 1)) {
        #ifdef USE_MCP23017
            i2c_write(0x00); i2c_write(0xFF); i2c_stop(); i2c_start();
            i2c_write(IO_EXPANDER_ADDRESS << 1); i2c_write(0x0C); i2c_write(0xFF);
        #elif defined(USE_PCF8575)
            i2c_write(0xFF); i2c_write(0xFF);
        #endif
        expanderActive = true;
    }
    i2c_stop();
    #endif

    if (ENABLE_AUTOBOOT && lastApp != "") {
        gfx->fillScreen(BLACK); gfx->setTextColor(WHITE); gfx->setTextSize(2);
        String autoMsg = "Autobooting:"; int16_t x1, y1; uint16_t w, h;
        gfx->getTextBounds(autoMsg, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((320 - w) / 2, 40); gfx->println(autoMsg);
        gfx->setTextColor(CYAN); gfx->setTextSize(3);
        String dispApp = lastApp; if (dispApp.endsWith(".bin")) dispApp = dispApp.substring(0, dispApp.length() - 4);
        gfx->getTextBounds(dispApp, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((320 - w) / 2, 80); gfx->println(dispApp);
        gfx->setTextColor(YELLOW); gfx->setTextSize(1);
        String cancelMsg = "Press button to enter menu";
        gfx->getTextBounds(cancelMsg, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((320 - w) / 2, 130); gfx->println(cancelMsg);
        bool cancelled = false;
        for (int i = AUTOBOOT_DELAY_SEC; i > 0; i--) {
            String countStr = String(i); gfx->fillRect(110, 160, 100, 40, BLACK);
            gfx->setTextSize(4); gfx->setTextColor(WHITE);
            gfx->getTextBounds(countStr, 0, 0, &x1, &y1, &w, &h);
            gfx->setCursor((320 - w) / 2, 160); gfx->print(countStr);
            for (int j = 0; j < 100; j++) {
                if (digitalRead(NAV_BUTTON) == LOW || isExpanderPressed(BTN_A)) { cancelled = true; break; }
                delay(10);
            }
            if (cancelled) break;
        }
        if (!cancelled) {
            const esp_partition_t* ota_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
            if (ota_part) { esp_ota_set_boot_partition(ota_part); ESP.restart(); }
        }
    }
    while(digitalRead(NAV_BUTTON) == LOW || isExpanderPressed(BTN_A)) { delay(10); }
    delay(100);
    const esp_partition_t* factory_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (factory_part) esp_ota_set_boot_partition(factory_part);
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (SD.begin(SD_CS)) sdMounted = true; else sdMounted = false;
    listApps(); updateSelection(0);
}

void loop() {
    bool isPressed = (digitalRead(NAV_BUTTON) == LOW);
    bool upPressed = isExpanderPressed(BTN_UP);
    bool downPressed = isExpanderPressed(BTN_DOWN);
    bool aPressed = isExpanderPressed(BTN_A);
    if (expanderActive && !appFiles.empty()) {
        static unsigned long lastMove = 0; static bool lastAPressed = false;
        if (aPressed && !lastAPressed) { flashApp(appFiles[selectedIndex]); lastAPressed = true; }
        else if (!aPressed) lastAPressed = false;
        if (millis() - lastMove > 250) { 
            if (upPressed) { updateSelection(selectedIndex - 1); lastMove = millis(); }
            else if (downPressed) { updateSelection(selectedIndex + 1); lastMove = millis(); }
        }
    }
    if (isPressed && !buttonActive) { buttonActive = true; buttonPressTime = millis(); }
    else if (!isPressed && buttonActive) {
        buttonActive = false; unsigned long duration = millis() - buttonPressTime;
        if (duration >= LONG_PRESS_MS) { if (!appFiles.empty()) flashApp(appFiles[selectedIndex]); }
        else if (duration >= DEBOUNCE_MS) { updateSelection(selectedIndex + 1); }
    }
}
