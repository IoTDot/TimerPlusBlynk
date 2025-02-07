#include "config.h"
#include <Arduino.h>

#if defined(ESP32)
  #include <WiFi.h>
  #include <BlynkSimpleEsp32.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <BlynkSimpleEsp8266.h>
#else
  #error "This code only supports ESP32 and ESP8266 boards."
#endif

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "wifi_icon.h"    // Zaktualizowany plik – WIFI_ICON_WIDTH ustawiony na 16
#include "led_control.h"  // Obsługa LED przez PWM (30% mocy)

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
BlynkTimer timer;

// Ustawienia czasów (w sekundach):
// STUDY = 10 minut, BREAK = 5 minut
volatile unsigned long studyTimeSetting = 10 * 60; // 600 s
volatile unsigned long breakTimeSetting = 5 * 60;  // 300 s
volatile unsigned long currentTimer = studyTimeSetting;
volatile bool isStudying = true;
unsigned long lastSecondMillis = 0;

// Statystyki
unsigned long totalStudyTime = 0;
unsigned long totalBreakTime = 0;
unsigned long overallTime    = 0;
unsigned int studySessions   = 0;
unsigned int breakSessions   = 0;

#define BOOT_BUTTON_PIN 0   // Przycisk BOOT (podciągnięty)
bool wifiActive = false;
bool wifiConnecting = false;
unsigned long wifiConnectStartTime = 0;
bool buttonPressed = false;
bool wifiToggleTriggered = false;
unsigned long buttonPressStartTime = 0;

// Funkcja przełączająca WiFi (asynchronicznie)
void toggleWiFi() {
  if (!wifiActive && !wifiConnecting) {
    Serial.println("Włączanie WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiConnecting = true;
    wifiConnectStartTime = millis();
  } else if (wifiActive) {
    Serial.println("Wyłączanie WiFi...");
    Blynk.disconnect();
    WiFi.disconnect();
    wifiActive = false;
  }
}

// Aktualizacja ustawień STUDY/BREAK przez Blynk
BLYNK_WRITE(V1) {
  int newStudyTime = param.asInt();
  studyTimeSetting = newStudyTime;
  Serial.print("Otrzymano czas STUDY (sekundy): ");
  Serial.println(newStudyTime);
  if (isStudying) {
    currentTimer = studyTimeSetting;
  }
}

BLYNK_WRITE(V2) {
  int newBreakTime = param.asInt();
  breakTimeSetting = newBreakTime;
  Serial.print("Otrzymano czas BREAK (sekundy): ");
  Serial.println(newBreakTime);
  if (!isStudying) {
    currentTimer = breakTimeSetting;
  }
}

// Przełączanie trybu STUDY/BREAK (przycisk V0 w aplikacji Blynk)
void switchMode();
BLYNK_WRITE(V0) {
  int value = param.asInt();
  if (value == 1) {
    switchMode();
  }
}

// Funkcja rysująca zawartość wyświetlacza OLED:
// - Etykieta (STUDY lub BREAK) czcionką size 2
// - Powiększony zegar (size 3)
// - Ikona WiFi (funkcja drawWiFiIcon z pliku wifi_icon.h)
void updateDisplay(unsigned long seconds, const char* label) {
  display.clearDisplay();

  // Rysowanie etykiety
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
  int labelX = (SCREEN_WIDTH - w) / 2;
  int labelY = 0;
  display.setCursor(labelX, labelY);
  display.print(label);

  // Formatowanie czasu jako MM:SS
  int minutes = seconds / 60;
  int sec = seconds % 60;
  char timeStr[10];
  sprintf(timeStr, "%02d:%02d", minutes, sec);

  // Rysowanie zegara (tekst size 3)
  display.setTextSize(3);
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  int timeX = (SCREEN_WIDTH - w) / 2;
  int timeY = 28;
  display.setCursor(timeX, timeY);
  display.print(timeStr);

  // Rysowanie ikony WiFi
  drawWiFiIcon(display, wifiActive, wifiConnecting);

  display.display();
}

// Wysyłanie statystyk do Blynk
void sendStatsToBlynk() {
  Blynk.virtualWrite(V3, totalStudyTime);
  Blynk.virtualWrite(V4, totalBreakTime);
  Blynk.virtualWrite(V5, overallTime);
  Blynk.virtualWrite(V6, studySessions);
  Blynk.virtualWrite(V7, breakSessions);
  Serial.println("Statystyki wysłane do Blynk.");
}

// Przełączanie trybu STUDY/BREAK po zakończeniu sesji
void switchMode() {
  if (isStudying) {
    Serial.println("Sesja STUDY zakończona.");
    isStudying = false;
    currentTimer = breakTimeSetting;
    breakSessions++;
  } else {
    Serial.println("Sesja BREAK zakończona.");
    isStudying = true;
    currentTimer = studyTimeSetting;
    studySessions++;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // Inicjalizacja LED (PWM, 30% mocy)
  initLED();

  wifiActive = false;
  wifiConnecting = false;
  Serial.println("WiFi jest wyłączone. Aby włączyć, przytrzymaj przycisk BOOT przez 5 sekund.");

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Błąd inicjalizacji OLED!");
    while (true);
  }
  display.clearDisplay();
  display.display();

  #if defined(ESP8266)
    Wire.begin();
  #endif

  isStudying = true;
  currentTimer = studyTimeSetting;
  studySessions++;
  lastSecondMillis = millis();

  timer.setInterval(5000L, sendStatsToBlynk);
}

void loop() {
  // Obsługa przycisku BOOT – wciśnięty przycisk przez 5 sekund wyzwala toggle WiFi
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      buttonPressStartTime = millis();
    } else {
      if (millis() - buttonPressStartTime >= 5000 && !wifiToggleTriggered) {
        wifiToggleTriggered = true;
        toggleWiFi();
      }
    }
  } else {
    buttonPressed = false;
    wifiToggleTriggered = false;
  }

  // Aktualizacja LED: LED świeci (30% PWM) gdy trwa łączenie lub przycisk jest przytrzymany
  bool ledShouldBeOn = wifiConnecting || wifiToggleTriggered;
  updateLED(ledShouldBeOn);

  // Asynchroniczne sprawdzanie statusu łączenia WiFi
  if (wifiConnecting) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi połączone.");
      wifiActive = true;
      wifiConnecting = false;
      Blynk.config(BLYNK_AUTH_TOKEN);
      Blynk.connect();
    } else if (millis() - wifiConnectStartTime >= 10000) {
      Serial.println("Błąd połączenia WiFi.");
      wifiConnecting = false;
      wifiActive = false;
    }
  }

  if (wifiActive) {
    Blynk.run();
    timer.run();
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastSecondMillis >= 1000) {
    lastSecondMillis = currentMillis;
    overallTime++;
    if (isStudying) {
      totalStudyTime++;
    } else {
      totalBreakTime++;
    }
    if (currentTimer > 0) {
      currentTimer--;
    } else {
      switchMode();
    }
    if (isStudying) {
      updateDisplay(currentTimer, "STUDY");
    } else {
      updateDisplay(currentTimer, "BREAK");
    }
  }
}
