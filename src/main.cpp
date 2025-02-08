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

// Zmienne do wyświetlania komunikatów potwierdzających zmianę ustawień
String confirmationMsg = "";
unsigned long confirmationMsgTimestamp = 0;
// Czas wyświetlania komunikatu zmiany statusu lub czasu timera np. STUDY: 20min lub Switch to
const unsigned long confirmationMsgDuration = 2000; // ms

// Nowe zmienne do szybszej aktualizacji wyświetlacza
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 200; // aktualizacja co 200 ms

// -------------------------------
// Nowa obsługa przycisku – multi-click
// -------------------------------
const unsigned long clickTimeout = 300;       // skrócony czas oczekiwania na kolejne kliknięcie (300 ms)
const unsigned long longPressThreshold = 5000;  // ms przytrzymania dla długiego naciśnięcia

unsigned long buttonPressStartTime = 0;
unsigned long lastButtonReleaseTime = 0;
int clickCount = 0;
bool buttonIsPressed = false;
bool longPressTriggered = false;

void processClicks(int count) {
  Serial.print("Wykryto ");
  Serial.print(count);
  Serial.println(" kliknięć");
  if (count == 1) {
    // 1 klik – reset licznika
    if (isStudying) {
      currentTimer = studyTimeSetting;
      confirmationMsg = "Reset\nSTUDY";
      Serial.println("Reset licznika STUDY");
    } else {
      currentTimer = breakTimeSetting;
      confirmationMsg = "Reset\nBREAK";
      Serial.println("Reset licznika BREAK");
    }
    confirmationMsgTimestamp = millis();
  }
  else if (count == 2) {
    // 2 kliki – zmiana czasu STUDY między 10 a 20 minut
    if (studyTimeSetting == 10 * 60) {
      studyTimeSetting = 20 * 60;
      confirmationMsg = "STUDY:\n20 min";
      Serial.println("Czas STUDY ustawiony na 20 minut");
    } else {
      studyTimeSetting = 10 * 60;
      confirmationMsg = "STUDY:\n10 min";
      Serial.println("Czas STUDY ustawiony na 10 minut");
    }
    if (isStudying) {
      currentTimer = studyTimeSetting;
    }
    confirmationMsgTimestamp = millis();
  }
  else if (count == 3) {
    // 3 kliki – zmiana czasu BREAK między 5 a 10 minut
    if (breakTimeSetting == 5 * 60) {
      breakTimeSetting = 10 * 60;
      confirmationMsg = "BREAK:\n10 min";
      Serial.println("Czas BREAK ustawiony na 10 minut");
    } else {
      breakTimeSetting = 5 * 60;
      confirmationMsg = "BREAK:\n5 min";
      Serial.println("Czas BREAK ustawiony na 5 minut");
    }
    if (!isStudying) {
      currentTimer = breakTimeSetting;
    }
    confirmationMsgTimestamp = millis();
  }
  else if (count == 4) {
    // 4 kliki – natychmiastowe przełączenie trybu
    confirmationMsg = "Switch\nto";
    confirmationMsgTimestamp = millis();
    Serial.println("Wymuszone przełączenie trybu");
    // Przełącz tryb
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
  else {
    Serial.println("Nieobsługiwana liczba kliknięć");
  }
}

// -------------------------------
// Funkcja przełączająca WiFi – po przytrzymaniu przycisku BOOT przez 5 sekund
// -------------------------------
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
BLYNK_WRITE(V0) {
  int value = param.asInt();
  if (value == 1) {
    // Wykonujemy przełączenie trybu (analogicznie jak w processClicks przy 4 kliknięciach)
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
}

// ---------------------------------------------------------------------
// Funkcja pomocnicza do rysowania wieloliniowego, wyśrodkowanego komunikatu
// ---------------------------------------------------------------------
void displayConfirmationMsg(const String &msg) {
  // Podział komunikatu na linie
  const int maxLines = 10; // maksymalna liczba linii
  String lines[maxLines];
  int numLines = 0;
  
  int startIndex = 0;
  for (int i = 0; i < msg.length(); i++) {
    if (msg.charAt(i) == '\n') {
      lines[numLines++] = msg.substring(startIndex, i);
      startIndex = i + 1;
    }
  }
  // Dodaj ostatnią linię (lub całość, gdy brak '\n')
  if (startIndex < msg.length() && numLines < maxLines) {
    lines[numLines++] = msg.substring(startIndex);
  }
  
  // Ustawienia czcionki – używamy textSize(2)
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  // Oblicz całkowitą wysokość komunikatu
  int lineHeight = 26;
  int totalHeight = numLines * lineHeight;
  
  // Wyznacz pionowy punkt startowy, aby komunikat był wyśrodkowany
  int startY = (SCREEN_HEIGHT - totalHeight) / 2;
  
  // Rysuj każdą linię
  for (int i = 0; i < numLines; i++) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(lines[i].c_str(), 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2 - x1;
    int y = startY + i * lineHeight;
    display.setCursor(x, y);
    display.print(lines[i]);
  }
}

// ---------------------------------------------------------------------
// Funkcja rysująca zawartość wyświetlacza OLED:
// - Jeśli aktywny komunikat potwierdzający, wyświetla go
// - W przeciwnym razie: etykieta (STUDY lub BREAK) (textSize 2),
//   licznik czasu (textSize 3) oraz ikonę WiFi
// ---------------------------------------------------------------------
void updateDisplay(unsigned long seconds, const char* label) {
  display.clearDisplay();
  
  // Jeśli aktywny komunikat potwierdzający – wyświetl go
  if (confirmationMsg != "" && (millis() - confirmationMsgTimestamp < confirmationMsgDuration)) {
    displayConfirmationMsg(confirmationMsg);
    display.display();
    return;
  } else {
    // Po upływie czasu komunikatu, czyścimy zmienną
    confirmationMsg = "";
  }
  
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
  unsigned long currentMillis = millis();
  
  // -------------------------------
  // Obsługa przycisku (multi-click + long press)
  // -------------------------------
  bool currentButtonState = digitalRead(BOOT_BUTTON_PIN); // LOW = przycisk wciśnięty
  
  // Wykrycie rozpoczęcia naciśnięcia
  if (currentButtonState == LOW && !buttonIsPressed) {
    buttonIsPressed = true;
    buttonPressStartTime = currentMillis;
    longPressTriggered = false;
  }
  
  // Jeśli przycisk nadal wciśnięty – sprawdzamy, czy przekroczono próg długiego przytrzymania
  if (currentButtonState == LOW && buttonIsPressed && !longPressTriggered) {
    if (currentMillis - buttonPressStartTime >= longPressThreshold) {
      longPressTriggered = true;
      toggleWiFi();
      // W przypadku długiego przytrzymania nie liczymy kliknięć:
      clickCount = 0;
    }
  }
  
  // Wykrycie puszczenia przycisku
  if (currentButtonState == HIGH && buttonIsPressed) {
    if (!longPressTriggered) {
      clickCount++;
      lastButtonReleaseTime = currentMillis;
    }
    buttonIsPressed = false;
  }
  
  // Jeśli minął czas oczekiwania na kolejne kliknięcie – przetwarzamy akcję
  if (!buttonIsPressed && clickCount > 0 && (currentMillis - lastButtonReleaseTime >= clickTimeout)) {
    processClicks(clickCount);
    clickCount = 0;
  }
  
  // -------------------------------
  // Sterowanie LED – włączona tylko, gdy WiFi jest włączone, ale jeszcze nie połączone.
  bool ledShouldBeOn = (wifiEnabled && !wifiActive);
  updateLED(ledShouldBeOn);
  
  // Obsługa łączenia – jeśli WiFi jest włączone, podejmujemy próbę połączenia.
  if (wifiEnabled) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!wifiActive) {
        Serial.println("WiFi połączone.");
        wifiActive = true;
        wifiConnecting = false;
        Blynk.config(BLYNK_AUTH_TOKEN);
        Blynk.connect();
      }
    } else {
      if (wifiActive) {
        Serial.println("Utracono połączenie, ponawiam próbę...");
        wifiActive = false;
        wifiConnecting = true;
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
    }
  }
  
  if (wifiActive) {
    Blynk.run();
    timer.run();
  }
  
  // -------------------------------
  // Aktualizacja timera (co 1 sekundę)
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
      // Automatyczne przełączenie trybu po zakończeniu odliczania
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
  }
  
  // -------------------------------
  // Szybsza aktualizacja wyświetlacza (co 200 ms)
  if (currentMillis - lastDisplayUpdate >= displayUpdateInterval) {
    lastDisplayUpdate = currentMillis;
    if (isStudying) {
      updateDisplay(currentTimer, "STUDY");
    } else {
      updateDisplay(currentTimer, "BREAK");
    }
  }
}
