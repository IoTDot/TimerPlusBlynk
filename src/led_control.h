#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <Arduino.h>

// Jeśli LED_BUILTIN nie jest zdefiniowany, definiujemy go ręcznie (dla ESP32 np. GPIO2)
#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

#ifdef ESP32
  // Używamy LEDC (PWM) dla ESP32
  #define LEDC_CHANNEL    0
  #define LEDC_TIMER      0
  #define LEDC_FREQ       5000   // częstotliwość PWM (Hz)
  #define LED_MAX         255
  #define LED_BRIGHTNESS  77     // 30% z 255 (około 77)
#else  // Zakładamy ESP8266
  #define LED_MAX         1023
  #define LED_BRIGHTNESS  307    // 30% z 1023 (około 307)
#endif

// Inicjuje PWM dla wbudowanej diody LED.
void initLED();

// Ustawia jasność LED (wartość od 0 do LED_MAX).
void setLEDBrightness(uint16_t brightness);

// Funkcja ułatwiająca: włącza LED na 30% (gdy on == true), albo wyłącza (gdy on == false).
void updateLED(bool on);

#endif // LED_CONTROL_H
