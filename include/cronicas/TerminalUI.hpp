#pragma once
#include "BodyCatalog.hpp"
#include "SolarApiClient.hpp"
#include <iosfwd>
namespace cronicas {
class TerminalUI {
    BodyCatalog& catalog_;
    const SolarApiClient& api_;
    std::istream& input_;
    std::ostream& output_;
    std::string read(const std::string& prompt);
    double number(const std::string& prompt);
    size_t integer(const std::string& prompt, size_t maximum);
    void showBody(const CelestialBody& body);
    void showList(const std::vector<const CelestialBody*>& bodies);
    void showStats();
    void load();
    void compare();
    void plan();

   public:
    TerminalUI(BodyCatalog& catalog, const SolarApiClient& api,
               std::istream& input, std::ostream& output)
        : catalog_(catalog), api_(api), input_(input), output_(output) {}
    void run();
};
}  // namespace cronicas
