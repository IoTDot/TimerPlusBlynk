#ifndef WIFI_ICON_H
#define WIFI_ICON_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>  // Potrzebne do rysowania bitmapy

// Bitmapa ikony WiFi (15x12 pikseli)
const unsigned char epd_bitmap_Bitmap [] PROGMEM = {
    0x0f, 0xe0, 0x3f, 0xf8, 0x70, 0x1c, 0xc3, 0x86,
    0x0f, 0xe0, 0x1c, 0x70, 0x10, 0x10, 0x07, 0xc0, 
    0x04, 0x40, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00
};

#define WIFI_ICON_WIDTH 15
#define WIFI_ICON_HEIGHT 12

// Funkcja rysująca ikonę WiFi
void drawWiFiIcon(Adafruit_SSD1306 &display, bool connected, bool connecting) {
    static unsigned long lastBlinkTime = 0;
    static bool iconVisible = true;

    // Jeśli połączony, wyświetlamy ikonę na stałe
    if (connected) {
        display.drawBitmap(111, 0, epd_bitmap_Bitmap, WIFI_ICON_WIDTH, WIFI_ICON_HEIGHT, SSD1306_WHITE);
    } 
    // Jeśli trwa łączenie lub WiFi jest odłączone, ikona miga
    else if (connecting) {
        if (millis() - lastBlinkTime > 500) {  // Przełączanie co 500 ms
            iconVisible = !iconVisible;
            lastBlinkTime = millis();
        }
        if (iconVisible) {
            display.drawBitmap(111, 0, epd_bitmap_Bitmap, WIFI_ICON_WIDTH, WIFI_ICON_HEIGHT, SSD1306_WHITE);
        }
    }
}

#endif  // WIFI_ICON_H
