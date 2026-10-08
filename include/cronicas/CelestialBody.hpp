#pragma once
#include <optional>
#include <string>
namespace cronicas {
struct Mass { double value; int exponent; };
struct CelestialBody {
    std::string id, englishName, bodyType;
    std::optional<bool> isPlanet;
    std::optional<double> gravity, meanRadius, semimajorAxis, avgTemp;
    std::optional<Mass> mass;
};
std::string displayName(const CelestialBody& body);
}
