#include "cronicas/TerminalUI.hpp"
#include "cronicas/MissionPlanner.hpp"
#include <algorithm>
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
    for (const auto& index : catalog_.numericIndexStatistics())
        output_ << "Árvore B " << index.field << " | altura: " << index.height
                << " | divisões: " << index.splits << '\n';
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
    if (network_ && std::any_of(network_->vertices.begin(), network_->vertices.end(),
                                [this](const auto& id) { return !catalog_.find(id); })) {
        network_.reset();
        output_ << "Rede descartada: o novo catálogo não contém todos os seus vértices.\n";
    }
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
void TerminalUI::plan(bool dynamic) {
    double budget = number("Orçamento em unidades didáticas: ");
    size_t limit = integer("Limite de missões: ", 1000000);
    auto estimates = estimateCandidates(catalog_.list());
    output_ << (dynamic ? "Modelo acadêmico: programação dinâmica em centésimos. "
                          "Custos arredondados para cima, orçamento para baixo.\n"
                        : "Modelo acadêmico: custos e benefícios fictícios; "
                          "prioridade benefício/custo.\n")
            << "Elegíveis: " << estimates.candidates.size()
            << " | Excluídos: " << estimates.excluded.size() << '\n';
    for (const auto& e : estimates.excluded)
        output_ << "Excluído " << e.id << ": " << e.reason << '\n';

    const auto showPlan = [this](const MissionPlan& plan, bool showRatio) {
        for (const auto& candidate : plan.selected) {
            output_ << candidate.id << " | " << candidate.name << " | benefício "
                    << candidate.benefit << " | custo " << candidate.cost;
            if (showRatio) output_ << " | razão " << candidate.benefit / candidate.cost;
            output_ << '\n';
        }
        output_ << "Benefício total: " << plan.totalBenefit
                << "\nCusto total: " << plan.totalCost << " / " << plan.budget
                << "\nOrçamento restante: " << plan.budget - plan.totalCost
                << "\nMissões: " << plan.selected.size() << " / " << plan.missionLimit
                << '\n';
    };

    if (dynamic) {
        const auto optimized = optimizeMissions(estimates.candidates, budget, limit);
        output_ << "\nProgramação dinâmica — melhor plano no modelo discretizado\n";
        if (optimized.selected.empty())
            output_ << "Nenhuma missão selecionada neste cenário.\n";
        showPlan(optimized, false);
        output_ << "\nGuloso no mesmo cenário — custos originais\n";
    }
    showPlan(planMissions(estimates.candidates, budget, limit), true);
    output_ << "A heurística não garante o ótimo global.\n";
}
void TerminalUI::networkMenu() {
    const auto mode = integer(
        "Rede espacial: 1 carregar cenário, 2 visualizar, 3 planejar deslocamento, "
        "4 ordenar atividades, 5 planejar infraestrutura: ",
        5);
    if (mode == 0) throw std::invalid_argument("Opção de rede inválida");
    if (mode == 1) {
        const auto path = read("Caminho do cenário JSON: ");
        network_ = readMissionNetwork(path, catalog_);
        output_ << "Rede carregada: " << network_->vertices.size() << " vértices.\n";
    } else if (!network_) {
        output_ << "Nenhum cenário de rede carregado.\n";
    } else if (mode == 2) {
        showNetwork(*network_);
    } else if (mode == 3) {
        planRoute(*network_);
    } else if (mode == 4) {
        showActivityOrder(*network_);
    } else {
        showInfrastructurePlan(*network_);
    }
}
void TerminalUI::planRoute(const MissionNetwork& network) {
    const auto origin = read("ID da origem: ");
    const auto destination = read("ID do destino: ");
    const auto plan = shortestTravelPath(network, origin, destination);
    if (!plan) {
        output_ << "Destino inalcançável: não há rota de " << origin << " até "
                << destination << ".\n";
        return;
    }

    output_ << "Percurso: ";
    for (size_t i = 0; i < plan->path.size(); ++i) {
        if (i > 0) output_ << " -> ";
        output_ << plan->path[i];
    }
    output_ << "\nCusto total: " << plan->totalCost << '\n';
}
void TerminalUI::showActivityOrder(const MissionNetwork& network) {
    const auto order = orderActivities(network);
    if (!order) {
        output_ << "Ciclo de dependências: não existe uma ordem válida de execução.\n";
        return;
    }
    if (order->empty()) {
        output_ << "O cenário não contém atividades.\n";
        return;
    }

    output_ << "Ordem de execução das atividades\n";
    for (std::size_t i = 0; i < order->size(); ++i) {
        const auto& activity = network.activities[(*order)[i]];
        const auto& bodyId = network.vertices[activity.body];
        output_ << "  " << i + 1 << ". " << activity.id << " | " << activity.name
                << " @ " << bodyId;
        if (const auto* body = catalog_.find(bodyId)) output_ << " | " << displayName(*body);
        output_ << '\n';
    }
}
void TerminalUI::showInfrastructurePlan(const MissionNetwork& network) {
    const auto plan = planInfrastructure(network);
    if (plan.componentCount == 0) {
        output_ << "O cenário não contém pontos de infraestrutura.\n";
        return;
    }
    if (plan.componentCount == 1) {
        output_ << "Árvore geradora mínima de infraestrutura\n";
    } else {
        output_ << "Rede de infraestrutura desconexa (" << plan.componentCount
                << " componentes).\nFloresta geradora mínima de infraestrutura\n";
    }
    output_ << "Conexões selecionadas (" << plan.selectedLinks.size() << ")\n";
    for (auto index : plan.selectedLinks) {
        const auto& link = network.infrastructure[index];
        output_ << "  " << network.vertices[link.first] << " -- "
                << network.vertices[link.second] << " | custo " << link.cost << '\n';
    }
    output_ << "Custo total da infraestrutura: " << plan.totalCost << '\n';
}
void TerminalUI::showNetwork(const MissionNetwork& network) {
    const auto vertexId = [&](size_t position) -> const std::string& {
        return network.vertices[position];
    };

    output_ << "Vértices (" << network.vertices.size() << ")\n";
    for (const auto& id : network.vertices) {
        output_ << "  " << id;
        if (const auto* body = catalog_.find(id)) output_ << " | " << displayName(*body);
        output_ << '\n';
    }
    output_ << "Deslocamentos (" << network.routes.size() << ")\n";
    for (const auto& route : network.routes)
        output_ << "  " << vertexId(route.from) << " -> " << vertexId(route.to)
                << " | custo " << route.cost << '\n';
    output_ << "Infraestrutura (" << network.infrastructure.size() << ")\n";
    for (const auto& link : network.infrastructure)
        output_ << "  " << vertexId(link.first) << " -- " << vertexId(link.second)
                << " | custo " << link.cost << '\n';
    output_ << "Transferências (" << network.transfers.size() << ")\n";
    for (const auto& transfer : network.transfers)
        output_ << "  " << vertexId(transfer.from) << " -> " << vertexId(transfer.to)
                << " | capacidade " << transfer.capacity << '\n';
    output_ << "Atividades (" << network.activities.size() << ")\n";
    for (const auto& activity : network.activities)
        output_ << "  " << activity.id << " | " << activity.name << " @ "
                << vertexId(activity.body) << '\n';
    output_ << "Dependências (" << network.dependencies.size() << ")\n";
    for (const auto& dependency : network.dependencies)
        output_ << "  " << network.activities[dependency.before].id << " -> "
                << network.activities[dependency.after].id << '\n';
}
void TerminalUI::run() {
    output_ << std::setprecision(8) << "Crônicas do Espaço — Parte 1\n";
    while (true) {
        try {
            output_
                << "\n1 Carregar dados\n2 Consultar ID\n3 Pesquisar nome ou atributo\n4 Listar corpos\n5 Filtrar tipo ou planeta\n6 Comparar dois corpos\n7 Estatísticas da Hash e da Árvore B\n8 Planejamento guloso\n9 Encerrar\n10 Planejamento por programação dinâmica\n11 Rede espacial\n";
            auto option = integer("Opção: ", 11);
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
                    auto mode = integer(
                        "Pesquisa: 1 trecho do nome, 2 intervalo físico, "
                        "3 nome exato, 4 prefixo do nome: ",
                        4);
                    if (mode == 1)
                        showList(catalog_.searchName(read("Trecho do nome: ")));
                    else if (mode == 2) {
                        auto field =
                            read("Atributo (gravity, meanRadius, avgTemp): ");
                        auto min = number("Mínimo: ");
                        auto max = number("Máximo: ");
                        showList(catalog_.filterRange(field, min, max));
                    } else if (mode == 3 || mode == 4) {
                        const bool exact = mode == 3;
                        const auto text =
                            read(exact ? "Nome exato: " : "Prefixo do nome: ");
                        showList(exact ? catalog_.searchNameExact(text)
                                       : catalog_.searchNamePrefix(text));
                        output_ << "Nós visitados na Trie: "
                                << catalog_.lastNameSearchVisitedNodes() << '\n';
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
                    plan(false);
                    break;
                case 10:
                    plan(true);
                    break;
                case 11:
                    networkMenu();
                    break;
                default:
                    output_ << "Escolha entre 1 e 11.\n";
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
