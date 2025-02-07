#include "config.h"
#include <Arduino.h>

// Warunkowy dobór bibliotek w zależności od platformy
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

// Konfiguracja wyświetlacza OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Timer Blynk (używany do okresowej wysyłki statystyk)
BlynkTimer timer;

// Ustawienia czasu (przechowywane w sekundach)
// Domyślne wartości: 25 minut nauki, 5 minut przerwy
volatile unsigned long studyTimeSetting = 25 * 60; // 1500 sekund
volatile unsigned long breakTimeSetting = 5 * 60;  // 300 sekund

// Zmienna aktualnego odliczania (w sekundach)
// Przy starcie rozpoczynamy od trybu nauki
volatile unsigned long currentTimer = studyTimeSetting;

// Flaga trybu – true: nauka, false: przerwa
volatile bool isStudying = true;

// Zmienna do odmierzania sekund (nieblokująca metoda oparta o millis())
unsigned long lastSecondMillis = 0;

// Statystyki (wszystkie liczby podawane w sekundach lub liczbie sesji)
// Czas spędzony na naukę, przerwach, oraz ogólny czas działania programu.
unsigned long totalStudyTime = 0;
unsigned long totalBreakTime = 0;
unsigned long overallTime    = 0;
unsigned int studySessions   = 0;
unsigned int breakSessions   = 0;

//
// BLYNK_WRITE – aktualizacja ustawień z aplikacji
//

// Ustawienie czasu nauki (V1)
// Zakładamy, że widget Time Input zwraca czas w sekundach
BLYNK_WRITE(V1) {
  int newStudyTime = param.asInt();  // wartość w sekundach
  studyTimeSetting = newStudyTime;
  Serial.print("Otrzymano czas nauki (sekundy): ");
  Serial.println(newStudyTime);
  // Jeśli jesteśmy w trybie nauki, od razu ustawiamy aktualny licznik
  if (isStudying) {
    currentTimer = studyTimeSetting;
  }
}

// Ustawienie czasu przerwy (V2)
// Zakładamy, że widget Time Input zwraca czas w sekundach
BLYNK_WRITE(V2) {
  int newBreakTime = param.asInt();  // wartość w sekundach
  breakTimeSetting = newBreakTime;
  Serial.print("Otrzymano czas przerwy (sekundy): ");
  Serial.println(newBreakTime);
  // Jeśli jesteśmy w trybie przerwy, od razu ustawiamy aktualny licznik
  if (!isStudying) {
    currentTimer = breakTimeSetting;
  }
}

// Przełącznik ręczny między czasem nauki i przerwy (V0)
void switchMode();

BLYNK_WRITE(V0) {
  int value = param.asInt();
  
  if (value == 1) {  // Jeśli przycisk został wciśnięty
    switchMode();  // Przełącz tryb
  }
}

// Funkcja aktualizująca wyświetlacz OLED
void updateDisplay(unsigned long seconds, const char* label) {
  int minutes = seconds / 60;
  int sec = seconds % 60;
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(label);
  display.setCursor(0, 30);
  display.printf("%02d:%02d", minutes, sec);
  display.display();
}

// Funkcja wysyłająca statystyki do aplikacji Blynk
void sendStatsToBlynk() {
  Blynk.virtualWrite(V3, totalStudyTime);
  Blynk.virtualWrite(V4, totalBreakTime);
  Blynk.virtualWrite(V5, overallTime);
  Blynk.virtualWrite(V6, studySessions);
  Blynk.virtualWrite(V7, breakSessions);
  Serial.println("Statystyki wysłane do Blynk.");
}

// Funkcja przełączająca tryb po zakończeniu sesji
void switchMode() {
  // Jeśli zakończyła się sesja nauki, to dodajemy czas nauki do statystyk
  if (isStudying) {
    Serial.println("Sesja nauki zakończona.");
    isStudying = false;             // przełącz na przerwę
    currentTimer = breakTimeSetting; // ustawienie nowego czasu przerwy
    breakSessions++;                // zliczamy sesję przerwy
  } else {
    Serial.println("Sesja przerwy zakończona.");
    isStudying = true;              // przełącz na naukę
    currentTimer = studyTimeSetting; // ustawienie nowego czasu nauki
    studySessions++;                // zliczamy sesję nauki
  }
}

void setup() {
  Serial.begin(115200);
  
  // Połączenie z Wi-Fi
  Serial.print("Łączenie z Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nPołączono z Wi-Fi!");

  // Inicjalizacja Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  
  // Inicjalizacja wyświetlacza OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // typowy adres I2C = 0x3C
    Serial.println("Błąd inicjalizacji OLED!");
    while(true); // zatrzymaj działanie
  }
  display.clearDisplay();
  display.display();

  // (Opcjonalnie dla ESP8266) – inicjalizacja magistrali I2C, jeśli wymagane
  #if defined(ESP8266)
    Wire.begin(); // Domyślne piny SDA i SCL są ustawione w bibliotece ESP8266
  #endif
  
  // Ustawienia początkowe
  isStudying = true;
  currentTimer = studyTimeSetting;
  studySessions++; // rozpoczynamy pierwszą sesję nauki
  lastSecondMillis = millis();
  
  // Ustawienie timera Blynk do wysyłania statystyk co 5 sekund
  timer.setInterval(5000L, sendStatsToBlynk);
}

void loop() {
  Blynk.run();
  timer.run();

  unsigned long currentMillis = millis();
  
  // Sprawdzamy, czy upłynęła sekunda
  if (currentMillis - lastSecondMillis >= 1000) {
    lastSecondMillis = currentMillis;
    
    // Zliczanie ogólnego czasu
    overallTime++;
    
    // Dodajemy upływającą sekundę do odpowiedniego licznika statystyk
    if (isStudying) {
      totalStudyTime++;
    } else {
      totalBreakTime++;
    }
    
    // Jeśli licznik odliczania jeszcze nie wygasł – zmniejszamy go
    if (currentTimer > 0) {
      currentTimer--;
    } else {
      // Gdy licznik osiągnie zero – przełącz tryb
      switchMode();
    }
    
    // Aktualizacja wyświetlacza OLED – wyświetlamy aktualny tryb i czas
    if (isStudying) {
      updateDisplay(currentTimer, "Nauka");
    } else {
      updateDisplay(currentTimer, "Przerwa");
    }
  }
}
