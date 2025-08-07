#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <AHTxx.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include "DEBUG.h"

// Variables globales
extern volatile bool pulso;
extern RTC_DS3231 rtc;
extern Adafruit_ADS1115 ads;

extern float TEMP, HUM, VEL_VIENTO;
extern int DIR_VIENTO, RAD_SOL, RAD_UV, PLUV;
extern uint8_t bandera_nulos;
extern uint8_t bandera_ceros;
extern RTC_DATA_ATTR unsigned int previous_count;
extern RTC_DATA_ATTR time_t UNIX_TIME;

extern const char* ssid;
extern const char* ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;
extern int base_tiempo;

// Pines
#define input_vel_viento 17
#define INTEGRADO_ADDR 0x32
#define RESET_PIN 18
#define MAX_COUNT 16777215

// Funciones
uint32_t init_RTC();
float medirVelocidadViento(unsigned int T_muestreo);
void ISR_VEL_VIENTO();
uint8_t calcular_ceros(int viento, int lluvia, int solar, int uv);
uint8_t* cargar_datos();
void mostrarBinario(uint8_t byte);
void gotoSleep(uint32_t seconds);
uint32_t calcularSleepSegundosAntesDelMinuto();

float* leerValoresADC();
unsigned int leerIncrementoPluviometro();
void init_AHT(float &temperature, float &humidity);


#endif // SENSORS_H
