#ifndef LORA_REPEATHER_CONFIG_H
#define LORA_REPEATHER_CONFIG_H

// Enable or disable optional hardware modules for this build.
const bool ENABLE_GPS = true;
const bool ENABLE_OLED = true;
const bool ENABLE_TEMPERATURE_SENSOR = true;

// Node and mesh configuration
const char MY_NODE_ID[] = "0001";   // Unique four-character ID for this node
const char DEST_NODE_ID[] = "9999"; // "9999" broadcasts to all nodes
const char BROADCAST_NODE_ID[] = "9999";
const int NETWORK_HOPS = 3;         // Maximum number of mesh hops

// OLED configuration
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_RESET = -1;
const uint8_t SCREEN_ADDRESS = 0x3C;

// Sensor and LoRa pins (ESP8266 GPIO numbers)
const uint8_t ONE_WIRE_BUS = 4; // D2 / GPIO 4
const uint8_t LORA_SS = 15;     // D8 / GPIO 15
const int LORA_RST = -1;        // Reset pin is not connected
const uint8_t LORA_DIO0 = 16;   // D0 / GPIO 16

// LoRa radio configuration
const int LORA_CODING_RATE = 5;
const int LORA_TRANSMIT_POWER = 2;
const long LORA_FREQUENCY = 433E6;
const long LORA_BANDWIDTH = 125E3;
const int LORA_SPREADING_FACTOR = 11;

// Timing configuration (milliseconds)
const unsigned long STARTUP_DELAY_MS = 5000;
const unsigned long GPS_BAUD_RATE = 9600;
const unsigned long GPS_DETECTION_TIMEOUT_MS = 2000;
const unsigned long REPEAT_DELAY_MIN_MS = 500;
const unsigned long REPEAT_DELAY_MAX_MS = 2000;
const unsigned long RECEIVE_DISPLAY_TIMEOUT_MS = 8000;
const unsigned long SEND_INTERVAL_MS = 30000;

#endif // LORA_REPEATHER_CONFIG_H
