#include "cronicas/BodyJsonParser.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace cronicas {
ParseResult parseBodies(const std::string& text) {
    nlohmann::json root;
    try { root=nlohmann::json::parse(text); } catch(const nlohmann::json::exception&) { throw std::runtime_error("JSON inválido"); }
    if(!root.is_object() || !root.contains("bodies") || !root["bodies"].is_array()) throw std::runtime_error("Resposta deve conter array bodies");
    ParseResult result;
    size_t index=0;
    for(const auto& record:root["bodies"]) {
        const std::string prefix="Registro "+std::to_string(index++)+": ";
        if(!record.is_object() || !record.contains("id") || !record["id"].is_string() || record["id"].get<std::string>().empty()) {
            result.warnings.push_back(prefix+"ignorado: id inválido"); ++result.rejected; continue;
        }
        CelestialBody b; b.id=record["id"].get<std::string>();
        auto stringField=[&](const char* name) { if(!record.contains(name) || record[name].is_null()) return std::string{}; if(record[name].is_string()) return record[name].get<std::string>(); result.warnings.push_back(prefix+name+" inválido"); return std::string{}; };
        auto numberField=[&](const char* name)->std::optional<double> {
            if(!record.contains(name) || record[name].is_null()) return {};
            if(record[name].is_number()) { double value=record[name].get<double>(); if(std::isfinite(value) && value>=0) return value; }
            result.warnings.push_back(prefix+name+" inválido"); return {};
        };
        b.englishName=stringField("englishName"); b.bodyType=stringField("bodyType");
        if(record.contains("isPlanet") && !record["isPlanet"].is_null()) {
            if(record["isPlanet"].is_boolean()) b.isPlanet=record["isPlanet"].get<bool>(); else result.warnings.push_back(prefix+"isPlanet inválido");
        }
        b.gravity=numberField("gravity"); b.meanRadius=numberField("meanRadius");
        b.semimajorAxis=numberField("semimajorAxis"); b.avgTemp=numberField("avgTemp");
        if(record.contains("mass") && !record["mass"].is_null()) {
            const auto& m=record["mass"];
            bool valid=false;
            if(m.is_object() && m.contains("massValue") && m["massValue"].is_number() && m.contains("massExponent") && m["massExponent"].is_number_integer()) {
                double value=m["massValue"].get<double>(); double exponent=m["massExponent"].get<double>();
                if(std::isfinite(value) && value>0 && exponent>=std::numeric_limits<int>::min() && exponent<=std::numeric_limits<int>::max()) { b.mass=Mass{value,m["massExponent"].get<int>()}; valid=true; }
            }
            if(!valid) result.warnings.push_back(prefix+"mass inválida");
        }
        result.bodies.push_back(std::move(b));
    }
    return result;
}
}
