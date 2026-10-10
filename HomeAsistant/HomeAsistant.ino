// LoRa Mesh Node: Transceiver met Autodetect voor GPS, DS18B20 én OLED
#include <SPI.h>
#include <LoRa.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "GPS.h"
#include "Navigation.h"
#include "OLED.h"
#include "TemperatureSensor.h"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
unsigned long lastMqttReconnectAttempt = 0;
wl_status_t lastWifiStatus = WL_IDLE_STATUS;
unsigned long lastSerialDiagnostic = 0;
unsigned long lastLoopTrace = 0;

const uint8_t MQTT_GPS_QUEUE_SIZE = 8;
struct QueuedGpsMessage {
  String senderID;
  String coordinates;
  String messageID;
  int rssi;
  int hops;
};
QueuedGpsMessage mqttGpsQueue[MQTT_GPS_QUEUE_SIZE];
uint8_t mqttGpsQueueRead = 0;
uint8_t mqttGpsQueueWrite = 0;
uint8_t mqttGpsQueueCount = 0;

// Hardware beschikbaarheidsvlaggen (DYNAMISCH)
bool ds18b20Beschikbaar = false;
bool displayBeschikbaar = false; 

// Variabelen voor berichtverwerking
String Received = "";  
String HeaderString = ""; 
String MessageString = ""; 
String hop = ""; 
String MessID = "";  
int Hop = 0; 
int ReceivedSize = 0; 
int ReceivedRSSI = 0;
unsigned long directReceivedPacketCount = 0;
unsigned long repeatedPacketCount = 0;

// Repeater-timer variabelen (Non-blocking)
int RepeathDelay = 0; 
unsigned long repeatTimestamp = 0;
bool pendingRepeat = false;
String messageToRepeat = "";

// Geschiedenis om duplicaten te voorkomen
String MessIDs[] = {"00000000", "00000000", "00000000", "00000000", "00000000"};
int messIdIndex = 0;

// Automatische zend-timer
unsigned long lastSendTime = 0;
int eigenBerichtTeller = 1000; 
float huidigeTemperatuur = 0.0;

// Variabelen voor het opslaan en tonen van de laatst ontvangen data
String rxDisplayType = "GEEN"; 
String rxAfzender   = "";
String rxLine1      = "";
String lastGPSNodeID = "";
String lastGPSNodeCoordinates = "";
bool hasLastGPSNode = false;
unsigned long displayReceivedTimeout = 0;
bool tonenOntvangenData = false;

// Functie-declaraties (Prototypes)
void updateOLEDDisplay(String statusText, String detailText);
void SendOwnMessage(String destID, int initialHops, String msgType, String message);
bool LoRaReceive();
void SplitString();
void HopCheck();
bool IsDuplicateMessage();
void parsePayloadData(String senderID, String type, String payload);
void maintainWifiAndMqtt();
void queueGpsForMqtt(String senderID, String coordinates, String messageID,
                     int rssi, int hops);
bool publishQueuedGpsMessage(const QueuedGpsMessage &message);
void publishHomeAssistantDiscovery(const String &senderID, const String &attributesTopic);
void serialDebug(const char *message);

void setup() {
  // Hardware Serial is shared with the GPS module when GPS is enabled.
  if (ENABLE_GPS) {
    Serial.begin(GPS_BAUD_RATE);
  } else {
    Serial.begin(115200);
  }
  serialDebug("[BOOT] Serial initialized; setup entered");
  serialDebug("[SETUP] Startup delay begins");
  delay(STARTUP_DELAY_MS);
  serialDebug("[SETUP] Startup delay complete");

  // 1. Detect and initialize the OLED.
  serialDebug("[SETUP] OLED initialization begins");
  displayBeschikbaar = ENABLE_OLED && beginOLED();
  serialDebug(displayBeschikbaar ? "[SETUP] OLED initialized" : "[SETUP] OLED skipped or unavailable");

  // 2. Detect the DS18B20 temperature sensor.
  serialDebug("[SETUP] Temperature sensor detection begins");
  ds18b20Beschikbaar = ENABLE_TEMPERATURE_SENSOR && beginTemperatureSensor();
  serialDebug(ds18b20Beschikbaar ? "[SETUP] Temperature sensor detected" : "[SETUP] Temperature sensor skipped or unavailable");

  // 3. Detect and configure the GPS module on Hardware Serial.
  if (ENABLE_GPS) {
    beginGPS();
  } else {
    serialDebug("[SETUP] GPS initialization skipped (ENABLE_GPS=false)");
  }

  // 4. Initialiseer LoRa
  serialDebug("[SETUP] SPI initialization begins");
  SPI.begin();
  serialDebug("[SETUP] SPI initialized; configuring LoRa pins");
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  serialDebug("[SETUP] LoRa.begin begins");
  if (!LoRa.begin(LORA_FREQUENCY)) {
    serialDebug("[SETUP] LoRa.begin failed; setup will stop here");
    while (1) { delay(10); yield(); }
  }
  serialDebug("[SETUP] LoRa initialized; applying radio settings");
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setTxPower(LORA_TRANSMIT_POWER);
  serialDebug("[SETUP] LoRa radio settings applied");

  // Start Wi-Fi asynchronously so the repeater can begin receiving immediately.
  serialDebug("[SETUP] WiFi.mode begins");
  WiFi.mode(WIFI_STA);
  serialDebug("[SETUP] WiFi.mode complete; enabling auto-reconnect");
  WiFi.setAutoReconnect(true);
  serialDebug("[SETUP] WiFi.begin begins");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  serialDebug("[SETUP] WiFi.begin returned; connecting asynchronously");

  serialDebug("[SETUP] Configuring MQTT client");
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(512);
  mqttClient.setSocketTimeout(2);
  mqttClient.setKeepAlive(15);
  serialDebug("[SETUP] MQTT client configured");

  if (displayBeschikbaar) {
    serialDebug("[SETUP] Updating OLED standby screen");
    updateOLEDDisplay("STANDBY", "Systeem actief");
  }
  serialDebug("[SETUP] Complete; entering loop");
}

void loop() {
  bool traceLoop = !ENABLE_GPS && millis() - lastLoopTrace >= 10000;
  if (traceLoop) {
    lastLoopTrace = millis();
    serialDebug("[LOOP] updateGPS begins");
  }
  updateGPS();
  if (traceLoop) serialDebug("[LOOP] updateGPS returned; maintainWifiAndMqtt begins");
  maintainWifiAndMqtt();
  if (traceLoop) serialDebug("[LOOP] maintainWifiAndMqtt returned; LoRaReceive begins");

  // 1. Luister naar binnenkomende LoRa pakketten
  bool packetReceived = LoRaReceive();
  if (traceLoop) serialDebug(packetReceived ? "[LOOP] LoRaReceive returned with packet" : "[LOOP] LoRaReceive returned; no packet");
  if (packetReceived) {
    SplitString();
    if (!ENABLE_GPS) {
      Serial.print("[LoRa] Packet received, bytes=");
      Serial.print(ReceivedSize);
      Serial.print(" RSSI=");
      Serial.println(ReceivedRSSI);
    }
    
    if (HeaderString.length() >= 14) {
      directReceivedPacketCount++;
      if (displayBeschikbaar) {
        updateOLEDDisplay("RX PACKET", "Receive count updated");
      }

      if (!IsDuplicateMessage()) {
        String senderID = HeaderString.substring(4, 8);
        String targetID = HeaderString.substring(8, 12);
        HopCheck();

        if (targetID == MY_NODE_ID || targetID == BROADCAST_NODE_ID) {
          String msgType = MessageString.substring(0, 1);
          String payload = MessageString.substring(1);

          if (!ENABLE_GPS) {
            Serial.print("[LoRa] Accepted message from ");
            Serial.print(senderID);
            Serial.print(" type=");
            Serial.println(msgType);
          }

          parsePayloadData(senderID, msgType, payload);
          if (msgType == "4") {
            queueGpsForMqtt(senderID, payload, MessID, ReceivedRSSI, Hop);
            if (!ENABLE_GPS) {
              Serial.print("[MQTT] Queued GPS update from ");
              Serial.println(senderID);
            }
          }
          tonenOntvangenData = true;
          displayReceivedTimeout = millis();

          if (displayBeschikbaar) {
            updateOLEDDisplay("RECEIVER", "Bericht verwerkt");
          }
        }

        // FIX: Alleen herhalen als we niet zelf de afzender zijn (voorkomt mesh-loops)
        if (Hop > 1 && senderID != MY_NODE_ID) {
          Hop--;
          String newHopStr = String(Hop);
          String retransmitHeader = HeaderString.substring(0, 12) + newHopStr + HeaderString.substring(13);

          messageToRepeat = retransmitHeader + MessageString;
          pendingRepeat = true;
          int boundedRSSI = constrain(ReceivedRSSI,
                                      REPEAT_RSSI_WEAK_DBM,
                                      REPEAT_RSSI_STRONG_DBM);
          RepeathDelay = map(boundedRSSI,
                             REPEAT_RSSI_WEAK_DBM,
                             REPEAT_RSSI_STRONG_DBM,
                             REPEAT_DELAY_MIN_MS,
                             REPEAT_DELAY_MAX_MS);
          repeatTimestamp = millis();

          // FIX: Voeg het ID NU al toe aan de geschiedenis om te voorkomen dat
          // je dadelijk je eigen herhaalde bericht weer als 'nieuw' ontvangt.
          MessIDs[messIdIndex] = MessID;
          messIdIndex = (messIdIndex + 1) % 5;

          if (displayBeschikbaar) {
            updateOLEDDisplay("REPEATER",
                              "Repeat delay: " + String(RepeathDelay) + " ms");
          }
        }
      }
    }
  }

  // 2. Afhandeling van herhalingen (Non-blocking)
  if (pendingRepeat && (millis() - repeatTimestamp >= (unsigned long)RepeathDelay)) {
    LoRa.beginPacket();
    LoRa.print(messageToRepeat);
    if (LoRa.endPacket()) {
      repeatedPacketCount++;
    }
    pendingRepeat = false;
    if (displayBeschikbaar) {
      updateOLEDDisplay("REPEATER",
                        "Repeated: " + String(RepeathDelay) + " ms");
    }
  }

  // Display time-out check naar standby status
  if (tonenOntvangenData && displayReceivedTimeout != 0) {
    if (millis() - displayReceivedTimeout > RECEIVE_DISPLAY_TIMEOUT_MS) {
      displayReceivedTimeout = 0;
      tonenOntvangenData = false;
      if (displayBeschikbaar) updateOLEDDisplay("STANDBY", "Systeem stand-by");
    }
  }

  // 3. Automatisch periodiek eigen data verzenden naar het Mesh-netwerk
  unsigned long sendInterval = (ds18b20Beschikbaar || isGPSAvailable())
      ? SEND_INTERVAL_MS
      : SENSORLESS_SEND_INTERVAL_MS;
  if (millis() - lastSendTime > sendInterval) {
    lastSendTime = millis();
    
    String payload = "";
    String typeFlag = "1"; 

    if (ds18b20Beschikbaar) {
      huidigeTemperatuur = readTemperatureC();
      payload = String(huidigeTemperatuur, 1);
      typeFlag = "2"; 
    } else if (hasGPSFix()) {
      payload = makeGPSLocationPayload();
      typeFlag = "4"; 

    } else {
      payload = String("Node ") + MY_NODE_ID + " OK";
      typeFlag = "1";
    }

    SendOwnMessage(String(DEST_NODE_ID), NETWORK_HOPS, typeFlag, payload);
  }

  yield(); 
}

void maintainWifiAndMqtt() {
  wl_status_t wifiStatus = WiFi.status();
  if (!ENABLE_GPS && millis() - lastSerialDiagnostic >= 10000) {
    lastSerialDiagnostic = millis();
    Serial.print("[STATUS] WiFi=");
    Serial.print(wifiStatus == WL_CONNECTED ? "connected" : "disconnected");
    if (wifiStatus == WL_CONNECTED) {
      Serial.print(" IP=");
      Serial.print(WiFi.localIP());
    }
    Serial.print(" MQTT=");
    Serial.print(mqttClient.connected() ? "connected" : "disconnected");
    if (!mqttClient.connected()) {
      Serial.print(" state=");
      Serial.print(mqttClient.state());
    }
    Serial.print(" queued_gps=");
    Serial.println(mqttGpsQueueCount);
  }

  if (wifiStatus != lastWifiStatus) {
    lastWifiStatus = wifiStatus;
    if (!ENABLE_GPS) {
      if (wifiStatus == WL_CONNECTED) {
        Serial.print("[WiFi] Connected, IP: ");
        Serial.println(WiFi.localIP());
      } else {
        Serial.print("[WiFi] Disconnected, status: ");
        Serial.println((int)wifiStatus);
      }
    }
  }
  if (wifiStatus != WL_CONNECTED) return;

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt < MQTT_RECONNECT_INTERVAL_MS) return;
    lastMqttReconnectAttempt = now;

    bool connected;
    if (MQTT_USERNAME[0] == '\0') {
      connected = mqttClient.connect(MQTT_CLIENT_ID);
    } else {
      connected = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
    }
    if (!ENABLE_GPS) {
      if (connected) {
        Serial.print("[MQTT] Connected to ");
        Serial.print(MQTT_HOST);
        Serial.print(':');
        Serial.println(MQTT_PORT);
      } else {
        Serial.print("[MQTT] Connection failed, state: ");
        Serial.println(mqttClient.state());
      }
    }
    if (!connected) return;
  }

  mqttClient.loop();
  if (mqttGpsQueueCount == 0) return;

  if (publishQueuedGpsMessage(mqttGpsQueue[mqttGpsQueueRead])) {
    mqttGpsQueueRead = (mqttGpsQueueRead + 1) % MQTT_GPS_QUEUE_SIZE;
    mqttGpsQueueCount--;
  }
}

void serialDebug(const char *message) {
  if (!ENABLE_GPS) Serial.println(message);
}

void queueGpsForMqtt(String senderID, String coordinates, String messageID,
                     int rssi, int hops) {
  if (mqttGpsQueueCount == MQTT_GPS_QUEUE_SIZE) {
    // Keep the newest locations if an extended network outage fills the queue.
    mqttGpsQueueRead = (mqttGpsQueueRead + 1) % MQTT_GPS_QUEUE_SIZE;
    mqttGpsQueueCount--;
  }

  mqttGpsQueue[mqttGpsQueueWrite].senderID = senderID;
  mqttGpsQueue[mqttGpsQueueWrite].coordinates = coordinates;
  mqttGpsQueue[mqttGpsQueueWrite].messageID = messageID;
  mqttGpsQueue[mqttGpsQueueWrite].rssi = rssi;
  mqttGpsQueue[mqttGpsQueueWrite].hops = hops;
  mqttGpsQueueWrite = (mqttGpsQueueWrite + 1) % MQTT_GPS_QUEUE_SIZE;
  mqttGpsQueueCount++;
}

bool publishQueuedGpsMessage(const QueuedGpsMessage &message) {
  int commaIndex = message.coordinates.indexOf(',');
  if (commaIndex <= 0) {
    if (!ENABLE_GPS) {
      Serial.print("[MQTT] Dropping malformed GPS payload from ");
      Serial.println(message.senderID);
    }
    return true; // Discard malformed GPS payloads.
  }

  String latitude = message.coordinates.substring(0, commaIndex);
  String longitude = message.coordinates.substring(commaIndex + 1);
  String nodeTopic = String(MQTT_TOPIC_PREFIX) + "/" + message.senderID + "/gps";
  String attributesTopic = nodeTopic + "/attributes";

  publishHomeAssistantDiscovery(message.senderID, attributesTopic);

  String attributes = String("{\"latitude\":") + latitude +
      ",\"longitude\":" + longitude +
      ",\"gps_accuracy\":0,\"message_id\":\"" + message.messageID +
      "\",\"rssi\":" + String(message.rssi) +
      ",\"hops\":" + String(message.hops) + "}";
  bool published = mqttClient.publish(attributesTopic.c_str(), attributes.c_str(), true);
  if (!ENABLE_GPS) {
    Serial.print("[MQTT] GPS publish ");
    Serial.print(published ? "succeeded: " : "FAILED: ");
    Serial.println(attributesTopic);
  }
  return published;
}

void publishHomeAssistantDiscovery(const String &senderID,
                                   const String &attributesTopic) {
  String objectID = String("lorarepeater_") + senderID;
  String discoveryTopic = String(HA_MQTT_DISCOVERY_PREFIX) +
      "/device_tracker/" + objectID + "/config";
  String discoveryPayload = String("{\"name\":\"LoRa GPS ") + senderID +
      "\",\"unique_id\":\"" + objectID +
      "\",\"source_type\":\"gps\",\"json_attributes_topic\":\"" +
      attributesTopic + "\",\"device\":{\"identifiers\":[\"" + objectID +
      "\"],\"name\":\"LoRa GPS " + senderID +
      "\",\"manufacturer\":\"LoRaRepeatherAI\",\"model\":\"LoRa GPS node\"}}";
  mqttClient.publish(discoveryTopic.c_str(), discoveryPayload.c_str(), true);
}

bool LoRaReceive(){
  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) return false; 
  ReceivedSize = packetSize;
  Received = "";
  while (LoRa.available()) {
    Received += (char)LoRa.read();
  }
  ReceivedRSSI = LoRa.packetRssi();
  return true; 
}

void SplitString(){
  if(Received.length() >= 14) {
    HeaderString = Received.substring(0, 14);
    MessageString = Received.substring(14); 
    MessID = HeaderString.substring(0, 4);
    hop = HeaderString.substring(12, 13);
  } else {
    HeaderString = "";
    MessageString = "";
    MessID = "";
    hop = "";
  }
}

void HopCheck(){
  Hop = hop.toInt();
}

bool IsDuplicateMessage() {
  for (int i = 0; i < 5; i++) {
    if (MessIDs[i] == MessID) {
      return true; // Al eerder gezien, negeer dit pakket
    }
  }
  // Sla het nieuwe unieke ID op in de cirkelvormige buffer
  MessIDs[messIdIndex] = MessID;
  messIdIndex = (messIdIndex + 1) % 5;
  return false;
}

void SendOwnMessage(String destID, int initialHops, String msgType, String message) {
  eigenBerichtTeller++;
  if(eigenBerichtTeller > 9999) eigenBerichtTeller = 1000;
  
  String generatedID = String(eigenBerichtTeller);
  String hopStr = String(initialHops);
  
  // Opbouw header: ID(4) + SENDER(4) + DEST(4) + HOP(1) + SPARE(1) = 14 tekens
  String packetHeader = generatedID + MY_NODE_ID + destID + hopStr + "0";
  String fullPacket = packetHeader + msgType + message;
  
  LoRa.beginPacket();
  LoRa.print(fullPacket);
  LoRa.endPacket();
  
  // Voeg eigen bericht ook toe aan de ID-geschiedenis
  MessIDs[messIdIndex] = generatedID;
  messIdIndex = (messIdIndex + 1) % 5;
  
  if (displayBeschikbaar) {
    updateOLEDDisplay("SENDING", "Type: " + msgType);
  }
}

void parsePayloadData(String senderID, String type, String payload) {
  rxAfzender = senderID;
  if (type == "1") {
    rxDisplayType = "TXT";
    rxLine1 = payload;
  } else if (type == "2") {
    rxDisplayType = "TEMP";
    rxLine1 = "Temp: " + payload + " C";
  } else if (type == "4") {
    rxDisplayType = "GPS";
    lastGPSNodeID = senderID;
    lastGPSNodeCoordinates = payload;
    hasLastGPSNode = true;
    double localLatitude = 0.0;
    double localLongitude = 0.0;
    bool localCoordinatesValid = getGPSCoordinates(localLatitude, localLongitude);
    rxLine1 = formatRelativePosition(payload, localCoordinatesValid,
                                     localLatitude, localLongitude);
  } else {
    rxDisplayType = "DATA";
    rxLine1 = payload;
  }
}

void updateOLEDDisplay(String statusText, String detailText) {
  double localLatitude = 0.0;
  double localLongitude = 0.0;
  bool localCoordinatesValid = getGPSCoordinates(localLatitude, localLongitude);
  String lastGPSNodePosition = hasLastGPSNode
      ? formatRelativePosition(lastGPSNodeCoordinates, localCoordinatesValid,
                               localLatitude, localLongitude)
      : "";
  String displayRxLine = (rxDisplayType == "GPS" && hasLastGPSNode)
      ? lastGPSNodePosition
      : rxLine1;

  renderOLEDDisplay(statusText, detailText, tonenOntvangenData,
                    rxAfzender, rxDisplayType, displayRxLine,
                    ds18b20Beschikbaar, huidigeTemperatuur,
                    isGPSAvailable(), hasGPSFix(), gpsLocationAgeMinutes(),
                    hasLastGPSNode,
                    lastGPSNodeID, lastGPSNodePosition,
                    directReceivedPacketCount, repeatedPacketCount);
} 
