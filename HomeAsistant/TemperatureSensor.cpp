#include "TemperatureSensor.h"

#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

namespace {
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
bool sensorAvailable = false;
}

bool beginTemperatureSensor() {
  if (!ENABLE_TEMPERATURE_SENSOR) {
    sensorAvailable = false;
    return false;
  }

  sensors.begin();
  sensorAvailable = sensors.getDeviceCount() > 0;
  return sensorAvailable;
}

bool isTemperatureSensorAvailable() {
  return sensorAvailable;
}

float readTemperatureC() {
  if (!ENABLE_TEMPERATURE_SENSOR || !sensorAvailable) return DEVICE_DISCONNECTED_C;
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}
