#ifndef LORA_REPEATHER_OLED_H
#define LORA_REPEATHER_OLED_H

#include <Arduino.h>

bool beginOLED();
void renderOLEDDisplay(const String &statusText, const String &detailText,
                       bool showingReceivedData, const String &rxSender,
                       const String &rxType, const String &rxLine,
                       bool temperatureAvailable, float temperatureC,
                       bool gpsAvailable, bool gpsFix,
                       bool hasLastGPSNode, const String &lastGPSNodeID,
                       const String &lastGPSNodePosition,
                       unsigned long receivedPacketCount,
                       unsigned long repeatedPacketCount);

#endif // LORA_REPEATHER_OLED_H
