#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
//#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 display(OLED_RESET);

// LoRa SX1278 pins
#define SS 15
#define RST -1 // Optional, connect to GPIO if used
#define DIO0 16
#define codingrate 7 //5 to 8
#define transmitpower 2 //2 to 17 db

// LoRa settings
long frequency = 433E6; // Match the sender frequency
int bandwidths[] = {7.8E3, 10.4e3, 62.5E3, 125E3, 250E3, 500E3};
int spreadingFactors[] = {12, 11, 10, 9, 8, 7};
int bandwidthCount = sizeof(bandwidths) / sizeof(bandwidths[0]);
int spreadingFactorCount = sizeof(spreadingFactors) / sizeof(spreadingFactors[0]);
int currentBandwidthIndex = 0;
int currentSpreadingFactorIndex = 0;

int wait = 1500; //wait time in ms for lora signal
int waitcount = 0 ;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("LoRa Receiver - Testing Bandwidth and Spreading Factor");
  delay(2000);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
  // Setup LoRa module
  SPI.begin();
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(frequency)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  Serial.println("LoRa initialized successfully!");
  LoRa.setSignalBandwidth(bandwidths[currentBandwidthIndex]);
  LoRa.setSpreadingFactor(spreadingFactors[currentSpreadingFactorIndex]);
  LoRa.setCodingRate4(codingrate);
  LoRa.setTxPower(transmitpower);

  Serial.print("Initial Bandwidth: ");
  Serial.print(bandwidths[currentBandwidthIndex] / 1000.0);
  Serial.print(" kHz, Spreading Factor: ");
  Serial.print(spreadingFactors[currentSpreadingFactorIndex]);
  Serial.println(" --- ");
}

int x=44;
int y=1;


void LoRaReceive(){
  while (waitcount < wait){
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      Serial.print("Received packet '");
      String receivedString = "";
      while (LoRa.available()) {
        receivedString += (char)LoRa.read();
      }
      Serial.print(receivedString);
      Serial.print("' with RSSI ");
      Serial.print(LoRa.packetRssi());
      Serial.print(" ");
      //bwsf();
      screen();
    }
    delay(1);
    waitcount=waitcount+1;
    //Serial.print(waitcount);
  }
}

void bwsf(){
  waitcount=0;
//Increment indexes to cycle through bandwidths and spreading factors.
    currentSpreadingFactorIndex++;
    if(currentSpreadingFactorIndex >= spreadingFactorCount){
      currentSpreadingFactorIndex = 0;
      currentBandwidthIndex++;
      y=y+8;
      if(currentBandwidthIndex >= bandwidthCount){
        currentBandwidthIndex = 0;
      }
    }
    x=x+14;
    
    
    LoRa.setSignalBandwidth(bandwidths[currentBandwidthIndex]);
    LoRa.setSpreadingFactor(spreadingFactors[currentSpreadingFactorIndex]);
  
    Serial.print("Bandwidth: ");
    Serial.print(bandwidths[currentBandwidthIndex] / 1000.0);
    Serial.print(" kHz, Spreading Factor: ");
    Serial.print(spreadingFactors[currentSpreadingFactorIndex]);
   
    Serial.print(" x,y:");
    Serial.print(x);
    Serial.print(",");
    Serial.println(y);
    if (y>47){
      y=1;
      //delay(500);
      //display.clearDisplay();
      //display.display();
    }
    if (x>121){
      x=44;
      
    }
    //LoRaReceive();
    //screen();
}

void screen(){
    display.setTextColor(WHITE);
    display.setCursor(1,y);
    display.print("B");
    display.print(bandwidths[currentBandwidthIndex] / 1000.0);
    display.setCursor(x,y);
    display.print(spreadingFactors[currentSpreadingFactorIndex]);
    display.display();  
}


void loop() {
  LoRaReceive();
  bwsf();
  //screen();
  
}
