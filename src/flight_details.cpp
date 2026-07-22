#include "flight_details.h"

namespace {

String stringValue(JsonVariantConst value) {
  if (value.is<const char *>()) {
    return value.as<String>();
  }
  if (value.is<long>()) {
    return String(value.as<long>());
  }
  return String();
}

AirportDetails parseAirport(JsonObjectConst airport) {
  AirportDetails details;
  details.code = stringValue(airport["iata"]);
  if (details.code.isEmpty()) {
    details.code = stringValue(airport["icao"]);
  }
  details.city = stringValue(airport["location"]);
  details.name = stringValue(airport["name"]);
  return details;
}

}  // namespace

void clearFlightDetails(FlightDetails &details, const String &callsign) {
  details.callsign = callsign;
  details.airlineIcao = "";
  details.number = "";
  details.origin = AirportDetails();
  details.destination = AirportDetails();
  details.routeFound = false;
  details.plausible = false;
}

bool parseFlightDetailsResponse(JsonVariantConst response, const String &callsign,
                                FlightDetails &details) {
  clearFlightDetails(details, callsign);
  if (!response.is<JsonArrayConst>()) {
    return false;
  }

  JsonArrayConst routes = response.as<JsonArrayConst>();
  if (routes.isNull() || routes.size() == 0 || !routes[0].is<JsonObjectConst>()) {
    return false;
  }

  JsonObjectConst route = routes[0].as<JsonObjectConst>();
  details.airlineIcao = stringValue(route["airline_code"]);
  details.number = stringValue(route["number"]);
  details.plausible = route["plausible"] | false;

  JsonArrayConst airports = route["_airports"].as<JsonArrayConst>();
  if (!airports.isNull() && airports.size() > 0) {
    details.origin = parseAirport(airports[0].as<JsonObjectConst>());
    details.destination = parseAirport(airports[airports.size() - 1].as<JsonObjectConst>());
  }

  String routeCodes = stringValue(route["_airport_codes_iata"]);
  if (!routeCodes.isEmpty()) {
    int firstSeparator = routeCodes.indexOf('-');
    int lastSeparator = routeCodes.lastIndexOf('-');
    if (details.origin.code.isEmpty() && firstSeparator > 0) {
      details.origin.code = routeCodes.substring(0, firstSeparator);
    }
    if (details.destination.code.isEmpty() && lastSeparator > 0 &&
        lastSeparator < static_cast<int>(routeCodes.length()) - 1) {
      details.destination.code = routeCodes.substring(lastSeparator + 1);
    }
  }

  details.routeFound = !details.origin.code.isEmpty() && !details.destination.code.isEmpty();
  return true;
}
