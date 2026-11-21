#include "cronicas/MissionNetwork.hpp"
#include "cronicas/BodyCatalog.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cronicas {
namespace {
using nlohmann::json;
using Positions = std::map<std::string, std::size_t>;

std::size_t vertexIndex(const MissionNetwork& network, const std::string& id,
                        const char* role) {
    const auto it = std::find(network.vertices.begin(), network.vertices.end(), id);
    if (it == network.vertices.end())
        throw std::invalid_argument(std::string("ID de ") + role + " fora do cenário: " + id);
    return static_cast<std::size_t>(it - network.vertices.begin());
}

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

class DisjointSets {
    std::vector<std::size_t> parent_;
    std::vector<std::size_t> size_;

    std::size_t findRoot(std::size_t vertex) {
        auto root = vertex;
        while (parent_[root] != root) root = parent_[root];
        while (vertex != root) {
            const auto next = parent_[vertex];
            parent_[vertex] = root;
            vertex = next;
        }
        return root;
    }

   public:
    explicit DisjointSets(std::size_t count) : parent_(count), size_(count, 1) {
        std::iota(parent_.begin(), parent_.end(), std::size_t{0});
    }

    bool unite(std::size_t first, std::size_t second) {
        auto rootA = findRoot(first);
        auto rootB = findRoot(second);
        if (rootA == rootB) return false;
        if (size_[rootA] < size_[rootB]) std::swap(rootA, rootB);
        parent_[rootB] = rootA;
        size_[rootA] += size_[rootB];
        return true;
    }
};

using IncidentRoutes = std::vector<std::vector<const TravelRoute*>>;

IncidentRoutes indexIncidentRoutes(const MissionNetwork& network) {
    IncidentRoutes incident(network.vertices.size());
    for (const auto& route : network.routes) {
        if (route.from >= incident.size() || route.to >= incident.size())
            throw std::invalid_argument("Rota com extremidade fora do cenário");
        incident[route.from].push_back(&route);
        if (route.from != route.to) incident[route.to].push_back(&route);
    }
    return incident;
}

struct RouteDfsFrame {
    std::size_t vertex;
    std::size_t nextRoute = 0;
};

std::vector<std::size_t> findRouteCycle(const IncidentRoutes& incident) {
    enum class Visit { unseen, active, finished };

    std::vector<Visit> state(incident.size(), Visit::unseen);
    std::vector<RouteDfsFrame> stack;
    for (std::size_t start = 0; start < incident.size(); ++start) {
        if (state[start] != Visit::unseen) continue;
        state[start] = Visit::active;
        stack.push_back({start});
        while (!stack.empty()) {
            auto& frame = stack.back();
            const auto vertex = frame.vertex;
            if (frame.nextRoute == incident[vertex].size()) {
                state[vertex] = Visit::finished;
                stack.pop_back();
                continue;
            }

            const auto& route = *incident[vertex][frame.nextRoute++];
            if (route.from != vertex) continue;
            if (state[route.to] == Visit::unseen) {
                state[route.to] = Visit::active;
                stack.push_back({route.to});
            } else if (state[route.to] == Visit::active) {
                // O trecho da pilha a partir do ancestral já é um caminho dirigido.
                auto first = std::find_if(stack.begin(), stack.end(), [&](const auto& step) {
                    return step.vertex == route.to;
                });
                std::vector<std::size_t> cycle;
                for (; first != stack.end(); ++first) cycle.push_back(first->vertex);
                cycle.push_back(route.to);
                return cycle;
            }
        }
    }
    return {};
}
}  // namespace

std::optional<TravelPlan> shortestTravelPath(const MissionNetwork& network,
                                             const std::string& origin,
                                             const std::string& destination) {
    const auto source = vertexIndex(network, origin, "origem");
    const auto target = vertexIndex(network, destination, "destino");
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

InfrastructurePlan planInfrastructure(const MissionNetwork& network) {
    const auto count = network.vertices.size();
    for (const auto& link : network.infrastructure) {
        if (link.first >= count || link.second >= count)
            throw std::invalid_argument("Ligação de infraestrutura com extremidade fora do cenário");
        if (!std::isfinite(link.cost) || link.cost < 0)
            throw std::invalid_argument("Custo de infraestrutura deve ser finito e não negativo");
    }

    // Ordena posições para preservar as ligações e a ordem do cenário.
    std::vector<std::size_t> links(network.infrastructure.size());
    std::iota(links.begin(), links.end(), std::size_t{0});
    std::sort(links.begin(), links.end(), [&](std::size_t a, std::size_t b) {
        const auto costA = network.infrastructure[a].cost;
        const auto costB = network.infrastructure[b].cost;
        if (costA != costB) return costA < costB;
        return a < b;
    });

    DisjointSets components(count);
    InfrastructurePlan plan;
    plan.componentCount = count;
    for (auto index : links) {
        const auto& link = network.infrastructure[index];
        if (!components.unite(link.first, link.second)) continue;
        plan.selectedLinks.push_back(index);
        plan.totalCost += link.cost;
        if (--plan.componentCount == 1) break;
    }
    if (!std::isfinite(plan.totalCost))
        throw std::overflow_error("Custo total da infraestrutura fora da faixa numérica");
    return plan;
}

TransferPlan maximumTransfer(const MissionNetwork& network, const std::string& origin,
                             const std::string& destination) {
    const auto source = vertexIndex(network, origin, "origem");
    const auto target = vertexIndex(network, destination, "destino");
    const auto count = network.vertices.size();
    TransferPlan plan;
    plan.channelFlows.resize(network.transfers.size(), 0);

    struct ResidualEdge {
        std::size_t to, reverse;
        std::uint64_t capacity;
    };
    std::vector<ResidualEdge> residual;
    residual.reserve(2 * network.transfers.size());
    std::vector<std::vector<std::size_t>> outgoing(count);
    for (const auto& channel : network.transfers) {
        if (channel.from >= count || channel.to >= count)
            throw std::invalid_argument("Canal de transferência com extremidade fora do cenário");
        const auto forward = residual.size();
        const auto reverse = forward + 1;
        residual.push_back({channel.to, reverse, channel.capacity});
        residual.push_back({channel.from, forward, 0});
        outgoing[channel.from].push_back(forward);
        outgoing[channel.to].push_back(reverse);
    }
    if (source == target) return plan;

    std::vector<bool> reached(count, false);
    std::vector<std::size_t> previousEdge(count);
    std::vector<std::size_t> queue;
    queue.reserve(count);
    while (true) {
        for (auto vertex : queue) reached[vertex] = false;
        queue.clear();
        queue.push_back(source);
        reached[source] = true;
        for (std::size_t next = 0; next < queue.size() && !reached[target]; ++next) {
            for (auto index : outgoing[queue[next]]) {
                const auto& edge = residual[index];
                if (edge.capacity == 0 || reached[edge.to]) continue;
                reached[edge.to] = true;
                previousEdge[edge.to] = index;
                queue.push_back(edge.to);
            }
        }
        if (!reached[target]) break;

        auto amount = std::numeric_limits<std::uint64_t>::max();
        for (auto vertex = target; vertex != source;) {
            const auto& edge = residual[previousEdge[vertex]];
            amount = std::min(amount, edge.capacity);
            vertex = residual[edge.reverse].to;
        }
        if (amount > std::numeric_limits<std::uint64_t>::max() - plan.totalFlow)
            throw std::overflow_error("Fluxo máximo excede a faixa de 64 bits");
        for (auto vertex = target; vertex != source;) {
            auto& edge = residual[previousEdge[vertex]];
            auto& reverse = residual[edge.reverse];
            // A soma das capacidades residuais do par permanece igual à original.
            edge.capacity -= amount;
            reverse.capacity += amount;
            vertex = reverse.to;
        }
        plan.totalFlow += amount;
    }

    auto& cut = plan.cutChannels.emplace();
    for (std::size_t i = 0; i < network.transfers.size(); ++i) {
        const auto& channel = network.transfers[i];
        // Cada canal ocupa duas posições, com a aresta reversa em segundo lugar.
        plan.channelFlows[i] = residual[2 * i + 1].capacity;
        if (reached[channel.from] && !reached[channel.to]) cut.push_back(i);
    }
    return plan;
}

RouteAnalysis analyzeRoutes(const MissionNetwork& network) {
    const auto incident = indexIncidentRoutes(network);

    RouteAnalysis analysis;
    analysis.cycle = findRouteCycle(incident);
    std::vector<bool> reached(incident.size(), false);
    for (std::size_t start = 0; start < incident.size(); ++start) {
        if (incident[start].empty()) analysis.isolatedVertices.push_back(start);
        if (reached[start]) continue;

        // Reutiliza o componente como fila e percorre rotas nos dois sentidos.
        auto& component = analysis.weakComponents.emplace_back();
        component.push_back(start);
        reached[start] = true;
        for (std::size_t next = 0; next < component.size(); ++next) {
            const auto vertex = component[next];
            for (const auto* route : incident[vertex]) {
                const auto neighbor = route->from == vertex ? route->to : route->from;
                if (reached[neighbor]) continue;
                reached[neighbor] = true;
                component.push_back(neighbor);
            }
        }
    }
    return analysis;
}

std::vector<std::vector<std::size_t>> stronglyConnectedRouteComponents(
    const MissionNetwork& network) {
    const auto incident = indexIncidentRoutes(network);
    std::vector<bool> seen(incident.size(), false);
    std::vector<std::size_t> finished;
    finished.reserve(incident.size());
    std::vector<RouteDfsFrame> stack;

    // Primeira DFS. Registra cada vértice ao terminar de explorar suas saídas.
    for (std::size_t start = 0; start < incident.size(); ++start) {
        if (seen[start]) continue;
        seen[start] = true;
        stack.push_back({start});
        while (!stack.empty()) {
            auto& frame = stack.back();
            const auto vertex = frame.vertex;
            if (frame.nextRoute == incident[vertex].size()) {
                finished.push_back(vertex);
                stack.pop_back();
                continue;
            }
            const auto* route = incident[vertex][frame.nextRoute++];
            if (route->from != vertex || seen[route->to]) continue;
            seen[route->to] = true;
            stack.push_back({route->to});
        }
    }

    std::fill(seen.begin(), seen.end(), false);
    std::vector<std::vector<std::size_t>> components;
    std::vector<std::size_t> pending;
    for (auto start = finished.rbegin(); start != finished.rend(); ++start) {
        if (seen[*start]) continue;
        auto& component = components.emplace_back();
        seen[*start] = true;
        pending.push_back(*start);
        while (!pending.empty()) {
            const auto vertex = pending.back();
            pending.pop_back();
            component.push_back(vertex);
            // No transposto, cada rota de entrada vira uma saída.
            for (const auto* route : incident[vertex]) {
                if (route->to != vertex || seen[route->from]) continue;
                seen[route->from] = true;
                pending.push_back(route->from);
            }
        }
    }
    return components;
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
