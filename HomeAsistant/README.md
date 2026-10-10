# Home Assistant LoRa repeater copy

This is a separate Arduino sketch based on the main repeater. It keeps LoRa
reception and retransmission, and publishes received GPS message type `4` to
MQTT. Each sender ID becomes a Home Assistant MQTT device tracker through MQTT
discovery. Forwarded packets retain their original sender ID and coordinates,
so they are published the same way as directly received packets.

## Setup

1. Open `HomeAsistant.ino` from this folder as an ESP8266 Arduino sketch.
2. Install the same Arduino libraries used by the main sketch, plus
   `PubSubClient` (Nick O'Leary).
3. Set `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_HOST`, and any broker credentials in
   this folder's `config.h`. The MQTT broker must be reachable from the ESP8266.
4. Flash the ESP8266 and make sure MQTT discovery is enabled in Home Assistant.

The gateway buffers up to eight GPS updates while MQTT is disconnected. If
that queue fills, it drops the oldest update and keeps newer locations. MQTT
uses the configured plain TCP port, normally `1883`; this sketch does not add
TLS.
