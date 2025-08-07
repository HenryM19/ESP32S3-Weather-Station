#ifndef _CONFIG_H
#define _CONFIG_H
#include <RadioLib.h>

// Intervalo para enviar uplink: 50 segundos
//const uint32_t uplinkIntervalSeconds = 50UL;

// CREDENCIALES OTAA - LORAWAN 1.1.0 
#define RADIOLIB_LORAWAN_JOIN_EUI  0x5EACBA9A4013C325
#define RADIOLIB_LORAWAN_DEV_EUI   0xBAE2671B63D4A3BC
#define RADIOLIB_LORAWAN_APP_KEY   0xCD, 0x4A, 0x4B, 0x88, 0x1A, 0xC5, 0x21, 0x94, 0x36, 0x96, 0x1D, 0x09, 0x26, 0xD5, 0x67, 0xF4
#define RADIOLIB_LORAWAN_NWK_KEY   0x18, 0xE5, 0x80, 0x5D, 0xDE, 0xF5, 0x47, 0x76, 0x67, 0xA6, 0xB4, 0x0B, 0xA2, 0x57, 0x2A, 0xBB


/*
#define RADIOLIB_LORAWAN_JOIN_EUI  0x92275F559B31F2BB
#define RADIOLIB_LORAWAN_DEV_EUI   0xEED70955BDBA55B8
#define RADIOLIB_LORAWAN_APP_KEY   0x2E, 0x2D, 0x1C, 0x58, 0xF7, 0x99, 0x7A, 0x1D, 0xDA, 0xB4, 0x49, 0x01, 0x38, 0x70, 0x32, 0x24
#define RADIOLIB_LORAWAN_NWK_KEY   0x3B, 0x48, 0x7B, 0x7D, 0xEA, 0xB9, 0xD8, 0x7A, 0xCD, 0xA2, 0xD5, 0x11, 0xD0, 0xC1, 0x8E, 0xD7
*/

// Regional choices: EU868, US915, AU915, AS923, IN865, KR920, CN780, CN500
const LoRaWANBand_t Region = US915;
const uint8_t subBand = 2;  // For US915, change this to 2, otherwise leave on 0

// SX1262  pin order: Module(NSS/CS, DIO1, RESET, BUSY);
SX1262 radio = new Module(10, 7, 14, 15);

// Copy over the EUI's & keys in to the something that will not compile if incorrectly formatted
uint64_t joinEUI =   RADIOLIB_LORAWAN_JOIN_EUI;
uint64_t devEUI  =   RADIOLIB_LORAWAN_DEV_EUI;
uint8_t appKey[] = { RADIOLIB_LORAWAN_APP_KEY };
uint8_t nwkKey[] = { RADIOLIB_LORAWAN_NWK_KEY };

// Create the LoRaWAN node
LoRaWANNode node(&radio, &Region, subBand);

// Helper function to display any issues
void debug(bool isFail, const __FlashStringHelper* message, int state, bool Freeze) {
  if (isFail) {
    Serial.print(message);
    Serial.print("(");
    Serial.print(state);
    Serial.println(")");
    while (Freeze);
  }
}

// Helper function to display a byte array
void arrayDump(uint8_t *buffer, uint16_t len) {
  for (uint16_t c = 0; c < len; c++) {
    Serial.printf("%02X", buffer[c]);
  }
  Serial.println();
}


#endif