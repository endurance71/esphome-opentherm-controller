/*
  OpenThermScanner – full read-out of everything the boiler exposes over OpenTherm
  ESP32-WROOM + OpenTherm adapter (Ihor Melnyk's circuit)

  What it does:
   1. Turns central heating OFF for the scan (DHW stays on, hot water at the tap keeps working).
   2. Reads every OpenTherm data ID 1–255 and records the boiler's answer:
      READ-ACK (supported), UNKNOWN-DATAID (not known), DATA-INVALID (known, but no data), no answer.
   3. Checks which values the boiler lets you WRITE – safe tests only
      (flow setpoint with CH off, writing back the same DHW and max CH values, max modulation 100%).
   4. Reads the manufacturer parameters (TSP) and fault history (FHB), if the boiler exposes them.
   5. After the scan it STOPS talking to the boiler. Then disconnect the adapter and reconnect the old control.

  Results: Serial Monitor (115200) and the page http://<ESP32 IP>/ on your Wi-Fi.

  Library: "OpenTherm Library" by Ihor Melnyk (Arduino Library Manager), version 1.1.5 or newer.
  Board: "ESP32 Dev Module".
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <OpenTherm.h>

// ---------- SETTINGS ----------
const char *WIFI_SSID = "YOUR_WIFI";
const char *WIFI_PASS = "YOUR_PASSWORD";
const int OT_IN_PIN = 21;   // GPIO21 <- adapter RX (U2)
const int OT_OUT_PIN = 22;  // GPIO22 -> adapter TX (U1)
// --------------------------------

OpenTherm ot(OT_IN_PIN, OT_OUT_PIN);
WebServer server(80);

enum Res : uint8_t { R_NONE = 0, R_ACK, R_UNKNOWN, R_INVALID, R_TIMEOUT, R_BAD };

struct Probe {
  Res res;
  uint16_t value;
};

Probe readRes[256];

struct WriteTest {
  uint8_t id;
  const char *what;
  uint16_t sent;
  Res res;
  uint16_t value;
  bool done;
  bool skipped;
};

WriteTest writes[] = {
  {1,   "CH flow setpoint (TSet) = 30.0 °C, with CH off", 0, R_NONE, 0, false, false},
  {56,  "DHW setpoint – write back the value read",            0, R_NONE, 0, false, false},
  {57,  "Max CH setpoint – write back the value read", 0, R_NONE, 0, false, false},
  {14,  "Max burner modulation = 100 %",                       0, R_NONE, 0, false, false},
  {16,  "Room setpoint (informative) = 20.0 °C",               0, R_NONE, 0, false, false},
  {24,  "Room temperature (informative) = 21.0 °C",            0, R_NONE, 0, false, false},
  {2,   "Master configuration / MemberID = 0",                 0, R_NONE, 0, false, false},
  {124, "Master OpenTherm version = 2.2",                      0, R_NONE, 0, false, false},
  {126, "Master product type / version = 0 / 1",               0, R_NONE, 0, false, false},
};
const int WRITE_COUNT = sizeof(writes) / sizeof(writes[0]);

uint16_t tspValues[256];
int tspCount = -1;   // -1 = unsupported / unknown
uint16_t fhbValues[256];
int fhbCount = -1;

uint16_t statusResponse = 0;
Res statusRes = R_NONE;

enum Phase : uint8_t { PH_WAIT, PH_READ, PH_WRITE, PH_TSP, PH_FHB, PH_DONE };
Phase phase = PH_WAIT;
int cursor = 1;
int sinceStatus = 0;
unsigned long bootMs = 0;
String report;  // finished text report

// ---------- data ID descriptions ----------
const char *idName(uint8_t id) {
  switch (id) {
    case 0:   return "Master/slave status (flags)";
    case 1:   return "CH flow setpoint (TSet)";
    case 2:   return "Master configuration / MemberID";
    case 3:   return "Boiler configuration / MemberID";
    case 4:   return "Remote command (e.g. reset)";
    case 5:   return "Fault flags + OEM fault code";
    case 6:   return "Remote parameters: access";
    case 7:   return "Cooling control signal";
    case 8:   return "CH2 flow setpoint";
    case 9:   return "Remote room setpoint override";
    case 10:  return "Number of manufacturer parameters (TSP)";
    case 11:  return "Manufacturer parameter (TSP) index/value";
    case 12:  return "Fault history size (FHB)";
    case 13:  return "Fault history (FHB) index/value";
    case 14:  return "Max burner modulation (setting)";
    case 15:  return "Max boiler capacity kW / min modulation %";
    case 16:  return "Room setpoint (from thermostat)";
    case 17:  return "Current burner modulation %";
    case 18:  return "CH water pressure (bar)";
    case 19:  return "DHW flow rate (l/min)";
    case 20:  return "Day of week and time";
    case 21:  return "Date";
    case 22:  return "Year";
    case 23:  return "Room setpoint circuit 2";
    case 24:  return "Room temperature (from thermostat)";
    case 25:  return "Flow temperature (boiler)";
    case 26:  return "DHW temperature";
    case 27:  return "Outdoor temperature";
    case 28:  return "Return temperature";
    case 29:  return "Solar storage temperature";
    case 30:  return "Solar collector temperature";
    case 31:  return "Flow temperature circuit 2";
    case 32:  return "DHW temperature 2";
    case 33:  return "Flue gas temperature";
    case 34:  return "Boiler heat exchanger temperature";
    case 35:  return "Fan speed setpoint/actual";
    case 36:  return "Flame current (ionisation) uA";
    case 37:  return "Room temperature circuit 2";
    case 38:  return "Relative humidity";
    case 48:  return "DHW setpoint range (max/min)";
    case 49:  return "Max CH setpoint range (max/min)";
    case 50:  return "Heating curve ratio range";
    case 56:  return "DHW setpoint";
    case 57:  return "Max CH flow setpoint";
    case 58:  return "Heating curve ratio";
    case 70:  return "Ventilation: status";
    case 71:  return "Ventilation: setpoint";
    case 99:  return "CH/DHW operating mode (OT 4.x)";
    case 100: return "Remote override: functions";
    case 101: return "Solar status";
    case 102: return "Solar: fault flags";
    case 113: return "Unsuccessful burner starts";
    case 114: return "Flame signal too low count";
    case 115: return "OEM diagnostic code";
    case 116: return "Burner starts";
    case 117: return "CH pump starts";
    case 118: return "DHW pump/valve starts";
    case 119: return "DHW burner starts";
    case 120: return "Burner operation hours";
    case 121: return "CH pump operation hours";
    case 122: return "DHW pump/valve operation hours";
    case 123: return "DHW burner operation hours";
    case 124: return "Master OpenTherm version";
    case 125: return "Boiler OpenTherm version";
    case 126: return "Master product type/version";
    case 127: return "Boiler product type/version";
    default:
      if (id >= 128) return "Manufacturer-specific (OEM)";
      return "Other / rarely used";
  }
}

enum Fmt : uint8_t { F_F88, F_U16, F_S16, F_U8U8, F_S8S8, F_FLAGS };

Fmt idFormat(uint8_t id) {
  switch (id) {
    case 1: case 7: case 8: case 9: case 14: case 16: case 17: case 18: case 19:
    case 23: case 24: case 25: case 26: case 27: case 28: case 29: case 31:
    case 32: case 34: case 36: case 37: case 38: case 56: case 57: case 58:
    case 124: case 125:
      return F_F88;
    case 30: case 33:
      return F_S16;
    case 48: case 49: case 50:
      return F_S8S8;
    case 0: case 3: case 5: case 6: case 70: case 100: case 101: case 102:
      return F_FLAGS;
    case 113: case 114: case 115: case 116: case 117: case 118: case 119:
    case 120: case 121: case 122: case 123: case 22:
      return F_U16;
    default:
      return F_U8U8;
  }
}

String hex4(uint16_t v) {
  char b[8];
  snprintf(b, sizeof(b), "%04X", v);
  return String(b);
}

String bin8(uint8_t v) {
  String s;
  for (int i = 7; i >= 0; i--) s += ((v >> i) & 1) ? '1' : '0';
  return s;
}

String decode(uint8_t id, uint16_t v) {
  uint8_t hi = v >> 8, lo = v & 0xFF;
  switch (idFormat(id)) {
    case F_F88:  return String((int16_t)v / 256.0f, 2);
    case F_S16:  return String((int16_t)v);
    case F_U16:  return String(v);
    case F_S8S8: return String((int8_t)hi) + " / " + String((int8_t)lo);
    case F_FLAGS: return bin8(hi) + " / " + bin8(lo) + " (" + String(lo) + ")";
    default:     return String(hi) + " / " + String(lo);
  }
}

const char *resName(Res r) {
  switch (r) {
    case R_ACK:     return "OK";
    case R_UNKNOWN: return "unknown";
    case R_INVALID: return "invalid";
    case R_TIMEOUT: return "no answer";
    case R_BAD:     return "bad frame";
    default:        return "-";
  }
}

// ---------- communication ----------
Res classify(unsigned long resp, uint16_t &value) {
  OpenThermResponseStatus st = ot.getLastResponseStatus();
  if (st == OpenThermResponseStatus::TIMEOUT) return R_TIMEOUT;
  if (st != OpenThermResponseStatus::SUCCESS) return R_BAD;
  value = resp & 0xFFFF;
  OpenThermMessageType t = OpenTherm::getMessageType(resp);
  if (t == OpenThermMessageType::READ_ACK || t == OpenThermMessageType::WRITE_ACK) return R_ACK;
  if (t == OpenThermMessageType::UNKNOWN_DATA_ID) return R_UNKNOWN;
  if (t == OpenThermMessageType::DATA_INVALID) return R_INVALID;
  return R_BAD;
}

Res request(OpenThermMessageType type, uint8_t id, uint16_t data, uint16_t &value) {
  unsigned long req = OpenTherm::buildRequest(type, (OpenThermMessageID)id, data);
  unsigned long resp = ot.sendRequest(req);
  Res r = classify(resp, value);
  delay(120);  // required gap between frames
  return r;
}

void keepStatus() {
  // CH off, DHW on – the boiler does not heat the radiators during the scan
  unsigned long resp = ot.setBoilerStatus(false, true, false, false, false);
  uint16_t v = 0;
  statusRes = classify(resp, v);
  if (statusRes == R_ACK) statusResponse = v;
  delay(120);
  sinceStatus = 0;
}

uint16_t f88(float t) { return (uint16_t)(int16_t)(t * 256.0f); }

// ---------- report ----------
void buildReport() {
  String s;
  s += "=== OpenTherm boiler scan ===\n";
  s += "Status (ID0): " + String(resName(statusRes)) + "  boiler flags: " + bin8(statusResponse & 0xFF) + "\n";
  s += "  bit0 fault=" + String(statusResponse & 1) + " bit1 CH=" + String((statusResponse >> 1) & 1) +
       " bit2 DHW=" + String((statusResponse >> 2) & 1) + " bit3 flame=" + String((statusResponse >> 3) & 1) +
       " bit6 diagnostic=" + String((statusResponse >> 6) & 1) + "\n\n";
  s += "--- Read (supported and 'invalid' only) ---\n";
  s += "ID  | result      | HEX  | value   | description\n";
  int ok = 0, unk = 0, inv = 0, to = 0;
  for (int id = 1; id < 256; id++) {
    Probe &p = readRes[id];
    if (p.res == R_ACK) ok++;
    else if (p.res == R_UNKNOWN) unk++;
    else if (p.res == R_INVALID) inv++;
    else if (p.res == R_TIMEOUT || p.res == R_BAD) to++;
    if (p.res == R_ACK || p.res == R_INVALID) {
      char line[160];
      snprintf(line, sizeof(line), "%-3d | %-11s | %s | %-7s | %s\n", id, resName(p.res), hex4(p.value).c_str(),
               p.res == R_ACK ? decode(id, p.value).c_str() : "-", idName(id));
      s += line;
    }
  }
  s += "\nSummary: OK=" + String(ok) + "  invalid=" + String(inv) + "  unknown=" + String(unk) +
       "  no answer=" + String(to) + "\n\n";
  s += "--- Write test ---\n";
  for (int i = 0; i < WRITE_COUNT; i++) {
    WriteTest &w = writes[i];
    s += "ID" + String(w.id) + ": " + (w.skipped ? String("skipped (no read value)") : String(resName(w.res))) +
         "  sent " + hex4(w.sent) + "  | " + w.what + "\n";
  }
  s += "\n--- Manufacturer parameters (TSP) ---\n";
  if (tspCount < 0) s += "unsupported\n";
  else for (int i = 0; i < tspCount; i++) s += "TSP[" + String(i) + "] = " + String(tspValues[i] & 0xFF) + "\n";
  s += "\n--- Fault history (FHB) ---\n";
  if (fhbCount < 0) s += "unsupported\n";
  else for (int i = 0; i < fhbCount; i++) s += "FHB[" + String(i) + "] = " + String(fhbValues[i] & 0xFF) + "\n";
  s += "\nScan finished. The ESP32 no longer talks to the boiler: disconnect the adapter and reconnect the old control.\n";
  report = s;
}

void handleRoot() {
  String h;
  h.reserve(report.length() + 2000);
  h += F("<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>OpenTherm scan</title><style>body{font-family:system-ui;margin:16px;background:#f4f6f5;color:#17201c}"
         "pre{background:#fff;border:1px solid #ccd;padding:12px;overflow-x:auto;font-size:13px}"
         "button{padding:10px 14px;font-size:15px}</style></head><body><h1>OpenTherm boiler scan</h1>");
  if (phase != PH_DONE) {
    h += F("<p>Scan in progress... (refreshes automatically) – phase: ");
    h += String((int)phase) + " / ID " + String(cursor) + "</p><meta http-equiv='refresh' content='5'>";
  } else {
    h += F("<p>Done. Copy the report and save it (or attach it to an issue).</p>"
           "<button onclick=\"navigator.clipboard.writeText(document.getElementById('r').innerText)\">Copy report</button>");
  }
  h += F("<pre id='r'>");
  String esc = report;
  esc.replace("&", "&amp;");
  esc.replace("<", "&lt;");
  h += esc;
  h += F("</pre></body></html>");
  server.send(200, "text/html; charset=utf-8", h);
}

// ---------- start ----------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nOpenThermScanner start");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi OK, open http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("No Wi-Fi – results in Serial Monitor only");
  }
  server.on("/", handleRoot);
  server.begin();

  ot.begin();
  report = "Scan starts in a few seconds...\n";
  bootMs = millis();
}

void loop() {
  server.handleClient();

  switch (phase) {
    case PH_WAIT:
      if (millis() - bootMs > 3000) {
        keepStatus();
        Serial.print("ID0 status: ");
        Serial.println(resName(statusRes));
        phase = PH_READ;
        cursor = 1;
      }
      break;

    case PH_READ: {
      uint16_t v = 0;
      Res r = request(OpenThermMessageType::READ_DATA, (uint8_t)cursor, 0, v);
      readRes[cursor].res = r;
      readRes[cursor].value = v;
      if (r == R_ACK || r == R_INVALID) {
        Serial.printf("ID%-3d %-11s %s  %s\n", cursor, resName(r), hex4(v).c_str(), idName(cursor));
      }
      if (++sinceStatus >= 4) keepStatus();
      if (++cursor > 255) {
        phase = PH_WRITE;
        cursor = 0;
      }
      if (cursor % 16 == 0) {
        report = "Reading: ID " + String(cursor) + "/255\n";
      }
      break;
    }

    case PH_WRITE: {
      WriteTest &w = writes[cursor];
      uint16_t data = 0;
      bool skip = false;
      switch (w.id) {
        case 1:   data = f88(30.0f); break;
        case 56:  skip = readRes[56].res != R_ACK; data = readRes[56].value; break;
        case 57:  skip = readRes[57].res != R_ACK; data = readRes[57].value; break;
        case 14:  data = f88(100.0f); break;
        case 16:  data = f88(20.0f); break;
        case 24:  data = f88(21.0f); break;
        case 2:   data = 0; break;
        case 124: data = f88(2.2f); break;
        case 126: data = 0x0001; break;
      }
      w.sent = data;
      w.skipped = skip;
      if (!skip) {
        uint16_t v = 0;
        w.res = request(OpenThermMessageType::WRITE_DATA, w.id, data, v);
        w.value = v;
        Serial.printf("WRITE ID%-3d %s\n", w.id, resName(w.res));
      }
      w.done = true;
      keepStatus();
      if (++cursor >= WRITE_COUNT) {
        phase = PH_TSP;
        cursor = 0;
      }
      break;
    }

    case PH_TSP: {
      if (cursor == 0) {
        tspCount = (readRes[10].res == R_ACK) ? (readRes[10].value >> 8) : -1;
        if (tspCount <= 0) {
          if (tspCount == 0) tspCount = 0;
          phase = PH_FHB;
          cursor = 0;
          break;
        }
      }
      uint16_t v = 0;
      Res r = request(OpenThermMessageType::READ_DATA, 11, (uint16_t)(cursor << 8), v);
      tspValues[cursor] = (r == R_ACK) ? v : 0xFFFF;
      if (++sinceStatus >= 4) keepStatus();
      if (++cursor >= tspCount) {
        phase = PH_FHB;
        cursor = 0;
      }
      break;
    }

    case PH_FHB: {
      if (cursor == 0) {
        fhbCount = (readRes[12].res == R_ACK) ? (readRes[12].value >> 8) : -1;
        if (fhbCount <= 0) {
          if (fhbCount == 0) fhbCount = 0;
          phase = PH_DONE;
          buildReport();
          Serial.println();
          Serial.println(report);
          break;
        }
      }
      uint16_t v = 0;
      Res r = request(OpenThermMessageType::READ_DATA, 13, (uint16_t)(cursor << 8), v);
      fhbValues[cursor] = (r == R_ACK) ? v : 0xFFFF;
      if (++sinceStatus >= 4) keepStatus();
      if (++cursor >= fhbCount) {
        phase = PH_DONE;
        buildReport();
        Serial.println();
        Serial.println(report);
      }
      break;
    }

    case PH_DONE:
      // Done: no further communication with the boiler.
      break;
  }
}
