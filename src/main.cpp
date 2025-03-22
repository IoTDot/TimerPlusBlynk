#include "config.h"
#include <Arduino.h>
#include <Bounce2.h>
#include <ArduinoJson.h>  // Dodajemy bibliotekę ArduinoJson

#if defined(ESP32)
  #include <WiFi.h>
  #include <BlynkSimpleEsp32.h>
  #include <HTTPClient.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <BlynkSimpleEsp8266.h>
  #include <ESP8266HTTPClient.h>
#else
  #error "This code only supports ESP32 and ESP8266 boards."
#endif

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "wifi_icon.h"    // Rysowanie ikony WiFi
#include "led_control.h"  // Sterowanie LED przez PWM (30% mocy)
#include <TM1637Display.h>

// -------------------------
// Definicje wyświetlacza OLED
// -------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------------------------
// Definicje wyświetlacza TM1637
// -------------------------
#define TM_CLK_PIN 4    // Przykładowy pin CLK – dostosuj do swojego układu
#define TM_DIO_PIN 5    // Przykładowy pin DIO – dostosuj do swojego układu
TM1637Display tmDisplay(TM_CLK_PIN, TM_DIO_PIN);

// -------------------------
// Timer Blynk
// -------------------------
BlynkTimer timer;

// -------------------------
// Ustawienia czasów (w sekundach)
// STUDY = 10 minut, BREAK = 5 minut
// -------------------------
volatile unsigned long studyTimeSetting = 10 * 60; // 600 s
volatile unsigned long breakTimeSetting = 5 * 60;  // 300 s
volatile unsigned long currentTimer = studyTimeSetting;
volatile bool isStudying = true;
unsigned long lastSecondMillis = 0;

// -------------------------
// Statystyki
// -------------------------
unsigned long totalStudyTime = 0;
unsigned long totalBreakTime = 0;
unsigned long overallTime    = 0;
unsigned int studySessions   = 0;
unsigned int breakSessions   = 0;

// -------------------------
// Flaga pobrania statystyk
// -------------------------
bool statsFetched = false;

// -------------------------
// Przycisk BOOT
// -------------------------
#define BOOT_BUTTON_PIN 0   // Przycisk BOOT (podciągnięty)

// -------------------------
// Zmienne związane z WiFi
// -------------------------
bool wifiActive = false;      // ESP połączone z siecią
bool wifiConnecting = false;  // Próba połączenia
bool wifiEnabled = false;     // Użytkownik włączył WiFi (przytrzymanie przycisku)

// -------------------------
// Zmienne do wyświetlania komunikatów
// -------------------------
String confirmationMsg = "";
unsigned long confirmationMsgTimestamp = 0;
const unsigned long confirmationMsgDuration = 1100; // ms

// -------------------------
// Zmienne do aktualizacji OLED
// -------------------------
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 200; // co 200 ms

// -------------------------
// Obsługa przycisku – multi-click
// -------------------------
const unsigned long clickTimeout = 300;       // 300 ms
const unsigned long longPressThreshold = 5000;  // 5000 ms
unsigned long buttonPressStartTime = 0;
unsigned long lastButtonReleaseTime = 0;
int clickCount = 0;
bool buttonIsPressed = false;
bool longPressTriggered = false;
bool newMultiClickAllowed = true;
unsigned long lastMultiClickProcessTime = 0;
const unsigned long multiClickProcessCooldown = 500; // ms
Bounce debouncedButton = Bounce();

// -------------------------
// Konfiguracja Firebase
// -------------------------
// Używamy danych zdefiniowanych w config.h
const char* firebaseURL = FIREBASE_URL;

// ---------------------------------------------------------------------
// Funkcja wysyłająca statystyki do Firebase
// ---------------------------------------------------------------------
void sendStatsToFirebase() {
  #if defined(ESP8266) || defined(ESP32)
    WiFiClientSecure client;
    client.setInsecure();  // Wyłącza weryfikację certyfikatu – używaj ostrożnie!
  #else
    WiFiClient client;
  #endif
  
  HTTPClient http;
  http.begin(client, firebaseURL);  // Używamy nowego API z klientem
  http.addHeader("Content-Type", "application/json");
  
  String jsonData = "{";
  jsonData += "\"totalStudyTime\":" + String(totalStudyTime) + ",";
  jsonData += "\"totalBreakTime\":" + String(totalBreakTime) + ",";
  jsonData += "\"overallTime\":" + String(overallTime) + ",";
  jsonData += "\"studySessions\":" + String(studySessions) + ",";
  jsonData += "\"breakSessions\":" + String(breakSessions);
  jsonData += "}";
  
  int httpResponseCode = http.PUT(jsonData);
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Firebase Response: ");
    Serial.println(response);
  } else {
    Serial.print("Błąd wysyłania do Firebase: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

// ---------------------------------------------------------------------
// Funkcja wysyłająca statystyki do Blynk i Firebase
// ---------------------------------------------------------------------
void sendAllStats() {
  Blynk.virtualWrite(V3, totalStudyTime / 60);
  Blynk.virtualWrite(V4, totalBreakTime / 60);
  Blynk.virtualWrite(V5, overallTime / 60);
  Blynk.virtualWrite(V6, studySessions);
  Blynk.virtualWrite(V7, breakSessions);
  Serial.println("Statystyki wysłane do Blynk.");
  sendStatsToFirebase();
}

// ---------------------------------------------------------------------
// Funkcja pobierająca dane ze Firebase
// ---------------------------------------------------------------------
void fetchStatsFromFirebase() {
  #if defined(ESP8266) || defined(ESP32)
    WiFiClientSecure client;
    client.setInsecure();
  #else
    WiFiClient client;
  #endif
  
  HTTPClient http;
  http.begin(client, firebaseURL);
  
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.print("Odebrane dane z Firebase: ");
    Serial.println(payload);
    
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, payload.c_str());
    if (!error) {
      totalStudyTime = doc["totalStudyTime"] | 0;
      totalBreakTime = doc["totalBreakTime"] | 0;
      overallTime    = doc["overallTime"] | 0;
      studySessions  = doc["studySessions"] | 0;
      breakSessions  = doc["breakSessions"] | 0;
      
      Serial.println("Pobrano statystyki z Firebase:");
      Serial.print("totalStudyTime: "); Serial.println(totalStudyTime);
      Serial.print("totalBreakTime: "); Serial.println(totalBreakTime);
      Serial.print("overallTime: "); Serial.println(overallTime);
      Serial.print("studySessions: "); Serial.println(studySessions);
      Serial.print("breakSessions: "); Serial.println(breakSessions);
      
      // Aktualizacja statystyk w Blynk:
      sendAllStats();
    } else {
      Serial.print("Błąd parsowania JSON: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("Błąd pobierania danych z Firebase, kod: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}


// ---------------------------------------------------------------------
// Funkcja przełączająca tryb (Study <-> Break)
// ---------------------------------------------------------------------
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
  sendAllStats();
}

// ---------------------------------------------------------------------
// Funkcja obsługująca sekwencję kliknięć przycisku
// ---------------------------------------------------------------------
void processClicks(int count) {
  Serial.print("Wykryto ");
  Serial.print(count);
  Serial.println(" kliknięć");
  
  if (count == 1) {
    if (isStudying) {
      currentTimer = studyTimeSetting;
      confirmationMsg = "Reset\nSTUDY";
      Serial.println("Reset licznika STUDY");
    }
    else {
      currentTimer = breakTimeSetting;
      confirmationMsg = "Reset\nBREAK";
      Serial.println("Reset licznika BREAK");
    }
  }
  else if (count == 2) {
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
  }
  else if (count == 3) {
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
  }
  else if (count == 4) {
    confirmationMsg = "Switch\nto";
    Serial.println("Wymuszone przełączenie trybu");
    switchMode();
  }
  else {
    Serial.println("Nieobsługiwana liczba kliknięć");
  }
  confirmationMsgTimestamp = millis();
  sendAllStats();
}

// ---------------------------------------------------------------------
// Funkcja do przełączania WiFi
// ---------------------------------------------------------------------
void toggleWiFi() {
  if (!wifiEnabled) {
    Serial.println("Włączanie WiFi...");
    wifiEnabled = true;
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiConnecting = true;
  } else {
    Serial.println("Wyłączanie WiFi...");
    wifiEnabled = false;
    Blynk.disconnect();
    WiFi.disconnect();
    wifiActive = false;
    wifiConnecting = false;
  }
}

// ---------------------------------------------------------------------
// Funkcje Blynk do odbioru nowych ustawień
// ---------------------------------------------------------------------
BLYNK_WRITE(V1) {
  int newStudyTime = param.asInt();
  studyTimeSetting = newStudyTime;
  Serial.print("Otrzymano czas STUDY (sekundy): ");
  Serial.println(newStudyTime);
  if (isStudying) {
    currentTimer = studyTimeSetting;
  }
  sendAllStats();
}

BLYNK_WRITE(V2) {
  int newBreakTime = param.asInt();
  breakTimeSetting = newBreakTime;
  Serial.print("Otrzymano czas BREAK (sekundy): ");
  Serial.println(newBreakTime);
  if (!isStudying) {
    currentTimer = breakTimeSetting;
  }
  sendAllStats();
}

BLYNK_WRITE(V0) {
  int value = param.asInt();
  if (value == 1) {
    switchMode();
  }
}

// ---------------------------------------------------------------------
// Funkcja pomocnicza do rysowania komunikatu na OLED
// ---------------------------------------------------------------------
void displayConfirmationMsg(const String &msg) {
  const int maxLines = 10;
  String lines[maxLines];
  int numLines = 0;
  
  size_t startIndex = 0;
  for (size_t i = 0; i < msg.length(); i++) {
    if (msg.charAt(i) == '\n') {
      lines[numLines++] = msg.substring(startIndex, i);
      startIndex = i + 1;
    }
  }
  if (startIndex < msg.length() && numLines < maxLines) {
    lines[numLines++] = msg.substring(startIndex);
  }
  
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  int lineHeight = 26;
  int totalHeight = numLines * lineHeight;
  int startY = (SCREEN_HEIGHT - totalHeight) / 2;
  
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
// Funkcja rysująca zawartość wyświetlacza OLED
// ---------------------------------------------------------------------
void updateDisplay(unsigned long seconds, const char* label) {
  display.clearDisplay();
  
  if (confirmationMsg != "" && (millis() - confirmationMsgTimestamp < confirmationMsgDuration)) {
    displayConfirmationMsg(confirmationMsg);
    display.display();
    return;
  } else {
    confirmationMsg = "";
  }
  
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
  int labelX = (SCREEN_WIDTH - w) / 2;
  int labelY = 0;
  display.setCursor(labelX, labelY);
  display.print(label);
  
  int minutes = seconds / 60;
  int sec = seconds % 60;
  char timeStr[16];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", minutes, sec);
  
  display.setTextSize(3);
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  int timeX = (SCREEN_WIDTH - w) / 2;
  int timeY = 28;
  display.setCursor(timeX, timeY);
  display.print(timeStr);
  
  drawWiFiIcon(display, wifiActive, wifiConnecting);
  display.display();
}

// ---------------------------------------------------------------------
// Funkcja wysyłająca dane do wykresu (History Chart) na V8
// ---------------------------------------------------------------------
void sendGraphStats() {
  Blynk.virtualWrite(V8, totalStudyTime / 60);
  Serial.println("Dane do wykresu wysłane do Blynk.");
}

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  
  debouncedButton.attach(BOOT_BUTTON_PIN);
  debouncedButton.interval(5);
  
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
  
  tmDisplay.setBrightness(0x0f);
  
  isStudying = true;
  currentTimer = studyTimeSetting;
  studySessions++;  // Pierwsza sesja STUDY
  lastSecondMillis = millis();
  
  // Wysyłanie danych do wykresu co minutę
  timer.setInterval(60000L, sendGraphStats);
  
  // Po starcie wysyłamy statystyki do Blynk oraz Firebase
  sendAllStats();
}

void loop() {
  unsigned long currentMillis = millis();

  debouncedButton.update();
  bool confirmationActive = (confirmationMsg != "" && (currentMillis - confirmationMsgTimestamp < confirmationMsgDuration));
  
  if (!confirmationActive) {
    if (!newMultiClickAllowed) {
      if (debouncedButton.read() == HIGH &&
          (currentMillis - lastMultiClickProcessTime >= multiClickProcessCooldown)) {
        newMultiClickAllowed = true;
      }
    }
    
    if (newMultiClickAllowed) {
      if (debouncedButton.fell()) {
        buttonIsPressed = true;
        buttonPressStartTime = currentMillis;
        longPressTriggered = false;
      }
      
      if (buttonIsPressed && debouncedButton.read() == LOW && !longPressTriggered) {
        if (currentMillis - buttonPressStartTime >= longPressThreshold) {
          longPressTriggered = true;
          toggleWiFi();
          clickCount = 0;
          newMultiClickAllowed = false;
          lastMultiClickProcessTime = currentMillis;
        }
      }
      
      if (debouncedButton.rose()) {
        if (!longPressTriggered) {
          clickCount++;
          lastButtonReleaseTime = currentMillis;
        }
        buttonIsPressed = false;
      }
      
      if (!buttonIsPressed && clickCount > 0 &&
          (currentMillis - lastButtonReleaseTime >= clickTimeout)) {
        processClicks(clickCount);
        clickCount = 0;
        newMultiClickAllowed = false;
        lastMultiClickProcessTime = currentMillis;
      }
    }
  }
  
  bool ledShouldBeOn = (wifiEnabled && !wifiActive);
  updateLED(ledShouldBeOn);
  
  if (wifiEnabled) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!wifiActive) {
        Serial.println("WiFi połączone.");
        wifiActive = true;
        wifiConnecting = false;
        Blynk.config(BLYNK_AUTH_TOKEN);
        Blynk.connect();
        // Przy pierwszym połączeniu pobieramy statystyki z Firebase (wykonujemy dwa pobrania)
        if (!statsFetched) {
          fetchStatsFromFirebase();
          delay(500);  // krótka przerwa
          fetchStatsFromFirebase();
          statsFetched = true;
        }
      }
    } else {
      if (wifiActive) {
        Serial.println("Utracono połączenie, ponawiam próbę...");
        wifiActive = false;
        wifiConnecting = true;
        statsFetched = false; // resetujemy flagę przy utracie połączenia
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
    }
  }
  
  if (wifiActive) {
    Blynk.run();
    timer.run();
  }
  
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
    int minutes = currentTimer / 60;
    int seconds = currentTimer % 60;
    int displayValue = minutes * 100 + seconds;
    tmDisplay.showNumberDecEx(displayValue, 0b01000000, true);
  }
  
  if (currentMillis - lastDisplayUpdate >= displayUpdateInterval) {
    lastDisplayUpdate = currentMillis;
    if (isStudying) {
      updateDisplay(currentTimer, "STUDY");
    } else {
      updateDisplay(currentTimer, "BREAK");
    }
  }
}
