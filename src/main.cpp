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
#include "wifi_icon.h"    // Rysowanie ikony WiFi
#include "led_control.h"  // Sterowanie LED przez PWM (30% mocy)

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

// Zmienne związane z WiFi
bool wifiActive = false;      // ESP połączone z siecią
bool wifiConnecting = false;  // Próba połączenia (flaga informacyjna)
bool wifiEnabled = false;     // Użytkownik włączył WiFi (przytrzymanie przycisku)

// Zmienne do obsługi przycisku BOOT (do przełączania WiFi)
bool buttonPressed = false;
bool toggleDone = false;      // Zapobiega wielokrotnemu wywołaniu toggleWiFi()
unsigned long buttonPressStartTime = 0;

// Funkcja przełączająca WiFi – po przytrzymaniu przycisku BOOT przez 5 sekund
void toggleWiFi() {
  if (!wifiEnabled) { // Jeśli WiFi jest wyłączone, to je włączamy
    Serial.println("Włączanie WiFi...");
    wifiEnabled = true;  // Użytkownik włączył WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiConnecting = true;
  } else {  // Jeśli WiFi jest włączone, to je wyłączamy
    Serial.println("Wyłączanie WiFi...");
    wifiEnabled = false;
    Blynk.disconnect();
    WiFi.disconnect();
    wifiActive = false;
    wifiConnecting = false;
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

  // Na starcie WiFi jest wyłączone – zatem też LED będzie zgaszony.
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
  // Obsługa przycisku BOOT – przytrzymanie przez 5 sekund przełącza stan WiFi
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      buttonPressStartTime = millis();
    } else {
      if (!toggleDone && (millis() - buttonPressStartTime >= 5000)) {
        toggleDone = true;
        toggleWiFi();
      }
    }
  } else {
    buttonPressed = false;
    toggleDone = false;
  }

  // Sterowanie LED wyłącznie na podstawie stanu WiFi:
  // - Gdy WiFi jest włączone (wifiEnabled == true) ale nie połączone (wifiActive == false),
  //   LED świeci z mocą 30% (PWM).
  // - W pozostałych przypadkach LED jest zgaszony.
  bool ledShouldBeOn = (wifiEnabled && !wifiActive);
  updateLED(ledShouldBeOn);

  // Obsługa łączenia – jeśli WiFi jest włączone, ciągle podejmujemy próbę połączenia.
  if (wifiEnabled) {
    if (WiFi.status() == WL_CONNECTED) {
      // Jeśli uzyskano połączenie – ustawiamy flagę i inicjujemy Blynk (tylko przy pierwszym połączeniu)
      if (!wifiActive) {
        Serial.println("WiFi połączone.");
        wifiActive = true;
        wifiConnecting = false;
        Blynk.config(BLYNK_AUTH_TOKEN);
        Blynk.connect();
      }
    } else {
      // Jeśli wcześniej mieliśmy połączenie, a teraz je straciliśmy – podejmujemy próbę ponownego łączenia.
      if (wifiActive) {
        Serial.println("Utracono połączenie, ponawiam próbę...");
        wifiActive = false;
        wifiConnecting = true;
        // Resetujemy połączenie
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
      // Jeśli wciąż nie ma połączenia, to nie robimy nic – ESP będzie stale próbowało połączyć się.
    }
  }

  // Jeśli połączenie jest nawiązane, wykonujemy Blynk
  if (wifiActive) {
    Blynk.run();
    timer.run();
  }

  // Obsługa odliczania czasu (study/break)
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
