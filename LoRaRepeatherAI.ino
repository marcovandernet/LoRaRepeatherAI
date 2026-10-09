// LoRa Mesh Node: Transceiver met Autodetect voor GPS, DS18B20 én OLED
#include <SPI.h>
#include <LoRa.h>
#include "config.h"
#include "GPS.h"
#include "Navigation.h"
#include "OLED.h"
#include "TemperatureSensor.h"

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
int RSSI = 0;
int SNR = 0;
int ReceivedSize = 0; 

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

void setup() {
  // Hardware Serial is used by the GPS module.
  if (ENABLE_GPS) {
    Serial.begin(GPS_BAUD_RATE);
  }
  delay(STARTUP_DELAY_MS);

  // 1. Detect and initialize the OLED.
  displayBeschikbaar = ENABLE_OLED && beginOLED();

  // 2. Detect the DS18B20 temperature sensor.
  ds18b20Beschikbaar = ENABLE_TEMPERATURE_SENSOR && beginTemperatureSensor();

  // 3. Detect and configure the GPS module on Hardware Serial.
  if (ENABLE_GPS) {
    beginGPS();
  }

  // 4. Initialiseer LoRa
  SPI.begin();
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    while (1) { delay(10); yield(); }
  }
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setTxPower(LORA_TRANSMIT_POWER);

  if (displayBeschikbaar) {
    updateOLEDDisplay("STANDBY", "Systeem actief");
  }
}

void loop() {
  updateGPS();

  // 1. Luister naar binnenkomende LoRa pakketten
  if (LoRaReceive()) {
    SplitString();
    
    if (HeaderString.length() >= 14 && !IsDuplicateMessage()) {
      String senderID = HeaderString.substring(4, 8);
      String targetID = HeaderString.substring(8, 12);
      HopCheck();

      if (targetID == MY_NODE_ID || targetID == BROADCAST_NODE_ID) {
        String msgType = MessageString.substring(0, 1);
        String payload = MessageString.substring(1);
        
        parsePayloadData(senderID, msgType, payload);
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
        RepeathDelay = random(REPEAT_DELAY_MIN_MS, REPEAT_DELAY_MAX_MS);
        repeatTimestamp = millis();
        
        // FIX: Voeg het ID NU al toe aan de geschiedenis om te voorkomen dat 
        // je dadelijk je eigen herhaalde bericht weer als 'nieuw' ontvangt.
        MessIDs[messIdIndex] = MessID;
        messIdIndex = (messIdIndex + 1) % 5;
      }
    }
  }

  // 2. Afhandeling van herhalingen (Non-blocking)
  if (pendingRepeat && (millis() - repeatTimestamp >= (unsigned long)RepeathDelay)) {
    LoRa.beginPacket();
    LoRa.print(messageToRepeat);
    LoRa.endPacket();
    pendingRepeat = false;
    if (displayBeschikbaar) updateOLEDDisplay("STANDBY", "Mesh herhaald");
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
  if (millis() - lastSendTime > SEND_INTERVAL_MS) {
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

bool LoRaReceive(){
  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) return false; 
  ReceivedSize = packetSize;
  Received = "";
  while (LoRa.available()) {
    Received += (char)LoRa.read();
  }
  RSSI = LoRa.packetRssi();
  SNR = LoRa.packetSnr();
  return true; 
}

void SplitString(){
  if(Received.length() >= 14) {
    HeaderString = Received.substring(0, 14);
    MessageString = Received.substring(14); 
    MessID = HeaderString.substring(0, 4);
    hop = HeaderString.substring(12, 13);
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
                    rxAfzender, rxDisplayType, displayRxLine, RSSI, SNR,
                    ds18b20Beschikbaar, huidigeTemperatuur,
                    isGPSAvailable(), hasGPSFix(), hasLastGPSNode,
                    lastGPSNodeID, lastGPSNodePosition);
} 
