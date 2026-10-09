#include "GPS.h"

#include <TinyGPS++.h>
#include "config.h"

namespace {
TinyGPSPlus gps;
bool gpsAvailable = false;

void configureGPS() {
  delay(500);
  Serial.println(F("$PUBX,40,GLL,0,0,0,0,0,0*5C"));
  delay(50);
  Serial.println(F("$PUBX,40,GSA,0,0,0,0,0,0*4E"));
  delay(50);
  Serial.println(F("$PUBX,40,GSV,0,0,0,0,0,0*59"));
  delay(50);
  Serial.println(F("$PUBX,40,VTG,0,0,0,0,0,0*5E"));
  delay(50);
  Serial.println(F("$PUBX,40,GGA,1,1,1,1,1,1*5A"));
  delay(50);
  Serial.println(F("$PUBX,40,RMC,1,1,1,1,1,1*47"));
  delay(50);

  uint8_t setNav[] = {
    0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x03, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00,
    0x64, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xDC
  };
  Serial.write(setNav, sizeof(setNav));
  delay(100);
  Serial.flush();
}
} // namespace

bool beginGPS() {
  if (!ENABLE_GPS) {
    gpsAvailable = false;
    return false;
  }

  unsigned long startCheck = millis();
  while (millis() - startCheck < GPS_DETECTION_TIMEOUT_MS) {
    if (Serial.available() > 0) {
      gpsAvailable = true;
      configureGPS();
      break;
    }
    delay(10);
    yield();
  }
  return gpsAvailable;
}

void updateGPS() {
  if (!gpsAvailable) return;
  while (Serial.available() > 0) {
    gps.encode(Serial.read());
  }
}

bool isGPSAvailable() {
  return gpsAvailable;
}

bool hasGPSFix() {
  return gpsAvailable && gps.location.isValid();
}

bool getGPSCoordinates(double &latitude, double &longitude) {
  if (!hasGPSFix()) return false;
  latitude = gps.location.lat();
  longitude = gps.location.lng();
  return true;
}

String makeGPSLocationPayload() {
  double latitude;
  double longitude;
  if (!getGPSCoordinates(latitude, longitude)) return "";

  char latBuf[16];
  char lngBuf[16];
  dtostrf(latitude, 2, 6, latBuf);
  dtostrf(longitude, 2, 6, lngBuf);

  String payload = String(latBuf) + "," + String(lngBuf);
  payload.trim();
  return payload;
}
