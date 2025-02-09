## ESP32/8266 timer additionally controlled by Blynk app

### Changing functions using the BOOT button on the development board:

-   1 x press - reset timer
-   2 x press - change STUDY countdown between 10 and 20 minutes
-   3 x press - change BREAK countdown between 5 and 10 minutes
-   4 x press - change between BREAK and STUDY time
-   Hold the button for 5 seconds - turning WiFi on/off (the device will connect to the network saved in the config.h file)
-   The reset button reboots the device

---

### Physical connections to the SSD1306 I2C OLED display for ESP32 and ESP8266

| **Display Pin** | **ESP32** | **ESP8266** |
|-----------------|-----------|-------------|
| VCC             | 3.3V      | 3.3V        |
| GND             | GND       | GND         |
| **SDA**         | GPIO 21   | D2          |
| **SCL**         | GPIO 22   | D1          |

### Physical connections to the TM1637 display for ESP32 and ESP8266

| **Display Pin** | **ESP32**  | **ESP8266** |
|-----------------|------------|-------------|
| VCC             | 3.3V or 5V | 3.3V or 5V  |
| GND             | GND        | GND         |
| **CLK**         | GPIO 4     | D2          |
| **DIO**         | GPIO 5     | D1          |

### Blynk and WIFI credentials

Edit the data in the file config.h.example and rename it to config.h

---

### Compiled releases

They don't really matter, because the code compiles the contents of the config.h.example file, and it doesn't contain your WiFi or Blynk credentials

### Online Firmware Compilation

For those who want to change their WiFi and Blynk credentials here (COM port drivers may be required to upload firmware to ESP)
<br>it is possible to launch a limited, free Codespace in the future, to edit config.h.example, compile and upload firmware via web

---

### TODO Control via Blynk App (Virtual Pins)

<br>Blynk Reports
<br>Blynk Statistics
<br>Blynk setup manual