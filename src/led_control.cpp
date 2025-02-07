#include "led_control.h"

void initLED() {
  pinMode(LED_BUILTIN, OUTPUT);
#ifdef ESP32
  // Konfiguracja LEDC: kanał, częstotliwość, rozdzielczość (8-bit)
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ, 8);
  ledcAttachPin(LED_BUILTIN, LEDC_CHANNEL);
#endif
  // Na starcie wyłączamy LED.
  setLEDBrightness(0);
}

void setLEDBrightness(uint16_t brightness) {
#ifdef ESP32
  ledcWrite(LEDC_CHANNEL, brightness);
#else
  analogWrite(LED_BUILTIN, brightness);
#endif
}

void updateLED(bool on) {
  if (on) {
    setLEDBrightness(LED_BRIGHTNESS);
  } else {
    setLEDBrightness(0);
  }
}
