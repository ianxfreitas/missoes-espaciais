#include "cronicas/CelestialBody.hpp"
#include "cronicas/SolarApiClient.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
using namespace cronicas;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    std::vector<std::pair<std::string, std::function<void()>>> tests;
    tests.push_back({"modelo", [] { CelestialBody b; b.id="terre"; check(displayName(b)=="terre", "nome alternativo"); check(!b.gravity, "ausência distinta de zero"); }});
    tests.push_back({"parsing", [] {
        auto r=parseBodies(R"({"bodies":[{"id":"x","mass":null,"gravity":0},{"id":"y","gravity":"bad"},{"englishName":"sem id"}]})");
        check(r.bodies.size()==2,"registros válidos"); check(r.bodies[0].gravity==0,"zero preservado"); check(!r.bodies[0].mass,"massa nula"); check(!r.bodies[1].gravity,"tipo inválido"); check(r.warnings.size()==2,"diagnósticos");
        for(auto text:{"{", "[]", "{}"}) { bool threw=false; try { parseBodies(text); } catch(const std::runtime_error&) { threw=true; } check(threw,"rejeição de JSON"); }
    }});
    tests.push_back({"falhas API", [] {
        struct Fake:HttpClient { long status=200; std::string text=R"({"bodies":[{"id":"x"}]})"; bool fail=false;
            HttpResponse get(const std::string&,const std::string&) const override { if(fail) throw std::runtime_error("rede simulada"); return {status,text}; }
        } fake;
        const char* previous=std::getenv("SOLAR_API_KEY"); const std::optional<std::string> saved=previous ? std::optional<std::string>(previous) : std::nullopt;
        setenv("SOLAR_API_KEY","token-de-teste",1);
        SolarApiClient api(fake); check(api.fetch().bodies.size()==1,"API simulada");
        for(long status:{401,403,429,500,302}) { fake.status=status; bool threw=false; try { api.fetch(); } catch(const std::runtime_error&) { threw=true; } check(threw,"status rejeitado"); }
        fake.status=200; fake.text="invalid"; bool threw=false; try { api.fetch(); } catch(const std::runtime_error&) { threw=true; } check(threw,"JSON API inválido");
        fake.fail=true; threw=false; try { api.fetch(); } catch(const std::runtime_error&) { threw=true; } check(threw,"rede");
        unsetenv("SOLAR_API_KEY"); threw=false; try { api.fetch(); } catch(const std::runtime_error&) { threw=true; } check(threw,"token ausente");
        if(saved) setenv("SOLAR_API_KEY",saved->c_str(),1);
    }});
    int failures=0;
    for (const auto& t:tests) { try { t.second(); std::cout<<"PASS "<<t.first<<'\n'; } catch(const std::exception& e) { ++failures; std::cerr<<"FAIL "<<t.first<<": "<<e.what()<<'\n'; } }
    return failures ? 1 : 0;
}
