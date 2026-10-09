#include "Navigation.h"

#include <TinyGPS++.h>
#include "config.h"

namespace {
TinyGPSPlus navigationCalculator;
}

String formatRelativePosition(const String &coordinatePayload,
                              bool localCoordinatesValid,
                              double localLatitude, double localLongitude) {
  if (!ENABLE_NAVIGATION) return coordinatePayload;

  int commaIndex = coordinatePayload.indexOf(',');
  if (commaIndex <= 0) return coordinatePayload;
  if (!localCoordinatesValid) return "Wacht op eigen GPS...";

  double remoteLatitude = coordinatePayload.substring(0, commaIndex).toDouble();
  double remoteLongitude = coordinatePayload.substring(commaIndex + 1).toDouble();
  double distance = navigationCalculator.distanceBetween(
      localLatitude, localLongitude, remoteLatitude, remoteLongitude);
  double bearing = navigationCalculator.courseTo(
      localLatitude, localLongitude, remoteLatitude, remoteLongitude);
  String direction = navigationCalculator.cardinal(bearing);

  if (distance < 1000) {
    return String(distance, 0) + "m " + direction;
  }
  return String(distance / 1000.0, 1) + "km " + direction;
}
