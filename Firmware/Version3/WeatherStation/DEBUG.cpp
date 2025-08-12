#include "DEBUG.h"
#include <Arduino.h>

bool DEBUG_ENABLE = true;

template <typename T>
void debug(T mensaje, bool nuevaLinea, int estado) {
  if (!DEBUG_ENABLE) return;
  if (estado == true) Serial.print("[ ✅ ] ");
  else if (estado == false) Serial.print("[ ❌ ] ");

  if (nuevaLinea) Serial.println(mensaje);
  else Serial.print(mensaje);
}

