#include "OLED.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "config.h"

namespace {
Adafruit_SSD1306 display(OLED_RESET);
bool displayAvailable = false;
}

bool beginOLED() {
  if (!ENABLE_OLED) {
    displayAvailable = false;
    return false;
  }

  Wire.begin();
  Wire.beginTransmission(SCREEN_ADDRESS);
  if (Wire.endTransmission() != 0) return false;

  displayAvailable = display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  return displayAvailable;
}

void renderOLEDDisplay(const String &statusText, const String &detailText,
                       bool showingReceivedData, const String &rxSender,
                       const String &rxType, const String &rxLine,
                       int rssi, int snr, bool temperatureAvailable,
                       float temperatureC, bool gpsAvailable, bool gpsFix) {
  if (!ENABLE_OLED || !displayAvailable) return;

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(String("NODE:") + MY_NODE_ID);
  display.setCursor(64, 0);
  display.print("[" + statusText + "]");
  display.drawFastHLine(0, 10, SCREEN_WIDTH, WHITE);

  if (showingReceivedData) {
    display.setCursor(0, 16);
    display.print("RX Van: " + rxSender + " (" + rxType + ")");
    display.setCursor(0, 32);
    display.print(rxLine);
    display.setCursor(0, 52);
    display.print("RSSI: " + String(rssi) + " SNR: " + String(snr));
  } else {
    display.setCursor(0, 16);
    display.print(detailText);
    display.setCursor(0, 32);
    if (temperatureAvailable) {
      display.print("Lokaal Temp: " + String(temperatureC, 1) + "C");
    } else {
      display.print("Geen temp sensor");
    }

    display.setCursor(0, 48);
    if (gpsFix) {
      display.print("GPS: FIX OK");
    } else if (gpsAvailable) {
      display.print("GPS: Zoeken naar sat.");
    } else {
      display.print("GPS: Niet verbonden");
    }
  }

  display.display();
}
