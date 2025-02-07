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

// Ustawienia czasu (w sekundach)
// Nauka = 10 minut (600 s), Przerwa = 5 minut (300 s)
volatile unsigned long studyTimeSetting = 10 * 60; // 600 sekund
volatile unsigned long breakTimeSetting = 5 * 60;  // 300 sekund

// Aktualny licznik (w sekundach) – startujemy od trybu nauki
volatile unsigned long currentTimer = studyTimeSetting;

// Flaga trybu – true: nauka, false: przerwa
volatile bool isStudying = true;

// Zmienna do odmierzania sekund (oparta o millis())
unsigned long lastSecondMillis = 0;

// Statystyki (czas w sekundach lub liczba sesji)
unsigned long totalStudyTime = 0;
unsigned long totalBreakTime = 0;
unsigned long overallTime    = 0;
unsigned int studySessions   = 0;
unsigned int breakSessions   = 0;

// Definicje przycisku BOOT do sterowania WiFi
#define BOOT_BUTTON_PIN 0   // Dla ESP32 – przycisk BOOT zwykle na GPIO0 (upewnij się, że to odpowiedni pin)
bool wifiActive = false;      // Czy WiFi jest aktywne
bool wifiConnecting = false;  // Flaga informująca, że trwa próba asynchronicznego łączenia
unsigned long wifiConnectStartTime = 0; // Czas rozpoczęcia próby łączenia
bool buttonPressed = false;   // Czy przycisk jest aktualnie wciśnięty
bool wifiToggleTriggered = false; // Czy akcja przełączania WiFi została już wywołana przy bieżącym przytrzymaniu
unsigned long buttonPressStartTime = 0;

// Funkcja inicjująca asynchroniczne łączenie lub rozłączanie WiFi
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

// BLYNK_WRITE – aktualizacja ustawień z aplikacji

// Ustawienie czasu nauki (V1)
// Zakładamy, że widget Time Input przesyła czas w sekundach
BLYNK_WRITE(V1) {
  int newStudyTime = param.asInt();  // wartość w sekundach
  studyTimeSetting = newStudyTime;
  Serial.print("Otrzymano czas nauki (sekundy): ");
  Serial.println(newStudyTime);
  // Jeśli jesteśmy w trybie nauki, natychmiast aktualizujemy licznik
  if (isStudying) {
    currentTimer = studyTimeSetting;
  }
}

// Ustawienie czasu przerwy (V2)
// Zakładamy, że widget Time Input przesyła czas w sekundach
BLYNK_WRITE(V2) {
  int newBreakTime = param.asInt();  // wartość w sekundach
  breakTimeSetting = newBreakTime;
  Serial.print("Otrzymano czas przerwy (sekundy): ");
  Serial.println(newBreakTime);
  // Jeśli jesteśmy w trybie przerwy, natychmiast aktualizujemy licznik
  if (!isStudying) {
    currentTimer = breakTimeSetting;
  }
}

// Przełącznik ręczny między trybem nauki a przerwy (V0)
void switchMode();

BLYNK_WRITE(V0) {
  int value = param.asInt();
  
  if (value == 1) {  // Jeśli przycisk został naciśnięty
    switchMode();  // przełącz tryb
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
  if (isStudying) {
    Serial.println("Sesja nauki zakończona.");
    isStudying = false;              // przełącz na przerwę
    currentTimer = breakTimeSetting; // ustaw nowy czas przerwy
    breakSessions++;                 // zlicz sesję przerwy
  } else {
    Serial.println("Sesja przerwy zakończona.");
    isStudying = true;               // przełącz na naukę
    currentTimer = studyTimeSetting; // ustaw nowy czas nauki
    studySessions++;                 // zlicz sesję nauki
  }
}

void setup() {
  Serial.begin(115200);
  
  // Inicjalizacja przycisku BOOT do sterowania WiFi
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  
  // Na starcie WiFi jest wyłączone
  wifiActive = false;
  wifiConnecting = false;
  Serial.println("WiFi jest wyłączone. Aby włączyć, przytrzymaj przycisk BOOT przez 5 sekund.");
  
  // Inicjalizacja wyświetlacza OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // typowy adres I2C = 0x3C
    Serial.println("Błąd inicjalizacji OLED!");
    while(true); // zatrzymaj działanie, jeśli wyświetlacz nie został zainicjalizowany
  }
  display.clearDisplay();
  display.display();

  // (Opcjonalnie dla ESP8266) – inicjalizacja magistrali I2C, jeśli wymagane
  #if defined(ESP8266)
    Wire.begin();
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
  // Obsługa przycisku BOOT do przełączania WiFi
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) { // przycisk wciśnięty (aktywny niski)
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
  
  // Asynchroniczne sprawdzanie statusu łączenia WiFi
  if (wifiConnecting) {
    if (WiFi.status() == WL_CONNECTED) {
         Serial.println("\nWiFi połączone.");
         wifiActive = true;
         wifiConnecting = false;
         Blynk.config(BLYNK_AUTH_TOKEN);
         Blynk.connect();
    } else if (millis() - wifiConnectStartTime >= 10000) {
         Serial.println("\nBłąd połączenia WiFi.");
         wifiConnecting = false;
         wifiActive = false;
    }
  }
  
  // Uruchamiaj Blynk i timer tylko, gdy WiFi jest aktywne
  if (wifiActive) {
    Blynk.run();
    timer.run();
  }
  
  unsigned long currentMillis = millis();
  
  // Aktualizacja zegara co sekundę
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
      // Po zakończeniu sesji automatycznie przełącz tryb
      switchMode();
    }
    
    // Aktualizacja wyświetlacza OLED z aktualnym trybem i czasem
    if (isStudying) {
      updateDisplay(currentTimer, "Nauka");
    } else {
      updateDisplay(currentTimer, "Przerwa");
    }
  }
}
