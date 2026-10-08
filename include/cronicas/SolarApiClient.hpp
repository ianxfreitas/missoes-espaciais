#pragma once
#include "HttpClient.hpp"
#include "BodyJsonParser.hpp"
namespace cronicas {
class SolarApiClient {
    const HttpClient& http_;

   public:
    explicit SolarApiClient(const HttpClient& http) : http_(http) {}
    ParseResult fetch() const;
};
ParseResult readLocalJson(const std::string& path);
}  // namespace cronicas
