// ==============================================================================
// DFE_ESP32_PendulumGroupsSequencer_v2.0
// Secuenciador de pendulos por grupos - Adafruit ESP32 Feather V2
// ==============================================================================
//
//  CREDITOS
//  --------
//  Codigo original (Arduino):  Sebastian Gonzalez Dixon - 09/2019
//                              "Pendulum_Groups_Sequencer_Setian_01.ino"
//
//  Version actualizada para ESP32: Carlos Adrian Serna - 08/2026
//  Esta es una version actualizada para ESP32 (Adafruit ESP32 Feather V2) del
//  archivo de Arduino creado por Sebastian Gonzalez Dixon en 09/2019, portada
//  al mismo hardware que utiliza el sistema DFC (Delirious Fields Coils).
//
//  Proyecto: Delirious Fields - Alba Triana Studio
//
// ==============================================================================
//
//  QUE HACE ESTE FIRMWARE
//  ----------------------
//  Controla bobinas electromagneticas organizadas en grupos independientes.
//  De cada grupo de N bobinas solo se enciende 1 a la vez, elegida al azar.
//  Las duraciones de Encendido (ON) y Apagado (OFF) se sortean con una
//  distribucion de probabilidad de 3 niveles, independiente para cada grupo.
//
//  Cada grupo corre su propio reloj: no hay sincronia entre grupos y no hay
//  comunicacion entre placas (a diferencia del DFC, que usa ESP-NOW).
//
// ==============================================================================

#include <Adafruit_NeoPixel.h>

// ==============================================================================
// CONFIGURATION SECTION - Easy to modify
// ==============================================================================

// === NeoPixel LED Configuration ===
#define NEOPIXEL_PIN 0            // Pin del LED NeoPixel integrado (Feather V2)
#define NUMPIXELS    1            // Cantidad de LEDs NeoPixel

// === Coil Groups Configuration ===
// coilsPerGroup * numberOfGroups debe ser <= TOTAL_OUTPUTS (8 salidas fisicas).
//
//   coilsPerGroup = 2, numberOfGroups = 4  -> 4 pendulos con bobina Izq/Der
//                                             (cableado fisico del PCB DFC)
//   coilsPerGroup = 1, numberOfGroups = 8  -> 8 bobinas independientes
//
const int coilsPerGroup  = 2;     // Bobinas por grupo (solo 1 encendida a la vez)
const int numberOfGroups = 4;     // Cantidad de grupos independientes

// Mapa plano de salidas fisicas del ESP32 Feather V2 (mismo PCB que el DFC).
// El orden es {Izq, Der} por pendulo, para que con coilsPerGroup = 2 cada grupo
// caiga exactamente sobre un pendulo del montaje.
//
//   Grupo 0: Izquierda = Pin 22, Derecha = Pin 20
//   Grupo 1: Izquierda = Pin 14, Derecha = Pin 32
//   Grupo 2: Izquierda = Pin 15, Derecha = Pin 33
//   Grupo 3: Izquierda = Pin 27, Derecha = Pin 12
//
#define TOTAL_OUTPUTS 8
const int coilPin[TOTAL_OUTPUTS] = { 22, 20, 14, 32, 15, 33, 27, 12 };

// === Probability Configuration (per group, in %) ===
// percentOne  -> probabilidad de usar el tiempo 1
// percentTwo  -> probabilidad de usar el tiempo 2
// El tiempo 3 toma el resto: 100 - (percentOne + percentTwo)
#define MAX_GROUPS 8
int percentOne[MAX_GROUPS] = { 60, 60, 60, 60, 60, 60, 60, 60 };
int percentTwo[MAX_GROUPS] = { 30, 30, 30, 30, 30, 30, 30, 30 };

// === Timing Configuration (per group, in MILISEGUNDOS) ===
// OJO: la version Arduino original trabajaba en segundos enteros.
// Aqui se trabaja en milisegundos para permitir tiempos finos (ej. 5770 ms),
// igual que el DFC. Los valores por defecto equivalen a 6 s / 12 s / 18 s.
unsigned long holdTimeOne[MAX_GROUPS]   = { 6000,  6000,  6000,  6000,  6000,  6000,  6000,  6000  };
unsigned long holdTimeTwo[MAX_GROUPS]   = { 12000, 12000, 12000, 12000, 12000, 12000, 12000, 12000 };
unsigned long holdTimeThree[MAX_GROUPS] = { 18000, 18000, 18000, 18000, 18000, 18000, 18000, 18000 };

unsigned long holdTimeOffOne[MAX_GROUPS]   = { 6000,  6000,  6000,  6000,  6000,  6000,  6000,  6000  };
unsigned long holdTimeOffTwo[MAX_GROUPS]   = { 12000, 12000, 12000, 12000, 12000, 12000, 12000, 12000 };
unsigned long holdTimeOffThree[MAX_GROUPS] = { 18000, 18000, 18000, 18000, 18000, 18000, 18000, 18000 };

// === System Timing Configuration ===
const unsigned long STARTUP_DEAD_TIME = 1000;   // Tiempo muerto inicial (ms)

// === Proteccion termica de la bobina ===
// El PCB del DFC esta dimensionado para encendidos de hasta ~7 s continuos
// (bobina de ~15 ohm a 18 V = ~1.2 A = ~21 W disipados).
//
//   SAFETY_MAX_ON_MS = 0     -> sin recorte (comportamiento identico al original)
//   SAFETY_MAX_ON_MS = 7000  -> recorta cualquier tiempo ON mayor a 7 s
//
// Por defecto va DESACTIVADO para respetar los tiempos originales de la obra,
// pero el arranque imprime una advertencia si algun tiempo ON supera el limite.
const unsigned long SAFETY_MAX_ON_MS      = 0;
const unsigned long HW_RECOMMENDED_MAX_ON = 7000;

// === LED Blink Configuration ===
const int BLINK_ON_DURATION  = 150;             // Verde: bobina encendida (ms)
const int BLINK_OFF_DURATION = 80;              // Azul: bobina apagada (ms)
const int HEARTBEAT_INTERVAL = 10000;           // Latido "sistema vivo" (ms)
const int HEARTBEAT_DURATION = 40;              // Duracion del latido (ms)
const int ERROR_BLINK_PERIOD = 600;             // Parpadeo rojo de error (ms)

// RGB color values for different blink types
const uint8_t GREEN_R = 0,  GREEN_G = 150, GREEN_B = 0;    // Bobina ON
const uint8_t BLUE_R  = 0,  BLUE_G  = 0,   BLUE_B  = 150;  // Bobina OFF
const uint8_t HEART_R = 12, HEART_G = 12,  HEART_B = 12;   // Latido
const uint8_t RED_R   = 150, RED_G  = 0,   RED_B   = 0;    // Error de config

// ==============================================================================
// END OF CONFIGURATION SECTION
// ==============================================================================

Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

const int outputNumber = coilsPerGroup * numberOfGroups;

// === Runtime Variables (Do not modify) ===
unsigned long nextEvent[MAX_GROUPS];   // Instante del proximo cambio por grupo
bool          state[MAX_GROUPS];       // true = grupo con una bobina encendida
int           activeCoil[MAX_GROUPS];  // Indice de la bobina encendida del grupo
bool          configError = false;     // Bloquea las salidas si la config es invalida

// === LED control variables (non-blocking) ===
bool          blinkActive = false;
unsigned long blinkStartTime = 0;
int           blinkDuration = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastErrorBlink = 0;
bool          errorLedOn = false;

// ==============================================================================
// HELPERS
// ==============================================================================

// Pide un parpadeo no bloqueante. La ultima peticion gana.
void requestBlink(uint8_t r, uint8_t g, uint8_t b, int durationMs) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
  blinkStartTime = millis();
  blinkDuration = durationMs;
  blinkActive = true;
}

// Pin fisico de la bobina j del grupo i (misma formula que la version original)
int pinOf(int group, int coil) {
  return coilPin[group * coilsPerGroup + coil];
}

// Sortea la duracion del proximo estado del grupo segun sus 3 probabilidades.
// turningOn = true  -> devuelve un tiempo de ENCENDIDO
// turningOn = false -> devuelve un tiempo de APAGADO
unsigned long pickDuration(int group, bool turningOn) {
  long r = random(100);                       // 0..99
  long p1 = percentOne[group];
  long p2 = percentTwo[group];
  unsigned long duration;

  if (r < p1) {
    duration = turningOn ? holdTimeOne[group] : holdTimeOffOne[group];
  } else if (r < p1 + p2) {
    duration = turningOn ? holdTimeTwo[group] : holdTimeOffTwo[group];
  } else {
    duration = turningOn ? holdTimeThree[group] : holdTimeOffThree[group];
  }

  if (turningOn && SAFETY_MAX_ON_MS > 0 && duration > SAFETY_MAX_ON_MS) {
    duration = SAFETY_MAX_ON_MS;
  }
  return duration;
}

// Apaga todas las bobinas de un grupo
void allCoilsOff(int group) {
  for (int j = 0; j < coilsPerGroup; j++) {
    digitalWrite(pinOf(group, j), LOW);
  }
}

// Valida la configuracion antes de energizar nada
bool validateConfig() {
  bool ok = true;

  if (coilsPerGroup < 1 || numberOfGroups < 1) {
    Serial.println("[X] coilsPerGroup y numberOfGroups deben ser >= 1");
    ok = false;
  }
  if (numberOfGroups > MAX_GROUPS) {
    Serial.print("[X] numberOfGroups > MAX_GROUPS ("); Serial.print(MAX_GROUPS); Serial.println(")");
    ok = false;
  }
  if (outputNumber > TOTAL_OUTPUTS) {
    Serial.print("[X] coilsPerGroup * numberOfGroups = "); Serial.print(outputNumber);
    Serial.print(" excede las "); Serial.print(TOTAL_OUTPUTS);
    Serial.println(" salidas fisicas del PCB");
    ok = false;
  }
  for (int i = 0; i < numberOfGroups && i < MAX_GROUPS; i++) {
    if (percentOne[i] + percentTwo[i] > 100) {
      Serial.print("[X] Grupo "); Serial.print(i);
      Serial.println(": percentOne + percentTwo supera 100%");
      ok = false;
    }
  }
  return ok;
}

// Avisa si algun tiempo ON supera lo que el PCB soporta comodamente
void warnLongOnTimes() {
  if (SAFETY_MAX_ON_MS > 0) {
    Serial.print("[i] Recorte de seguridad ACTIVO: tiempos ON limitados a ");
    Serial.print(SAFETY_MAX_ON_MS); Serial.println(" ms");
    return;
  }
  for (int i = 0; i < numberOfGroups; i++) {
    unsigned long maxOn = max(holdTimeOne[i], max(holdTimeTwo[i], holdTimeThree[i]));
    if (maxOn > HW_RECOMMENDED_MAX_ON) {
      Serial.print("[!] Grupo "); Serial.print(i);
      Serial.print(": tiempo ON maximo de "); Serial.print(maxOn);
      Serial.print(" ms supera los "); Serial.print(HW_RECOMMENDED_MAX_ON);
      Serial.println(" ms de diseno termico del PCB. Vigilar temperatura de la bobina.");
    }
  }
}

// ==============================================================================
// SETUP
// ==============================================================================

void setup() {
  Serial.begin(115200);
  delay(100);

  pixels.begin();
  pixels.clear();
  pixels.show();

  Serial.println();
  Serial.println("==============================================");
  Serial.println("DFE - Secuenciador de Pendulos por Grupos v2.0");
  Serial.println("ESP32 Feather V2 | Delirious Fields");
  Serial.println("Original: Sebastian Gonzalez Dixon (09/2019)");
  Serial.println("Port ESP32: Carlos Adrian Serna (08/2026)");
  Serial.println("==============================================");

  configError = !validateConfig();
  if (configError) {
    Serial.println("[X] Configuracion invalida: salidas DESACTIVADAS (LED rojo).");
    return;
  }

  // Todas las salidas arrancan en LOW antes de cualquier otra cosa
  for (int i = 0; i < outputNumber; i++) {
    pinMode(coilPin[i], OUTPUT);
    digitalWrite(coilPin[i], LOW);
  }

  // Semilla real de hardware: cada arranque produce una secuencia distinta
  randomSeed(esp_random());

  unsigned long now = millis();
  for (int i = 0; i < numberOfGroups; i++) {
    state[i] = false;
    activeCoil[i] = 0;
    nextEvent[i] = now + STARTUP_DEAD_TIME;   // Tiempo muerto al encender
  }

  Serial.print("[OK] Grupos: "); Serial.print(numberOfGroups);
  Serial.print(" | Bobinas por grupo: "); Serial.print(coilsPerGroup);
  Serial.print(" | Salidas usadas: "); Serial.print(outputNumber);
  Serial.print("/"); Serial.println(TOTAL_OUTPUTS);

  for (int i = 0; i < numberOfGroups; i++) {
    Serial.print("     Grupo "); Serial.print(i); Serial.print(" -> pines: ");
    for (int j = 0; j < coilsPerGroup; j++) {
      Serial.print(pinOf(i, j));
      if (j < coilsPerGroup - 1) Serial.print(", ");
    }
    Serial.print("  | probabilidades: ");
    Serial.print(percentOne[i]); Serial.print("% / ");
    Serial.print(percentTwo[i]); Serial.print("% / ");
    Serial.print(100 - percentOne[i] - percentTwo[i]); Serial.println("%");
  }

  warnLongOnTimes();

  Serial.print("[i] Tiempo muerto inicial: ");
  Serial.print(STARTUP_DEAD_TIME); Serial.println(" ms");
  Serial.println("[>] Secuenciador en marcha.");

  lastHeartbeat = now;
}

// ==============================================================================
// LOOP PRINCIPAL
// ==============================================================================

void loop() {
  unsigned long now = millis();

  // --- Modo error: nunca energiza bobinas, solo parpadea en rojo ---
  if (configError) {
    if (now - lastErrorBlink >= (unsigned long)ERROR_BLINK_PERIOD) {
      lastErrorBlink = now;
      errorLedOn = !errorLedOn;
      pixels.setPixelColor(0, errorLedOn ? pixels.Color(RED_R, RED_G, RED_B) : 0);
      pixels.show();
    }
    return;
  }

  // --- Secuenciador: cada grupo avanza con su propio reloj ---
  for (int i = 0; i < numberOfGroups; i++) {

    // Comparacion con resta: inmune al desbordamiento de millis() (~49 dias)
    if ((long)(now - nextEvent[i]) < 0) continue;

    if (state[i]) {
      // Estaba encendido -> apagar todo el grupo
      allCoilsOff(i);
      state[i] = false;
      unsigned long tOff = pickDuration(i, false);
      nextEvent[i] = now + tOff;

      Serial.print("[ ] OFF - Grupo "); Serial.print(i);
      Serial.print(" | TOFF: "); Serial.print(tOff); Serial.println(" ms");

      requestBlink(BLUE_R, BLUE_G, BLUE_B, BLINK_OFF_DURATION);

    } else {
      // Estaba apagado -> encender UNA bobina al azar del grupo
      activeCoil[i] = random(coilsPerGroup);
      digitalWrite(pinOf(i, activeCoil[i]), HIGH);
      state[i] = true;
      unsigned long tOn = pickDuration(i, true);
      nextEvent[i] = now + tOn;

      Serial.print("[*] ON  - Grupo "); Serial.print(i);
      Serial.print(" | Bobina "); Serial.print(activeCoil[i]);
      Serial.print(" (pin "); Serial.print(pinOf(i, activeCoil[i]));
      Serial.print(") | TON: "); Serial.print(tOn); Serial.println(" ms");

      requestBlink(GREEN_R, GREEN_G, GREEN_B, BLINK_ON_DURATION);
    }
  }

  // --- Gestion del LED (no bloqueante) ---
  if (blinkActive) {
    if (now - blinkStartTime >= (unsigned long)blinkDuration) {
      pixels.clear();
      pixels.show();
      blinkActive = false;
    }
  } else if (now - lastHeartbeat >= (unsigned long)HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    requestBlink(HEART_R, HEART_G, HEART_B, HEARTBEAT_DURATION);
  }
}

// ==============================================================================
// NOTAS DE LA VERSION ESP32 (v2.0)
// ------------------------------------------------------------------------------
//  - Hardware: Adafruit ESP32 Feather V2 + PCB de potencia del DFC (8 salidas).
//  - Sin delay() dentro del loop: el temporizado de todos los grupos es real.
//  - Probabilidades con cortes acumulados: ya no hay valores "muertos"
//    (en la version Arduino, r == 60 y r == 90 no entraban en ninguna rama).
//  - Comparacion de tiempos inmune al desbordamiento de millis().
//  - random() sembrado con esp_random(): cada arranque es distinto.
//  - Tiempos en milisegundos (antes segundos enteros).
//  - LED NeoPixel de diagnostico y traza por puerto serie a 115200 baudios.
// ==============================================================================
