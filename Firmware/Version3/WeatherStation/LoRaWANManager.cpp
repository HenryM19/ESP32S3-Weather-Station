#include "LoRaWANManager.h"

// Configuración región y subbanda
const LoRaWANBand_t Region = US915;
const uint8_t subBand      = 2;  // Para US915

// Pines y módulos LoRa
Module loraModule(10, 7, 14, 15);  // NSS, DIO1, RESET, BUSY
SX1262 radio(&loraModule);

LoRaWANNode node(&radio, &Region, subBand);
Preferences preferences;

// Claves y EUIs
uint64_t joinEUI = RADIOLIB_LORAWAN_JOIN_EUI;
uint64_t devEUI  = RADIOLIB_LORAWAN_DEV_EUI;
uint8_t appKey[16] = { RADIOLIB_LORAWAN_APP_KEY };
uint8_t nwkKey[16] = { RADIOLIB_LORAWAN_NWK_KEY };


bool initLoRaWAN() {
  bool exito = true;
  String mensaje = "Init LoRaWAN";

  int16_t radioStatus = radio.begin(903.0, 125.0, 8, 5, 0x34, 15, 8);
  if (radioStatus != RADIOLIB_ERR_NONE) {
    mensaje = "Init LoRaWAN: Error al inicializar radio (code " + String(radioStatus) + ")";
    exito = false;
  } else {
    radio.setSpreadingFactor(7);

    preferences.begin("lorawan", false);
    size_t session_len = preferences.getBytesLength(SESSION_KEY);
    size_t nonces_len = preferences.getBytesLength(NONCES_KEY);
    bool sesionRestaurada = false;

    if (session_len == SESSION_SIZE && nonces_len == NONCES_SIZE) {
      uint8_t sessionBuffer[SESSION_SIZE];
      uint8_t noncesBuffer[NONCES_SIZE];
      preferences.getBytes(SESSION_KEY, sessionBuffer, SESSION_SIZE);
      preferences.getBytes(NONCES_KEY, noncesBuffer, NONCES_SIZE);

      if (node.setBufferSession(sessionBuffer) == 0 &&
          node.setBufferNonces(noncesBuffer) == 0) {
        int16_t estadoSesion = node.activateOTAA();
        if (estadoSesion == RADIOLIB_ERR_NONE || estadoSesion == RADIOLIB_LORAWAN_NEW_SESSION) {
          sesionRestaurada = true;
        }
      }
    }

    if (!sesionRestaurada) {
      node.setADR(false);
      node.setDatarate(4); // DR4 = SF7 US915
      int16_t joinStatus = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
      if (joinStatus != RADIOLIB_ERR_NONE) {
        mensaje = "Init LoRaWAN: Falló OTAA join (code " + String(joinStatus) + ")";
        exito = false;
      } else {
        int16_t estadoSesion = node.activateOTAA();
        if (estadoSesion == RADIOLIB_LORAWAN_NEW_SESSION) {
          uint8_t* session = node.getBufferSession();
          uint8_t* nonces = node.getBufferNonces();
          preferences.putBytes(SESSION_KEY, session, SESSION_SIZE);
          preferences.putBytes(NONCES_KEY, nonces, NONCES_SIZE);
        } else {
          mensaje = "Init LoRaWAN: Error al activar nueva sesión";
          exito = false;
        }
      }
    }

    preferences.end();
  }

  debug(mensaje, true, exito);
  return exito;
}

void debugIssue(bool isFail, const __FlashStringHelper* message, int state, bool Freeze) {
  if (isFail) {
    Serial.print(message);
    Serial.print("(");
    Serial.print(state);
    Serial.println(")");
    while (Freeze);
  }
}

void dumpArray(uint8_t *buffer, uint16_t len) {
  for (uint16_t c = 0; c < len; c++) {
    Serial.printf("%02X", buffer[c]);
  }
  Serial.println();
}
