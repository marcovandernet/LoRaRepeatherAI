#ifndef LORA_REPEATHER_NAVIGATION_H
#define LORA_REPEATHER_NAVIGATION_H

#include <Arduino.h>

// Convert a received "latitude,longitude" payload to distance and bearing text.
String formatRelativePosition(const String &coordinatePayload,
                              bool localCoordinatesValid,
                              double localLatitude, double localLongitude);

#endif // LORA_REPEATHER_NAVIGATION_H
