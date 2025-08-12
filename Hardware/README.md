
markdown
Copiar
Editar
# ⚙️ Hardware de la estación

La estación meteorológica está compuesta por dos módulos PCB:  
1. **Módulo de conexión de sensores**  
2. **Módulo de procesamiento y comunicaciones**

Los archivos del diseño PCB se encuentran en la carpeta `Designs`.

---

## 📦 Módulo de conexión de sensores
Incluye:  
- **ADC** para adquisición de señales analógicas  
- **RTC** (Real Time Clock) para mantener la hora  
- **Contador** S-35770E01I-K8T2U  
- **Controlador de alimentación** MIC2545A-2YM  
- **Conectores** para la interfaz con sensores externos

---

## 🖥️ Módulo de procesamiento y comunicaciones
Incluye:  
- **ESP32-S3** como microcontrolador principal  
- **SX1262** como transceptor LoRa

---

## 🌦️ Sensores
- **6450** Davis Instruments: Sensor de radiación solar  
- **6490** Davis Instruments: Sensor de radiación UV  
- **6410** Davis Instruments: Anemómetro (dirección y velocidad del viento)  
- **7857** Davis Instruments: Pluviómetro  
- **AHT25**: Sensor de temperatura y humedad  

---