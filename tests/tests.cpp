#include "cronicas/CelestialBody.hpp"
#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
using namespace cronicas;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    std::vector<std::pair<std::string, std::function<void()>>> tests;
    tests.push_back({"modelo", [] { CelestialBody b; b.id="terre"; check(displayName(b)=="terre", "nome alternativo"); check(!b.gravity, "ausência distinta de zero"); }});
    int failures=0;
    for (const auto& t:tests) { try { t.second(); std::cout<<"PASS "<<t.first<<'\n'; } catch(const std::exception& e) { ++failures; std::cerr<<"FAIL "<<t.first<<": "<<e.what()<<'\n'; } }
    return failures ? 1 : 0;
}
