#ifndef LORA_REPEATHER_OLED_H
#define LORA_REPEATHER_OLED_H

#include <Arduino.h>

bool beginOLED();
void renderOLEDDisplay(const String &statusText, const String &detailText,
                       bool showingReceivedData, const String &rxSender,
                       const String &rxType, const String &rxLine,
                       int rssi, int snr, bool temperatureAvailable,
                       float temperatureC, bool gpsAvailable, bool gpsFix);

#endif // LORA_REPEATHER_OLED_H
