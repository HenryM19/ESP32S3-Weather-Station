#include <Adafruit_ADS1X15.h>
#include <AHTxx.h> 
#include <SPI.h> 
#include <Wire.h>
#include <RTClib.h>
#include <Preferences.h>
#include "config.h"
#include <algorithm>
#include <WiFi.h>
#include "time.h"

Preferences preferences;
#define DEBUG_ENABLE 1
#define SESSION_KEY "session"
#define NONCES_KEY "nonces"
#define SESSION_SIZE 256  // Ajustar según la librería
#define NONCES_SIZE 16    // Ajustar según la librería

//============================================== FUNCIONES ======================================================
uint32_t init_RTC();
float medirVelocidadViento(unsigned int T_muestreo);
bool inicializarRadio();
bool cargarSesion();
bool activarSesion(bool restaurada);
bool enviar_datos(uint8_t* payload, size_t longitud);
void ISR_VEL_VIENTO();
void guardarSesion();
bool initLoRaWAN();
uint8_t calcular_ceros(int viento, int lluvia, int solar, int uv);
uint8_t* cargar_datos();
void mostrarBinario(uint8_t byte);
void gotoSleep(uint32_t seconds);
uint32_t calcularSleepSegundosAntesDelMinuto();

//==================================== VARIABLES Y FUNCIONES DEL ANEMÓMETRO =====================================
#define input_vel_viento 17                    // Pin de ingreso del Pluviometro
volatile bool pulso    = false;                // Boleano que se activa cuando hay un pulso

//======================================== VARIABLES DEL PLUVIOMETRO ============================================
#define INTEGRADO_ADDR 0x32                             // Define la dirección del integrado Contador 
#define RESET_PIN      18                               // Pin reset del integrado no funciona (Se hace reset por software)
#define MAX_COUNT      16777215                         // Valor máximo del contador (Según datasheet) (24 bits)
const float MM_PER_PULSE = 0.254;
RTC_DATA_ATTR unsigned int previous_count = 0xFFFFFFFF; // Almacena el último valor leído del contador, persiste durante deep sleep

//========================================== CONFIGURACIÓN DEL RTC ==============================================
RTC_DS3231 rtc;                                // Declaramos un RTC DS3231
//RTC_DATA_ATTR int bootCount = 0;             // Guardamos fecha y hora entre reinicios
RTC_DATA_ATTR time_t UNIX_TIME = 0;           // Variable para almacenar el tiempo en epoch
const char* ssid = "SMT";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600; // Ecuador UTC-5
const int daylightOffset_sec = 0;
int base_tiempo = 1735689600 + (5 * 3600);  // 01 enero 2025 a las 00:00:00 en UTC−5 (Hora Ecuador) equivale a: 1735707600

//============================================ LECTURA DEL ADC =====================================
Adafruit_ADS1115 ads;                          /* Use this for the 16-bit version */

//=========================================== CONFIGURACION I2C =================================================
#define I2C_SDA 47
#define I2C_SCL 48

//================================================== VMTP =======================================================
float TEMP, HUM, VEL_VIENTO;
int DIR_VIENTO, RAD_SOL, RAD_UV, PLUV;
bool    bandera_variacion = 0;
uint8_t bandera_nulos     = 0b00000000;  // Bandera de nulos, si todos los sensores inicializa bien de las 8 variables. 
uint8_t bandera_ceros     = 0b00000000;  // Bandera de ceros si ni Vel Viento, Lluvia, Rad. Sol, Rad. UV son cero.
//uint8_t moduleStatus      = 0; // Variable de 8 bits para el estado de los módulos, usamos solo 4 bits

//==================================== Función de depuración centralizada =======================================
template <typename T>
void debug(T mensaje, bool nuevaLinea = true, int estado = -1) {
  if (!DEBUG_ENABLE) return;

  if (estado == true) {
    Serial.print("[ ✅ ] ");
  } else if (estado == false) {
    Serial.print("[ ❌ ] ");
  }

  if (nuevaLinea) {
    Serial.println(mensaje);
  } else {
    Serial.print(mensaje);
  }
}

//============================================== SETUP ================================================
void setup() {
  Serial.begin(115200); while(!Serial); delay(1000); // Inicia comunicacion serial
  Wire.begin(I2C_SDA, I2C_SCL);                      // Inicia comunicacion I2C

  debug("Init Station", true, true);
  
  preferences.begin("lorawan", false);
  pinMode(input_vel_viento, INPUT);                                                 // Configurar el pin del sensor como entrada 
  attachInterrupt(digitalPinToInterrupt(input_vel_viento), ISR_VEL_VIENTO, RISING); // Interrupción activa

  float*       lecturas_ADC  = leerValoresADC();             // [0] UV, [1] Solar, [2] BAT Lv, [3] Dir Viento,
  unsigned int pulsos_pluv   = leerIncrementoPluviometro();  // Pulsos contados pluviometro
  
  // DATOS ADQUIRIDOS Y CONVERSIÓN
  init_AHT(TEMP, HUM); 
  TEMP           = round(TEMP*10); // Para tomar un decimal de la temperatura
  HUM            = round(HUM);
  VEL_VIENTO     = round(medirVelocidadViento(10));
  PLUV           = round(pulsos_pluv*MM_PER_PULSE*100);
  DIR_VIENTO     = std::min(15, (int)round(lecturas_ADC[3] * (360.0 / 3.3) / 22.5));   // Rosa de 16 puntas. El brazo debe apuntar al norte
  RAD_UV         = std::min(16, std::max(0, (int)round(lecturas_ADC[0] / 0.15)));      // Índice de 0 a 16
  RAD_SOL        = std::min(1800, std::max(0, (int)round(lecturas_ADC[1] / 0.00167))); // 
  bandera_ceros  = calcular_ceros(VEL_VIENTO, PLUV, RAD_SOL, RAD_UV);                  //

  initLoRaWAN();
  UNIX_TIME      = init_RTC()-base_tiempo;

  uint8_t* datos=cargar_datos();
  uint8_t uplinkPayload[14];
  for (int i = 0; i < 14; i++) {
    uplinkPayload[i] = datos[i];
    //mostrarBinario(uplinkPayload[i]);
  }

  
  enviar_datos(uplinkPayload, sizeof(uplinkPayload));

  String resumenPayload = 
  "Tiempo: "     + String(UNIX_TIME) +
  " || BN: "     + String(bandera_nulos) +
  " || BC: "     + String(bandera_ceros) +
  " || T: "      + String(TEMP) + " °C" +
  " || H: "      + String(HUM) + " %" +
  " || Vv: "     + String(VEL_VIENTO) + " m/s" +
  " || Dv: "     + String(DIR_VIENTO) +
  " || LL: "     + String(PLUV) + " mm" +
  " || Rs: "     + String(RAD_SOL) + " W/m²" +
  " || UV: "     + String(RAD_UV);
  debug("Payload           : || " + resumenPayload, true, true);


  uint32_t sleepTime = calcularSleepSegundosAntesDelMinuto();
  gotoSleep(sleepTime);
}

// === LOOP ===
void loop() {}

/////////////////////////////////////////////////////////////////////////////////////
uint8_t* cargar_datos(){
  static uint8_t vector[14] = {0};  // Ahora 14 bytes, uno más para 32 bits de tiempo

  int bandera = 1;                // 1 bit  : Bandera de variación
  uint32_t var0 = UNIX_TIME;     // 32 bits: Marca de tiempo completa
  int var00 = bandera_nulos;      // 8 bits : Bandera Nulos
  int var000 = bandera_ceros;     // 4 bits : Bandera de ceros
  int var1 = TEMP;                // 9 bits : Temperatura
  int var2 = HUM;                 // 7 bits : Humedad
  int var3 = VEL_VIENTO;          // 9 bits : Vel.Viento
  int var4 = DIR_VIENTO;          // 4 bits : Dir. Viento
  int var5 = 0;                  // 9 bits : Presión atmosférica
  int var6 = PLUV;                // 9 bits : Precipitación
  int var7 = RAD_SOL;             // 12 bits: Radiación solar
  uint8_t var8 = RAD_UV;              // 8 bits : Radiación UV

  // bandera (1 bit)
  vector[0] |= (bandera & 0x01) << 7;

  // var0 (32 bits) marca de tiempo
  vector[0] |= (var0 >> 25) & 0x7F;  // 7 bits (restantes del byte 0)
  vector[1] = (var0 >> 17) & 0xFF;   // 8 bits
  vector[2] = (var0 >> 9) & 0xFF;    // 8 bits
  vector[3] = (var0 >> 1) & 0xFF;    // 8 bits
  vector[4] = (var0 & 0x01) << 7;    // 1 bit en bit 7 del byte 4

  // var00 (8 bits): Bandera Nulos
  vector[4] |= (var00 >> 1) & 0x7F;  // 7 bits restantes del byte 4
  vector[5] = (var00 & 0x01) << 7;   // 1 bit en bit 7 del byte 5

  // var000 (4 bits): bandera de ceros
  vector[5] |= (var000 & 0x0F) << 3; // 4 bits en byte 5 bits 3-6

  // var1 (9 bits): Temperatura
  vector[5] |= (var1 >> 6) & 0x07;   // 3 bits restantes en byte 5 bits 0-2
  vector[6] = (var1 << 2) & 0xFC;    // 6 bits en byte 6 bits 2-7

  // var2 (7 bits): Humedad
  vector[6] |= (var2 >> 5) & 0x03;   // 2 bits restantes byte 6 bits 0-1
  vector[7] = (var2 << 3) & 0xF8;    // 5 bits byte 7 bits 3-7

  // var3 (9 bits): Velocidad viento
  vector[7] |= (var3 >> 6) & 0x07;   // 3 bits restantes byte 7 bits 0-2
  vector[8] = (var3 << 2) & 0xFC;    // 6 bits byte 8 bits 2-7

  // var4 (4 bits): Dirección viento
  vector[8] |= (var4 >> 2) & 0x03;   // 2 bits restantes byte 8 bits 0-1
  vector[9] = (var4 << 6) & 0xC0;    // 2 bits byte 9 bits 6-7

  // var5 (9 bits): Presión atmosférica
  vector[9] |= (var5 >> 3) & 0x3F;   // 6 bits byte 9 bits 0-5
  vector[10] = (var5 << 5) & 0xE0;   // 3 bits byte 10 bits 5-7

  // var6 (9 bits): Precipitación
  vector[10] |= (var6 >> 4) & 0x1F;  // 5 bits byte 10 bits 0-4
  vector[11] = (var6 << 4) & 0xF0;   // 4 bits byte 11 bits 4-7

  // var7 (12 bits): Radiación solar
  vector[11] |= (var7 >> 8) & 0x0F;  // 4 bits byte 11 bits 0-3
  vector[12] = var7 & 0xFF;          // 8 bits byte 12

  // var8 (8 bits): Radiación UV
  vector[13] = var8 & 0xFF;          // 8 bits byte 13

  return vector;
}


//============================== Función que inicializa el sensor de temperatura y humedad
void init_AHT(float &temperature, float &humidity) {
  AHTxx aht20(AHTXX_ADDRESS_X38, AHT2x_SENSOR);
  bool exito=true;
  String mensaje = "Init AHT25        :";

  if (!aht20.begin()) {
    mensaje = "Init AHT25 failed.";
    exito=false;
    bandera_nulos |= 0b11000000;   // Nulo en temperatura y humedad, sensor no iniciado bien
  } else {
    temperature = aht20.readTemperature();
    humidity = aht20.readHumidity();
  }

  if (exito){
    debug(mensaje, false, exito);
    debug(" || Temp: " + String(temperature), false, -1);
    debug(" || Hum : " + String(humidity), false, -1);
    debug("", true, -1);
  }else{
    debug(mensaje, true, exito);
  } 
}


//============================== Función que lee los voltajes y los retorna en un arreglo
float* leerValoresADC() {
  static float voltajes[4];  // [0] = UV, [1] = Solar, [2] = Bat Lv, [3] = Viento
  bool exito=true;
  String mensaje="Init ADC          :";

  ads.setGain(GAIN_ONE); // Ganancia 1x: ±4.096V

  if (!ads.begin()) {
    exito           = false;
    mensaje         = "Init ADC fail: Error al iniciar."; 
    bandera_nulos  &= ~0b00010101; // Nulos en Dir. Viento, Rad. UV, Rad, Solar. 
    voltajes[0]     = voltajes[1] = voltajes[2] = voltajes[3] = 0.0;
    return voltajes;
  }

  // Lecturas analógicas
  int16_t   adc0 = ads.readADC_SingleEnded(0);  // Radiación UV
  int16_t   adc1 = ads.readADC_SingleEnded(1);  // Radiación solar
  int16_t   adc2 = ads.readADC_SingleEnded(2);  // BAT Lv
  int16_t   adc3 = ads.readADC_SingleEnded(3);  // Dirección de viento

  voltajes[0] = std::max(0.0f, ads.computeVolts(adc0));
  voltajes[1] = std::max(0.0f, ads.computeVolts(adc1));
  voltajes[2] = std::max(0.0f, ads.computeVolts(adc2));
  voltajes[3] = std::max(0.0f, ads.computeVolts(adc3));


  // Mostrar resultados con debug()
  if(exito){
    debug(mensaje, false , exito);
    debug(" || Rad. UV  : " + String(voltajes[0]) + " V", false, -1);
    debug(" || Rad. Sol : " + String(voltajes[1]) + " V", false, -1);
    debug(" || Bat. Lv  : " + String(voltajes[2]) + " V", false, -1);
    debug(" || Dir. Vie : " + String(voltajes[3]) + " V", false, -1);
    debug("", true, -1);
  }else{
    debug(mensaje, true, exito);
  }
  
  return voltajes;
}

unsigned int leerIncrementoPluviometro() {
  unsigned int incremento = 0;
  bool exito = true;

  // Verificar si el contador responde en el bus I2C
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
        // Primera vez: solo inicializar sin calcular incremento
        incremento = 0;
        debug("Init pluv count   : primera lectura, valor inicial = " + String(current_count), true, true);
      } else if (current_count < previous_count) {
        // Desbordamiento del contador de 24 bits
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
    bandera_nulos |= 0b00001000; // Nulo en precipitación
  } else {
    bandera_nulos &= ~0b00001000; // No nulo en precipitación
  }

  return incremento;
}

//============================== Función que obtiene el tiempo en UNIX del RTC
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

      // Conectarse a WiFi temporalmente
      WiFi.begin(ssid); // Red abierta, sin contraseña
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
        WiFi.disconnect(true); // Desconectar WiFi
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

  // Mostrar resumen final
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

// ============================= Función que obtiene la velocidad del viento
float medirVelocidadViento(unsigned int T_muestreo) {
  unsigned long t_inicio_muestra = micros();
  unsigned long tiempo_pasado = micros();
  unsigned long contador = 0;
  float media_freq_viento = 0.0;

  extern volatile bool pulso;

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
        float freq = 1000000.0 / periodo; // Hz
        media_freq_viento += freq;
      }

      pulso = false;
      tiempo_pasado = tiempo_actual;
    }
  }

  float vel_viento = (media_freq_viento * 3.6208) / T_muestreo;

  if (contador > 0) {
    debug("Init Wind speed   : " + String(vel_viento) + " m/s", true, true);
    bandera_nulos|=0b00100000;
  } else {
    debug("Init Wind speed   : 0 m/s (no pulses detected)", true, true);
  }

  return vel_viento;
}

//======================================== Función de interrupción para pulsos de viento
void ISR_VEL_VIENTO() {
  pulso = true; // Incrementar el contador de pulsos
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

bool initLoRaWAN() {
  bool exito = true;
  String mensaje = "Init LoRaWAN";

  // === Inicializar radio LoRa ===
  int16_t radioStatus = radio.begin(903.0, 125.0, 8, 5, 0x34, 15, 8);
  if (radioStatus != RADIOLIB_ERR_NONE) {
    mensaje = "Init LoRaWAN: Error al inicializar radio (code " + String(radioStatus) + ")";
    exito = false;
  } else {
    radio.setSpreadingFactor(7);

    // === Restaurar sesión desde memoria ===
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
        // Intentar activar sesión restaurada
        int16_t estadoSesion = node.activateOTAA();
        if (estadoSesion == RADIOLIB_ERR_NONE || estadoSesion == RADIOLIB_LORAWAN_NEW_SESSION) {
          sesionRestaurada = true;
        }
      }
    }

    // === Si no se pudo restaurar, intentar join OTAA ===
    if (!sesionRestaurada) {
      node.setADR(false);       // Desactiva ADR
      node.setDatarate(4);      // DR4 = SF7 US915

      int16_t joinStatus = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
      if (joinStatus != RADIOLIB_ERR_NONE) {
        mensaje = "Init LoRaWAN: Falló OTAA join (code " + String(joinStatus) + ")";
        exito = false;
      } else {
        int16_t estadoSesion = node.activateOTAA();
        if (estadoSesion == RADIOLIB_LORAWAN_NEW_SESSION) {
          // Guardar nueva sesión en memoria
          uint8_t* session = node.getBufferSession();
          uint8_t* nonces = node.getBufferNonces();
          preferences.putBytes(SESSION_KEY, session, SESSION_SIZE);
          preferences.putBytes(NONCES_KEY, nonces, NONCES_SIZE);
          // debug("Sesión LoRaWAN guardada en memoria", true, true);
        } else {
          mensaje = "Init LoRaWAN: Error al activar nueva sesión";
          exito = false;
        }
      }
    }
  }

  debug(mensaje, true, exito);
  return exito;
}


bool enviar_datos(uint8_t* payload, size_t longitud) {
  node.setDatarate(4);  // Forzar SF8
  node.setDwellTime(false, 10);

  uint8_t maxPayload = node.getMaxPayloadLen();

  if (longitud > maxPayload) {
    debug("Error: Payload (" + String(longitud) + "B) > Max (" + String(maxPayload) + "B)", true, false);
    return false;
  }

  int16_t state = node.sendReceive(payload, longitud, 1, false);
  bool enviado = (state >= 0);
  bool recibido = (state > 0);

  String resumen = "Packet            : ";
  resumen += String(longitud) + "B";
  resumen += " || MAX: " + String(maxPayload);
  resumen += " || Uplink: " + String(enviado ? "✅" : "❌");
  resumen += " || Downlink: " + String(recibido ? "✅" : "❌");

  debug(resumen, true, enviado);

  return enviado;
}



void mostrarBinario(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    Serial.print(bitRead(byte, i));
  }
  Serial.println();
}

void gotoSleep(uint32_t seconds) {
  esp_sleep_enable_timer_wakeup(seconds * 1000000UL); // microsegundos

  debug("Sleeping for " + String(seconds) + " seconds...", true, true);
  Serial.flush();  // Asegura que los datos en el buffer se envíen antes de dormir

  esp_deep_sleep_start();

  // Si llegamos aquí, falló entrar en modo deep sleep
  debug("Sleep failed! Waiting 5 minutes before restart...", true, false);
  delay(5UL * 60UL * 1000UL);
  ESP.restart();
}

uint32_t calcularSleepSegundosAntesDelMinuto() {
  DateTime now = rtc.now();
  int segundosActuales = now.second();
  int sleepSegundos = 60 - segundosActuales - 10;  // 10 seg antes del siguiente minuto
  if (sleepSegundos < 1) sleepSegundos = 1;        // mínimo 1 seg
  return sleepSegundos;
}




