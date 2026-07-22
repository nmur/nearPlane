#pragma once

#include <Arduino.h>

struct AirlineIdentity {
  const char *icao;
  const char *iata;
  const char *name;
};

struct AirlineLogo {
  const char *iata;
  const uint8_t *png;
  size_t length;
  uint16_t width;
  uint16_t height;
};

const AirlineIdentity *findAirlineByCode(const String &code);
const AirlineIdentity *inferAirlineFromCallsign(const String &callsign);
const AirlineLogo *findAirlineLogo(const String &iata);
