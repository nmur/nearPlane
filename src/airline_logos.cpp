#include "airline_logos.h"

namespace {

const AirlineIdentity airlineIdentities[] = {
    {"QFA", "QF", "Qantas"},
    {"QLK", "QF", "QantasLink"},
    {"QJE", "QF", "QantasLink"},
    {"NWK", "QF", "Network Aviation"},
    {"JST", "JQ", "Jetstar"},
    {"JSA", "3K", "Jetstar Asia"},
    {"VOZ", "VA", "Virgin Australia"},
    {"ANZ", "NZ", "Air New Zealand"},
    {"RXA", "ZL", "Rex Airlines"},
    {"UTY", "QQ", "Alliance Airlines"},
    {"FJI", "FJ", "Fiji Airways"},
    {"SIA", "SQ", "Singapore Airlines"},
    {"UAE", "EK", "Emirates"},
    {"QTR", "QR", "Qatar Airways"},
    {"ETD", "EY", "Etihad Airways"},
    {"CPA", "CX", "Cathay Pacific"},
    {"BAW", "BA", "British Airways"},
    {"UAL", "UA", "United Airlines"},
    {"DAL", "DL", "Delta Air Lines"},
    {"AAL", "AA", "American Airlines"},
    {"ACA", "AC", "Air Canada"},
    {"JAL", "JL", "Japan Airlines"},
    {"ANA", "NH", "All Nippon Airways"},
    {"MAS", "MH", "Malaysia Airlines"},
    {"THA", "TG", "Thai Airways"},
    {"GIA", "GA", "Garuda Indonesia"},
    {"PAL", "PR", "Philippine Airlines"},
    {"LAN", "LA", "LATAM Airlines"},
    {"CAL", "CI", "China Airlines"},
    {"EVA", "BR", "EVA Air"},
    {"CSN", "CZ", "China Southern"},
    {"CES", "MU", "China Eastern"},
    {"CCA", "CA", "Air China"},
    {"KAL", "KE", "Korean Air"},
    {"AAR", "OZ", "Asiana Airlines"},
    {"HVN", "VN", "Vietnam Airlines"},
    {"SAS", "SK", "SAS"},
    {"FIN", "AY", "Finnair"},
    {"KLM", "KL", "KLM"},
    {"AFR", "AF", "Air France"},
    {"DLH", "LH", "Lufthansa"},
    {"THY", "TK", "Turkish Airlines"},
    {"RYR", "FR", "Ryanair"},
    {"SWR", "LX", "SWISS"},
    {"IBE", "IB", "Iberia"},
    {"TAP", "TP", "TAP Air Portugal"},
    {"AUA", "OS", "Austrian Airlines"},
    {"BEL", "SN", "Brussels Airlines"},
    {"EIN", "EI", "Aer Lingus"},
    {"SWA", "WN", "Southwest Airlines"},
    {"JBU", "B6", "JetBlue"},
    {"ASA", "AS", "Alaska Airlines"},
    {"HAL", "HA", "Hawaiian Airlines"},
    {"WJA", "WS", "WestJet"},
    {"UPS", "5X", "UPS Airlines"},
    {"FDX", "FX", "FedEx Express"},
    {"GTI", "5Y", "Atlas Air"},
    {"PAC", "PO", "Polar Air Cargo"},
};

String normalizedCode(String code) {
  code.trim();
  code.toUpperCase();
  return code;
}

}  // namespace

#if defined(ARDUINO_M5STACK_Core2)
#include "airline_logo_data.inc"
#endif

const AirlineIdentity *findAirlineByCode(const String &code) {
  String normalized = normalizedCode(code);
  if (normalized.isEmpty()) {
    return nullptr;
  }
  for (const AirlineIdentity &identity : airlineIdentities) {
    if (normalized == identity.icao || normalized == identity.iata) {
      return &identity;
    }
  }
  return nullptr;
}

const AirlineIdentity *inferAirlineFromCallsign(const String &callsign) {
  String normalized = normalizedCode(callsign);
  if (normalized.length() >= 3) {
    const AirlineIdentity *identity = findAirlineByCode(normalized.substring(0, 3));
    if (identity != nullptr) {
      return identity;
    }
  }
  if (normalized.length() >= 2) {
    return findAirlineByCode(normalized.substring(0, 2));
  }
  return nullptr;
}

const AirlineLogo *findAirlineLogo(const String &iata) {
#if defined(ARDUINO_M5STACK_Core2)
  String normalized = normalizedCode(iata);
  for (const AirlineLogo &logo : embeddedAirlineLogos) {
    if (normalized == logo.iata) {
      return &logo;
    }
  }
#else
  (void)iata;
#endif
  return nullptr;
}
