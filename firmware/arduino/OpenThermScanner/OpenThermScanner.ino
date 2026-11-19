/*
  OpenThermScanner – pełny odczyt tego, co wystawia kocioł po OpenTherm
  ESP32-WROOM + adapter OpenTherm (układ Ihora Melnyka)

  Co robi:
   1. Wyłącza grzanie CO na czas skanu (CWU zostaje włączona, ciepła woda w kranie działa).
   2. Czyta wszystkie numery danych OpenTherm 1–255 i zapisuje odpowiedź kotła:
      READ-ACK (obsługuje), UNKNOWN-DATAID (nie zna), DATA-INVALID (zna, ale brak danych), brak odpowiedzi.
   3. Sprawdza, które wartości kocioł pozwala ZAPISAĆ – tylko bezpieczne testy
      (temperatura zasilania przy wyłączonym CO, wpisanie z powrotem tych samych wartości CWU i max CO, moc 100%).
   4. Czyta parametry producenta (TSP) i historię błędów (FHB), jeśli kocioł je udostępnia.
   5. Po skanie PRZESTAJE rozmawiać z kotłem. Odłącz wtedy adapter i podłącz z powrotem stare sterowanie.

  Wyniki: Serial Monitor (115200) oraz strona http://<IP ESP32>/ w sieci Wi-Fi.

  Biblioteka: "OpenTherm Library" Ihor Melnyk (Menedżer bibliotek Arduino), wersja 1.1.5 lub nowsza.
  Płytka: "ESP32 Dev Module".
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <OpenTherm.h>

// ---------- USTAWIENIA ----------
const char *WIFI_SSID = "TWOJA_SIEC";
const char *WIFI_PASS = "TWOJE_HASLO";
const int OT_IN_PIN = 21;   // GPIO21 <- RX adaptera (U2)
const int OT_OUT_PIN = 22;  // GPIO22 -> TX adaptera (U1)
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
  {1,   "Temperatura zasilania CO (TSet) = 30,0 °C, przy wylaczonym CO", 0, R_NONE, 0, false, false},
  {56,  "Temperatura CWU – wpisanie z powrotem odczytanej wartosci",    0, R_NONE, 0, false, false},
  {57,  "Max temperatura CO – wpisanie z powrotem odczytanej wartosci", 0, R_NONE, 0, false, false},
  {14,  "Max modulacja palnika = 100 %",                                0, R_NONE, 0, false, false},
  {16,  "Zadana temperatura pokoju (informacyjnie) = 20,0 °C",          0, R_NONE, 0, false, false},
  {24,  "Temperatura pokoju (informacyjnie) = 21,0 °C",                 0, R_NONE, 0, false, false},
  {2,   "Konfiguracja mastera / MemberID = 0",                          0, R_NONE, 0, false, false},
  {124, "Wersja OpenTherm mastera = 2.2",                               0, R_NONE, 0, false, false},
  {126, "Typ / wersja produktu mastera = 0 / 1",                        0, R_NONE, 0, false, false},
};
const int WRITE_COUNT = sizeof(writes) / sizeof(writes[0]);

uint16_t tspValues[256];
int tspCount = -1;   // -1 = nieobsługiwane / nieznane
uint16_t fhbValues[256];
int fhbCount = -1;

uint16_t statusResponse = 0;
Res statusRes = R_NONE;

enum Phase : uint8_t { PH_WAIT, PH_READ, PH_WRITE, PH_TSP, PH_FHB, PH_DONE };
Phase phase = PH_WAIT;
int cursor = 1;
int sinceStatus = 0;
unsigned long bootMs = 0;
String report;  // gotowy raport tekstowy

// ---------- opis numerów danych ----------
const char *idName(uint8_t id) {
  switch (id) {
    case 0:   return "Status master/slave (flagi)";
    case 1:   return "Zadana temp. zasilania CO (TSet)";
    case 2:   return "Konfiguracja mastera / MemberID";
    case 3:   return "Konfiguracja kotla / MemberID";
    case 4:   return "Zdalne polecenie (np. reset)";
    case 5:   return "Flagi bledow + kod bledu OEM";
    case 6:   return "Zdalne parametry: dostep";
    case 7:   return "Sygnal chlodzenia";
    case 8:   return "Zadana temp. CO obieg 2";
    case 9:   return "Zdalne nadpisanie temp. pokoju";
    case 10:  return "Liczba parametrow producenta (TSP)";
    case 11:  return "Parametr producenta (TSP) indeks/wartosc";
    case 12:  return "Rozmiar historii bledow (FHB)";
    case 13:  return "Historia bledow (FHB) indeks/wartosc";
    case 14:  return "Max modulacja palnika (ustawienie)";
    case 15:  return "Max moc kotla kW / min modulacja %";
    case 16:  return "Zadana temp. pokoju (od termostatu)";
    case 17:  return "Aktualna modulacja palnika %";
    case 18:  return "Cisnienie instalacji CO (bar)";
    case 19:  return "Przeplyw CWU (l/min)";
    case 20:  return "Dzien tygodnia i godzina";
    case 21:  return "Data";
    case 22:  return "Rok";
    case 23:  return "Zadana temp. pokoju obieg 2";
    case 24:  return "Temperatura pokoju (od termostatu)";
    case 25:  return "Temperatura zasilania (kociol)";
    case 26:  return "Temperatura CWU";
    case 27:  return "Temperatura zewnetrzna";
    case 28:  return "Temperatura powrotu";
    case 29:  return "Temperatura zasobnika solarnego";
    case 30:  return "Temperatura kolektora solarnego";
    case 31:  return "Temperatura zasilania obieg 2";
    case 32:  return "Temperatura CWU 2";
    case 33:  return "Temperatura spalin";
    case 34:  return "Temperatura wymiennika kotla";
    case 35:  return "Obroty wentylatora zadane/aktualne";
    case 36:  return "Prad plomienia (jonizacja) uA";
    case 37:  return "Temperatura pokoju obieg 2";
    case 38:  return "Wilgotnosc wzgledna";
    case 48:  return "Zakres temp. CWU (max/min)";
    case 49:  return "Zakres max temp. CO (max/min)";
    case 50:  return "Zakres krzywej grzewczej";
    case 56:  return "Zadana temp. CWU";
    case 57:  return "Max temp. zasilania CO";
    case 58:  return "Wspolczynnik krzywej grzewczej";
    case 70:  return "Wentylacja: status";
    case 71:  return "Wentylacja: poziom zadany";
    case 99:  return "Tryb pracy CO/CWU (OT 4.x)";
    case 100: return "Zdalne nadpisanie: funkcje";
    case 101: return "Status solarny";
    case 102: return "Solar: flagi bledow";
    case 113: return "Licznik nieudanych zaplonow";
    case 114: return "Licznik zaniku plomienia";
    case 115: return "Kod diagnostyczny OEM";
    case 116: return "Licznik uruchomien palnika";
    case 117: return "Licznik uruchomien pompy CO";
    case 118: return "Licznik uruchomien pompy/zaworu CWU";
    case 119: return "Licznik uruchomien palnika na CWU";
    case 120: return "Godziny pracy palnika";
    case 121: return "Godziny pracy pompy CO";
    case 122: return "Godziny pracy pompy/zaworu CWU";
    case 123: return "Godziny pracy palnika na CWU";
    case 124: return "Wersja OpenTherm mastera";
    case 125: return "Wersja OpenTherm kotla";
    case 126: return "Typ/wersja produktu mastera";
    case 127: return "Typ/wersja produktu kotla";
    default:
      if (id >= 128) return "Numer producenta (OEM)";
      return "Inny / rzadko uzywany";
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
    case R_UNKNOWN: return "nie zna";
    case R_INVALID: return "brak danych";
    case R_TIMEOUT: return "brak odpowiedzi";
    case R_BAD:     return "zla ramka";
    default:        return "-";
  }
}

// ---------- komunikacja ----------
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
  delay(120);  // przerwa wymagana miedzy ramkami
  return r;
}

void keepStatus() {
  // CO wylaczone, CWU wlaczona – kociol nie grzeje grzejnikow w trakcie skanu
  unsigned long resp = ot.setBoilerStatus(false, true, false, false, false);
  uint16_t v = 0;
  statusRes = classify(resp, v);
  if (statusRes == R_ACK) statusResponse = v;
  delay(120);
  sinceStatus = 0;
}

uint16_t f88(float t) { return (uint16_t)(int16_t)(t * 256.0f); }

// ---------- raport ----------
void buildReport() {
  String s;
  s += "=== OpenTherm skan kotla ===\n";
  s += "Status (ID0): " + String(resName(statusRes)) + "  flagi kotla: " + bin8(statusResponse & 0xFF) + "\n";
  s += "  bit0 blad=" + String(statusResponse & 1) + " bit1 CO=" + String((statusResponse >> 1) & 1) +
       " bit2 CWU=" + String((statusResponse >> 2) & 1) + " bit3 plomien=" + String((statusResponse >> 3) & 1) +
       " bit6 diagnostyka=" + String((statusResponse >> 6) & 1) + "\n\n";
  s += "--- Odczyt (tylko obslugiwane i 'brak danych') ---\n";
  s += "ID  | wynik       | HEX  | wartosc | opis\n";
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
  s += "\nPodsumowanie: OK=" + String(ok) + "  brak danych=" + String(inv) + "  nie zna=" + String(unk) +
       "  bez odpowiedzi=" + String(to) + "\n\n";
  s += "--- Test zapisu ---\n";
  for (int i = 0; i < WRITE_COUNT; i++) {
    WriteTest &w = writes[i];
    s += "ID" + String(w.id) + ": " + (w.skipped ? String("pominiety (brak odczytu)") : String(resName(w.res))) +
         "  wyslano " + hex4(w.sent) + "  | " + w.what + "\n";
  }
  s += "\n--- Parametry producenta (TSP) ---\n";
  if (tspCount < 0) s += "nieobslugiwane\n";
  else for (int i = 0; i < tspCount; i++) s += "TSP[" + String(i) + "] = " + String(tspValues[i] & 0xFF) + "\n";
  s += "\n--- Historia bledow (FHB) ---\n";
  if (fhbCount < 0) s += "nieobslugiwane\n";
  else for (int i = 0; i < fhbCount; i++) s += "FHB[" + String(i) + "] = " + String(fhbValues[i] & 0xFF) + "\n";
  s += "\nSkan zakonczony. ESP32 nie rozmawia juz z kotlem: odlacz adapter i podlacz stare sterowanie.\n";
  report = s;
}

void handleRoot() {
  String h;
  h.reserve(report.length() + 2000);
  h += F("<!DOCTYPE html><html lang='pl'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>OpenTherm skan</title><style>body{font-family:system-ui;margin:16px;background:#f4f6f5;color:#17201c}"
         "pre{background:#fff;border:1px solid #ccd;padding:12px;overflow-x:auto;font-size:13px}"
         "button{padding:10px 14px;font-size:15px}</style></head><body><h1>OpenTherm skan kotla</h1>");
  if (phase != PH_DONE) {
    h += F("<p>Skan w toku... (odswiez za chwile) – faza: ");
    h += String((int)phase) + " / ID " + String(cursor) + "</p><meta http-equiv='refresh' content='5'>";
  } else {
    h += F("<p>Gotowe. Skopiuj raport i wklej go Claude.</p>"
           "<button onclick=\"navigator.clipboard.writeText(document.getElementById('r').innerText)\">Kopiuj raport</button>");
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
    Serial.print("Wi-Fi OK, otworz http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Brak Wi-Fi – wyniki tylko w Serial Monitor");
  }
  server.on("/", handleRoot);
  server.begin();

  ot.begin();
  report = "Skan startuje za kilka sekund...\n";
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
        report = "Odczyt w toku: ID " + String(cursor) + "/255\n";
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
        Serial.printf("ZAPIS ID%-3d %s\n", w.id, resName(w.res));
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
      // Koniec: brak dalszej komunikacji z kotlem.
      break;
  }
}
