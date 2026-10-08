#include "cronicas/TerminalUI.hpp"
#include <iostream>
int main(int argc, char* argv[]) {
    try {
        cronicas::BodyCatalog catalog;
        cronicas::CurlHttpClient http;
        cronicas::SolarApiClient api(http);
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout
                << "Uso: missoes [--json arquivo.json]\nSem argumento, utilize o menu para carregar a API.\n";
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--json") {
            catalog.load(cronicas::readLocalJson(argv[2]),
                         "JSON local explícito");
            std::cout << "Dados locais carregados; não houve consumo da API.\n";
        } else if (argc != 1) {
            std::cerr << "Uso: missoes [--json arquivo.json]\n";
            return 1;
        }
        cronicas::TerminalUI ui(catalog, api, std::cin, std::cout);
        ui.run();
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << '\n';
        return 1;
    }
}
