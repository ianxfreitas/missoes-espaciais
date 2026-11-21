#include "cronicas/MissionNetwork.hpp"
#include "cronicas/BodyCatalog.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cronicas {
namespace {
using nlohmann::json;
using Positions = std::map<std::string, std::size_t>;

const json& field(const json& object, const char* name) {
    if (!object.is_object()) throw std::invalid_argument("esperado um objeto");
    const auto value = object.find(name);
    if (value == object.end())
        throw std::invalid_argument(std::string("campo ausente: ") + name);
    return *value;
}

std::string nonEmptyText(const json& value, const char* name) {
    if (!value.is_string() || value.get_ref<const std::string&>().empty())
        throw std::invalid_argument(std::string(name) + " deve ser um texto preenchido");
    return value.get<std::string>();
}

double readCost(const json& entry) {
    const auto& value = field(entry, "cost");
    if (value.is_number()) {
        const auto cost = value.get<double>();
        if (std::isfinite(cost) && cost >= 0) return cost;
    }
    throw std::invalid_argument("cost deve ser um número finito não negativo");
}

std::uint64_t readCapacity(const json& entry) {
    const auto& value = field(entry, "capacity");
    if (value.is_number_unsigned()) return value.get<std::uint64_t>();
    if (value.is_number_integer() && value.get<std::int64_t>() >= 0)
        return value.get<std::uint64_t>();
    throw std::invalid_argument("capacity deve ser um inteiro não negativo de até 64 bits");
}

std::size_t resolve(const Positions& positions, const json& entry, const char* name) {
    const auto id = nonEmptyText(field(entry, name), name);
    const auto vertex = positions.find(id);
    if (vertex == positions.end())
        throw std::invalid_argument(std::string(name) + " referencia um ID fora do cenário: " + id);
    return vertex->second;
}

template <typename ReadEntry>
void readEntries(const json& root, const char* section, ReadEntry readEntry) {
    const auto& entries = field(root, section);
    if (!entries.is_array())
        throw std::invalid_argument(std::string(section) + " deve ser uma lista");

    for (std::size_t i = 0; i < entries.size(); ++i) {
        try {
            readEntry(entries[i]);
        } catch (const std::invalid_argument& error) {
            throw std::invalid_argument(std::string(section) + "[" +
                                        std::to_string(i) + "]: " + error.what());
        }
    }
}

template <typename AddRelation>
void readRelations(const json& root, const char* section, const Positions& positions,
                   const char* fromField, const char* toField, bool directed,
                   AddRelation addRelation) {
    std::set<std::pair<std::size_t, std::size_t>> seen;
    readEntries(root, section, [&](const json& entry) {
        auto from = resolve(positions, entry, fromField);
        auto to = resolve(positions, entry, toField);
        if (!directed && to < from) std::swap(from, to);
        if (!seen.emplace(from, to).second)
            throw std::invalid_argument("relação duplicada");
        addRelation(entry, from, to);
    });
}
}  // namespace

std::optional<TravelPlan> shortestTravelPath(const MissionNetwork& network,
                                             const std::string& origin,
                                             const std::string& destination) {
    const auto vertexIndex = [&](const std::string& id, const char* role) {
        const auto it = std::find(network.vertices.begin(), network.vertices.end(), id);
        if (it == network.vertices.end())
            throw std::invalid_argument(std::string("ID de ") + role + " fora do cenário: " + id);
        return static_cast<std::size_t>(it - network.vertices.begin());
    };
    const auto source = vertexIndex(origin, "origem");
    const auto target = vertexIndex(destination, "destino");
    const auto vertexCount = network.vertices.size();

    // A lista de adjacência referencia as rotas da rede existente.
    std::vector<std::vector<const TravelRoute*>> outgoing(vertexCount);
    for (const auto& route : network.routes) {
        if (route.from >= vertexCount || route.to >= vertexCount)
            throw std::invalid_argument("Rota com extremidade fora do cenário");
        if (!std::isfinite(route.cost) || route.cost < 0)
            throw std::invalid_argument("Custo da rota deve ser finito e não negativo");
        outgoing[route.from].push_back(&route);
    }

    using QueueEntry = std::pair<long double, std::size_t>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>,
                        std::greater<QueueEntry>> pending;
    std::vector<long double> cost(vertexCount);
    std::vector<std::size_t> previous(vertexCount, vertexCount);
    previous[source] = source;
    pending.emplace(0, source);

    while (!pending.empty()) {
        const auto [currentCost, from] = pending.top();
        pending.pop();
        if (currentCost != cost[from]) continue;  // Entrada anterior a uma melhora.
        if (from == target) break;

        for (const auto* route : outgoing[from]) {
            const auto to = route->to;
            const auto nextCost = currentCost + route->cost;
            // Uma alternativa de mesmo custo preserva o predecessor, inclusive
            // em ciclos de custo zero.
            if (previous[to] != vertexCount && nextCost >= cost[to]) continue;
            cost[to] = nextCost;
            previous[to] = from;
            pending.emplace(nextCost, to);
        }
    }

    if (previous[target] == vertexCount) return std::nullopt;
    if (!std::isfinite(cost[target]))
        throw std::overflow_error("Custo mínimo fora da faixa numérica");

    TravelPlan plan{{}, cost[target]};
    for (auto step = target; step != source; step = previous[step])
        plan.path.push_back(network.vertices[step]);
    plan.path.push_back(network.vertices[source]);
    std::reverse(plan.path.begin(), plan.path.end());
    return plan;
}

std::optional<std::vector<std::size_t>> orderActivities(const MissionNetwork& network) {
    const auto count = network.activities.size();
    std::vector<std::size_t> pendingPrerequisites(count, 0);
    std::vector<std::vector<std::size_t>> dependents(count);
    for (const auto& dependency : network.dependencies) {
        if (dependency.before >= count || dependency.after >= count)
            throw std::invalid_argument("Dependência com atividade fora do cenário");
        dependents[dependency.before].push_back(dependency.after);
        ++pendingPrerequisites[dependency.after];
    }

    std::vector<std::size_t> order;
    order.reserve(count);
    for (std::size_t activity = 0; activity < count; ++activity) {
        if (pendingPrerequisites[activity] == 0) order.push_back(activity);
    }

    // A ordem funciona também como fila FIFO de atividades prontas.
    for (std::size_t next = 0; next < order.size(); ++next) {
        for (auto dependent : dependents[order[next]]) {
            if (--pendingPrerequisites[dependent] == 0) order.push_back(dependent);
        }
    }

    if (order.size() != count) return std::nullopt;
    return order;
}

MissionNetwork parseMissionNetwork(const std::string& text,
                                   const BodyCatalog& catalog) {
    json root;
    try {
        root = json::parse(text);
    } catch (const json::exception& error) {
        throw std::invalid_argument(std::string("JSON do cenário inválido: ") + error.what());
    }
    if (!root.is_object()) throw std::invalid_argument("O cenário deve ser um objeto JSON");

    MissionNetwork network;
    Positions bodies;
    readEntries(root, "vertices", [&](const json& entry) {
        auto id = nonEmptyText(entry, "ID do corpo");
        if (!catalog.find(id)) throw std::invalid_argument("ID ausente no catálogo: " + id);
        if (!bodies.emplace(id, network.vertices.size()).second)
            throw std::invalid_argument("vértice duplicado: " + id);
        network.vertices.push_back(std::move(id));
    });
    readRelations(root, "routes", bodies, "from", "to", true,
                  [&](const json& entry, std::size_t from, std::size_t to) {
                      network.routes.push_back({from, to, readCost(entry)});
                  });
    readRelations(root, "infrastructure", bodies, "a", "b", false,
                  [&](const json& entry, std::size_t first, std::size_t second) {
                      network.infrastructure.push_back({first, second, readCost(entry)});
                  });
    readRelations(root, "transfers", bodies, "from", "to", true,
                  [&](const json& entry, std::size_t from, std::size_t to) {
                      network.transfers.push_back({from, to, readCapacity(entry)});
                  });

    Positions activities;
    readEntries(root, "activities", [&](const json& entry) {
        auto id = nonEmptyText(field(entry, "id"), "id");
        auto name = nonEmptyText(field(entry, "name"), "name");
        const auto body = resolve(bodies, entry, "body");
        if (!activities.emplace(id, network.activities.size()).second)
            throw std::invalid_argument("atividade duplicada: " + id);
        network.activities.push_back({std::move(id), std::move(name), body});
    });
    readRelations(root, "dependencies", activities, "before", "after", true,
                  [&](const json&, std::size_t before, std::size_t after) {
                      network.dependencies.push_back({before, after});
                  });
    return network;
}

MissionNetwork readMissionNetwork(const std::string& path, const BodyCatalog& catalog) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Não foi possível abrir o cenário: " + path);
    std::ostringstream content;
    content << file.rdbuf();
    if (file.bad() || content.bad()) throw std::runtime_error("Falha ao ler o cenário: " + path);
    return parseMissionNetwork(content.str(), catalog);
}
}  // namespace cronicas
