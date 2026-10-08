#include "cronicas/BodyCatalog.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
namespace cronicas {
namespace {
std::string lower(std::string text) { for(auto& c:text) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return text; }
}
LoadSummary BodyCatalog::load(const ParseResult& parsed,const std::string& source) {
    if(parsed.bodies.empty()) throw std::runtime_error("Carga sem corpos válidos; catálogo anterior preservado");
    HashTable replacement;
    LoadSummary summary{parsed.bodies.size(),0,0,0,parsed.warnings};
    for(const auto& body:parsed.bodies) { if(replacement.insert(body)) ++summary.inserted; else ++summary.updated; }
    for(const auto& warning:parsed.warnings) if(warning.find("ignorado: id inválido")!=std::string::npos) ++summary.rejected;
    std::string newSource=source;
    table_=std::move(replacement); source_.swap(newSource); return summary;
}
std::vector<const CelestialBody*> BodyCatalog::select(const std::function<bool(const CelestialBody&)>& predicate) const {
    auto elements=table_.elements();
    elements.erase(std::remove_if(elements.begin(),elements.end(),[&](auto body){return !predicate(*body);}),elements.end());
    std::sort(elements.begin(),elements.end(),[](auto a,auto b){return a->id<b->id;}); return elements;
}
std::vector<const CelestialBody*> BodyCatalog::list() const { return select([](const auto&){return true;}); }
std::vector<const CelestialBody*> BodyCatalog::searchName(const std::string& text) const {
    const auto needle=lower(text); return select([&](const auto& b){return lower(displayName(b)).find(needle)!=std::string::npos;});
}
std::vector<const CelestialBody*> BodyCatalog::filterType(const std::string& type) const { const auto target=lower(type); return select([&](const auto& b){return lower(b.bodyType)==target;}); }
std::vector<const CelestialBody*> BodyCatalog::filterPlanet(bool planet) const { return select([&](const auto& b){return b.isPlanet && *b.isPlanet==planet;}); }
std::vector<const CelestialBody*> BodyCatalog::filterRange(const std::string& field,double minimum,double maximum) const {
    if(!std::isfinite(minimum) || !std::isfinite(maximum) || minimum<0 || minimum>maximum) throw std::invalid_argument("Intervalo inválido");
    std::optional<double> CelestialBody::* member=nullptr;
    if(field=="gravity") member=&CelestialBody::gravity;
    else if(field=="meanRadius") member=&CelestialBody::meanRadius;
    else if(field=="avgTemp") member=&CelestialBody::avgTemp;
    else throw std::invalid_argument("Atributo deve ser gravity, meanRadius ou avgTemp");
    return select([&](const auto& b){const auto& value=b.*member; return value && *value>=minimum && *value<=maximum;});
}
}
