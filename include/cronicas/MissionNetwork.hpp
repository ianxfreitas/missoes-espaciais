#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cronicas {
class BodyCatalog;

// As extremidades referenciam posições em MissionNetwork::vertices.
struct TravelRoute {
    std::size_t from, to;
    double cost;
};

struct InfrastructureLink {
    std::size_t first, second;
    double cost;
};

struct TransferChannel {
    std::size_t from, to;
    std::uint64_t capacity;
};

struct MissionActivity {
    std::string id, name;
    std::size_t body;
};

// As extremidades referenciam posições em MissionNetwork::activities.
struct ActivityDependency {
    std::size_t before, after;
};

struct MissionNetwork {
    std::vector<std::string> vertices;  // IDs exatos do catálogo.
    std::vector<TravelRoute> routes;
    std::vector<InfrastructureLink> infrastructure;
    std::vector<TransferChannel> transfers;
    std::vector<MissionActivity> activities;
    std::vector<ActivityDependency> dependencies;
};

struct TravelPlan {
    std::vector<std::string> path;
    long double totalCost;
};

// Retorna nullopt para um destino inalcançável. IDs fora do cenário são inválidos.
std::optional<TravelPlan> shortestTravelPath(const MissionNetwork& network,
                                             const std::string& origin,
                                             const std::string& destination);

MissionNetwork parseMissionNetwork(const std::string& text,
                                   const BodyCatalog& catalog);
MissionNetwork readMissionNetwork(const std::string& path,
                                  const BodyCatalog& catalog);
}  // namespace cronicas
