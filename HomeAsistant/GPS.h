#ifndef LORA_REPEATHER_GPS_H
#define LORA_REPEATHER_GPS_H

#include <Arduino.h>

// Start GPS detection/configuration, and call updateGPS() regularly afterward.
bool beginGPS();
void updateGPS();
bool isGPSAvailable();
bool hasGPSFix();
unsigned long gpsLocationAgeMinutes();
bool getGPSCoordinates(double &latitude, double &longitude);
String makeGPSLocationPayload();

#endif // LORA_REPEATHER_GPS_H
