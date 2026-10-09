#ifndef LORA_REPEATHER_CONFIG_H
#define LORA_REPEATHER_CONFIG_H

// Enable or disable optional hardware modules for this build.
const bool ENABLE_GPS = false;
const bool ENABLE_OLED = true;
const bool ENABLE_TEMPERATURE_SENSOR = false;
const bool ENABLE_NAVIGATION = false; // Convert received coordinates to distance/direction

// Select exactly one OLED layout: leave its #define uncommented and comment out
// the other three. The current selection is repeater with navigation.
enum DisplayLayout {
  LAYOUT_REPEATER,
  LAYOUT_REPEATER_WITH_NAVIGATION,
  LAYOUT_REPEATER_WITH_TEMPERATURE
};

// #define DISPLAY_LAYOUT LAYOUT_REPEATER_WITH_NAVIGATION
#define DISPLAY_LAYOUT LAYOUT_REPEATER
// #define DISPLAY_LAYOUT LAYOUT_REPEATER_WITH_TEMPERATURE

// Node and mesh configuraton
const char MY_NODE_ID[] = "0001";   // Unique four-character ID for this node 0xxx repeater 1xxx gps 2xxx temperature 3xxx navigator
const char DEST_NODE_ID[] = "9999"; // "9999" broadcasts to all nodes
const char BROADCAST_NODE_ID[] = "9999";
const int NETWORK_HOPS = 3;         // Maximum number of mesh hops

// OLED configuration
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_YELLOW_HEIGHT = 16; // Two 8-pixel text rows on the dual-color panel
const int OLED_BLUE_CONTENT_TOP = 20;
const int OLED_RESET = -1;
const uint8_t SCREEN_ADDRESS = 0x3C;

// Sensor and LoRa pins (ESP8266 GPIO numbers)
const uint8_t ONE_WIRE_BUS = 4; // D2 / GPIO 4
const uint8_t LORA_SS = 15;     // D8 / GPIO 15
const int LORA_RST = -1;        // Reset pin is not connected
const uint8_t LORA_DIO0 = 16;   // D0 / GPIO 16

// LoRa radio configuration
const int LORA_CODING_RATE = 5; // 5 = 4/5 coding rate, 6 = 4/6, 7 = 4/7, 8 = 4/8 higer = more robust, lower = faster data rate
const int LORA_TRANSMIT_POWER = 16; // 2 = 2 dBm,  18 = 18 dBm (upper limmit of the sx1278 module)
const long LORA_FREQUENCY = 433E6; // 433 MHz
const long LORA_BANDWIDTH = 125E3; // 125 kHz bandwidth range of the sx1278 module is 7.8 kHz to 500 kHz higher = faster data rate, lower = more range
const int LORA_SPREADING_FACTOR = 11; // 7 to 12, higher = longer range, lower = faster data rate

// Timing configuration (milliseconds)
const unsigned long STARTUP_DELAY_MS = 5000;
const unsigned long GPS_BAUD_RATE = 9600;
const unsigned long GPS_DETECTION_TIMEOUT_MS = 2000; // Timeout for GPS detection
const unsigned long GPS_COORDINATE_MAX_AGE_MS = 300000; // Clear cached fix after five minutes
const unsigned long REPEAT_DELAY_MIN_MS = 500; //the shortest delay for weak signals
const unsigned long REPEAT_DELAY_MAX_MS = 2000; //the longest delay for strong signals
const int REPEAT_RSSI_WEAK_DBM = -120;   // Weak signals receive the shortest delay
const int REPEAT_RSSI_STRONG_DBM = -60;  // Strong signals receive the longest delay
const unsigned long RECEIVE_DISPLAY_TIMEOUT_MS = 300000; // Keep received data visible for five minutes
const unsigned long SEND_INTERVAL_MS = 60000; // Send interval in milliseconds

#endif // LORA_REPEATHER_CONFIG_H
