// LoRa Mesh Node: Transceiver met Autodetect voor GPS, DS18B20 én OLED
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> // Maakt gebruik van de Wemos Mini OLED variant
#include <Wire.h>               // Nodig voor I2C scanner
#include <OneWire.h>            
#include <DallasTemperature.h>  
// SoftwareSerial is volledig verwijderd om de Hardware Serial (TX/RX) te gebruiken
#include <TinyGPS++.h>          

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET     -1 
#define SCREEN_ADDRESS 0x3C

// Voeg deze variabelen toe aan het begin van je code (boven de setup) 
// om de berekende waarden te onthouden voor het scherm:
double afstandTotAnder = 0.0;
String richtingNaarAnder = "---";

// Constructor van de Wemos Mini OLED bibliotheek
Adafruit_SSD1306 display(OLED_RESET);

// =========================================================================
// --- CONFIGURATIE VAN DEZE NODE (Pas dit aan per ESP8266) ---------------
// =========================================================================
const String MY_NODE_ID   = "0001"; // De unieke ID van deze node (altijd 4 tekens)
const String DEST_NODE_ID = "9999"; // Doelstation ("9999" voor broadcast naar iedereen)
const int NETWERK_HOPS    = 3;      // Maximaal aantal stappen
// =========================================================================

// DS18B20 Pin configuratie
#define ONE_WIRE_BUS 4 // Pin D2 (GPIO 4) op de ESP8266
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// GPS configuratie via de Hardware Serial poort (Fysieke TX/RX pinnen)
TinyGPSPlus gps;

// Hardware beschikbaarheidsvlaggen (DYNAMISCH)
bool ds18b20Beschikbaar = false;
bool gpsBeschikbaar     = false;
bool displayBeschikbaar = false; 

// LoRa SX1278 pins
#define SS 15
#define RST -1 
#define DIO0 16

// LoRa settings
#define codingrate 5        // FIX: Aangepast van 5/8 naar 5 om te voorkomen dat de waarde 0 wordt!
#define transmitpower 2 
long frequency = 433E6; 
int bandwidth = 125E3;
int spreadingFactor = 11;   

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
int repeathwaitbase = 15000; 
int RepeathDelay = 0; 
unsigned long repeatTimestamp = 0;
bool pendingRepeat = false;
String messageToRepeat = "";

// Geschiedenis om duplicaten te voorkomen
String MessIDs[] = {"00000000", "00000000", "00000000", "00000000", "00000000"};
int messIdIndex = 0;

// Automatische zend-timer
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 30000; 
int eigenBerichtTeller = 1000; 
float huidigeTemperatuur = 0.0;

// Variabelen voor het opslaan en tonen van de laatst ontvangen data
String rxDisplayType = "GEEN"; 
String rxAfzender   = "";
String rxLine1      = "";
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
void configureerGPS();

void setup() {
  // Gestart op 9600 baud om direct synchroon te lopen met de GPS hardware data stream
  Serial.begin(9600);
  delay(5000);

  // 1. ONTDEK OLED SCHERM (I2C Scanner check)
  Wire.begin(); 
  Wire.beginTransmission(SCREEN_ADDRESS);
  byte error = Wire.endTransmission();
  
  if (error == 0) {
    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    displayBeschikbaar = true;
  }

  // 2. Ontdek DS18B20
  sensors.begin();
  if (sensors.getDeviceCount() > 0) {
    ds18b20Beschikbaar = true;
  }

  // 3. Ontdek en configureer GPS module via Hardware Serial poort activiteit
  unsigned long startCheck = millis();
  while (millis() - startCheck < 2000) {
    if (Serial.available() > 0) {
      gpsBeschikbaar = true;
      configureerGPS(); 
      break;
    }
    delay(10);
    yield();
  }

  // 4. Initialiseer LoRa
  SPI.begin();
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(frequency)) {
    while (1) { delay(10); yield(); }
  }
  LoRa.setSignalBandwidth(bandwidth);
  LoRa.setSpreadingFactor(spreadingFactor);
  LoRa.setCodingRate4(codingrate);
  LoRa.setTxPower(transmitpower);

  if (displayBeschikbaar) {
    updateOLEDDisplay("STANDBY", "Systeem actief");
  }
}

void loop() {
  // GPS data stream continu inlezen via Hardware Serial
  if (gpsBeschikbaar) {
    while (Serial.available() > 0) {
      gps.encode(Serial.read());
    }
  }

  // 1. Luister naar binnenkomende LoRa pakketten
  if (LoRaReceive()) {
    SplitString();
    
    if (HeaderString.length() >= 14 && !IsDuplicateMessage()) {
      String senderID = HeaderString.substring(4, 8);
      String targetID = HeaderString.substring(8, 12);
      HopCheck();

      if (targetID == MY_NODE_ID || targetID == "9999") {
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
        RepeathDelay = random(500, 2000); 
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
    if (millis() - displayReceivedTimeout > 8000) { 
      displayReceivedTimeout = 0;
      tonenOntvangenData = false;
      if (displayBeschikbaar) updateOLEDDisplay("STANDBY", "Systeem stand-by");
    }
  }

  // 3. Automatisch periodiek eigen data verzenden naar het Mesh-netwerk
  if (millis() - lastSendTime > sendInterval) {
    lastSendTime = millis();
    
    String payload = "";
    String typeFlag = "1"; 

    if (ds18b20Beschikbaar) {
      sensors.requestTemperatures();
      huidigeTemperatuur = sensors.getTempCByIndex(0);
      payload = String(huidigeTemperatuur, 1);
      typeFlag = "2"; 
    } else if (gpsBeschikbaar && gps.location.isValid()) {
      char latBuf[16];
      char lngBuf[16];
      
      dtostrf(gps.location.lat(), 2, 6, latBuf);
      dtostrf(gps.location.lng(), 2, 6, lngBuf);
      
      payload = String(latBuf) + "," + String(lngBuf);
      payload.trim(); 
      
      typeFlag = "4"; 

    } else {
      payload = "Node " + MY_NODE_ID + " OK";
      typeFlag = "1";
    }

    SendOwnMessage(DEST_NODE_ID, NETWERK_HOPS, typeFlag, payload);
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
    
    int kommaIndex = payload.indexOf(',');
    if (kommaIndex > 0) {
      double andereLat = payload.substring(0, kommaIndex).toDouble();
      double andereLng = payload.substring(kommaIndex + 1).toDouble();
      
      if (gps.location.isValid()) {
        afstandTotAnder = gps.distanceBetween(gps.location.lat(), gps.location.lng(), andereLat, andereLng);
        double graden = gps.courseTo(gps.location.lat(), gps.location.lng(), andereLat, andereLng);
        richtingNaarAnder = gps.cardinal(graden);
        
        if (afstandTotAnder < 1000) {
          rxLine1 = String(afstandTotAnder, 0) + "m " + richtingNaarAnder;
        } else {
          rxLine1 = String((afstandTotAnder / 1000.0), 1) + "km " + richtingNaarAnder;
        }
      } else {
        rxLine1 = "Wacht op eigen GPS...";
      }
    }
  } else {
    rxDisplayType = "DATA";
    rxLine1 = payload;
  }
}

void updateOLEDDisplay(String statusText, String detailText) {
  display.clearDisplay();
  display.setTextColor(WHITE); 
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("NODE:" + MY_NODE_ID);
  display.setCursor(64, 0);
  display.print("[" + statusText + "]");
  display.drawFastHLine(0, 10, 128, WHITE); 
  
  if (tonenOntvangenData) {
    display.setCursor(0, 16);
    display.print("RX Van: " + rxAfzender + " (" + rxDisplayType + ")");
    display.setCursor(0, 32);
    display.setTextSize(1);
    display.print(rxLine1);
    
    display.setTextSize(1);
    display.setCursor(0, 52);
    display.print("RSSI: " + String(RSSI) + " SNR: " + String(SNR));
  } else {
    display.setCursor(0, 16);
    display.print(detailText);
    
    display.setCursor(0, 32);
    if (ds18b20Beschikbaar) {
      display.print("Lokaal Temp: " + String(huidigeTemperatuur, 1) + "C");
    } else {
      display.print("Geen temp sensor");
    }
    
    display.setCursor(0, 48);
    if (gpsBeschikbaar && gps.location.isValid()) {
      display.print("GPS: FIX OK");
    } else if (gpsBeschikbaar) {
      display.print("GPS: Zoeken naar sat.");
    } else {
      display.print("GPS: Niet verbonden");
    }
  }
  
  display.display();
} 

void configureerGPS() {
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
