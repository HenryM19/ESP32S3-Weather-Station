#ifndef LORAWAN_MANAGER_H
#define LORAWAN_MANAGER_H

#include <Arduino.h>
#include <RadioLib.h>
#include <Preferences.h>
#include "DEBUG.h"

// Credenciales OTAA - LoRaWAN 1.1.0 
#define RADIOLIB_LORAWAN_JOIN_EUI  0x5EACBA9A4013C325
#define RADIOLIB_LORAWAN_DEV_EUI   0xBAE2671B63D4A3BC
#define RADIOLIB_LORAWAN_APP_KEY   0xCD, 0x4A, 0x4B, 0x88, 0x1A, 0xC5, 0x21, 0x94, 0x36, 0x96, 0x1D, 0x09, 0x26, 0xD5, 0x67, 0xF4
#define RADIOLIB_LORAWAN_NWK_KEY   0x18, 0xE5, 0x80, 0x5D, 0xDE, 0xF5, 0x47, 0x76, 0x67, 0xA6, 0xB4, 0x0B, 0xA2, 0x57, 0x2A, 0xBB

#define SESSION_KEY "session"
#define NONCES_KEY  "nonces"
#define SESSION_SIZE 64
#define NONCES_SIZE 16

// Regiones LoRaWAN
extern const LoRaWANBand_t Region;
extern const uint8_t subBand;

// Objetos RadioLib para LoRaWAN
extern Module loraModule;
extern SX1262 radio;
extern LoRaWANNode node;
extern Preferences preferences;

// Claves y EUI
extern uint64_t joinEUI;
extern uint64_t devEUI;
extern uint8_t appKey[16];
extern uint8_t nwkKey[16];

// Funciones principales y auxiliares
bool initLoRaWAN();
void debugIssue(bool isFail, const __FlashStringHelper* message, int state, bool Freeze);
void dumpArray(uint8_t *buffer, uint16_t len);

#endif
