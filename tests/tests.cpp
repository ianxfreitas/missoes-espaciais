#include "cronicas/CelestialBody.hpp"
#include "cronicas/SolarApiClient.hpp"
#include "cronicas/HashTable.hpp"
#include "cronicas/BodyCatalog.hpp"
#include "cronicas/TrieIndex.hpp"
#include "cronicas/BTreeIndex.hpp"
#include "cronicas/MissionNetwork.hpp"
#include <type_traits>
#include "cronicas/MissionPlanner.hpp"
#include <limits>
#include <cmath>
#include "cronicas/TerminalUI.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
using namespace cronicas;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    std::vector<std::pair<std::string, std::function<void()>>> tests;
    tests.push_back({"modelo", [] {
                         CelestialBody b;
                         b.id = "terre";
                         check(displayName(b) == "terre", "nome alternativo");
                         check(!b.gravity, "ausência distinta de zero");
                     }});
    tests.push_back(
        {"parsing", [] {
             auto r = parseBodies(
                 R"({"bodies":[{"id":"x","mass":null,"gravity":0},{"id":"y","gravity":"bad"},{"englishName":"sem id"}]})");
             check(r.bodies.size() == 2, "registros válidos");
             check(r.bodies[0].gravity == 0, "zero preservado");
             check(!r.bodies[0].mass, "massa nula");
             check(!r.bodies[1].gravity, "tipo inválido");
             check(r.warnings.size() == 2 && r.rejected == 1,
                   "diagnósticos e rejeição");
             for (auto text : {"{", "[]", "{}"}) {
                 bool threw = false;
                 try {
                     parseBodies(text);
                 } catch (const std::runtime_error&) {
                     threw = true;
                 }
                 check(threw, "rejeição de JSON");
             }
         }});
    tests.push_back(
        {"falhas API", [] {
             struct Fake : HttpClient {
                 long status = 200;
                 std::string text = R"({"bodies":[{"id":"x"}]})";
                 bool fail = false;
                 HttpResponse get(const std::string&,
                                  const std::string&) const override {
                     if (fail) throw std::runtime_error("rede simulada");
                     return {status, text};
                 }
             } fake;
             const char* previous = std::getenv("SOLAR_API_KEY");
             const std::optional<std::string> saved =
                 previous ? std::optional<std::string>(previous) : std::nullopt;
             setenv("SOLAR_API_KEY", "token-de-teste", 1);
             SolarApiClient api(fake);
             check(api.fetch().bodies.size() == 1, "API simulada");
             for (long status : {401, 403, 429, 500, 302}) {
                 fake.status = status;
                 bool threw = false;
                 try {
                     api.fetch();
                 } catch (const std::runtime_error&) {
                     threw = true;
                 }
                 check(threw, "status rejeitado");
             }
             fake.status = 200;
             fake.text = "invalid";
             bool threw = false;
             try {
                 api.fetch();
             } catch (const std::runtime_error&) {
                 threw = true;
             }
             check(threw, "JSON API inválido");
             fake.fail = true;
             threw = false;
             try {
                 api.fetch();
             } catch (const std::runtime_error&) {
                 threw = true;
             }
             check(threw, "rede");
             unsetenv("SOLAR_API_KEY");
             threw = false;
             try {
                 api.fetch();
             } catch (const std::runtime_error&) {
                 threw = true;
             }
             check(threw, "token ausente");
             if (saved) setenv("SOLAR_API_KEY", saved->c_str(), 1);
         }});
    tests.push_back(
        {"hash e colisões", [] {
             HashTable table(16);
             CelestialBody b;
             b.id = "a";
             check(table.insert(b), "inserção");
             check(table.find("a"), "busca existente");
             check(!table.find("missing"), "busca ausente");
             b.englishName = "updated";
             check(!table.insert(b), "duplicata");
             check(table.size() == 1 && table.collisions() == 0,
                   "duplicata não conta");
             check(table.find("a")->englishName == "updated", "atualização");
             std::string colliding;
             for (int i = 0; i < 1000; ++i) {
                 auto key = "key" + std::to_string(i);
                 if (HashTable::hashKey(key) % 16 ==
                     HashTable::hashKey("a") % 16) {
                     colliding = key;
                     break;
                 }
             }
             check(!colliding.empty(), "chave controlada");
             b.id = colliding;
             table.insert(b);
             check(table.collisions() == 1, "colisão exata");
             check(table.loadFactor() == 2.0 / 16, "fator de carga");
             check(HashTable::hashKey("a") == 12638187200555641996ULL,
                   "FNV determinístico");
         }});
    tests.push_back(
        {"rehashing e integridade", [] {
             HashTable t(4);
             CelestialBody b;
             std::vector<std::string> keys;
             // Chaves no mesmo balde também após a primeira expansão.
             for (int i = 0; keys.size() < 4; ++i) {
                 auto k = "collision" + std::to_string(i);
                 if (HashTable::hashKey(k) % 8 == 0) keys.push_back(k);
             }
             for (int i = 0; i < 3; ++i) {
                 b.id = keys[i];
                 t.insert(b);
             }
             check(t.capacity() == 4 && t.collisions() == 2,
                   "limiar inclusivo");
             const auto* stable = t.find(keys[0]);
             b.id = keys[3];
             t.insert(b);
             check(stable == t.find(keys[0]), "ponteiro estável após rehash");
             check(t.capacity() == 8 && t.rehashes() == 1, "expansão");
             check(t.collisions() == 3, "rehash não conta colisões");
             for (int i = 0; i < 1000; ++i) {
                 b.id = "body" + std::to_string(i);
                 b.meanRadius = i;
                 t.insert(b);
             }
             for (int i = 0; i < 1000; ++i) {
                 auto found = t.find("body" + std::to_string(i));
                 check(found && found->meanRadius == i, "integridade");
             }
             auto stats = t.statistics();
             check(stats.elements == 1004 && stats.loadFactor <= 0.75,
                   "estatísticas");
             HashTable moved(std::move(t));
             check(moved.size() == 1004 && !t.find("body0"), "movimentação");
             b.id = "reused";
             t.insert(b);
             check(t.size() == 1, "objeto movido reutilizável");
             t = std::move(moved);
             check(t.size() == 1004, "atribuição por movimento");
             bool threw = false;
             try {
                 HashTable invalid(0);
             } catch (const std::invalid_argument&) {
                 threw = true;
             }
             check(threw, "capacidade zero");
             b.id = "";
             threw = false;
             try {
                 t.insert(b);
             } catch (const std::invalid_argument&) {
                 threw = true;
             }
             check(threw, "id vazio");
         }});
    tests.push_back(
        {"catálogo pesquisa filtros e carga", [] {
             BodyCatalog c;
             check(c.list().empty(), "vazio");
             auto parsed = parseBodies(
                 R"({"bodies":[{"id":"terre","englishName":"Earth","bodyType":"Planet","isPlanet":true,"gravity":9.8},{"id":"lune","englishName":"Moon","bodyType":"Moon","isPlanet":false},{"id":"terre","englishName":"Earth","bodyType":"Planet","isPlanet":true,"gravity":9.80665}]})");
             auto s = c.load(parsed, "teste");
             check(s.inserted == 2 && s.updated == 1, "duplicatas na carga");
             check(c.searchName("EAR").size() == 1, "pesquisa sem maiúsculas");
             check(c.filterType("moon").size() == 1, "tipo");
             check(c.filterPlanet(true).size() == 1, "planeta");
             check(c.filterRange("gravity", 9, 10).size() == 1,
                   "intervalo e ausência");
             bool threw = false;
             try {
                 c.load(ParseResult{}, "inválido");
             } catch (const std::runtime_error&) {
                 threw = true;
             }
             check(threw && c.find("terre") && c.source() == "teste",
                   "carga transacional");
             for (auto field : {"gravity", "bad"}) {
                 threw = false;
                 try {
                     c.filterRange(field, 10, 9);
                 } catch (const std::invalid_argument&) {
                     threw = true;
                 }
                 check(threw, "intervalo inválido");
             }
             c.load(parsed, "recarga");
             check(c.statistics().elements == 2, "recarga substitui");
         }});
    tests.push_back({"interfaces Parte 2", [] {
                         static_assert(std::is_abstract_v<TrieIndex> &&
                                       std::is_abstract_v<BTreeIndex>);
                         static_assert(
                             std::has_virtual_destructor_v<TrieIndex> &&
                             std::has_virtual_destructor_v<BTreeIndex>);
                     }});
    tests.push_back({"trie inserção, busca e prefixos", [] {
                         auto trie = makeTrieIndex();
                         trie->insert("Mar", "z");
                         trie->insert("Mars", "b");
                         trie->insert("Mars", "a");
                         trie->insert("Mars", "a");
                         trie->insert("Marsupial", "a");
                         trie->insert("Moon", "moon");
                         check(trie->exact("Mars") ==
                                   std::vector<std::string>{"a", "b"},
                               "IDs distintos para o mesmo nome e par idempotente");
                         check(trie->lastVisitedNodes() == 5, "raiz e quatro letras");
                         check(trie->exact("Ma").empty(), "nó sem ID próprio");
                         check(trie->prefix("Mar") ==
                                   std::vector<std::string>{"a", "b", "z"},
                               "inclui o próprio prefixo e deduplica IDs");
                         check(trie->lastVisitedNodes() == 10,
                               "caminho e subárvore contados uma vez");
                         check(trie->prefix("Max").empty() &&
                                   trie->lastVisitedNodes() == 3,
                               "prefixo ausente conta até o último nó encontrado");
                         check(trie->exact("mars").empty(), "chaves em bytes");
                     }});
    tests.push_back({"trie remoção de pares e poda", [] {
                         auto trie = makeTrieIndex();
                         trie->insert("Mar", "root");
                         trie->insert("Mars", "a");
                         trie->insert("Mars", "b");
                         trie->insert("Moon", "moon");
                         check(!trie->erase("Mars", "missing") &&
                                   !trie->erase("Unknown", "a"),
                               "remoção de associação ausente");
                         check(trie->erase("Mar", "root") &&
                                   trie->exact("Mar").empty(),
                               "remove o nome intermediário");
                         check(trie->erase("Mars", "a") &&
                                   trie->exact("Mars") == std::vector<std::string>{"b"},
                               "preserva outro ID no nome e o ramo descendente");
                         check(trie->erase("Mars", "b") &&
                                   trie->lastVisitedNodes() == 5,
                               "remoção preserva a métrica da última busca");
                         check(trie->prefix("Mar").empty() &&
                                   trie->lastVisitedNodes() == 2,
                               "poda o ramo vazio até a bifurcação");
                         check(trie->exact("Moon") == std::vector<std::string>{"moon"},
                               "preserva outro ramo");
                         trie->insert("Mars", "a");
                         check(trie->exact("Mars") == std::vector<std::string>{"a"},
                               "reinserção após a poda");
                     }});
    tests.push_back({"trie entradas vazias", [] {
                         auto trie = makeTrieIndex();
                         check(trie->lastVisitedNodes() == 0, "métrica inicial");
                         check(trie->exact("").empty() && trie->lastVisitedNodes() == 1,
                               "busca exata vazia visita a raiz sem encontrar IDs");
                         check(trie->prefix("").empty(), "prefixo vazio em trie vazia");
                         for (auto name : {"", "Mars"}) {
                             auto id = *name ? "" : "mars";
                             bool threw = false;
                             try {
                                 trie->insert(name, id);
                             } catch (const std::invalid_argument&) {
                                 threw = true;
                             }
                             check(threw && !trie->erase(name, id),
                                   "nome ou ID vazio");
                         }
                         check(trie->prefix("").empty() && trie->lastVisitedNodes() == 1,
                               "inserção inválida não cria nós");
                         trie->insert("Car", "car");
                         trie->insert("Cat", "cat");
                         check(trie->lastVisitedNodes() == 1,
                               "inserção preserva a métrica da última busca");
                         check(trie->prefix("") == std::vector<std::string>{"car", "cat"} &&
                                   trie->lastVisitedNodes() == 5,
                               "prefixo vazio visita a trie toda");
                     }});
    tests.push_back(
        {"catálogo com trie e recarga", [] {
             BodyCatalog c;
             check(c.searchNameExact("Earth").empty() &&
                       c.searchNamePrefix("").empty() &&
                       c.lastNameSearchVisitedNodes() == 0,
                   "catálogo antes da primeira carga");
             auto parsed = parseBodies(R"({"bodies":[
                 {"id":"terre","englishName":"Terra antiga"},
                 {"id":"clone","englishName":"eArTh"},
                 {"id":"terre","englishName":"Earth"},
                 {"id":"mars"}
             ]})");
             auto summary = c.load(parsed, "original");
             check(summary.inserted == 3 && summary.updated == 1,
                   "carga com atualização de ID");
             const std::vector<const CelestialBody*> earth{c.find("clone"),
                                                           c.find("terre")};
             check(earth[0] && earth[1] && c.searchNameExact("eARtH") == earth &&
                       c.lastNameSearchVisitedNodes() == 6,
                   "busca exata normalizada retorna registros da Hash por ID");
             check(c.searchNamePrefix("EA") == earth && c.searchName("art") == earth,
                   "prefixo e trecho continuam disponíveis");
             check(c.searchNameExact("Terra antiga").empty() &&
                       c.searchNamePrefix("Ter").empty(),
                   "nome anterior do ID atualizado fica fora do índice");
             const std::vector<const CelestialBody*> mars{c.find("mars")};
             check(mars.front() && c.searchNameExact("MARS") == mars,
                   "nome de exibição usa ID quando englishName está ausente");
             auto replacement = parseBodies(
                 R"({"bodies":[{"id":"venus","englishName":"Venus"}]})");
             auto invalid = replacement;
             invalid.bodies.emplace_back();
             bool threw = false;
             try {
                 c.load(invalid, "falha");
             } catch (const std::invalid_argument&) {
                 threw = true;
             }
             check(threw && c.source() == "original" &&
                       c.searchNamePrefix("ea") == earth &&
                       !c.find("venus") && c.searchNameExact("Venus").empty(),
                   "falha durante a carga preserva Hash, Trie e origem");
             c.load(replacement, "recarga");
             const std::vector<const CelestialBody*> venus{c.find("venus")};
             check(c.source() == "recarga" && c.searchNamePrefix("ea").empty() &&
                       venus.front() && c.searchNamePrefix("Ve") == venus,
                   "recarga troca os registros e o índice");
         }});
    tests.push_back({"árvore B inserção e intervalos inclusivos", [] {
                         auto tree = makeBTreeIndex();
                         check(tree->height() == 0 && tree->splits() == 0 &&
                                   tree->range(-2, 2).empty() &&
                                   !tree->erase(0, "ausente"),
                               "árvore vazia");
                         tree->insert(2, "z");
                         tree->insert(-2, "negative");
                         tree->insert(0, "zero");
                         tree->insert(2, "z");
                         check(tree->height() == 1 && tree->splits() == 0,
                               "par duplicado não divide a raiz cheia");
                         tree->insert(2, "a");
                         tree->insert(2, "c");
                         check(tree->height() == 2 && tree->splits() == 1,
                               "divisão da raiz");
                         check(tree->exact(2) == std::vector<std::string>{"a", "c", "z"},
                               "valor repetido em mais de um nó");
                         check(tree->range(-2, 2) == std::vector<std::string>{
                                                         "negative", "zero", "a", "c", "z"},
                               "intervalo inclusivo na ordem de valor e ID");
                         check(tree->exact(-0.0) == std::vector<std::string>{"zero"} &&
                                   tree->range(-1, -0.5).empty(),
                               "zero e intervalo sem resultados");
                         check(tree->erase(2, "c") && !tree->erase(2, "c") &&
                                   tree->exact(2) == std::vector<std::string>{"a", "z"},
                               "remoção de um par preserva os demais IDs");
                     }});
    tests.push_back({"árvore B redistribuição, fusão e raiz", [] {
                         struct Scenario {
                             std::vector<int> order;
                             int first, second;
                         };
                         const std::vector<Scenario> scenarios = {
                             {{1, 2, 3, 4}, 1, 2},  // Empréstimo da direita.
                             {{4, 3, 2, 1}, 4, 3},  // Empréstimo da esquerda.
                             {{1, 2, 3, 4}, 2, 3},  // Sucessor da raiz.
                             {{4, 3, 2, 1}, 3, 2},  // Predecessor da raiz.
                         };
                         for (const auto& scenario : scenarios) {
                             auto tree = makeBTreeIndex();
                             for (int n : scenario.order) tree->insert(n, std::to_string(n));
                             check(tree->height() == 2 && tree->splits() == 1,
                                   "raiz dividida: um filho com excedente");
                             check(tree->erase(scenario.first, std::to_string(scenario.first)) &&
                                       tree->height() == 2 && tree->range(1, 4).size() == 3 &&
                                       tree->exact(scenario.first).empty(),
                                   "redistribuição ou substituição preserva a altura");
                             check(tree->erase(scenario.second, std::to_string(scenario.second)) &&
                                       tree->height() == 1 && tree->splits() == 1,
                                   "fusão de filhos mínimos reduz a altura");
                             std::vector<std::string> remaining;
                             for (int n : scenario.order) {
                                 if (n != scenario.first && n != scenario.second)
                                     remaining.push_back(std::to_string(n));
                             }
                             std::sort(remaining.begin(), remaining.end());
                             check(tree->range(1, 4) == remaining, "pares restantes após fusão");
                             for (const auto& id : remaining)
                                 check(tree->erase(std::stoi(id), id), "remove da raiz folha");
                             check(tree->height() == 0 && tree->splits() == 1 &&
                                       tree->range(1, 4).empty(),
                                   "última remoção esvazia a árvore e preserva a contagem");
                         }
                     }});
    tests.push_back({"árvore B multinível comparada com referência", [] {
                         auto tree = makeBTreeIndex();
                         std::vector<std::pair<double, std::string>> reference;
                         const auto expected = [&](double minimum, double maximum) {
                             std::vector<std::string> ids;
                             for (const auto& key : reference) {
                                 if (key.first >= minimum && key.first <= maximum)
                                     ids.push_back(key.second);
                             }
                             return ids;
                         };
                         constexpr int count = 128;
                         for (int i = 0; i < count; ++i) {
                             const double value = i % 11 - 5;
                             const auto id = std::to_string(i);
                             reference.emplace_back(value, id);
                             tree->insert(value, id);
                         }
                         std::sort(reference.begin(), reference.end());
                         const auto splits = tree->splits();
                         check(tree->height() > 2 && splits > 1 &&
                                   tree->range(-5, 5) == expected(-5, 5),
                               "divisões internas e ordem global das chaves");
                         for (int i = 0; i < count; ++i) {
                             const int n = (i * 37) % count;
                             const auto key = std::make_pair(double(n % 11 - 5), std::to_string(n));
                             const auto previousHeight = tree->height();
                             check(tree->erase(key.first, key.second), "remoção multinível");
                             reference.erase(std::lower_bound(reference.begin(), reference.end(), key));
                             check(tree->height() <= previousHeight && tree->splits() == splits &&
                                       tree->range(-5, 5) == expected(-5, 5) &&
                                       tree->range(-2, 3) == expected(-2, 3) &&
                                       tree->exact(key.first) == expected(key.first, key.first),
                                   "métricas e consultas após cada remoção");
                         }
                         check(tree->height() == 0, "reduções sucessivas da raiz");
                         tree->insert(-2, "novo");
                         check(tree->height() == 1 && tree->exact(-2).size() == 1,
                               "reutiliza a árvore após esvaziá-la");
                     }});
    tests.push_back({"árvore B entradas inválidas", [] {
                         auto tree = makeBTreeIndex();
                         const auto rejects = [](auto operation) {
                             try {
                                 operation();
                             } catch (const std::invalid_argument&) {
                                 return true;
                             }
                             return false;
                         };
                         const auto infinity = std::numeric_limits<double>::infinity();
                         tree->insert(0, "zero");
                         for (double value : {infinity, -infinity,
                                              std::numeric_limits<double>::quiet_NaN()}) {
                             check(rejects([&] { tree->insert(value, "a"); }) &&
                                       rejects([&] { tree->erase(value, "a"); }) &&
                                       rejects([&] { tree->exact(value); }) &&
                                       rejects([&] { tree->range(value, 1); }) &&
                                       rejects([&] { tree->range(0, value); }),
                                   "valores não finitos rejeitados em todas as operações");
                         }
                         check(rejects([&] { tree->range(1, 0); }) &&
                                   rejects([&] { tree->insert(0, ""); }) &&
                                   !tree->erase(0, "") &&
                                   tree->exact(0) == std::vector<std::string>{"zero"},
                               "intervalo invertido e ID vazio preservam a árvore");
                     }});
    tests.push_back(
        {"catálogo com árvore B e recarga", [] {
             BodyCatalog c;
             check(c.numericIndexStatistics()[0].height == 0 &&
                       c.filterRange("gravity", 0, 0).empty(),
                   "catálogo antes da primeira carga");
             auto parsed = parseBodies(R"({"bodies":[
                 {"id":"z","englishName":"Old","gravity":10,"meanRadius":99},
                 {"id":"b","gravity":0,"meanRadius":1,"avgTemp":180},
                 {"id":"a","gravity":2,"meanRadius":3,"avgTemp":180},
                 {"id":"z","englishName":"Zed","gravity":0,"meanRadius":2,"avgTemp":200},
                 {"id":"mid","gravity":3},
                 {"id":"absent"},
                 {"id":"negative","gravity":-1,"meanRadius":-1,"avgTemp":-1}
             ]})");
             auto summary = c.load(parsed, "original");
             const std::vector<const CelestialBody*> zeros{c.find("b"), c.find("z")};
             check(summary.updated == 1 && summary.warnings.size() == 3 &&
                       c.filterRange("gravity", 0, 0) == zeros,
                   "valores finais, zeros, repetidos e atributos ausentes ou inválidos");
             check(c.filterRange("gravity", 0, 2) ==
                       std::vector<const CelestialBody*>{c.find("a"), c.find("b"), c.find("z")} &&
                       c.filterRange("meanRadius", 1, 2) == zeros &&
                       c.filterRange("meanRadius", 99, 99).empty() &&
                       c.filterRange("avgTemp", 180, 180) ==
                           std::vector<const CelestialBody*>{c.find("a"), c.find("b")},
                   "três índices independentes e resultados ordenados por ID");
             const auto stats = c.numericIndexStatistics();
             check(stats[0].height == 2 && stats[0].splits == 1 &&
                       stats[1].height == 1 && stats[1].splits == 0 &&
                       stats[2].height == 1 && stats[2].splits == 0,
                   "métricas por atributo");
             for (const auto* field : {"gravity", "avgTemp", "meanRadius", "bad"}) {
                 bool threw = false;
                 try {
                     c.filterRange(field, field == std::string("bad") ? 0 : -1, 2);
                 } catch (const std::invalid_argument&) {
                     threw = true;
                 }
                 check(threw, "campo desconhecido ou limite físico negativo");
             }
             auto replacement = parseBodies(R"({"bodies":[{"id":"venus","meanRadius":2}]})");
             auto invalid = replacement;
             invalid.bodies.front().meanRadius = std::numeric_limits<double>::infinity();
             bool threw = false;
             try {
                 c.load(invalid, "falha");
             } catch (const std::invalid_argument&) {
                 threw = true;
             }
             check(threw && c.source() == "original" && !c.find("venus") &&
                       c.filterRange("gravity", 0, 0) == zeros &&
                       c.numericIndexStatistics()[0].splits == 1 &&
                       c.searchNamePrefix("ze") == std::vector<const CelestialBody*>{c.find("z")},
                   "falha ao construir índices preserva Hash, Trie, árvores e origem");
             c.load(replacement, "recarga");
             check(c.filterRange("gravity", 0, 0).empty() &&
                       c.filterRange("meanRadius", 2, 2) ==
                           std::vector<const CelestialBody*>{c.find("venus")} &&
                       c.numericIndexStatistics()[0].height == 0 &&
                       c.numericIndexStatistics()[0].splits == 0,
                   "recarga substitui os índices e reinicia as métricas");
         }});
    tests.push_back(
        {"guloso e contraexemplo", [] {
             std::vector<MissionCandidate> c = {
                 {"a", "A", 12, 6}, {"b", "B", 9, 5}, {"c", "C", 9, 5}};
             auto p = planMissions(c, 10, 2);
             check(p.selected.size() == 1 && p.selected[0].id == "a" &&
                       p.totalBenefit == 12,
                   "guloso");
             check(c[1].cost + c[2].cost == 10 &&
                       c[1].benefit + c[2].benefit > p.totalBenefit,
                   "solução melhor 18");
             check(planMissions(c, 0, 2).selected.empty(), "orçamento zero");
             check(planMissions(c, 100, 0).selected.empty(), "limite zero");
             check(planMissions({}, 10, 2).selected.empty(), "sem candidatos");
             p = planMissions(c, 16, 3);
             check(p.totalCost == 16 && p.selected.size() == 3,
                   "fronteira orçamento");
             auto tied = planMissions(
                 {{"z", "Z", 10, 5}, {"a", "A", 10, 5}, {"x", "X", 4, 2}}, 20,
                 3);
             check(tied.selected[0].id == "x" && tied.selected[1].id == "a",
                   "desempates");
             for (double bad :
                  {-1.0, std::numeric_limits<double>::infinity()}) {
                 bool threw = false;
                 try {
                     planMissions(c, bad, 1);
                 } catch (const std::invalid_argument&) {
                     threw = true;
                 }
                 check(threw, "orçamento inválido");
             }
             bool threw = false;
             try {
                 planMissions({{"a", "", 1, 0}}, 10, 1);
             } catch (const std::invalid_argument&) {
                 threw = true;
             }
             check(threw, "custo zero");
             threw = false;
             try {
                 planMissions({c[0], c[0]}, 10, 2);
             } catch (const std::invalid_argument&) {
                 threw = true;
             }
             check(threw, "duplicado");
         }});
    tests.push_back(
        {"programação dinâmica e comparação com guloso", [] {
             const std::vector<MissionCandidate> candidates = {
                 {"c", "C", 9, 5}, {"a", "A", 12, 6}, {"b", "B", 9, 5}};
             const auto greedy = planMissions(candidates, 10, 2);
             const auto optimal = optimizeMissions(candidates, 10, 2);
             check(greedy.totalBenefit == 12 && optimal.totalBenefit == 18 &&
                       optimal.totalCost == 10 && optimal.selected.size() == 2 &&
                       optimal.selected[0].id == "b" && optimal.selected[1].id == "c",
                   "PD supera o guloso com o mesmo orçamento e limite");
             const auto all = optimizeMissions(candidates, 16, 3);
             const auto greedyAll = planMissions(candidates, 16, 3);
             check(all.selected.size() == 3 && all.totalBenefit == 30 &&
                       all.totalBenefit == greedyAll.totalBenefit &&
                       all.totalCost == greedyAll.totalCost,
                   "ambos selecionam as três missões quando cabem");
             const auto one = optimizeMissions(candidates, 10, 1);
             check(one.selected.size() == 1 && one.selected.front().id == "a",
                   "limite impede seleção de duas missões");
             const auto tied = optimizeMissions(candidates, 100, 2);
             check(tied.selected.size() == 2 && tied.selected[0].id == "a" &&
                       tied.selected[1].id == "b",
                   "custos e benefícios repetidos com desempate determinístico");
             check(optimizeMissions({{"a", "", 8, 2}, {"b", "", 4, 1},
                                     {"c", "", 4, 1}}, 2, 2).selected.size() == 1,
                   "mesmo benefício favorece menos missões");
             const auto cheapest = optimizeMissions(
                 {{"a", "", 5, 3}, {"b", "", 5, 2}}, 10, 1);
             check(cheapest.selected.size() == 1 && cheapest.selected.front().id == "b",
                   "mesmo benefício e quantidade favorecem custo discretizado menor");
         }});
    tests.push_back(
        {"programação dinâmica com custos decimais e plano vazio", [] {
             const auto decimals = optimizeMissions(
                 {{"a", "", 3, 0.10}, {"b", "", 4, 0.20}}, 0.30, 2);
             check(decimals.totalCost == 0.30 && decimals.totalBenefit == 7 &&
                       decimals.selected.size() == 2,
                   "centésimos exatos cabem no limite decimal");
             const std::vector<MissionCandidate> boundary = {{"a", "", 1, 0.29}};
             check(optimizeMissions(boundary, 0.29, 1).selected.size() == 1 &&
                       optimizeMissions(boundary, std::nextafter(0.29, 0.0), 1)
                           .selected.empty(),
                   "orçamento exatamente na fronteira ou logo abaixo dela");
             const auto justAbove = std::nextafter(0.29, 1.0);
             check(optimizeMissions({{"a", "", 1, justAbove}}, 0.299, 1)
                       .selected.empty(),
                   "custo acima da fronteira arredonda para cima");
             const std::vector<MissionCandidate> fractional = {
                 {"a", "", 4, 1.001}, {"b", "", 4, 1.001}};
             const auto rounded = optimizeMissions(fractional, 2.019, 2);
             check(rounded.selected.size() == 1 && rounded.totalCost == 1.001 &&
                       planMissions(fractional, 2.019, 2).selected.size() == 2,
                   "arredondamento conservador pode excluir combinação do guloso");
             const std::vector<MissionCandidate> noBenefit = {{"z", "", 0, 1}};
             const std::vector<MissionCandidate> subcent = {{"a", "", 1, 0.001}};
             check(optimizeMissions(fractional, 2.02, 2).selected.size() == 2 &&
                       optimizeMissions(fractional, 1.00, 2).selected.empty() &&
                       optimizeMissions(fractional, 10, 0).selected.empty() &&
                       optimizeMissions({}, 10, 3).selected.empty() &&
                       optimizeMissions(noBenefit, 10, 3).selected.empty(),
                   "fronteira, orçamento insuficiente, limite zero e benefício zero");
             check(optimizeMissions(fractional, 0, 2).selected.empty() &&
                       optimizeMissions(subcent, 0.009, 1).selected.empty(),
                   "orçamento abaixo de um centésimo");
             const auto tinyCost = std::numeric_limits<double>::min();
             const auto tiny = optimizeMissions({{"a", "", 5, tinyCost}}, 0.01, 1);
             check(tiny.totalBenefit == 5 && tiny.totalCost == tinyCost,
                   "PD aceita custo minúsculo sem calcular razão benefício/custo");
         }});
    tests.push_back(
        {"programação dinâmica comparada com enumeração exaustiva", [] {
             for (size_t n = 0; n <= 10; ++n) {
                 std::vector<MissionCandidate> candidates;
                 std::vector<size_t> costs;
                 for (size_t i = 0; i < n; ++i) {
                     const size_t units = (7 * i + n) % 11 + 1;
                     const double cost = (10 * units - (i % 3 == 0 ? 4 : 0)) / 1000.0;
                     costs.push_back(units);
                     candidates.push_back({std::string(1, static_cast<char>('a' + i)),
                                           "", static_cast<double>((i + n) * 3 % 7), cost});
                 }
                 for (size_t budget : {0, 1, 5, 9, 16, 25, 40}) {
                     for (size_t limit : {0, 1, 3, 10}) {
                         double bestBenefit = 0;
                         size_t bestCount = 0, bestCost = 0;
                         for (size_t mask = 0; mask < (size_t{1} << n); ++mask) {
                             size_t count = 0, cost = 0;
                             double benefit = 0;
                             for (size_t i = 0; i < n; ++i) {
                                 if (!(mask & (size_t{1} << i))) continue;
                                 ++count;
                                 cost += costs[i];
                                 benefit += candidates[i].benefit;
                             }
                             if (count > limit || cost > budget) continue;
                             if (benefit > bestBenefit ||
                                 (benefit == bestBenefit &&
                                  std::make_pair(count, cost) < std::make_pair(bestCount, bestCost))) {
                                 bestBenefit = benefit;
                                 bestCount = count;
                                 bestCost = cost;
                             }
                         }

                         const auto available = (10 * budget + 7) / 1000.0;
                         const auto plan = optimizeMissions(candidates, available, limit);
                         check(plan.totalBenefit == bestBenefit &&
                                   plan.selected.size() == bestCount && plan.totalCost <= available,
                               "ótimo e viabilidade coincidem com enumeração");
                         size_t mask = 0, usedCost = 0;
                         double benefit = 0, cost = 0;
                         for (const auto& candidate : plan.selected) {
                             const auto i = static_cast<size_t>(candidate.id.front() - 'a');
                             check(i < n && !(mask & (size_t{1} << i)),
                                   "reconstrução seleciona cada candidato no máximo uma vez");
                             mask |= size_t{1} << i;
                             usedCost += costs[i];
                             benefit += candidate.benefit;
                             cost += candidate.cost;
                         }
                         check(usedCost == bestCost && benefit == bestBenefit &&
                                   std::abs(cost - plan.totalCost) < 1e-12,
                               "itens reconstruídos produzem os totais e desempates esperados");
                     }
                 }
             }
         }});
    tests.push_back(
        {"programação dinâmica validação e limite de memória", [] {
             const MissionCandidate valid{"a", "", 1, 1};
             const auto invalid = [](auto operation) {
                 try {
                     operation();
                 } catch (const std::invalid_argument&) {
                     return true;
                 }
                 return false;
             };
             const auto infinity = std::numeric_limits<double>::infinity();
             for (const auto budget : {-1.0, infinity,
                                       std::numeric_limits<double>::quiet_NaN()})
                 check(invalid([&] { optimizeMissions({valid}, budget, 1); }),
                       "orçamento inválido");
             check(invalid([&] { optimizeMissions({valid, valid}, 1, 2); }) &&
                       invalid([] { optimizeMissions({{"", "", 1, 1}}, 1, 1); }) &&
                       invalid([] { optimizeMissions({{"a", "", 1, 0}}, 1, 1); }) &&
                       invalid([] { optimizeMissions({{"a", "", -1, 1}}, 1, 1); }),
                   "candidatos inválidos e IDs duplicados");

             const auto max = std::numeric_limits<double>::max();
             const auto available = optimizeMissions({valid}, max,
                                                       std::numeric_limits<size_t>::max());
             check(available.totalBenefit == 1 && available.totalCost == 1,
                   "dimensões efetivas limitadas pelos candidatos e custos");
             check(optimizeMissions({{"a", "", 1, max}}, 10, 1).selected.empty(),
                   "custo fora do orçamento é descartado antes da conversão");
             std::vector<MissionCandidate> many;
             for (size_t i = 0; i < 200; ++i)
                 many.push_back({std::to_string(i), "", 1, 5});
             for (const auto& candidates :
                  {std::vector<MissionCandidate>{{"a", "", 1, max}},
                   std::vector<MissionCandidate>{{"a", "", 1, 50000}}, many}) {
                 bool rejected = false;
                 try {
                     optimizeMissions(candidates, max, 50);
                 } catch (const std::length_error&) {
                     rejected = true;
                 }
                 check(rejected, "limites do custo, da tabela ou da reconstrução");
             }
             bool overflow = false;
             try {
                 optimizeMissions({{"a", "", max, 1}, {"b", "", max, 1}}, 2, 2);
             } catch (const std::overflow_error&) {
                 overflow = true;
             }
             check(overflow, "benefício acumulado fora da faixa numérica");
         }});
    tests.push_back(
        {"menu programação dinâmica e comparação", [] {
             BodyCatalog catalog;
             catalog.load(parseBodies(R"({"bodies":[
                 {"id":"a","englishName":"A","bodyType":"Dwarf Planet","meanRadius":100,"gravity":19.6133},
                 {"id":"b","englishName":"B","bodyType":"Moon","meanRadius":100,"gravity":4.903325},
                 {"id":"c","englishName":"C","bodyType":"Moon","meanRadius":100,"gravity":4.903325}
             ]})"), "teste");
             CurlHttpClient http;
             SolarApiClient api(http);
             std::istringstream input("10\n25.02\n2\n10\n1\n2\n10\n-1\n9\n");
             std::ostringstream output;
             TerminalUI(catalog, api, input, output).run();
             const auto text = output.str();
             const auto comparison = text.find("Guloso no mesmo cenário");
             check(comparison != std::string::npos &&
                       text.find("b | B | benefício") < comparison &&
                       text.find("c | C | benefício") < comparison &&
                       text.find("Missões: 2 / 2") < comparison &&
                       text.find("a | A | benefício", comparison) != std::string::npos,
                   "mesmos candidatos e cenário exibem planos diferentes");
             check(text.find("Benefício total: 15.20412") != std::string::npos &&
                       text.find("Benefício total: 10.60206") != std::string::npos &&
                       text.find("Orçamento restante:") != std::string::npos &&
                       text.find("Nenhuma missão selecionada") != std::string::npos &&
                       text.find("Erro: Digite um número finito não negativo") != std::string::npos &&
                       text.find("Encerrado.") != std::string::npos,
                   "totais, cenário sem seleção e recuperação após orçamento inválido");
         }});
    tests.push_back({"estimativas físicas", [] {
                         CelestialBody b;
                         b.id = "mars";
                         b.bodyType = "Planet";
                         b.meanRadius = 100;
                         b.gravity = 9.80665;
                         auto c = estimateCandidates({&b});
                         check(c.candidates.size() == 1, "elegível");
                         check(std::abs(c.candidates[0].benefit -
                                        (8 + 2 * std::log10(2))) < 1e-10,
                               "fórmula benefício");
                         check(std::abs(c.candidates[0].cost -
                                        (12 + 5 * std::log10(2))) < 1e-10,
                               "fórmula custo");
                         b.gravity = 0;
                         check(estimateCandidates({&b}).excluded.size() == 1,
                               "zero excluído");
                         b.id = "terre";
                         check(estimateCandidates({&b}).excluded.size() == 1,
                               "base excluída");
                     }});
    tests.push_back(
        {"menu integrado e leitura local", [] {
             BodyCatalog c;
             c.load(readLocalJson(FIXTURE_PATH), "fixture");
             struct Fake : HttpClient {
                 HttpResponse get(const std::string&,
                                  const std::string&) const override {
                     throw std::runtime_error("rede simulada indisponível");
                 }
             } http;
             SolarApiClient api(http);
             std::istringstream input(
                 "bad\n2\nmars\n3\n1\nart\n3\n2\ngravity\n1\n4\n"
                 "3\n2\ngravity\n0\n0\n"
                 "3\n3\neArTh\n3\n4\nMo\n"
                 "4\n5\n1\nMoon\n5\n2\n1\n6\nterre\nmars\n7\n8\n100\n2\n1\n2\n/arquivo/inexistente\n9\n");
             std::ostringstream output;
             TerminalUI(c, api, input, output).run();
             auto text = output.str();
             check(text.find("Erro:") != std::string::npos, "entrada inválida");
             check(text.find("Mars") != std::string::npos, "consulta");
             check(text.find("Trecho do nome: terre | Earth | Planet\nResultados: 1") !=
                       std::string::npos, "busca por trecho interno");
             check(text.find("Nome exato: terre | Earth | Planet\nResultados: 1\n"
                             "Nós visitados na Trie: 6") != std::string::npos,
                   "busca exata no menu");
             check(text.find("Prefixo do nome: lune | Moon | Moon\nResultados: 1\n"
                             "Nós visitados na Trie: 5") != std::string::npos,
                   "busca por prefixo no menu");
             check(text.find("Diferença gravidade") != std::string::npos,
                   "comparação");
             check(text.find("Colisões de inserção") != std::string::npos,
                   "instrumentação");
             check(text.find("lune | Moon | Moon\nmars | Mars | Planet\nResultados: 2") !=
                       std::string::npos &&
                       text.find("unknown | Synthetic Unknown | Asteroid\nResultados: 1") !=
                           std::string::npos,
                   "intervalo e valor exato pelo menu");
             check(text.find("Árvore B gravity | altura: 2 | divisões: 1\n"
                             "Árvore B meanRadius | altura: 1 | divisões: 0\n"
                             "Árvore B avgTemp | altura: 1 | divisões: 0") !=
                       std::string::npos,
                   "métricas de cada árvore no menu");
             check(text.find("Benefício total") != std::string::npos,
                   "planejamento");
             check(c.find("mars"), "falha local preserva dados");
             std::istringstream eof("8\n");
             std::ostringstream end;
             TerminalUI(c, api, eof, end).run();
             check(end.str().find("Entrada encerrada") != std::string::npos,
                   "EOF durante operação");
             std::istringstream local(std::string("1\n2\n") + FIXTURE_PATH +
                                      "\n9\n");
             std::ostringstream log;
             TerminalUI(c, api, local, log).run();
             check(c.source().find("JSON local:") == 0, "origem explícita");
         }});
    tests.push_back(
        {"parsing massa e extremos", [] {
             auto r = parseBodies(
                 R"({"bodies":[{"id":"good","mass":{"massValue":5.97,"massExponent":24},"isPlanet":false},{"id":"bad","mass":{"massValue":-1,"massExponent":24},"gravity":-1,"avgTemp":true,"isPlanet":"false"},{"id":"exponent","mass":{"massValue":1,"massExponent":18446744073709551615}},null,{"id":""}]})");
             check(r.bodies.size() == 3 && r.rejected == 2, "rejeitados");
             check(r.bodies[0].mass && r.bodies[0].mass->exponent == 24,
                   "massa científica");
             check(r.bodies[0].isPlanet && !*r.bodies[0].isPlanet,
                   "false preservado");
             check(!r.bodies[1].mass && !r.bodies[1].gravity &&
                       !r.bodies[1].avgTemp && !r.bodies[1].isPlanet,
                   "atributos inválidos");
             check(!r.bodies[2].mass, "expoente fora de faixa");
             BodyCatalog c;
             auto summary = c.load(r, "extremos");
             check(summary.received == 5 && summary.rejected == 2,
                   "resumo conta todos os registros");
             bool threw = false;
             try {
                 readLocalJson("/arquivo/inexistente");
             } catch (const std::runtime_error&) {
                 threw = true;
             }
             check(threw, "arquivo ausente");
         }});
    tests.push_back(
        {"limites pequenos da hash e candidato ignorado", [] {
             HashTable table(1);
             CelestialBody b;
             b.id = "one";
             table.insert(b);
             check(table.capacity() == 2 && table.loadFactor() == 0.5,
                   "capacidade mínima");
             auto collisions = table.collisions();
             auto rehashes = table.rehashes();
             table.insert(b);
             check(table.collisions() == collisions &&
                       table.rehashes() == rehashes,
                   "duplicata não expande");
             auto p = planMissions(
                 {{"expensive", "", 100, 50}, {"fits", "", 1, 1}}, 1, 1);
             check(p.selected.size() == 1 && p.selected[0].id == "fits",
                   "continua após candidato caro");
             check(planMissions({{"zero", "", 0, 1}}, 10, 1).selected.empty(),
                   "benefício zero não ocupa recurso");
         }});
    tests.push_back(
        {"rede espacial cenário de exemplo", [] {
             BodyCatalog catalog;
             catalog.load(readLocalJson(FIXTURE_PATH), "fixture");
             const auto network = readMissionNetwork(NETWORK_SCENARIO_PATH, catalog);
             check(network.vertices == std::vector<std::string>{"terre", "lune", "mars"},
                   "vértices preservam os IDs do catálogo e a ordem do cenário");
             check(network.routes.size() == 4 && network.routes[0].from == 0 &&
                       network.routes[0].to == 1 && network.routes[0].cost == 2 &&
                       network.routes[3].from == 2 && network.routes[3].to == 0,
                   "deslocamentos preservam sentidos e custos");
             check(network.infrastructure.size() == 3 &&
                       network.infrastructure[0].first == 0 &&
                       network.infrastructure[0].second == 1 &&
                       network.infrastructure[0].cost == 3,
                   "infraestrutura é armazenada como uma ligação por par");
             check(network.transfers.size() == 3 && network.transfers[0].from == 0 &&
                       network.transfers[0].to == 1 && network.transfers[0].capacity == 5,
                   "o mesmo par representa dados independentes em cada relação");
             check(network.activities.size() == 3 && network.activities[1].id == "relay" &&
                       network.activities[1].body == 1 &&
                       network.dependencies.size() == 2 &&
                       network.dependencies[1].before == 1 && network.dependencies[1].after == 2,
                   "dependências usam atividades e atividades apontam para locais");
         }});
    tests.push_back(
        {"rede espacial desconexa, limites e ciclos representáveis", [] {
             BodyCatalog catalog;
             catalog.load(readLocalJson(FIXTURE_PATH), "fixture");
             const auto network = parseMissionNetwork(R"({
                 "vertices":["lune","terre","mars"],
                 "routes":[{"from":"terre","to":"lune","cost":0},
                           {"from":"lune","to":"terre","cost":0.125}],
                 "infrastructure":[{"a":"terre","b":"lune","cost":0}],
                 "transfers":[{"from":"terre","to":"lune","capacity":0},
                              {"from":"lune","to":"terre","capacity":18446744073709551615}],
                 "activities":[{"id":"t","name":"T","body":"terre"},
                               {"id":"l","name":"L","body":"lune"}],
                 "dependencies":[{"before":"t","after":"l"},{"before":"l","after":"t"},
                                 {"before":"l","after":"l"}]
             })", catalog);
             check(network.vertices.size() == 3 && network.vertices[2] == "mars" &&
                       network.routes.size() == 2 && network.routes[0].from == 1 &&
                       network.routes[0].to == 0 && network.routes[0].cost == 0 &&
                       network.routes[1].from == 0 && network.routes[1].to == 1 &&
                       network.routes[1].cost == 0.125,
                   "vértice isolado e arcos opostos com custos distintos");
             check(network.infrastructure.size() == 1 &&
                       network.infrastructure[0].first == 0 &&
                       network.infrastructure[0].second == 1,
                   "extremidades da ligação não direcionada são canônicas");
             check(network.transfers.size() == 2 && network.transfers[0].from == 1 &&
                       network.transfers[0].to == 0 && network.transfers[0].capacity == 0 &&
                       network.transfers[1].from == 0 && network.transfers[1].to == 1 &&
                       network.transfers[1].capacity == std::numeric_limits<std::uint64_t>::max(),
                   "capacidades inteiras nos dois extremos");
             check(network.dependencies.size() == 3 &&
                       network.dependencies[0].before == 0 && network.dependencies[0].after == 1 &&
                       network.dependencies[1].before == 1 && network.dependencies[1].after == 0 &&
                       network.dependencies[2].before == 1 && network.dependencies[2].after == 1,
                   "ciclo e autodependência são preservados para análise posterior");
             const auto empty = parseMissionNetwork(R"({"vertices":[],"routes":[],
                 "infrastructure":[],"transfers":[],"activities":[],"dependencies":[]})",
                 catalog);
             check(empty.vertices.empty() && empty.dependencies.empty(), "rede vazia");
         }});
    tests.push_back(
        {"rede espacial entradas inválidas e duplicatas", [] {
             BodyCatalog catalog;
             catalog.load(readLocalJson(FIXTURE_PATH), "fixture");
             const auto valid = nlohmann::json::parse(R"({
                 "vertices":["terre","lune"], "routes":[], "infrastructure":[], "transfers":[],
                 "activities":[{"id":"a","name":"A","body":"terre"},
                               {"id":"b","name":"B","body":"lune"}], "dependencies":[]
             })");
             const auto rejects = [&](const std::string& text, const char* expected) {
                 try {
                     parseMissionNetwork(text, catalog);
                 } catch (const std::invalid_argument& error) {
                     return std::string(error.what()).find(expected) != std::string::npos;
                 }
                 return false;
             };
             check(rejects("{", "JSON do cenário inválido") && rejects("[]", "objeto JSON") &&
                       rejects("{}", "campo ausente: vertices"),
                   "JSON malformado, tipo da raiz e campo obrigatório");
             struct Case {
                 const char* section;
                 const char* value;
                 const char* error;
             };
             const Case cases[] = {
                 {"vertices", "{}", "deve ser uma lista"},
                 {"vertices", R"([""])", "texto preenchido"},
                 {"vertices", R"(["Terre"])", "ID ausente no catálogo"},
                 {"vertices", R"(["terre","terre"])", "vértice duplicado"},
                 {"routes", R"([1])", "esperado um objeto"},
                 {"routes", R"([{"from":"terre","to":"mars","cost":1}])", "ID fora do cenário"},
                 {"routes", R"([{"from":"terre","to":"lune"}])", "campo ausente: cost"},
                 {"routes", R"([{"from":"terre","to":"lune","cost":-1}])", "cost deve ser"},
                 {"routes", R"([{"from":"terre","to":"lune","cost":"1"}])", "cost deve ser"},
                 {"routes", R"([{"from":"terre","to":"lune","cost":1},
                                {"from":"terre","to":"lune","cost":2}])", "relação duplicada"},
                 {"infrastructure", R"([{"a":"terre","b":"lune","cost":1},
                                         {"a":"lune","b":"terre","cost":2}])", "relação duplicada"},
                 {"transfers", R"([{"from":"terre","to":"lune","capacity":-1}])", "capacity deve ser"},
                 {"transfers", R"([{"from":"terre","to":"lune","capacity":0.5}])", "capacity deve ser"},
                 {"transfers", R"([{"from":"terre","to":"lune","capacity":18446744073709551616}])",
                  "capacity deve ser"},
                 {"transfers", R"([{"from":"terre","to":"lune","capacity":1},
                                   {"from":"terre","to":"lune","capacity":1}])", "relação duplicada"},
                 {"activities", R"([{"id":"a","name":"A","body":"mars"}])", "ID fora do cenário"},
                 {"activities", R"([{"id":"a","name":"A","body":"terre"},
                                    {"id":"a","name":"B","body":"lune"}])", "atividade duplicada"},
                 {"dependencies", R"([{"before":"a","after":"terre"}])", "ID fora do cenário"},
                 {"dependencies", R"([{"before":"a","after":"b"},
                                      {"before":"a","after":"b"}])", "relação duplicada"},
             };
             for (const auto& test : cases) {
                 auto invalid = valid;
                 invalid[test.section] = nlohmann::json::parse(test.value);
                 check(rejects(invalid.dump(), test.error), test.value);
             }
             const auto tooLarge = R"({"vertices":["terre","lune"],
                 "routes":[{"from":"terre","to":"lune","cost":1e400}],
                 "infrastructure":[],"transfers":[],"activities":[],"dependencies":[]})";
             check(rejects(tooLarge, "JSON do cenário inválido"), "custo acima da faixa finita");
         }});
    tests.push_back(
        {"menu rede espacial e recuperação de falha", [] {
             BodyCatalog catalog;
             CurlHttpClient http;
             SolarApiClient api(http);
             const std::string scenario = NETWORK_SCENARIO_PATH;
             const std::string fixture = FIXTURE_PATH;
             std::istringstream input(
                 "11\n2\n11\n1\n" + scenario + "\n1\n2\n" + fixture +
                 "\n11\n1\n" + scenario + "\n11\n1\n" + scenario + "/inexistente" +
                 "\n1\n2\n" + fixture + "\n11\n2\n9\n");
             std::ostringstream output;
             TerminalUI(catalog, api, input, output).run();
             const auto text = output.str();
             const auto loaded = text.find("Rede carregada: 3 vértices.");
             check(text.find("Nenhum cenário de rede carregado.") < loaded &&
                       text.find("ID ausente no catálogo: terre") < loaded,
                   "visualização sem cenário e carga antes do catálogo");
             const auto failed = text.find("Não foi possível abrir o cenário", loaded);
             check(failed != std::string::npos &&
                       text.find("Vértices (3)\n  terre | Earth", failed) != std::string::npos &&
                       text.find("Rede descartada") == std::string::npos,
                   "rede anterior sobrevive à falha e à recarga com os mesmos IDs");
             check(text.find("Deslocamentos (4)\n  terre -> lune | custo 2") != std::string::npos &&
                       text.find("Infraestrutura (3)\n  terre -- lune | custo 3") != std::string::npos &&
                       text.find("Transferências (3)\n  terre -> lune | capacidade 5") != std::string::npos &&
                       text.find("relay | Instalar o retransmissor @ lune") != std::string::npos &&
                       text.find("Dependências (2)\n  prepare -> relay") != std::string::npos &&
                       text.find("Encerrado.") != std::string::npos,
                   "menu mostra cada relação, seus dados e seu sentido");
         }});
    tests.push_back(
        {"Dijkstra caminho direto e alternativa mais barata", [] {
             BodyCatalog catalog;
             catalog.load(readLocalJson(FIXTURE_PATH), "fixture");
             const auto network = readMissionNetwork(NETWORK_SCENARIO_PATH, catalog);
             const auto direct = shortestTravelPath(network, "lune", "mars");
             check(direct && direct->path == std::vector<std::string>{"lune", "mars"} &&
                       direct->totalCost == 4,
                   "a melhor rota pode ser uma aresta direta");
             check(std::any_of(network.routes.begin(), network.routes.end(),
                               [&](const auto& route) {
                                   return network.vertices[route.from] == "terre" &&
                                          network.vertices[route.to] == "mars" &&
                                          route.cost == 8;
                               }),
                   "cenário tem rota direta Terra–Marte com custo 8");
             const auto indirect = shortestTravelPath(network, "terre", "mars");
             check(indirect && indirect->path == std::vector<std::string>{"terre", "lune", "mars"} &&
                       indirect->totalCost == 6,
                   "rota intermediária reduz o custo de 8 para 6");
             const auto reverse = shortestTravelPath(network, "mars", "terre");
             check(reverse && reverse->path == std::vector<std::string>{"mars", "terre"} &&
                       reverse->totalCost == 9,
                   "sentido inverso tem o custo do arco correspondente");
         }});
    tests.push_back(
        {"Dijkstra custos zero, ciclos, empates e destino inalcançável", [] {
             MissionNetwork network;
             network.vertices = {"terre", "lune", "mars", "unknown"};
             network.routes = {{0, 1, 0}, {1, 0, 0}, {1, 1, 0}, {1, 2, 0}, {0, 2, 3}};
             network.infrastructure = {{0, 3, 0}, {2, 1, 0}};
             network.transfers = {{0, 3, 100}, {2, 0, 100}};

             const auto costless = shortestTravelPath(network, "terre", "mars");
             check(costless && costless->path == std::vector<std::string>{"terre", "lune", "mars"} &&
                       costless->totalCost == 0,
                   "ciclo e laço de custo zero não entram no percurso");
             check(!shortestTravelPath(network, "terre", "unknown") &&
                       !shortestTravelPath(network, "unknown", "terre") &&
                       !shortestTravelPath(network, "mars", "terre"),
                   "isolados e sentidos ausentes ficam inalcançáveis mesmo em outras relações");
             const auto same = shortestTravelPath(network, "unknown", "unknown");
             check(same && same->path == std::vector<std::string>{"unknown"} &&
                       same->totalCost == 0,
                   "origem e destino iguais mesmo em vértice isolado");

             network.routes.back().cost = 0;
             const auto tie = shortestTravelPath(network, "terre", "mars");
             check(tie && tie->path == std::vector<std::string>{"terre", "mars"} &&
                       tie->totalCost == 0,
                   "custo igual mantém o predecessor que já foi registrado");
         }});
    tests.push_back(
        {"Dijkstra validação e custos acumulados grandes", [] {
             MissionNetwork network;
             network.vertices = {"terre", "lune", "mars"};
             const auto rejects = [&](const auto& origin, const auto& destination,
                                      const char* message) {
                 try {
                     shortestTravelPath(network, origin, destination);
                 } catch (const std::invalid_argument& error) {
                     return std::string(error.what()).find(message) != std::string::npos;
                 }
                 return false;
             };
             check(rejects("", "mars", "origem") && rejects("Terre", "mars", "origem") &&
                       rejects("terre", "unknown", "destino") &&
                       rejects("unknown", "unknown", "origem"),
                   "IDs precisam existir no cenário, inclusive quando são iguais");
             network.routes = {{0, 3, 0}};
             check(rejects("terre", "mars", "extremidade fora do cenário"),
                   "valida índices em redes montadas em memória");
             for (double cost : {-1.0, std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::quiet_NaN()}) {
                 network.routes = {{0, 2, cost}};
                 check(rejects("terre", "mars", "Custo da rota"),
                       "valida custos em redes montadas em memória");
             }

             const auto large = std::numeric_limits<double>::max();
             network.routes = {{0, 1, large}, {1, 2, large}};
             if (std::numeric_limits<long double>::max_exponent >
                 std::numeric_limits<double>::max_exponent) {
                 const auto plan = shortestTravelPath(network, "terre", "mars");
                 check(plan && plan->totalCost == static_cast<long double>(large) * 2,
                       "custo acumulado pode exceder a faixa de double");
             } else {
                 bool overflow = false;
                 try {
                     shortestTravelPath(network, "terre", "mars");
                 } catch (const std::overflow_error&) {
                     overflow = true;
                 }
                 check(overflow, "percurso existe, mas o custo excede a faixa numérica");
             }
         }});
    tests.push_back(
        {"menu planejamento de deslocamentos", [] {
             BodyCatalog catalog;
             catalog.load(readLocalJson(FIXTURE_PATH), "fixture");
             CurlHttpClient http;
             SolarApiClient api(http);
             const std::string scenario = NETWORK_SCENARIO_PATH;
             std::istringstream input("11\n3\n11\n1\n" + scenario +
                                      "\n11\n2\n11\n3\nterre\nmars\n"
                                      "11\n3\nlune\nlune\n11\n3\nterre\nunknown\n9\n");
             std::ostringstream output;
             TerminalUI(catalog, api, input, output).run();
             const auto text = output.str();
             check(text.find("Nenhum cenário de rede carregado.") != std::string::npos &&
                       text.find("terre -> mars | custo 8") != std::string::npos &&
                       text.find("Percurso: terre -> lune -> mars\nCusto total: 6") != std::string::npos,
                   "menu exibe a aresta direta e o resultado de Dijkstra");
             check(text.find("Percurso: lune\nCusto total: 0") != std::string::npos &&
                       text.find("Erro: ID de destino fora do cenário: unknown") != std::string::npos &&
                       text.find("Encerrado.") != std::string::npos,
                   "percurso trivial e retorno ao menu após ID inválido");
         }});
    int failures = 0;
    for (const auto& t : tests) {
        try {
            t.second();
            std::cout << "PASS " << t.first << '\n';
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "FAIL " << t.first << ": " << e.what() << '\n';
        }
    }
    return failures ? 1 : 0;
}
