#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <cmath>
#include <cstdio>
#include <time.h>

#include "airline_logos.h"
#include "flight_details.h"

Preferences preferences;
WebServer server(80);
DNSServer dnsServer;
M5Canvas canvas(&M5.Display);

#if defined(ARDUINO_M5STACK_Core2)
const int NUM_PAGES = 7;
const int LEGACY_PAGE_OFFSET = 1;
#else
const int NUM_PAGES = 6;
const int LEGACY_PAGE_OFFSET = 0;
#endif
unsigned long pollInterval = 2000;
const unsigned long SHORT_POLL_INTERVAL = 2000;
const unsigned long NO_AIRCRAFT_POLL_INTERVAL = 10000;
const unsigned long ERROR_POLL_INTERVAL = 60000;
const unsigned long FLIGHT_DETAILS_RETRY_INTERVAL_MS = 30000;
const long RESET_HOLD_TIME_MS = 5000;

// Audio settings. Volume ranges from 0 (muted) to 255 (maximum).
// Frequencies are in hertz, so these can be changed to choose another chime.
const uint8_t SPEAKER_VOLUME = 32;
const uint16_t STARTUP_TONE_HZ = 523;          // C5
const uint16_t NEW_AIRCRAFT_TONE_1_HZ = 659;  // E5
const uint16_t NEW_AIRCRAFT_TONE_2_HZ = 784;  // G5

bool configMode = false;
String wifi_ssid, wifi_password, latitude, longitude, radius_km;
String api_url;
unsigned long lastPollTime = 0;
int currentPage = 0;
JsonDocument doc;
String lastSeenAircraftReg = "";
String lastFlightCodeForDetails = "";
FlightDetails flightDetails;
bool flightDetailsLookupComplete = false;
unsigned long lastFlightDetailsAttemptTime = 0;

unsigned long btnB_press_start_time = 0;
bool isResetting = false;

unsigned long lastActivityTime = 0;
const unsigned long INACTIVITY_TIMEOUT_MS = 300000;
const uint64_t DEEP_SLEEP_DURATION_S = 60;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;
const int daylightOffset_sec = 0;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"><title>nearPlane Tracker Setup</title><style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f5f5f7;color:#1d1d1f}.container{padding:25px;max-width:550px;margin:30px auto;background-color:#fff;border-radius:12px;box-shadow:0 4px 12px rgba(0,0,0,.1)}h1{color:#1d1d1f;text-align:center;margin-bottom:25px;font-weight:600}form{display:flex;flex-direction:column}label{margin-bottom:8px;color:#6e6e73;font-weight:500}input{padding:14px;margin-bottom:20px;border:1px solid #d2d2d7;border-radius:8px;font-size:16px;transition:border-color .2s,box-shadow .2s}input:focus{border-color:#007aff;box-shadow:0 0 0 3px rgba(0,122,255,.25);outline:none}button{background-color:#007aff;color:#fff;padding:16px;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;transition:background-color .2s}button:hover{background-color:#0056b3}.footer{text-align:center;margin-top:20px;color:#86868b;font-size:12px}</style></head><body><div class="container"><h1>nearPlane ADSB Tracker Setup</h1><form action="/save" method="POST"><label for="ssid">WiFi Network (SSID)</label><input type="text" id="ssid" name="ssid" required><label for="password">WiFi Password</label><input type="text" id="password" name="password"><label for="lat">Your Latitude</label><input type="text" id="lat" name="lat" required placeholder="e.g., 41.015137"><label for="lon">Your Longitude</label><input type="text" id="lon" name="lon" required placeholder="e.g., 28.979530"><label for="radius">Scan Radius (km)</label><input type="number" id="radius" name="radius" value="50" required><button type="submit">Save & Reboot</button></form></div><div class="footer">nearPlane ADSB Tracker</div></body></html>
)rawliteral";

void handleRoot();
void handleSave();
void handleNotFound();
void displayCurrentPage();
void startConfigMode();
void loadSettingsAndConnect();
void fetchAircraftData();
void fetchFlightDetails(String flightCode);
void playNewAircraftSound();
void handleButtons();
void drawResetScreen(int seconds_left);
void drawDegreeSymbol(int x, int y);
void drawBatteryStatus();
void goToSleep();
void drawClock();
void drawCenteredStatusScreen(uint32_t background, uint32_t accent,
                              const String &title, const String &message,
                              const String &footer);
String ellipsizeToWidth(String text, int maxWidth);
void resetFlightDetailsState();
#if defined(ARDUINO_M5STACK_Core2)
void drawFlightOverview(JsonObjectConst aircraft);
#endif

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Power.begin();
  M5.Speaker.setVolume(SPEAKER_VOLUME);
  M5.Speaker.tone(STARTUP_TONE_HZ, 50);
  M5.Display.setRotation(1);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  preferences.begin("adsb-config", false);
  wifi_ssid = preferences.getString("ssid", "");
  if (wifi_ssid == "") {
    configMode = true;
    startConfigMode();
  } else {
    configMode = false;
    loadSettingsAndConnect();
  }
  lastActivityTime = millis();
}

void loop() {
  M5.update();
  handleButtons();
  if (isResetting) { return; }
  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
  } else {
    if (WiFi.status() == WL_CONNECTED && (millis() - lastPollTime > pollInterval)) {
      fetchAircraftData();
      lastPollTime = millis();
    }
    if (millis() - lastActivityTime > INACTIVITY_TIMEOUT_MS) {
      goToSleep();
    }
  }
}

void goToSleep() {
  drawCenteredStatusScreen(BLACK, TFT_YELLOW, "No Activity", "Going to sleep...",
                           "Waking again in 60 seconds");
  delay(3000);
  M5.Display.sleep();
  M5.Power.deepSleep(DEEP_SLEEP_DURATION_S * 1000000ULL);
}

void drawBatteryStatus() {
    int batt_level = M5.Power.getBatteryLevel();
    bool is_charging = M5.Power.isCharging();
    uint32_t batt_color;

    if (is_charging) {
        batt_color = TFT_GREEN;
    } else if (batt_level < 15) {
        batt_color = TFT_RED;
    } else if (batt_level < 40) {
        batt_color = TFT_YELLOW;
    } else {
        batt_color = TFT_GREEN;
    }

    canvas.drawRect(5, 5, 20, 10, TFT_WHITE);
    canvas.drawRect(25, 7, 2, 6, TFT_WHITE);

    int fill_width = (18 * batt_level) / 100;
    if (fill_width > 0) {
        canvas.fillRect(6, 6, fill_width, 8, batt_color);
    }

    if (is_charging) {
        canvas.drawLine(14, 7, 11, 10, TFT_YELLOW);
        canvas.drawLine(11, 10, 14, 13, TFT_YELLOW);
    }
}

void drawClock() {
    struct tm timeinfo;
    char timeBuffer[6];

    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(TR_DATUM);

    if(!getLocalTime(&timeinfo)){
        canvas.drawString("--:--", canvas.width() - 5, 5);
        return;
    }

    sprintf(timeBuffer, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    canvas.drawString(timeBuffer, canvas.width() - 5, 5);
}

String ellipsizeToWidth(String text, int maxWidth) {
  text.trim();
  if (canvas.textWidth(text) <= maxWidth) {
    return text;
  }
  while (text.length() > 1 && canvas.textWidth(text + "...") > maxWidth) {
    text.remove(text.length() - 1);
  }
  return text + "...";
}

void drawCenteredStatusScreen(uint32_t background, uint32_t accent,
                              const String &title, const String &message,
                              const String &footer) {
  const bool largeLayout = canvas.width() >= 300;
  const int centerX = canvas.width() / 2;
  const int centerY = canvas.height() / 2;
  const int maxTextWidth = canvas.width() - (largeLayout ? 32 : 20);

  canvas.fillScreen(background);
  drawBatteryStatus();
  drawClock();
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(accent);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.drawString(ellipsizeToWidth(title, maxTextWidth), centerX,
                    centerY - (largeLayout ? 38 : 25));

  if (!message.isEmpty()) {
    canvas.setTextColor(TFT_WHITE);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.drawString(ellipsizeToWidth(message, maxTextWidth), centerX,
                      centerY + (largeLayout ? 8 : 8));
  }
  if (!footer.isEmpty()) {
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.setFont(&fonts::Font0);
    canvas.drawString(ellipsizeToWidth(footer, maxTextWidth), centerX,
                      centerY + (largeLayout ? 42 : 31));
  }
  canvas.pushSprite(0, 0);
}

#if defined(ARDUINO_M5STACK_Core2)
namespace {

struct AircraftTypeIdentity {
  const char *code;
  const char *name;
};

const AircraftTypeIdentity aircraftTypeIdentities[] = {
    {"A319", "Airbus A319"},       {"A320", "Airbus A320"},
    {"A321", "Airbus A321"},       {"A20N", "Airbus A320neo"},
    {"A21N", "Airbus A321neo"},    {"A332", "Airbus A330-200"},
    {"A333", "Airbus A330-300"},   {"A339", "Airbus A330-900"},
    {"A359", "Airbus A350-900"},   {"A35K", "Airbus A350-1000"},
    {"A388", "Airbus A380-800"},   {"B712", "Boeing 717-200"},
    {"B737", "Boeing 737-700"},    {"B738", "Boeing 737-800"},
    {"B739", "Boeing 737-900"},    {"B38M", "Boeing 737 MAX 8"},
    {"B39M", "Boeing 737 MAX 9"},  {"B744", "Boeing 747-400"},
    {"B748", "Boeing 747-8"},      {"B752", "Boeing 757-200"},
    {"B763", "Boeing 767-300"},    {"B772", "Boeing 777-200"},
    {"B77W", "Boeing 777-300ER"},  {"B788", "Boeing 787-8"},
    {"B789", "Boeing 787-9"},      {"B78X", "Boeing 787-10"},
    {"E170", "Embraer E170"},      {"E190", "Embraer E190"},
    {"E195", "Embraer E195"},      {"E290", "Embraer E190-E2"},
    {"E295", "Embraer E195-E2"},   {"AT72", "ATR 72"},
    {"AT76", "ATR 72-600"},        {"DH8D", "Dash 8 Q400"},
    {"F100", "Fokker 100"},        {"SF34", "Saab 340"},
};

String airportCodeOrPlaceholder(const AirportDetails &airport) {
  return airport.code.isEmpty() ? "---" : airport.code;
}

String airportCity(const AirportDetails &airport) {
  if (!airport.city.isEmpty()) {
    return airport.city;
  }
  if (!airport.name.isEmpty()) {
    return airport.name;
  }
  return "City unavailable";
}

String aircraftTypeLabel(String code) {
  code.trim();
  code.toUpperCase();
  if (code.isEmpty() || code == "N/A") {
    return "Aircraft type unavailable";
  }
  for (const AircraftTypeIdentity &identity : aircraftTypeIdentities) {
    if (code == identity.code) {
      return code + " - " + identity.name;
    }
  }
  return code;
}

const AirlineIdentity *currentAirlineIdentity(const String &callsign) {
  const AirlineIdentity *identity = findAirlineByCode(flightDetails.airlineIcao);
  return identity != nullptr ? identity : inferAirlineFromCallsign(callsign);
}

String displayFlightNumber(const String &callsign, const AirlineIdentity *identity) {
  if (!flightDetails.number.isEmpty()) {
    String prefix = identity != nullptr ? String(identity->iata) : flightDetails.airlineIcao;
    if (!prefix.isEmpty()) {
      return prefix + flightDetails.number;
    }
  }
  return callsign.isEmpty() || callsign == "N/A" ? "Flight unknown" : callsign;
}

void drawFallbackLogo(const String &code) {
  canvas.fillRoundRect(12, 32, 114, 95, 8, TFT_WHITE);
  canvas.fillTriangle(25, 85, 111, 62, 72, 87, TFT_DARKGREY);
  canvas.fillTriangle(50, 77, 65, 48, 76, 74, TFT_DARKGREY);
  canvas.setTextColor(TFT_BLACK);
  canvas.setTextDatum(BC_DATUM);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.drawString(code.isEmpty() ? "?" : code, 69, 124);
}

}  // namespace

void drawFlightOverview(JsonObjectConst aircraft) {
  canvas.fillScreen(BLACK);

  String callsign = aircraft["flight"] | "N/A";
  callsign.trim();
  String registration = aircraft["r"] | "N/A";
  String type = aircraft["t"] | "N/A";
  String squawk = aircraft["squawk"] | "----";
  String emergency = aircraft["emergency"] | "none";
  bool isEmergency = emergency != "none" || squawk == "7700" || squawk == "7600" ||
                     squawk == "7500";

  if (isEmergency) {
    canvas.fillRect(0, 0, canvas.width(), 21, TFT_RED);
    canvas.drawRect(0, 0, canvas.width(), canvas.height(), TFT_RED);
    canvas.drawRect(1, 1, canvas.width() - 2, canvas.height() - 2, TFT_RED);
  }

  drawBatteryStatus();
  drawClock();
  canvas.setFont(&fonts::Font0);
  canvas.setTextColor(isEmergency ? TFT_WHITE : TFT_DARKGREY);
  canvas.setTextDatum(TC_DATUM);
  canvas.drawString(isEmergency ? "EMERGENCY" : "nearPlane", canvas.width() / 2, 5);
  canvas.drawLine(5, 21, canvas.width() - 5, 21, isEmergency ? TFT_RED : TFT_DARKGREY);

  const AirlineIdentity *identity = currentAirlineIdentity(callsign);
  String airlineCode = identity != nullptr ? String(identity->iata) : flightDetails.airlineIcao;
  if (airlineCode.isEmpty() && callsign.length() >= 3) {
    airlineCode = callsign.substring(0, 3);
  }
  String airlineName = identity != nullptr ? String(identity->name) : "Airline unavailable";
  const AirlineLogo *logo = identity != nullptr ? findAirlineLogo(identity->iata) : nullptr;

  canvas.drawRoundRect(8, 28, 122, 103, 9, TFT_DARKGREY);
  bool logoDrawn = false;
  if (logo != nullptr) {
    canvas.fillRoundRect(12, 32, 114, 95, 8, TFT_WHITE);
    logoDrawn = canvas.drawPng(logo->png, logo->length, 24, 34, 96, 90, 0, 0, 0.70f);
  }
  if (!logoDrawn) {
    drawFallbackLogo(airlineCode);
  }

  canvas.setTextDatum(TC_DATUM);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.drawString(ellipsizeToWidth(airlineName, 174), 226, 27);

  canvas.setTextColor(isEmergency ? TFT_RED : TFT_YELLOW);
  canvas.setFont(&fonts::Orbitron_Light_32);
  String flightNumber = displayFlightNumber(callsign, identity);
  if (canvas.textWidth(flightNumber) > 174) {
    canvas.setFont(&fonts::FreeSansBold18pt7b);
  }
  canvas.drawString(ellipsizeToWidth(flightNumber, 174), 226, 48);

  canvas.setTextColor(TFT_WHITE);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.drawString(ellipsizeToWidth(aircraftTypeLabel(type), 174), 226, 93);
  canvas.setFont(&fonts::Font0);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("REG " + registration, 226, 119);

  canvas.drawLine(8, 138, 312, 138, TFT_DARKGREY);
  canvas.setFont(&fonts::Font0);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("ORIGIN", 72, 144);
  canvas.drawString("DESTINATION", 248, 144);

  canvas.setFont(&fonts::FreeSansBold24pt7b);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(airportCodeOrPlaceholder(flightDetails.origin), 72, 153);
  canvas.drawString(airportCodeOrPlaceholder(flightDetails.destination), 248, 153);

  canvas.drawLine(137, 177, 181, 177, TFT_YELLOW);
  canvas.fillTriangle(181, 172, 181, 182, 190, 177, TFT_YELLOW);

  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(TFT_LIGHTGREY);
  if (flightDetails.routeFound) {
    canvas.drawString(ellipsizeToWidth(airportCity(flightDetails.origin), 136), 72, 202);
    canvas.drawString(ellipsizeToWidth(airportCity(flightDetails.destination), 136), 248, 202);
  } else {
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(flightDetailsLookupComplete ? "Route unavailable" : "Looking up route...",
                      canvas.width() / 2, 202);
  }

  canvas.setFont(&fonts::Font0);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.setTextDatum(BL_DATUM);
  canvas.drawString("BtnA: details", 6, canvas.height() - 2);
  canvas.setTextDatum(BR_DATUM);
  canvas.drawString("1/" + String(NUM_PAGES), canvas.width() - 5, canvas.height() - 2);
}
#endif

void drawDegreeSymbol(int x, int y) {
  canvas.drawCircle(x, y, 2, TFT_WHITE);
}

void handleButtons() {
  if (!isResetting && M5.BtnA.wasPressed()) {
    currentPage = (currentPage + 1) % NUM_PAGES;
    lastActivityTime = millis();
    displayCurrentPage();
  }
  if (M5.BtnB.wasPressed()) {
    btnB_press_start_time = millis();
    lastActivityTime = millis();
    isResetting = true;
  }
  if (M5.BtnB.wasReleased()) {
    if (isResetting) {
      isResetting = false;
      displayCurrentPage();
    }
  }
  if (isResetting && M5.BtnB.isPressed()) {
    unsigned long press_duration = millis() - btnB_press_start_time;
    if (press_duration >= RESET_HOLD_TIME_MS) {
      drawCenteredStatusScreen(TFT_ORANGE, TFT_WHITE, "Settings Reset",
                               "Restarting nearPlane...", "");
      preferences.clear();
      delay(2500);
      ESP.restart();
    } else {
      int seconds_left = ceil((RESET_HOLD_TIME_MS - press_duration) / 1000.0);
      drawResetScreen(seconds_left);
    }
  }
}

void drawResetScreen(int seconds_left) {
  const bool largeLayout = canvas.width() >= 300;
  canvas.fillScreen(TFT_RED);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(MC_DATUM);
  canvas.setFont(largeLayout ? &fonts::FreeSansBold12pt7b : &fonts::FreeSans9pt7b);
  canvas.drawString("Release to cancel", canvas.width() / 2,
                    canvas.height() / 2 - (largeLayout ? 42 : 28));
  if (largeLayout) {
    canvas.setFont(&fonts::FreeSansBold18pt7b);
  } else {
    canvas.setFont(&fonts::Font4);
  }
  canvas.drawString("Reset in " + String(seconds_left), canvas.width() / 2,
                    canvas.height() / 2 + (largeLayout ? 20 : 22));
  canvas.pushSprite(0, 0);
}

void playNewAircraftSound() {
  M5.Speaker.tone(NEW_AIRCRAFT_TONE_1_HZ, 75);
  delay(90);
  M5.Speaker.tone(NEW_AIRCRAFT_TONE_2_HZ, 110);
}

void startConfigMode() {
  const char *ap_ssid = "nearPlane-ADSB-Tracker-Setup";
  const bool largeLayout = canvas.width() >= 300;
  const int centerX = canvas.width() / 2;
  canvas.fillScreen(TFT_BLUE);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(MC_DATUM);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.drawString("SETUP MODE", centerX, largeLayout ? 34 : 17);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.drawString("1. Connect to this Wi-Fi", centerX, largeLayout ? 78 : 43);
  canvas.setTextColor(TFT_YELLOW);
  if (largeLayout) {
    canvas.setFont(&fonts::FreeSansBold9pt7b);
  } else {
    canvas.setFont(&fonts::Font0);
  }
  canvas.drawString(ellipsizeToWidth(ap_ssid, canvas.width() - 20), centerX,
                    largeLayout ? 108 : 64);
  canvas.setTextColor(TFT_WHITE);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.drawString("2. Open a browser to", centerX, largeLayout ? 151 : 86);
  canvas.setTextColor(TFT_YELLOW);
  canvas.setFont(largeLayout ? &fonts::FreeSansBold12pt7b : &fonts::FreeSansBold9pt7b);
  canvas.drawString("192.168.4.1", centerX, largeLayout ? 181 : 108);
  if (largeLayout) {
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.setFont(&fonts::Font0);
    canvas.drawString("The setup page may open automatically", centerX, 222);
  }
  canvas.pushSprite(0, 0);
  WiFi.softAP(ap_ssid);
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loadSettingsAndConnect() {
  wifi_password = preferences.getString("password", "");
  latitude = preferences.getString("lat", "0.0");
  longitude = preferences.getString("lon", "0.0");
  radius_km = preferences.getString("radius", "50");
  api_url = "https://api.adsb.lol/v2/closest/" + latitude + "/" + longitude + "/" + radius_km;
  const bool largeLayout = canvas.width() >= 300;
  const int centerX = canvas.width() / 2;
  const int progressWidth = largeLayout ? 256 : 180;
  const int progressX = (canvas.width() - progressWidth) / 2;
  const int progressY = largeLayout ? 150 : 85;
  canvas.fillScreen(BLACK);
  drawBatteryStatus();
  drawClock();
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(MC_DATUM);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.drawString("Connecting to Wi-Fi", centerX, largeLayout ? 62 : 35);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.drawString("Network", centerX, largeLayout ? 99 : 58);
  canvas.setTextColor(TFT_YELLOW);
  canvas.setFont(largeLayout ? &fonts::FreeSansBold12pt7b
                             : &fonts::FreeSansBold9pt7b);
  canvas.drawString(ellipsizeToWidth(wifi_ssid, canvas.width() - 24), centerX,
                    largeLayout ? 124 : 74);
  canvas.drawRoundRect(progressX, progressY, progressWidth, largeLayout ? 14 : 10,
                       largeLayout ? 5 : 3, TFT_WHITE);
  if (largeLayout) {
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.setFont(&fonts::Font0);
    canvas.drawString("This can take up to 15 seconds", centerX, 180);
  }
  canvas.pushSprite(0, 0);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    const int innerWidth = progressWidth - 4;
    canvas.fillRoundRect(progressX + 2, progressY + 2,
                         (innerWidth * (attempts + 1)) / 30,
                         largeLayout ? 10 : 6, largeLayout ? 3 : 2, TFT_GREEN);
    canvas.pushSprite(0, 0);
    attempts++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    drawCenteredStatusScreen(TFT_MAROON, TFT_YELLOW, "Connection Failed",
                             "Unable to join " + wifi_ssid,
                             "Hold BtnB for 5 seconds to reset");
  } else {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    drawCenteredStatusScreen(TFT_DARKGREEN, TFT_GREEN, "Connected", wifi_ssid,
                             "Waiting for aircraft data...");
    delay(2000);
  }
}

void handleRoot() { server.send_P(200, "text/html", index_html); }

void handleSave() {
  preferences.putString("ssid", server.arg("ssid"));
  preferences.putString("password", server.arg("password"));
  preferences.putString("lat", server.arg("lat"));
  preferences.putString("lon", server.arg("lon"));
  preferences.putString("radius", server.arg("radius"));
  server.send(200, "text/html", "<h1>Settings Saved!</h1><p>Device will reboot in 3 seconds.</p>");
  delay(3000);
  ESP.restart();
}

void handleNotFound() {
  server.sendHeader("Location", "http://192.168.4.1", true);
  server.send(302, "text/plain", "");
}

void displayCurrentPage() {
  canvas.fillScreen(BLACK);
  drawBatteryStatus();
  drawClock();

  if (doc.isNull() || !doc["ac"].is<JsonArray>() || doc["ac"].as<JsonArray>().size() == 0) {
    drawCenteredStatusScreen(BLACK, TFT_YELLOW, "No Aircraft Nearby",
                             "Scanning your configured area...",
                             "Checking again soon");
    return;
  }
  JsonObject aircraft = doc["ac"][0];
#if defined(ARDUINO_M5STACK_Core2)
  if (currentPage == 0) {
    drawFlightOverview(aircraft);
    canvas.pushSprite(0, 0);
    return;
  }
#endif
  int telemetryPage = currentPage - LEGACY_PAGE_OFFSET;
  String flight = aircraft["flight"] | "N/A";
  flight.trim();
  String reg = aircraft["r"] | "N/A";
  String type = aircraft["t"] | "N/A";
  String squawk = aircraft["squawk"] | "----";
  String emergency = aircraft["emergency"] | "none";
  const bool largeLayout = canvas.width() >= 300;
  const int centerX = canvas.width() / 2;
  const int labelX = largeLayout ? 28 : 15;
  const int valueX = largeLayout ? canvas.width() - 28 : 225;
  const int titleY = largeLayout ? 28 : 5;
  const int registrationY = largeLayout ? 72 : 45;
  const int dividerY = largeLayout ? 101 : 65;
  const int row1Y = largeLayout ? 116 : 75;
  const int row2Y = largeLayout ? 150 : 95;
  const int row3Y = largeLayout ? 184 : 115;
  const int degreeValueX = valueX - 7;
  const int degreeSymbolX = valueX - 3;
  if (emergency != "none" || squawk == "7700" || squawk == "7600" || squawk == "7500") {
    canvas.fillRect(0, largeLayout ? 22 : 0, canvas.width(), largeLayout ? 47 : 40,
                    TFT_RED);
    canvas.setTextColor(TFT_WHITE);
  } else {
    canvas.setTextColor(TFT_YELLOW);
  }
  canvas.setFont(&fonts::Orbitron_Light_32);
  canvas.setTextDatum(TC_DATUM);
  canvas.drawString(ellipsizeToWidth(flight, canvas.width() - 40), centerX, titleY);
  canvas.setTextColor(TFT_WHITE);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextDatum(TC_DATUM);
  canvas.drawString(ellipsizeToWidth(reg + " (" + type + ")", canvas.width() - 32),
                    centerX, registrationY);
  canvas.drawLine(10, dividerY, canvas.width() - 10, dividerY, TFT_DARKGREY);
  switch (telemetryPage) {
  case 0: {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawString("ALT", labelX, row1Y);
    canvas.drawString("GND SPD", labelX, row2Y);
    canvas.drawString("ROUTE", labelX, row3Y);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(String(aircraft["alt_baro"] | 0) + " ft", valueX, row1Y);
    canvas.drawString(String(aircraft["gs"].as<float>(), 0) + " kt", valueX, row2Y);
    String origin = flightDetails.origin.code.isEmpty() ? "N/A" : flightDetails.origin.code;
    String destination =
        flightDetails.destination.code.isEmpty() ? "N/A" : flightDetails.destination.code;
    String route = origin + " > " + destination;
    canvas.drawString(ellipsizeToWidth(route, valueX - labelX - 80), valueX, row3Y);
    break;
  }
  case 1: {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawString("HDG", labelX, row1Y);
    canvas.drawString("V/S", labelX, row2Y);
    canvas.drawString("ROLL", labelX, row3Y);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(String(aircraft["true_heading"].as<float>(), 0), degreeValueX,
                      row1Y);
    drawDegreeSymbol(degreeSymbolX, row1Y + 3);
    canvas.drawString(String(aircraft["baro_rate"] | 0) + " ft/m", valueX, row2Y);
    canvas.drawString(String(aircraft["roll"].as<float>(), 1), degreeValueX, row3Y);
    drawDegreeSymbol(degreeSymbolX, row3Y + 3);
    break;
  }
  case 2: {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawString("MACH", labelX, row1Y);
    canvas.drawString("IAS/TAS", labelX, row2Y);
    canvas.drawString("GEOM ALT", labelX, row3Y);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(String(aircraft["mach"].as<float>(), 3), valueX, row1Y);
    canvas.drawString(String(aircraft["ias"] | 0) + "/" +
                          String(aircraft["tas"] | 0) + "kt",
                      valueX, row2Y);
    canvas.drawString(String(aircraft["alt_geom"] | 0) + " ft", valueX, row3Y);
    break;
  }
  case 3: {
    String nav_modes_str = "";
    JsonArray nav_modes = aircraft["nav_modes"];
    for (JsonVariant v : nav_modes)
      nav_modes_str += v.as<String>() + " ";
    nav_modes_str.trim();
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawString("AP ALT", labelX, row1Y);
    canvas.drawString("AP HDG", labelX, row2Y);
    canvas.drawString("AP MODE", labelX, row3Y);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(String(aircraft["nav_altitude_mcp"] | 0) + " ft", valueX, row1Y);
    canvas.drawString(String(aircraft["nav_heading"].as<float>(), 0), degreeValueX,
                      row2Y);
    drawDegreeSymbol(degreeSymbolX, row2Y + 3);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.drawString(ellipsizeToWidth(nav_modes_str, valueX - labelX - 80), valueX,
                      row3Y);
    break;
  }
  case 4: {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawString("SQK", labelX, row1Y);
    canvas.drawString("ICAO", labelX, row2Y);
    canvas.drawString("CAT", labelX, row3Y);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(squawk, valueX, row1Y);
    canvas.drawString(aircraft["hex"] | "N/A", valueX, row2Y);
    canvas.drawString(aircraft["category"] | "N/A", valueX, row3Y);
    break;
  }
  case 5: {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawString("OAT/TAT", labelX, row1Y);
    canvas.drawString("WIND", labelX, row2Y);
    canvas.drawString("RSSI", labelX, row3Y);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(TR_DATUM);
    int x_pos = valueX;
    int y_pos = row1Y;
    String tat_val = String(aircraft["tat"] | 0);
    String oat_val = String(aircraft["oat"] | 0);
    canvas.drawString("C", x_pos, y_pos);
    x_pos -= canvas.textWidth("C");
    drawDegreeSymbol(x_pos, y_pos + 3);
    x_pos -= 4;
    canvas.drawString(tat_val, x_pos, y_pos);
    x_pos -= canvas.textWidth(tat_val);
    canvas.drawString("/", x_pos, y_pos);
    x_pos -= canvas.textWidth("/");
    canvas.drawString("C", x_pos, y_pos);
    x_pos -= canvas.textWidth("C");
    drawDegreeSymbol(x_pos, y_pos + 3);
    x_pos -= 4;
    canvas.drawString(oat_val, x_pos, y_pos);
    String windStr = String(aircraft["wd"] | 0);
    String speedStr = " / " + String(aircraft["ws"] | 0) + "kt";
    canvas.drawString(windStr, valueX - canvas.textWidth(speedStr), row2Y);
    drawDegreeSymbol(valueX - canvas.textWidth(speedStr) + 4, row2Y + 3);
    canvas.drawString(speedStr, valueX, row2Y);
    canvas.drawString(String(aircraft["rssi"].as<float>(), 1) + " dBm", valueX,
                      row3Y);
    break;
  }
  }
  canvas.setTextColor(TFT_DARKGREY);
  canvas.setFont(&fonts::Font0);
  canvas.setTextDatum(BC_DATUM);
  canvas.drawString(String(currentPage + 1) + "/" + String(NUM_PAGES), centerX,
                    canvas.height() - 2);
  canvas.pushSprite(0, 0);
}

void fetchFlightDetails(String flightCode) {
  if (flightCode == "N/A" || flightCode.isEmpty()) {
    resetFlightDetailsState();
    return;
  }

  if (flightCode != lastFlightCodeForDetails) {
    clearFlightDetails(flightDetails, flightCode);
    lastFlightCodeForDetails = flightCode;
    flightDetailsLookupComplete = false;
    lastFlightDetailsAttemptTime = 0;
  }
  if (flightDetailsLookupComplete) {
    return;
  }
  unsigned long now = millis();
  if (lastFlightDetailsAttemptTime != 0 &&
      now - lastFlightDetailsAttemptTime < FLIGHT_DETAILS_RETRY_INTERVAL_MS) {
    return;
  }
  lastFlightDetailsAttemptTime = now;

  JsonDocument requestDoc;
  JsonArray planes = requestDoc["planes"].to<JsonArray>();
  JsonObject plane = planes.add<JsonObject>();
  plane["callsign"] = flightCode;
  plane["lat"] = latitude.toFloat();
  plane["lng"] = longitude.toFloat();
  String requestBody;
  serializeJson(requestDoc, requestBody);
  HTTPClient http;
  http.begin("https://api.adsb.lol/api/0/routeset");
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(requestBody);
  if (httpCode >= 200 && httpCode < 300) {
    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, http.getStream());
    if (!error && parseFlightDetailsResponse(responseDoc.as<JsonVariantConst>(), flightCode,
                                             flightDetails)) {
      flightDetailsLookupComplete = true;
    }
  }
  http.end();
}

void resetFlightDetailsState() {
  lastFlightCodeForDetails = "";
  lastFlightDetailsAttemptTime = 0;
  flightDetailsLookupComplete = false;
  clearFlightDetails(flightDetails);
}

void fetchAircraftData() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  HTTPClient http;
  http.begin(api_url);
  http.setTimeout(5000);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    doc.clear();
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) {
      drawCenteredStatusScreen(TFT_DARKCYAN, TFT_YELLOW, "Data Error",
                               "The aircraft response was invalid",
                               "Retrying in 60 seconds");
      pollInterval = ERROR_POLL_INTERVAL;
    } else if (doc["ac"].is<JsonArray>() && doc["ac"].as<JsonArray>().size() > 0) {
      lastActivityTime = millis();
      String currentAircraftReg = doc["ac"][0]["r"] | "N/A";
      if (currentAircraftReg != "N/A" && currentAircraftReg != lastSeenAircraftReg) {
        playNewAircraftSound();
      }
      lastSeenAircraftReg = currentAircraftReg;
      String flightCode = doc["ac"][0]["flight"] | "N/A";
      flightCode.trim();
      fetchFlightDetails(flightCode);
      displayCurrentPage();
      pollInterval = SHORT_POLL_INTERVAL;
    } else {
      lastSeenAircraftReg = "";
      resetFlightDetailsState();
      doc.clear();
      displayCurrentPage();
      pollInterval = NO_AIRCRAFT_POLL_INTERVAL;
    }
  } else {
    doc.clear();
    lastSeenAircraftReg = "";
    resetFlightDetailsState();
    drawCenteredStatusScreen(TFT_MAROON, TFT_YELLOW, "Service Unavailable",
                             "HTTP code " + String(httpCode),
                             "Retrying in 60 seconds");
    pollInterval = ERROR_POLL_INTERVAL;
  }
  http.end();
}
