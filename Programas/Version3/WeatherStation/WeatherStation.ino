#include "DEBUG.h"           // Libreria propia solo para tener la función de debug
#include <LoRaWANManager.h>  // Funciones de LoRaWAN
//#include <Sensors.h>         // Funciones de lectura de los sensores



bool DEBUG_ENABLE = true;  // O false para desactivar debug

void setup() {
  Serial.begin(115200); while(!Serial); delay(1000); // Inicia comunicacion serial

  debug("Init Station", true, true);

  initLoRaWAN();

}

void loop() {
  // put your main code here, to run repeatedly:

}
