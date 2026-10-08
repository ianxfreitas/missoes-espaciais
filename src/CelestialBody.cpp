#include "cronicas/CelestialBody.hpp"
namespace cronicas {
std::string displayName(const CelestialBody& body) {
    return body.englishName.empty() ? body.id : body.englishName;
}
}  // namespace cronicas
