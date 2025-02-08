## ESP32/8266 timer additionally controlled by Blynk app

### Changing functions using the BOOT button on the development board:

-   1 x press - reset timer
-   2 x press - change STUDY countdown between 10 and 20 minutes
-   3 x press - change BREAK countdown between 5 and 10 minutes
-   4 x press - change between BREAK and STUDY time
-   Hold the button for 5 seconds - turning ON/OFF WiFi depending on whether it is ON or OFF and if WiFi is on, connecting to the network for which the credentials were provided in the code (your hotspot s)
-   The reset button reboots the device

---

### Blynk and WIFI credentials

Edit the data in the file config.h.example and rename it to config.h

---

### TODO Control via Blynk App (Virtual Pins)

Timers changes
Reports
Statistics