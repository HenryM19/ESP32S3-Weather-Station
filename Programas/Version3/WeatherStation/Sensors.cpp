#include "Sensors.h"
#include <algorithm>
#include <WiFi.h>
#include "time.h"

// Variables globales definidas aquí
volatile bool pulso = false;
RTC_DS3231 rtc;
Adafruit_ADS1115 ads;

float TEMP = 0, HUM = 0, VEL_VIENTO = 0;
int DIR_VIENTO = 0, RAD_SOL = 0, RAD_UV = 0, PLUV = 0;
uint8_t bandera_nulos = 0b00000000;
uint8_t bandera_ceros = 0b00000000;

RTC_DATA_ATTR unsigned int previous_count = 0xFFFFFFFF;
RTC_DATA_ATTR time_t UNIX_TIME = 0;

const char* ssid = "SMT";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 0;
int base_tiempo = 1735689600 + (5 * 3600);

// === Funciones ===

uint32_t init_RTC() {
  bool exito = true;
  String mensajeFinal;
  DateTime now;

  if (!rtc.begin()) {
    exito = false;
    mensajeFinal = "[RTC] Error: No se pudo comunicar con el RTC.";
    now = DateTime(1735689600);  // Fecha base UTC−5
  } else {
    if (rtc.lostPower()) {
      mensajeFinal = "[RTC] Se detectó pérdida de energía. Intentando sincronizar...";
      WiFi.begin(ssid);
      uint32_t t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        delay(500);
        Serial.print(".");
      }
      if (WiFi.status() == WL_CONNECTED) {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          time_t unix_now;
          time(&unix_now);
          rtc.adjust(DateTime(unix_now));
          mensajeFinal += " Sincronización NTP exitosa.";
          now = DateTime(unix_now);
        } else {
          mensajeFinal += " Error: No se pudo obtener hora NTP.";
          exito = false;
          now = DateTime(1735689600);
        }
        WiFi.disconnect(true);
      } else {
        mensajeFinal += " Error: No se pudo conectar a WiFi.";
        exito = false;
        now = DateTime(1735689600);
      }
    } else {
      mensajeFinal = "Init RTC          : ";
      now = rtc.now();
    }
  }

  String horaRTC = String(now.year()) + "-" +
                   String(now.month()) + "-" +
                   String(now.day()) + " " +
                   String(now.hour()) + ":" +
                   String(now.minute()) + ":" +
                   String(now.second());

  String timestampRTC = String(now.unixtime());

  debug(mensajeFinal, false, exito);
  debug("|| Hora: " + horaRTC + " || UNIX: " + timestampRTC, true, -1);

  return now.unixtime();
}

float medirVelocidadViento(unsigned int T_muestreo) {
  unsigned long t_inicio_muestra = micros();
  unsigned long tiempo_pasado = micros();
  unsigned long contador = 0;
  float media_freq_viento = 0.0;

  while (true) {
    unsigned long t_actual_muestra = micros();
    unsigned long t_transcurrido = t_actual_muestra - t_inicio_muestra;

    if (t_transcurrido >= T_muestreo * 1000000UL) {
      break;
    }

    if (pulso) {
      contador++;
      unsigned long tiempo_actual = micros();
      unsigned long periodo = tiempo_actual - tiempo_pasado;

      if (periodo > 0) {
        float freq = 1000000.0 / periodo;
        media_freq_viento += freq;
      }

      pulso = false;
      tiempo_pasado = tiempo_actual;
    }
  }

  float vel_viento = (media_freq_viento * 3.6208) / T_muestreo;

  if (contador > 0) {
    debug("Init Wind speed   : " + String(vel_viento) + " m/s", true, true);
    bandera_nulos |= 0b00100000;
  } else {
    debug("Init Wind speed   : 0 m/s (no pulses detected)", true, true);
  }

  return vel_viento;
}

void ISR_VEL_VIENTO() {
  pulso = true;
}

uint8_t calcular_ceros(int viento, int lluvia, int solar, int uv) {
  uint8_t bandera_ceros = 0;

  if (viento == 0) bandera_ceros |= 0b00001000;
  if (lluvia == 0) bandera_ceros |= 0b00000100;
  if (solar  == 0) bandera_ceros |= 0b00000010;
  if (uv     == 0) bandera_ceros |= 0b00000001;

  debug("Ceros calculados  :", false, true);
  if (bandera_ceros & 0b00001000) debug(" || Vel. viento ", false, -1);
  if (bandera_ceros & 0b00000100) debug(" || Pluv ", false, -1);
  if (bandera_ceros & 0b00000010) debug(" || Rad. Sol ", false, -1);
  if (bandera_ceros & 0b00000001) debug(" || Rad. UV ", false, -1);
  debug("", true, -1);

  return bandera_ceros;
}

uint8_t* cargar_datos() {
  static uint8_t vector[14] = {0};

  int bandera = 1;
  uint32_t var0 = UNIX_TIME;
  int var00 = bandera_nulos;
  int var000 = bandera_ceros;
  int var1 = TEMP;
  int var2 = HUM;
  int var3 = VEL_VIENTO;
  int var4 = DIR_VIENTO;
  int var5 = 0;
  int var6 = PLUV;
  int var7 = RAD_SOL;
  uint8_t var8 = RAD_UV;

  vector[0] |= (bandera & 0x01) << 7;

  vector[0] |= (var0 >> 25) & 0x7F;
  vector[1] = (var0 >> 17) & 0xFF;
  vector[2] = (var0 >> 9) & 0xFF;
  vector[3] = (var0 >> 1) & 0xFF;
  vector[4] = (var0 & 0x01) << 7;

  vector[4] |= (var00 >> 1) & 0x7F;
  vector[5] = (var00 & 0x01) << 7;

  vector[5] |= (var000 & 0x0F) << 3;
  vector[5] |= (var1 >> 6) & 0x07;
  vector[6] = (var1 << 2) & 0xFC;

  vector[6] |= (var2 >> 5) & 0x03;
  vector[7] = (var2 << 3) & 0xF8;

  vector[7] |= (var3 >> 6) & 0x07;
  vector[8] = (var3 << 2) & 0xFC;

  vector[8] |= (var4 >> 2) & 0x03;
  vector[9] = (var4 << 6) & 0xC0;

  vector[9] |= (var5 >> 3) & 0x3F;
  vector[10] = (var5 << 5) & 0xE0;

  vector[10] |= (var6 >> 4) & 0x1F;
  vector[11] = (var6 << 4) & 0xF0;

  vector[11] |= (var7 >> 8) & 0x0F;
  vector[12] = var7 & 0xFF;

  vector[13] = var8 & 0xFF;

  return vector;
}

void init_AHT(float &temperature, float &humidity) {
  AHTxx aht20(AHTXX_ADDRESS_X38, AHT2x_SENSOR);
  bool exito = true;
  String mensaje = "Init AHT25        :";

  if (!aht20.begin()) {
    mensaje = "Init AHT25 failed.";
    exito = false;
    bandera_nulos |= 0b11000000;
  } else {
    temperature = aht20.readTemperature();
    humidity = aht20.readHumidity();
  }

  if (exito) {
    debug(mensaje, false, exito);
    debug(" || Temp: " + String(temperature), false, -1);
    debug(" || Hum : " + String(humidity), false, -1);
    debug("", true, -1);
  } else {
    debug(mensaje, true, exito);
  }
}

float* leerValoresADC() {
  static float voltajes[4];
  bool exito = true;
  String mensaje = "Init ADC          :";

  ads.setGain(GAIN_ONE);

  if (!ads.begin()) {
    exito = false;
    mensaje = "Init ADC fail: Error al iniciar.";
    bandera_nulos &= ~0b00010101;
    voltajes[0] = voltajes[1] = voltajes[2] = voltajes[3] = 0.0;
    return voltajes;
  }

  int16_t adc0 = ads.readADC_SingleEnded(0);
  int16_t adc1 = ads.readADC_SingleEnded(1);
  int16_t adc2 = ads.readADC_SingleEnded(2);
  int16_t adc3 = ads.readADC_SingleEnded(3);

  voltajes[0] = std::max(0.0f, ads.computeVolts(adc0));
  voltajes[1] = std::max(0.0f, ads.computeVolts(adc1));
  voltajes[2] = std::max(0.0f, ads.computeVolts(adc2));
  voltajes[3] = std::max(0.0f, ads.computeVolts(adc3));

  if (exito) {
    debug(mensaje, false, exito);
    debug(" || Rad. UV  : " + String(voltajes[0]) + " V", false, -1);
    debug(" || Rad. Sol : " + String(voltajes[1]) + " V", false, -1);
    debug(" || Bat. Lv  : " + String(voltajes[2]) + " V", false, -1);
    debug(" || Dir. Vie : " + String(voltajes[3]) + " V", false, -1);
    debug("", true, -1);
  } else {
    debug(mensaje, true, exito);
  }

  return voltajes;
}

unsigned int leerIncrementoPluviometro() {
  unsigned int incremento = 0;
  bool exito = true;

  Wire.beginTransmission(INTEGRADO_ADDR);
  if (Wire.endTransmission() != 0) {
    exito = false;
  } else {
    Wire.requestFrom(INTEGRADO_ADDR, 3);
    if (Wire.available() < 3) {
      exito = false;
    } else {
      byte data[3];
      for (int i = 0; i < 3; i++) data[i] = Wire.read();

      unsigned int current_count = ((unsigned long)data[0] << 16) |
                                   ((unsigned long)data[1] << 8)  |
                                   (unsigned long)data[2];

      if (previous_count == 0xFFFFFFFF) {
        incremento = 0;
        debug("Init pluv count   : primera lectura, valor inicial = " + String(current_count), true, true);
      } else if (current_count < previous_count) {
        incremento = (MAX_COUNT - previous_count) + current_count + 1;
        debug("Init pluv count   : desbordamiento, inc = " + String(incremento), true, true);
      } else {
        incremento = current_count - previous_count;
        debug("Init pluv count   : prev = " + String(previous_count) + ", inc = " + String(incremento), true, true);
      }

      previous_count = current_count;
    }
  }

  if (!exito) {
    debug("Init pluv count.", true, false);
    bandera_nulos |= 0b00001000;
  } else {
    bandera_nulos &= ~0b00001000;
  }

  return incremento;
}

void mostrarBinario(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    Serial.print(bitRead(byte, i));
  }
  Serial.println();
}

void gotoSleep(uint32_t seconds) {
  esp_sleep_enable_timer_wakeup(seconds * 1000000UL);

  debug("Sleeping for " + String(seconds) + " seconds...", true, true);
  Serial.flush();

  esp_deep_sleep_start();

  debug("Sleep failed! Waiting 5 minutes before restart...", true, false);
  delay(5UL * 60UL * 1000UL);
  ESP.restart();
}

uint32_t calcularSleepSegundosAntesDelMinuto() {
  DateTime now = rtc.now();
  int segundosActuales = now.second();
  int sleepSegundos = 60 - segundosActuales - 10;
  if (sleepSegundos < 1) sleepSegundos = 1;
  return sleepSegundos;
}
