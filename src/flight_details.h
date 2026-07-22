#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

struct AirportDetails {
  String code;
  String city;
  String name;
};

struct FlightDetails {
  String callsign;
  String airlineIcao;
  String number;
  AirportDetails origin;
  AirportDetails destination;
  bool routeFound = false;
  bool plausible = false;
};

void clearFlightDetails(FlightDetails &details, const String &callsign = "");
bool parseFlightDetailsResponse(JsonVariantConst response, const String &callsign,
                                FlightDetails &details);
