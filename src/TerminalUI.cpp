#include "cronicas/TerminalUI.hpp"
#include "cronicas/MissionPlanner.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
namespace cronicas {
namespace {
struct EndInput {};
}  // namespace
std::string TerminalUI::read(const std::string& prompt) {
    output_ << prompt << std::flush;
    std::string line;
    if (!std::getline(input_, line)) throw EndInput{};
    return line;
}
double TerminalUI::number(const std::string& prompt) {
    std::istringstream stream(read(prompt));
    double value;
    std::string extra;
    if (!(stream >> value) || stream >> extra || !std::isfinite(value) ||
        value < 0)
        throw std::invalid_argument(
            "Digite um número finito não negativo (decimal com ponto)");
    return value;
}
size_t TerminalUI::integer(const std::string& prompt, size_t maximum) {
    std::istringstream stream(read(prompt));
    long long value;
    std::string extra;
    if (!(stream >> value) || stream >> extra || value < 0 ||
        static_cast<unsigned long long>(value) > maximum)
        throw std::invalid_argument("Inteiro fora do intervalo permitido");
    return static_cast<size_t>(value);
}
void TerminalUI::showBody(const CelestialBody& b) {
    output_ << b.id << " | " << displayName(b)
            << " | tipo: " << (b.bodyType.empty() ? "desconhecido" : b.bodyType)
            << '\n';
    output_ << "Planeta: "
            << (b.isPlanet ? (*b.isPlanet ? "sim" : "não") : "desconhecido")
            << '\n';
    auto field = [&](const char* label, const std::optional<double>& value) {
        output_ << label << ": ";
        if (value)
            output_ << *value;
        else
            output_ << "desconhecido";
        output_ << '\n';
    };
    field("Gravidade (m/s²)", b.gravity);
    field("Raio médio (km)", b.meanRadius);
    field("Semieixo maior orbital (km; não é distância de viagem)",
          b.semimajorAxis);
    field("Temperatura média (K)", b.avgTemp);
    output_ << "Massa (kg): ";
    if (b.mass)
        output_ << b.mass->value << " x 10^" << b.mass->exponent;
    else
        output_ << "desconhecida";
    output_
        << "\nZeros recebidos são preservados; podem indicar dados indisponíveis.\n";
}
void TerminalUI::showList(const std::vector<const CelestialBody*>& bodies) {
    for (auto b : bodies)
        output_ << b->id << " | " << displayName(*b) << " | " << b->bodyType
                << '\n';
    output_ << "Resultados: " << bodies.size() << '\n';
}
void TerminalUI::showStats() {
    auto s = catalog_.statistics();
    output_ << "Fonte: " << catalog_.source() << "\nElementos: " << s.elements
            << "\nCapacidade: " << s.capacity
            << "\nColisões de inserção: " << s.collisions
            << "\nFator de carga: " << s.loadFactor
            << "\nRehashings: " << s.rehashes << '\n';
}
void TerminalUI::load() {
    auto mode = integer("Origem: 1 API dinâmica, 2 JSON local explícito: ", 2);
    ParseResult parsed;
    std::string source;
    if (mode == 1) {
        parsed = api_.fetch();
        source = "API dinâmica";
    } else if (mode == 2) {
        auto path = read("Caminho: ");
        source = "JSON local: " + path;
        parsed = readLocalJson(path);
    } else
        throw std::invalid_argument("Origem inválida");
    auto summary = catalog_.load(parsed, source);
    output_ << "Carga concluída: " << summary.inserted << " únicos, "
            << summary.updated << " atualizações, " << summary.rejected
            << " rejeitados. Avisos: " << summary.warnings.size() << '\n';
    for (size_t i = 0; i < summary.warnings.size() && i < 10; ++i)
        output_ << summary.warnings[i] << '\n';
    showStats();
}
void TerminalUI::compare() {
    auto first = read("Primeiro ID: ");
    auto second = read("Segundo ID: ");
    const auto* a = catalog_.find(first);
    const auto* b = catalog_.find(second);
    if (!a || !b) throw std::invalid_argument("Um dos IDs não foi encontrado");
    showBody(*a);
    showBody(*b);
    auto difference = [&](const char* label, const std::optional<double>& x,
                          const std::optional<double>& y) {
        output_ << "Diferença " << label << " (primeiro - segundo): ";
        if (x && y)
            output_ << *x - *y;
        else
            output_ << "indisponível";
        output_ << '\n';
    };
    difference("gravidade (m/s²)", a->gravity, b->gravity);
    difference("raio (km)", a->meanRadius, b->meanRadius);
    difference("temperatura (K)", a->avgTemp, b->avgTemp);
}
void TerminalUI::plan() {
    double budget = number("Orçamento em unidades didáticas: ");
    size_t limit = integer("Limite de missões: ", 1000000);
    auto estimates = estimateCandidates(catalog_.list());
    output_
        << "Modelo acadêmico: custos e benefícios fictícios; prioridade benefício/custo.\nElegíveis: "
        << estimates.candidates.size()
        << " | Excluídos: " << estimates.excluded.size() << '\n';
    for (const auto& e : estimates.excluded)
        output_ << "Excluído " << e.id << ": " << e.reason << '\n';
    auto p = planMissions(estimates.candidates, budget, limit);
    for (const auto& c : p.selected)
        output_ << c.id << " | " << c.name << " | benefício " << c.benefit
                << " | custo " << c.cost << " | razão " << c.benefit / c.cost
                << '\n';
    output_ << "Benefício total: " << p.totalBenefit
            << "\nCusto total: " << p.totalCost << " / " << budget
            << "\nOrçamento restante: " << budget - p.totalCost
            << "\nMissões: " << p.selected.size() << " / " << limit
            << "\nA heurística não garante o ótimo global.\n";
}
void TerminalUI::run() {
    output_ << std::setprecision(8) << "Crônicas do Espaço — Parte 1\n";
    while (true) {
        try {
            output_
                << "\n1 Carregar dados\n2 Consultar ID\n3 Pesquisar nome ou atributo\n4 Listar corpos\n5 Filtrar tipo ou planeta\n6 Comparar dois corpos\n7 Estatísticas da Hash\n8 Planejamento guloso\n9 Encerrar\n";
            auto option = integer("Opção: ", 9);
            if (option == 9) {
                output_ << "Encerrado.\n";
                return;
            }
            switch (option) {
                case 1:
                    load();
                    break;
                case 2: {
                    auto body = catalog_.find(read("ID exato: "));
                    if (body)
                        showBody(*body);
                    else
                        output_ << "ID não encontrado.\n";
                    break;
                }
                case 3: {
                    auto mode =
                        integer("Pesquisa: 1 nome, 2 intervalo físico: ", 2);
                    if (mode == 1)
                        showList(catalog_.searchName(read("Trecho do nome: ")));
                    else if (mode == 2) {
                        auto field =
                            read("Atributo (gravity, meanRadius, avgTemp): ");
                        auto min = number("Mínimo: ");
                        auto max = number("Máximo: ");
                        showList(catalog_.filterRange(field, min, max));
                    } else
                        throw std::invalid_argument("Pesquisa inválida");
                    break;
                }
                case 4:
                    showList(catalog_.list());
                    break;
                case 5: {
                    auto mode =
                        integer("Filtro: 1 tipo, 2 condição de planeta: ", 2);
                    if (mode == 1)
                        showList(catalog_.filterType(read("Tipo: ")));
                    else if (mode == 2)
                        showList(catalog_.filterPlanet(
                            integer("Planeta? 1 sim, 0 não: ", 1) == 1));
                    else
                        throw std::invalid_argument("Filtro inválido");
                    break;
                }
                case 6:
                    compare();
                    break;
                case 7:
                    showStats();
                    break;
                case 8:
                    plan();
                    break;
                default:
                    output_ << "Escolha entre 1 e 9.\n";
            }
        } catch (const EndInput&) {
            output_ << "\nEntrada encerrada.\n";
            return;
        } catch (const std::exception& e) {
            output_ << "Erro: " << e.what() << '\n';
        }
    }
}
}  // namespace cronicas
