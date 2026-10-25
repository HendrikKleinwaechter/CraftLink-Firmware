/*
 * CraftLink – Raspberry Pi Pico als Controller für ControlHub · Pico / Pico W / Pico 2 / Pico 2 W
 * ===========================================================================================
 * Wozu: ControlHub schickt seine Ergebnisse (Tastendrücke, Gamepad-Tasten, Achsen) nicht selbst ins Spiel, sondern
 * über ein USB-Gerät, das sich am PC als Gamepad + Tastatur meldet. Bei der CraftConsole macht das deren Pico.
 *  - Ohne alles angeschlossen: Überbrückung für alle ohne CraftConsole (früher „ControlHub-Dummy“) -- Kachel-Tasten
 *    auf dem Tablet, Makros und GPS-Auslöser funktionieren genauso.
 *  - Mit einer CraftUnit (Unterbau + Griff aus der Armlehne) am CraftUnit-Anschluss: die Unit allein am PC nutzen.
 *    Bisher ist der Anschluss nur VORBEREITET (Pins reserviert, Abschnitt „CraftUnit“) -- ausgelesen wird später,
 *    wenn Testhardware daneben liegt (Hendrik 23.10.2026).
 *  - Eigene Taster (GP0–3, GP8–14, GP21–22 → Tasten 31–43) und ein Poti (GP28) für kleine Selbstbau-Controller.
 *  - BOOTSEL-Taste auf dem Pico = Taste 30 (Test ohne Verkabelung, z. B. Windows joy.cpl).
 * Pinbelegung wie die CraftCoreMini-Platine, damit eine CraftUnit dort und an einer CraftLink-Platine gleich
 * angeschlossen wird.
 *
 * Protokoll wie die CraftConsole (Repo CraftConsole-Firmware):
 *   Pico → ControlHub:  <FRAME>B:128 Tasten A:16 Achsen</FRAME>   (Vollzustand, höchstens 50/s)
 *   ControlHub → Pico:  <OUT>GP:AXES:16 GP:BUTTONS:128 K:Tasten</OUT>
 *   Ab 3 gültigen <OUT> in Folge gilt ControlHub als verbunden; 200 ms ohne <OUT> → eigener Betrieb (Fallback):
 *   angeschlossene Taster gehen 1:1 als Gamepad-Tasten raus, die Tastatur ist leer.
 *   Zeilenbefehle: PING, VER, GETALL, STATUS, DIAG, LEARN ON|OFF, MON, BEEP, (SET/CAL → ERR: kommt mit der CraftUnit).
 *   Neu gegenüber der CraftConsole: GETALL nennt device=…, buttons=…, axes=…, unit=… -- Grundlage für Geräteprofile.
 *
 * Arduino IDE: Boardverwalter „Raspberry Pi Pico/RP2040/RP2350“ (Earle Philhower, ab 4.x, getestet mit 5.6.0),
 *   Board „Raspberry Pi Pico“ (oder Pico W / Pico 2 / Pico 2 W),  Werkzeuge → USB Stack → „Adafruit TinyUSB“ (wichtig!).
 *   Keine weiteren Bibliotheken nötig.
 *
 * ControlCraft · Hendriks Werkstatt · Version siehe FW_VERSION
 */

#include <Adafruit_TinyUSB.h>

// ================= Einstellungen =================
#define FW_VERSION   "1.1"   // 1.1 (24.10.2026): restliche Tastaturtasten (Feststell, Num-Lock, Num-Enter, Windows, Menü, F13–F24)
#define DEVICE_ID    "craftlink"         // für ControlHub (Geräteprofil); bei eigenen Abwandlungen ändern, z. B. "eigen-schaltpult"
#define USB_VID      0x2E8A              // Raspberry Pi
#define USB_PID      0xCC02              // CraftConsole = CC01, CraftLink = CC02 (ControlHub erkennt beide)
#define NAME_MANUF   "ControlCraft"
#define NAME_PROD    "CraftLink"
#define NAME_KBD     "CraftLink Tastatur"

// ---- CraftUnit-Anschluss (wie CraftCoreMini J5/J4) -- bisher RESERVIERT, nicht als Taster benutzen ----
// Core 8P: 3V3 · SDA1 · SCL1 · GND · SDA2 · SCL2 · A0 · A1   ·   Aux 5P: D1..D5 (aktiv-low gegen GND)
#define UNIT_SDA_A   4                   // I2C0 (Wire),  Griff-Kanal A  (Core Pin 2)
#define UNIT_SCL_A   5                   //                                (Core Pin 3)
#define UNIT_SDA_B   6                   // I2C1 (Wire1), Griff-Kanal B  (Core Pin 5) -- CraftCoreMini: auch Satelliten-Bus
#define UNIT_SCL_B   7                   //                                (Core Pin 6)
#define UNIT_A0      26                  // Thumbstick X (Core Pin 7, ADC0)
#define UNIT_A1      27                  // Thumbstick Y (Core Pin 8, ADC1)
const uint8_t UNIT_AUX_PINS[5] = { 16, 17, 18, 19, 20 };   // D1..D5
#define PIEZO_PIN    15                  // CraftCoreMini: Piezo (100 Ω) -- deshalb kein Taster an GP15
#define PIEZO_ENABLED 0                  // 1 = BEEP auch als Ton an GP15 (nur mit Piezo, z. B. auf der CraftCoreMini)

// ---- Eigene Taster: GPIO gegen GND, interner Pull-up. Reihenfolge = Tastennummer ab BUTTON_FIRST ----
// Frei auf allen Pico-Varianten und nicht vom CraftUnit-Anschluss belegt (GP23–25 und GP29 belegt der Pico selbst).
const uint8_t BUTTON_PINS[] = { 0, 1, 2, 3, 8, 9, 10, 11, 12, 13, 14, 21, 22 };
#define BUTTON_COUNT  (sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]))
#define BUTTON_FIRST  31                 // Tasten 31–43 (ControlHub zeigt sie als Modul „Tasten“, Geräteprofil craftlink)
#define BOOTSEL_BUTTON 30                // BOOTSEL-Taste auf dem Pico; 0 = aus. Unter 32, damit Windows joy.cpl sie zeigt
// CraftUnit (später): Tasten 1–29 und Achsen 0–3 wie CraftSlot 1 der CraftConsole

// ---- Eigenes Poti: Schleifer an GP28, Enden an 3V3(OUT) und AGND. Ab Werk AUS (offener Eingang „rauscht“) ----
#define POT_ENABLED  0                   // 1 = Poti an GP28 einlesen
#define POT_PIN      28
#define POT_AXIS     4                   // Gamepad-Achse Ry (0–3 bleiben für die CraftUnit frei)
#define AXIS_DEADZONE 40                 // um die Mitte (von 0..4095), damit ein Poti in Ruhe nicht zittert

// ================= Ab hier nichts anpassen =================
#define MAIN_BTNS 128                    // wie CraftConsole und ControlHub (MAIN_BUTTON_COUNT)
#define FRAME_MS 20                      // höchstens 50 Frames/s
#define SERVER_TIMEOUT_MS 200
#define DEBOUNCE_MS 5

// HID: Gamepad mit 128 Tasten + 8 Achsen (gleicher Aufbau wie das Haupt-Gamepad der CraftConsole) und Boot-Tastatur
uint8_t const desc_pad[] = { 0x05,0x01,0x09,0x05,0xA1,0x01,0x05,0x09,0x19,0x01,0x29,0x80,0x15,0x00,0x25,0x01,
  0x75,0x01,0x95,0x80,0x81,0x02,0x05,0x01,0x16,0x00,0x80,0x26,0xFF,0x7F,0x75,0x10,0x95,0x08,
  0x09,0x30,0x09,0x31,0x09,0x32,0x09,0x33,0x09,0x34,0x09,0x35,0x09,0x36,0x09,0x37,0x81,0x02,0xC0 };
uint8_t const desc_kbd[] = { TUD_HID_REPORT_DESC_KEYBOARD() };
struct __attribute__((packed)) PadReport { uint8_t btn[16]; int16_t ax[8]; };
Adafruit_USBD_HID usbPad, usbKbd;
PadReport rPad;
hid_keyboard_report_t rKbd = { 0, 0, { 0, 0, 0, 0, 0, 0 } };

// Zustand
uint8_t phys[16] = { 0 };                // entprellter physischer Zustand (Bits, Taste 1 = Bit 0)
uint8_t rawLast[16] = { 0 }; unsigned long rawSince[MAIN_BTNS] = { 0 };
int16_t physAxis[8] = { 0 };
int16_t outAxis[16] = { 0 }; uint8_t outBtn[16] = { 0 };
#define OUT_KEY_MAX 6
char outKeys[OUT_KEY_MAX][12]; uint8_t outKeyCount = 0;
bool serverActive = false; unsigned long lastValidOut = 0; uint8_t goodOutStreak = 0;
bool learnMode = false, monEnabled = false; uint8_t learnPrev[16] = { 0 };
unsigned long lastFrame = 0, lastMon = 0, lastBootsel = 0, beepUntil = 0;
bool bootselDown = false;
char cmdBuf[1100]; uint16_t cmdLen = 0;  // <OUT> ist bis ~900 Zeichen lang
char frameBuf[640];
unsigned long maxLoopUs = 0;
const char* unitName = "none";          // erkannte CraftUnit (GETALL unit=, DIAG); später z. B. "directus"

inline void setBit(uint8_t* a, uint16_t n, bool on) { uint16_t i = n - 1; if (i >= MAIN_BTNS) return; if (on) a[i >> 3] |= (1 << (i & 7)); else a[i >> 3] &= ~(1 << (i & 7)); }
inline bool getBit(const uint8_t* a, uint16_t n) { uint16_t i = n - 1; return i < MAIN_BTNS && (a[i >> 3] & (1 << (i & 7))); }

// ---- Tastenname → HID-Keycode (dieselben Namen wie die CraftConsole, s. Schnittstelle §6) ----
#define HID_MOD_LCTRL 0x01
#define HID_MOD_LSHIFT 0x02
#define HID_MOD_LALT 0x04
#define HID_MOD_RCTRL 0x10
#define HID_MOD_RSHIFT 0x20
#define HID_MOD_RALT 0x40
#define HID_MOD_LGUI 0x08
#define HID_MOD_RGUI 0x80
uint8_t keycodeFor(const char* n, uint8_t* mod) {
  if (!strcmp(n, "LSHIFT")) { *mod |= HID_MOD_LSHIFT; return 0; }
  if (!strcmp(n, "RSHIFT")) { *mod |= HID_MOD_RSHIFT; return 0; }
  if (!strcmp(n, "LCTRL")) { *mod |= HID_MOD_LCTRL; return 0; }
  if (!strcmp(n, "RCTRL")) { *mod |= HID_MOD_RCTRL; return 0; }
  if (!strcmp(n, "LALT")) { *mod |= HID_MOD_LALT; return 0; }
  if (!strcmp(n, "RALT")) { *mod |= HID_MOD_RALT; return 0; }
  if (!strcmp(n, "LGUI")) { *mod |= HID_MOD_LGUI; return 0; }   // Windows-Tasten (1.1)
  if (!strcmp(n, "RGUI")) { *mod |= HID_MOD_RGUI; return 0; }
  if (strlen(n) == 1) {
    char c = n[0];
    if (c >= 'A' && c <= 'Z') return 0x04 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 0x04 + (c - 'a');
    if (c >= '1' && c <= '9') return 0x1E + (c - '1');
    if (c == '0') return 0x27;
  }
  if (n[0] == 'F' && strlen(n) <= 3) { int f = atoi(n + 1); if (f >= 1 && f <= 12) return 0x3A + (f - 1); if (f >= 13 && f <= 24) return 0x68 + (f - 13); }
  if (!strncmp(n, "KP", 2) && strlen(n) == 3) { char d = n[2]; if (d >= '1' && d <= '9') return 0x59 + (d - '1'); if (d == '0') return 0x62; }
  static const struct { const char* name; uint8_t code; } T[] = {
    { "SPACE", 0x2C }, { "ENTER", 0x28 }, { "ESC", 0x29 }, { "TAB", 0x2B }, { "BACKSPACE", 0x2A },
    { "UP", 0x52 }, { "DOWN", 0x51 }, { "LEFT", 0x50 }, { "RIGHT", 0x4F },
    { "MINUS", 0x2D }, { "EQUALS", 0x2E }, { "LBRACKET", 0x2F }, { "RBRACKET", 0x30 }, { "BACKSLASH", 0x31 },
    { "SEMICOLON", 0x33 }, { "QUOTE", 0x34 }, { "GRAVE", 0x35 }, { "COMMA", 0x36 }, { "PERIOD", 0x37 }, { "SLASH", 0x38 },
    { "PRINTSCREEN", 0x46 }, { "SCROLLLOCK", 0x47 }, { "PAUSE", 0x48 }, { "INSERT", 0x49 }, { "HOME", 0x4A },
    { "PAGEUP", 0x4B }, { "DELETE", 0x4C }, { "END", 0x4D }, { "PAGEDOWN", 0x4E },
    { "KPDIV", 0x54 }, { "KPMUL", 0x55 }, { "KPMINUS", 0x56 }, { "KPPLUS", 0x57 }, { "KPDOT", 0x63 }, { "LESS", 0x64 },
    // 1.1 (24.10.2026): restliche Tasten einer vollen Tastatur (ControlHub-Bildschirmtastatur)
    { "CAPSLOCK", 0x39 }, { "NUMLOCK", 0x53 }, { "KPENTER", 0x58 }, { "MENU", 0x65 }
  };
  for (auto& e : T) if (!strcmp(n, e.name)) return e.code;
  return 0;   // unbekannt: ignorieren
}

// ---- <OUT> von ControlHub (Vollzustand) ----
bool parseOut(char* s) {
  char* body = s + 5;
  char* end = strstr(body, "</OUT>");
  if (!end) return false;
  *end = 0;
  int16_t nAxis[16]; uint8_t nBtn[16] = { 0 };
  char nKeys[OUT_KEY_MAX][12]; uint8_t nKeyCount = 0;
  bool gotAxes = false, gotBtns = false;
  char *so, *si;
  for (char* tok = strtok_r(body, " ", &so); tok; tok = strtok_r(NULL, " ", &so)) {
    if (!strncmp(tok, "GP:AXES:", 8)) {
      int i = 0;
      for (char* v = strtok_r(tok + 8, ",", &si); v && i < 16; v = strtok_r(NULL, ",", &si)) nAxis[i++] = (int16_t)(atol(v) - 32768);
      gotAxes = (i == 16);
    } else if (!strncmp(tok, "GP:BUTTONS:", 11)) {
      int i = 0;
      for (char* v = strtok_r(tok + 11, ",", &si); v && i < MAIN_BTNS; v = strtok_r(NULL, ",", &si)) { setBit(nBtn, i + 1, atoi(v) != 0); i++; }
      gotBtns = (i == MAIN_BTNS);
    } else if (!strncmp(tok, "K:", 2) && tok[2]) {
      for (char* v = strtok_r(tok + 2, ",", &si); v && nKeyCount < OUT_KEY_MAX; v = strtok_r(NULL, ",", &si)) {
        strncpy(nKeys[nKeyCount], v, 11); nKeys[nKeyCount][11] = 0; nKeyCount++;
      }
    }
  }
  if (!gotAxes || !gotBtns) return false;   // unvollständig: verwerfen
  memcpy(outAxis, nAxis, sizeof(outAxis)); memcpy(outBtn, nBtn, sizeof(outBtn));
  memcpy(outKeys, nKeys, sizeof(outKeys)); outKeyCount = nKeyCount;
  return true;
}

void emitAll() {
  Serial.print("firmware="); Serial.println(FW_VERSION);
  Serial.print("device="); Serial.println(DEVICE_ID);
  Serial.print("buttons="); Serial.println(BUTTON_COUNT + (BOOTSEL_BUTTON ? 1 : 0));   // eigene Taster + BOOTSEL (ohne CraftUnit)
  Serial.print("axes="); Serial.println(POT_ENABLED ? 1 : 0);
  Serial.print("unit="); Serial.println(unitName);                                   // CraftUnit am Anschluss (bisher immer "none")
  Serial.println("END");
}

void emitStatus() {
  Serial.print("DET serverActive "); Serial.println(serverActive ? 1 : 0);
  Serial.print("DET goodOutStreak "); Serial.println(goodOutStreak);
  Serial.print("DET maxLoopUs "); Serial.println(maxLoopUs);
  maxLoopUs = 0;
}

void processCmd(char* s) {
  if (!strncmp(s, "<OUT>", 5)) {
    if (parseOut(s)) { lastValidOut = millis(); if (goodOutStreak < 255) goodOutStreak++; if (!serverActive && goodOutStreak >= 3) serverActive = true; }
    return;
  }
  if (!strcmp(s, "PING")) { Serial.println("PONG"); return; }
  if (!strcmp(s, "VER")) { Serial.print("firmware="); Serial.println(FW_VERSION); Serial.print("device="); Serial.println(DEVICE_ID); return; }
  if (!strcmp(s, "GETALL")) { emitAll(); return; }
  if (!strcmp(s, "STATUS")) { emitStatus(); return; }
  if (!strcmp(s, "DIAG")) {
    Serial.print("DIAG CraftLink FW "); Serial.print(FW_VERSION); Serial.print(" device="); Serial.println(DEVICE_ID);
    Serial.print("DIAG Tasten "); Serial.print(BUTTON_COUNT + (BOOTSEL_BUTTON ? 1 : 0)); Serial.print(", ControlHub "); Serial.println(serverActive ? "verbunden" : "nicht verbunden");
    Serial.print("DIAG CraftUnit "); Serial.println(unitName);
    Serial.println("DIAGEND"); return;
  }
  if (!strncmp(s, "LEARN", 5)) { const char* p = s + 5; while (*p == ' ') p++; learnMode = !strncmp(p, "ON", 2); memcpy(learnPrev, phys, sizeof(learnPrev)); Serial.print("LEARN="); Serial.println(learnMode ? "on" : "off"); return; }
  if (!strcmp(s, "MON")) { monEnabled = !monEnabled; Serial.print("MON="); Serial.println(monEnabled ? "on" : "off"); return; }
  if (!strncmp(s, "BEEP", 4)) {   // GPS-Signalton von ControlHub: LED flackert so lange, mit Piezo (PIEZO_ENABLED) auch Ton
    const char* p = s + 4; while (*p == ' ') p++; long ms = *p ? atol(p) : 150; if (ms < 1) ms = 1; if (ms > 5000) ms = 5000;
#if PIEZO_ENABLED
    tone(PIEZO_PIN, 2700, ms);
#endif
    beepUntil = millis() + ms; Serial.print("OK BEEP "); Serial.println(ms); return;
  }
  if (!strncmp(s, "SET ", 4)) { Serial.print("ERR key "); Serial.println(s + 4); return; }   // Einstellungen kommen mit der CraftUnit
  if (!strncmp(s, "CAL ", 4)) { Serial.println("ERR CAL"); return; }
  Serial.println("ERR");
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { if (cmdLen) { cmdBuf[cmdLen] = 0; processCmd(cmdBuf); cmdLen = 0; } }
    else if (cmdLen < sizeof(cmdBuf) - 1) cmdBuf[cmdLen++] = c;
    else cmdLen = 0;   // Überlänge: Zeile verwerfen
  }
}

// ================= CraftUnit (Anschluss vorbereitet, Auslesen später) =================
// Hendrik 23.10.2026: erst das Gerüst, das Auslesen kommt mit Testhardware (CraftCoreMini-Platine oder CraftLink-Platine;
// ein nackter Pico reicht nicht: es fehlen Stecker und die I2C-Pull-ups 3,3 kΩ). Plan, aus CraftConsolePro_Pico.ino (FW 2.12)
// übernehmen:
//  - unitBegin():  Wire (GP4/5) und Wire1 (GP6/7) starten, Aux-Pins GP16–20 als INPUT_PULLUP, analogReadResolution(12).
//  - unitDetect(): auf BEIDEN Bussen nach MCP23017 0x20 (Griff Directus/Mortis) und AS5600 0x36/0x40 (Hebel) suchen --
//                  Doku sagt Griff an Kanal B, FW 2.12 liest Kanal A (scanSlots()); unitName = erkannter Aufbau.
//  - unitRead():   Griff 16 Tasten (mapMcp + mapDirectus) → Tasten 1–16; Hebel (readAS5600/shortestDelta, leverAxis) →
//                  Achsen 2/3 oder Tasten 17–20; Thumbstick über den Pico-ADC GP26/27 (statt ADS1115 0x49, andere Skala →
//                  eigene Kalibrier-Standardwerte) → Achsen 0/1 oder Tasten 21–24; Aux D1–D5 → Tasten 25–29.
//                  Nummern und Achsen wie CraftSlot 1, damit Ebenen und die Modulbilder in ControlHub passen.
//  - danach SET/GETALL/CAL mit denselben Schlüsseln wie CraftSlot 1 der CraftConsole (thumb…, levera…, CAL thumb/leverA/leverB),
//    Speicher im Flash (EEPROM wie FW 2.12).
void unitBegin() {}                      // bisher nichts -- die reservierten Pins bleiben unbenutzt (Eingang, hochohmig)
bool unitDetect() { return false; }
void unitRead(uint8_t* raw) { (void)raw; }   // füllt später raw-Bits 1–29 und physAxis[0..3]

// ---- Eingänge lesen (entprellt) ----
void readInputs() {
  unsigned long now = millis();
  uint8_t raw[16] = { 0 };
  unitRead(raw);
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) setBit(raw, BUTTON_FIRST + i, digitalRead(BUTTON_PINS[i]) == LOW);
  // BOOTSEL nur alle 50 ms lesen: das Lesen hält kurz den Flash-Zugriff an
  if (BOOTSEL_BUTTON && now - lastBootsel >= 50) { lastBootsel = now; bootselDown = BOOTSEL; }
  if (BOOTSEL_BUTTON) setBit(raw, BOOTSEL_BUTTON, bootselDown);
  for (uint16_t b = 1; b <= MAIN_BTNS; b++) {
    bool r = getBit(raw, b);
    if (r != getBit(rawLast, b)) { setBit(rawLast, b, r); rawSince[b - 1] = now; }
    else if (r != getBit(phys, b) && now - rawSince[b - 1] >= DEBOUNCE_MS) setBit(phys, b, r);
  }
#if POT_ENABLED
  int v = analogRead(POT_PIN) - 2048;   // 12 Bit, Mitte 2048
  if (v > -AXIS_DEADZONE && v < AXIS_DEADZONE) v = 0;
  long o = (long)v * 32767 / 2047; if (o > 32767) o = 32767; if (o < -32767) o = -32767;
  physAxis[POT_AXIS] = (int16_t)o;
#endif
}

// ---- <FRAME> an ControlHub (Vollzustand, nicht blockierend) ----
void sendFrame() {
  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();
  int n = snprintf(frameBuf, sizeof(frameBuf), "<FRAME>B:");
  for (int g = 1; g <= MAIN_BTNS; g++) n += snprintf(frameBuf + n, sizeof(frameBuf) - n, g == 1 ? "%d" : " %d", getBit(phys, g) ? 1 : 0);
  n += snprintf(frameBuf + n, sizeof(frameBuf) - n, " A:");
  for (int i = 0; i < 16; i++) {
    uint16_t u = (uint16_t)((int32_t)(i < 8 ? physAxis[i] : 0) + 32768);
    n += snprintf(frameBuf + n, sizeof(frameBuf) - n, i == 0 ? "%u" : " %u", u);
  }
  n += snprintf(frameBuf + n, sizeof(frameBuf) - n, "</FRAME>\n");
  if (n <= 0 || n >= (int)sizeof(frameBuf)) return;
  // Nur ganz oder gar nicht schreiben: sonst wartet Serial.write() auf Platz und hält die Schleife an
  if (Serial.availableForWrite() >= n) Serial.write((const uint8_t*)frameBuf, n);
}

// ---- HID-Berichte: von ControlHub (verbunden) oder 1:1 aus den Tastern (eigener Betrieb) ----
void updateReports() {
  if (serverActive && millis() - lastValidOut > SERVER_TIMEOUT_MS) { serverActive = false; goodOutStreak = 0; }
  if (serverActive) {
    memcpy(rPad.btn, outBtn, sizeof(rPad.btn));
    for (int i = 0; i < 8; i++) rPad.ax[i] = outAxis[i];   // Achsen 9–16 (zweites Gerät der CraftConsole) gibt es hier nicht
    rKbd.modifier = 0; memset(rKbd.keycode, 0, sizeof(rKbd.keycode));
    uint8_t slot = 0;
    for (uint8_t k = 0; k < outKeyCount && slot < 6; k++) { uint8_t code = keycodeFor(outKeys[k], &rKbd.modifier); if (code) rKbd.keycode[slot++] = code; }
  } else {
    memcpy(rPad.btn, phys, sizeof(rPad.btn));
    for (int i = 0; i < 8; i++) rPad.ax[i] = physAxis[i];
    rKbd.modifier = 0; memset(rKbd.keycode, 0, sizeof(rKbd.keycode));
  }
  // Je Runde nur EIN HID-Bericht, reihum -- zwei Berichte an verschiedene Interfaces direkt hintereinander bringen den
  // TinyUSB-Stack ins Stolpern (bei der CraftConsole gefunden, „busy endpoint“)
  static uint8_t turn = 0;
  if (turn == 0) { if (usbPad.ready()) usbPad.sendReport(0, &rPad, sizeof(rPad)); }
  else { if (usbKbd.ready()) usbKbd.sendReport(0, &rKbd, sizeof(rKbd)); }
  turn ^= 1;
}

void updateLed() {
#ifdef LED_BUILTIN
  unsigned long t = millis();
  bool on;
  if (t < beepUntil) on = (t / 60) % 2;              // Signalton von ControlHub: schnelles Flackern
  else if (serverActive) on = true;                   // mit ControlHub verbunden: dauerhaft an
  else on = (t / 500) % 2;                            // ohne ControlHub: langsames Blinken
  static bool last = false;
  if (on != last) { digitalWrite(LED_BUILTIN, on ? HIGH : LOW); last = on; }
#endif
}

void setup() {
  // USB zuerst: HID-Interfaces registrieren, bevor sich der Pico als reines Serielles Gerät meldet
  TinyUSBDevice.setID(USB_VID, USB_PID);
  TinyUSBDevice.setManufacturerDescriptor(NAME_MANUF);
  TinyUSBDevice.setProductDescriptor(NAME_PROD);
  usbPad.setPollInterval(2); usbPad.setReportDescriptor(desc_pad, sizeof(desc_pad)); usbPad.setStringDescriptor(NAME_PROD); usbPad.begin();
  usbKbd.setPollInterval(2); usbKbd.setReportDescriptor(desc_kbd, sizeof(desc_kbd)); usbKbd.setStringDescriptor(NAME_KBD); usbKbd.begin();
  if (TinyUSBDevice.mounted()) { TinyUSBDevice.detach(); delay(10); TinyUSBDevice.attach(); }
  Serial.begin(115200);

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  analogReadResolution(12);
  unitBegin(); unitDetect();
#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
#endif
  memset(&rPad, 0, sizeof(rPad));
  { unsigned long t0 = millis(); while (!TinyUSBDevice.mounted() && millis() - t0 < 2000) delay(1); }
  Serial.println("CraftLink bereit.");
  rp2040.wdt_begin(4000);   // hängt die Schleife länger als 4 s, startet der Pico neu
}

void loop() {
  rp2040.wdt_reset();
  unsigned long t0 = micros();
  handleSerial();
  readInputs();
  if (learnMode) {
    for (int g = 1; g <= MAIN_BTNS; g++) if (getBit(phys, g) && !getBit(learnPrev, g)) { Serial.print("LRN "); Serial.println(g); }
    memcpy(learnPrev, phys, sizeof(learnPrev));
  }
  sendFrame();
  updateReports();
  updateLed();
  if (monEnabled && millis() - lastMon > 150) {
    lastMon = millis(); Serial.print("Btn:");
    for (int g = 1; g <= MAIN_BTNS; g++) if (rPad.btn[(g - 1) >> 3] & (1 << ((g - 1) & 7))) { Serial.print(' '); Serial.print(g); }
    Serial.print("  ControlHub="); Serial.println(serverActive ? "ja" : "nein");
  }
  unsigned long dt = micros() - t0; if (dt > maxLoopUs) maxLoopUs = dt;
  delay(2);
}
