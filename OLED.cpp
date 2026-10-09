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

  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  displayAvailable = true;
  return displayAvailable;
}

void renderOLEDDisplay(const String &statusText, const String &detailText,
                       bool showingReceivedData, const String &rxSender,
                       const String &rxType, const String &rxLine,
                       bool temperatureAvailable, float temperatureC,
                       bool gpsAvailable, bool gpsFix,
                       bool hasLastGPSNode, const String &lastGPSNodeID,
                       const String &lastGPSNodePosition,
                       unsigned long receivedPacketCount,
                       unsigned long repeatedPacketCount) {
  if (!ENABLE_OLED || !displayAvailable) return;

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);

  // The display's physical color boundary is after the first 16 pixels.
  // Keep both yellow rows for node and communication status.
  display.setCursor(0, 0);
  display.print(String("NODE:") + MY_NODE_ID + " [" + statusText + "]");
  display.setCursor(0, 8);
  display.print(detailText);
  display.drawFastHLine(0, OLED_YELLOW_HEIGHT, SCREEN_WIDTH, WHITE);
  String packetCounts = "RX:" + String(receivedPacketCount) +
                        " REP:" + String(repeatedPacketCount);

  if (showingReceivedData) {
    display.setCursor(0, OLED_BLUE_CONTENT_TOP);
    display.print("RX Van: " + rxSender + " (" + rxType + ")");
    display.setCursor(0, 36);
    display.print(rxLine);
  } else {
    display.setCursor(0, OLED_BLUE_CONTENT_TOP);
    if (DISPLAY_LAYOUT == LAYOUT_REPEATER_WITH_NAVIGATION && hasLastGPSNode) {
      display.print("GPS " + lastGPSNodeID + ": " + lastGPSNodePosition);
    } else if (DISPLAY_LAYOUT == LAYOUT_REPEATER_WITH_TEMPERATURE && temperatureAvailable) {
      display.print("Lokaal Temp: " + String(temperatureC, 1) + "C");
    } else if (temperatureAvailable && DISPLAY_LAYOUT == LAYOUT_REPEATER) {
      display.print("Lokaal Temp: " + String(temperatureC, 1) + "C");
    } else {
      display.print(DISPLAY_LAYOUT == LAYOUT_REPEATER_WITH_NAVIGATION
                        ? "Nog geen GPS-node"
                        : "Repeater actief");
    }

    display.setCursor(0, 36);
    if (DISPLAY_LAYOUT == LAYOUT_REPEATER_WITH_NAVIGATION) {
      display.print(gpsFix ? "Eigen GPS: FIX OK" : "Eigen GPS: zoeken");
    } else if (DISPLAY_LAYOUT == LAYOUT_REPEATER_WITH_TEMPERATURE) {
      display.print(temperatureAvailable ? "Temperatuursensor OK" : "Geen temp sensor");
    } else if (DISPLAY_LAYOUT == LAYOUT_REPEATER && gpsAvailable) {
      display.print(gpsFix ? "GPS: FIX OK" : "GPS: Zoeken naar sat.");
    }
  }

  display.setCursor(0, 52);
  display.print(packetCounts);

  display.display();
}
