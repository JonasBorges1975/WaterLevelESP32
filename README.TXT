============================================================
ESP32 Water Level Logger - Ultrasonic + OLED + Google Sheet
============================================================

This project measures the water level in a cistern using an ultrasonic sensor,
displays the reading on an OLED screen when a button is pressed, and logs the
average of 10 valid readings to a Google Sheet. It works both online and offline
and uses deep sleep to conserve power.

------------------------
FEATURES OF THE SYSTEM:
------------------------

1. Uses an ultrasonic sensor (JSN-SR04M) to measure distance to water surface.
2. Displays real-time readings on a 128x32 OLED screen while button is held.
3. Registers an average of up to 10 valid readings when the button is released.
4. Performs automatic measurements every 1 hour (also averaged from 10 readings).
5. Sends readings to a Google Sheet using Wi-Fi and Apps Script.
6. If Wi-Fi is unavailable, readings are saved locally in SPIFFS as fallback.
7. On the next successful connection, offline data is automatically synced.
8. Watchdog timer restarts the system in case of software crash.
9. Deep sleep mode saves battery by sleeping after each reading.

------------------------
HARDWARE CONNECTIONS:
------------------------

| Component       | ESP32 Pin |
|----------------|------------|
| Trigger (SR04M) | GPIO 5     |
| Echo (SR04M)    | GPIO 18    |
| OLED SDA        | GPIO 21    |
| OLED SCL        | GPIO 22    |
| Button          | GPIO 4     |
| VCC             | 5V / VIN   |
| GND             | GND        |

------------------------
GOOGLE SHEET SETUP:
------------------------

1. Go to https://sheets.google.com and create a new spreadsheet.
2. Rename the active sheet/tab to: Dados
3. In Row 1, enter the following headers:
   A1: Data e Hora
   B1: Distância (cm)
   C1: Tipo

------------------------
GOOGLE SCRIPT SETUP:
------------------------

1. Open the spreadsheet and click:
   Extensions > Apps Script

2. Delete any default code and paste this:

---------------------------------------------------------
function doGet(e) {
  var sheet = SpreadsheetApp.openById("YOUR_SPREADSHEET_ID").getSheetByName("Dados");
  var data = new Date();
  sheet.appendRow([
    Utilities.formatDate(data, "GMT-3", "dd/MM/yyyy HH:mm:ss"),
    e.parameter.distancia,
    e.parameter.tipo
  ]);
  return ContentService.createTextOutput("OK");
}
---------------------------------------------------------

3. Replace YOUR_SPREADSHEET_ID with your actual Sheet ID.
   (You can find it in the Sheet URL between /d/ and /edit)

4. Click the "Deploy" button > "Manage deployments" > "New Deployment"
   - Select "Web app"
   - Execute as: Me
   - Who has access: Anyone

5. Click "Deploy", authorize the script, and copy the generated web app URL.
   It should look like:
   https://script.google.com/macros/s/AKf.../exec

6. Paste this URL in your Arduino sketch in the line:
   const char* googleScriptURL = "PASTE_URL_HERE";

------------------------
SPIFFS FILE USAGE:
------------------------

- Offline readings are stored in:
  /pendentes.txt

- Format of each line:
  distance_in_cm;type;timestamp

- On reconnection, the ESP32 reads this file and attempts to sync all entries.

------------------------
COMPATIBILITY:
------------------------

- Works on ESP32 DevKit V1
- Tested with Arduino IDE 2.3.4
- Requires the following libraries:
   - Adafruit SSD1306
   - Adafruit GFX
   - WiFi
   - HTTPClient
   - SPIFFS (built-in)

------------------------
NOTES:
------------------------

- Keep the button connected to GPIO 4 and GND.
- A pull-up resistor is optional since INPUT_PULLUP is used.
- Maximum measurable range depends on your SR04M sensor model.
- Deep sleep wake-up occurs on timer (1 hour) or button press.

------------------------
MAINTAINER:
------------------------

Developed by: [JONAS PINHEIRO BORGES FILHO]
Contact: [jonasborges@gmail.com]
Date: [04-JUN-2025]
