/*
 * ============================================================
 *  ARDUINO RECEPTOR (ESTACIÓN)
 * ============================================================
 *  Componentes :
 *    - RF RX    : pin 13
 *    - Servo 1  : pin 3  (simbólico — apertura de puerta)
 *    - Servo 2  : pin 5  (simbólico — apertura de puerta)
 *    - Servo 3  : pin 7  (simbólico — apertura de puerta)
 *    - USB      : → PC
 *
 *  Comportamiento servos :
 *    Posición inicial  : 180°
 *    Acceso CONCEDIDO  : 180° → 0°  (simula apertura)
 *    Después de 5 seg  : 0°  → 180° (simula cierre)
 *
 *  ⚠ Servo.h y RH_ASK tienen conflicto en Timer1
 *    → control manual de servos (sin biblioteca)
 * ============================================================
 */

#include <RH_ASK.h>
#include <SPI.h>

// ── Pines ────────────────────────────────────────────────────
#define SERVO1_PIN  3
#define SERVO2_PIN  5
#define SERVO3_PIN  7
#define RF_RX_PIN  13

// RH_ASK(velocidad, pinRX, pinTX)
RH_ASK driver(2000, RF_RX_PIN, 12);   // TX ficticio pin 12

// ── Heartbeat ────────────────────────────────────────────────
unsigned long ultimoHeartbeat = 0;
const unsigned long INTERVALO_HEARTBEAT = 5000;   // cada 5 segundos

// ══════════════════════════════════════════════════════════════
//  SERVOS — control manual (evita conflicto Timer1 con RH_ASK)
// ══════════════════════════════════════════════════════════════

// Mueve los 3 servos simultáneamente hacia un ángulo dado
void moverServos(int grados) {
    int anchoPulso = map(grados, 0, 180, 544, 2400);
    unsigned long inicio = millis();
    while (millis() - inicio < 800) {
        digitalWrite(SERVO1_PIN, HIGH);
        digitalWrite(SERVO2_PIN, HIGH);
        digitalWrite(SERVO3_PIN, HIGH);
        delayMicroseconds(anchoPulso);
        digitalWrite(SERVO1_PIN, LOW);
        digitalWrite(SERVO2_PIN, LOW);
        digitalWrite(SERVO3_PIN, LOW);
        delayMicroseconds(20000 - anchoPulso);
    }
}

// ── Secuencia apertura / cierre ──────────────────────────────
void abrirPuerta() {
    moverServos(0);      // 180° → 0° (apertura)
}

void cerrarPuerta() {
    moverServos(180);    // 0° → 180° (cierre)
}

// ══════════════════════════════════════════════════════════════
//  Setup
// ══════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(9600);

    if (!driver.init()) {
        Serial.println("RF_INIT_FAILED");
    } else {
        Serial.println("STATION,READY");
    }

    // Configurar pines de servos
    pinMode(SERVO1_PIN, OUTPUT);
    pinMode(SERVO2_PIN, OUTPUT);
    pinMode(SERVO3_PIN, OUTPUT);

    // Posición inicial : 180°
    moverServos(180);
}

// ══════════════════════════════════════════════════════════════
//  Loop
// ══════════════════════════════════════════════════════════════
void loop() {
    // ── Lectura RF ──────────────────────────────────────────
    uint8_t buf[40];
    uint8_t buflen = sizeof(buf);

    if (driver.recv(buf, &buflen)) {
        buf[buflen] = '\0';
        String mensaje = String((char *)buf);

        // Retransmitir al PC
        Serial.println(mensaje);

        // Si acceso concedido → activar los servos
        if (mensaje.indexOf("GRANTED") >= 0) {
            abrirPuerta();       // 180° → 0°
            delay(5000);         // Esperar 5 segundos
            cerrarPuerta();      // 0° → 180°
        }
    }

    // ── Heartbeat ───────────────────────────────────────────
    if (millis() - ultimoHeartbeat >= INTERVALO_HEARTBEAT) {
        Serial.println("HEARTBEAT");
        ultimoHeartbeat = millis();
    }
}
