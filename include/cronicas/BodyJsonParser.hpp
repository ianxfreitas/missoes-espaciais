#pragma once
#include "CelestialBody.hpp"
#include <vector>
namespace cronicas {
struct ParseResult { std::vector<CelestialBody> bodies; std::vector<std::string> warnings; size_t rejected=0; };
ParseResult parseBodies(const std::string& json);
}
