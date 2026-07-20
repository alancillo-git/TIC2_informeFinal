/*
 * ============================================================
 *  ARDUINO EMISOR (PUERTA)
 * ============================================================
 *  Componentes :
 *    - RC522 RFID : RST=9, SS=10, MOSI=11, MISO=12, SCK=13
 *    - Relé       : pin 2
 *    - RF TX      : pin 6 (RH_ASK, RX ficticio pin 4)
 *
 *  Lógica del relé :
 *    RELAY_NO true  → HIGH = activar (desbloquear)
 *    RELAY_NO false → LOW  = activar (desbloquear)
 * ============================================================
 */

#include <SPI.h>
#include <MFRC522.h>
#include <RH_ASK.h>
#include <EEPROM.h>

// ── Tipo de relé ─────────────────────────────────────────────
// true  = Normalmente Abierto (NA) — el más común
// false = Normalmente Cerrado (NC)
#define RELAY_NO true

// ── Pines ────────────────────────────────────────────────────
#define RELAY_PIN  2
#define SS_PIN    10
#define RST_PIN    9

// ── Objetos ──────────────────────────────────────────────────
MFRC522 rfid(SS_PIN, RST_PIN);
RH_ASK  rfDriver(2000, 4, 6);   // RX ficticio pin 4, TX pin 6

// ── EEPROM ───────────────────────────────────────────────────
#define EEPROM_COUNT_ADDR  0
#define EEPROM_UIDS_ADDR   1
#define MAX_UIDS          40
#define UID_SIZE           4

byte storedUIDs[MAX_UIDS][UID_SIZE];
int  uidCount = 0;

// ── Control del relé ─────────────────────────────────────────
void bloquearPuerta()    { digitalWrite(RELAY_PIN, RELAY_NO ? LOW  : HIGH); }
void desbloquearPuerta() { digitalWrite(RELAY_PIN, RELAY_NO ? HIGH : LOW);  }

// ── Carga y guardado en EEPROM ───────────────────────────────
void cargarUIDs() {
    uidCount = EEPROM.read(EEPROM_COUNT_ADDR);
    if (uidCount > MAX_UIDS) uidCount = 0;
    for (int i = 0; i < uidCount; i++)
        for (int j = 0; j < UID_SIZE; j++)
            storedUIDs[i][j] = EEPROM.read(EEPROM_UIDS_ADDR + i * UID_SIZE + j);
}

void guardarUIDs() {
    EEPROM.write(EEPROM_COUNT_ADDR, uidCount);
    for (int i = 0; i < uidCount; i++)
        for (int j = 0; j < UID_SIZE; j++)
            EEPROM.write(EEPROM_UIDS_ADDR + i * UID_SIZE + j, storedUIDs[i][j]);
}

// ── Gestión de UIDs ──────────────────────────────────────────
bool hexABytes(String hex, byte *out) {
    hex.trim(); hex.toUpperCase();
    if (hex.length() != 8) return false;
    for (int i = 0; i < 4; i++) {
        String b = hex.substring(i * 2, i * 2 + 2);
        out[i] = (byte) strtol(b.c_str(), NULL, 16);
    }
    return true;
}

bool agregarUID(String hexUID) {
    if (uidCount >= MAX_UIDS) { Serial.println("ERR:LLENO"); return false; }
    byte nuevoUID[UID_SIZE];
    if (!hexABytes(hexUID, nuevoUID)) { Serial.println("ERR:INVALIDO"); return false; }
    for (int i = 0; i < uidCount; i++) {
        bool igual = true;
        for (int j = 0; j < UID_SIZE; j++) if (storedUIDs[i][j] != nuevoUID[j]) { igual = false; break; }
        if (igual) { Serial.println("OK:ADD:" + hexUID); return true; }
    }
    for (int j = 0; j < UID_SIZE; j++) storedUIDs[uidCount][j] = nuevoUID[j];
    uidCount++; guardarUIDs();
    Serial.println("OK:ADD:" + hexUID);
    return true;
}

bool eliminarUID(String hexUID) {
    byte objetivo[UID_SIZE];
    if (!hexABytes(hexUID, objetivo)) { Serial.println("ERR:INVALIDO"); return false; }
    for (int i = 0; i < uidCount; i++) {
        bool igual = true;
        for (int j = 0; j < UID_SIZE; j++) if (storedUIDs[i][j] != objetivo[j]) { igual = false; break; }
        if (igual) {
            for (int k = i; k < uidCount - 1; k++)
                for (int j = 0; j < UID_SIZE; j++) storedUIDs[k][j] = storedUIDs[k+1][j];
            uidCount--; guardarUIDs();
            Serial.println("OK:DEL:" + hexUID);
            return true;
        }
    }
    Serial.println("ERR:NO_ENCONTRADO");
    return false;
}

void manejarComandoSerial(String cmd) {
    cmd.trim(); cmd.toUpperCase();
    if      (cmd.startsWith("ADD:"))  agregarUID(cmd.substring(4));
    else if (cmd.startsWith("DEL:"))  eliminarUID(cmd.substring(4));
    else if (cmd == "LIST")           { Serial.print("UIDS:"); Serial.println(uidCount); }
    else if (cmd == "CLEAR")          { uidCount = 0; guardarUIDs(); Serial.println("OK:CLEAR"); }
    else                              Serial.println("ERR:DESCONOCIDO");
}

// ── Verificación de autorización ─────────────────────────────
bool estaAutorizado(byte *uid, byte size) {
    for (int i = 0; i < uidCount; i++) {
        bool coincide = true;
        for (byte j = 0; j < UID_SIZE; j++) if (uid[j] != storedUIDs[i][j]) { coincide = false; break; }
        if (coincide) return true;
    }
    return false;
}

String uidAString(byte *uid, byte size) {
    String resultado = "";
    for (byte i = 0; i < size; i++) {
        if (uid[i] < 0x10) resultado += "0";
        resultado += String(uid[i], HEX);
    }
    resultado.toUpperCase();
    return resultado;
}

void enviarRF(String msg) {
    char buf[40];
    msg.toCharArray(buf, sizeof(buf));
    rfDriver.send((uint8_t *)buf, strlen(buf));
    rfDriver.waitPacketSent();
}

// ── Rango horario (listo para RTC) ──────────────────────────
bool horarioPermitido() { return true; }

// ── Acceso concedido / denegado ──────────────────────────────
void accesoConcedido(String uid) {
    desbloquearPuerta();
    enviarRF("ACCESS," + uid + ",GRANTED");
    delay(5000);             // Cerradura abierta 5 segundos
    bloquearPuerta();
}

void accesoDenegado(String uid) {
    enviarRF("ACCESS," + uid + ",DENIED");
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
    Serial.begin(9600);
    SPI.begin();
    rfid.PCD_Init();
    if (!rfDriver.init()) Serial.println("RF init failed");
    pinMode(RELAY_PIN, OUTPUT);
    bloquearPuerta();        // Cerradura bloqueada al inicio
    cargarUIDs();
    Serial.print("DOOR_READY:");
    Serial.println(uidCount);
}

// ── Loop ─────────────────────────────────────────────────────
void loop() {
    // Comandos serie desde el PC
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        manejarComandoSerial(cmd);
    }

    // Lectura RFID
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

    String uid = uidAString(rfid.uid.uidByte, rfid.uid.size);
    Serial.print("UID detectado: ");
    Serial.println(uid);

    if (estaAutorizado(rfid.uid.uidByte, rfid.uid.size) && horarioPermitido()) {
        accesoConcedido(uid);
    } else {
        accesoDenegado(uid);
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    delay(1000);
}
