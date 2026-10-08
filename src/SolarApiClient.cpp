#include "cronicas/SolarApiClient.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace cronicas {
ParseResult SolarApiClient::fetch() const {
    const char* key=std::getenv("SOLAR_API_KEY");
    if(!key || !*key) throw std::runtime_error("Defina SOLAR_API_KEY para carregar a API");
    const std::string token(key);
    if(token.find_first_of("\r\n")!=std::string::npos) throw std::runtime_error("Token contém caracteres inválidos");
    auto response=http_.get("https://api.le-systeme-solaire.net/rest/bodies/",token);
    if(response.status==401 || response.status==403) throw std::runtime_error("Autenticação recusada pela API");
    if(response.status==429) throw std::runtime_error("Limite de requisições: tente novamente mais tarde");
    if(response.status!=200) throw std::runtime_error("API retornou HTTP "+std::to_string(response.status));
    return parseBodies(response.body);
}
ParseResult readLocalJson(const std::string& path) {
    std::ifstream file(path); if(!file) throw std::runtime_error("Não foi possível abrir JSON local");
    std::ostringstream content; content<<file.rdbuf();
    if(file.bad()) throw std::runtime_error("Falha ao ler JSON local");
    return parseBodies(content.str());
}
}
