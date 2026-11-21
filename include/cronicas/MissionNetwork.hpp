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

struct InfrastructurePlan {
    std::vector<std::size_t> selectedLinks;  // Posições em infrastructure.
    long double totalCost = 0;
    std::size_t componentCount = 0;
};

struct TransferPlan {
    std::uint64_t totalFlow = 0;
    std::vector<std::uint64_t> channelFlows;  // Um fluxo por posição em transfers.
    // Posições dos canais que cruzam o corte. Ausente se origem e destino coincidem.
    std::optional<std::vector<std::size_t>> cutChannels;
};

struct RouteAnalysis {
    // Índices em vertices. Um ciclo repete o primeiro vértice no fim.
    std::vector<std::size_t> cycle;
    std::vector<std::vector<std::size_t>> weakComponents;
    std::vector<std::size_t> isolatedVertices;
};

// Retorna nullopt para um destino inalcançável. IDs fora do cenário são inválidos.
std::optional<TravelPlan> shortestTravelPath(const MissionNetwork& network,
                                             const std::string& origin,
                                             const std::string& destination);

// Retorna índices em activities. Um ciclo produz nullopt.
// Um cenário sem atividades produz uma ordem vazia.
std::optional<std::vector<std::size_t>> orderActivities(const MissionNetwork& network);

// Retorna uma floresta mínima. Um único componente indica uma árvore completa.
InfrastructurePlan planInfrastructure(const MissionNetwork& network);

TransferPlan maximumTransfer(const MissionNetwork& network, const std::string& origin,
                             const std::string& destination);

// Analisa apenas routes. Componentes fracos ignoram o sentido das rotas.
RouteAnalysis analyzeRoutes(const MissionNetwork& network);

MissionNetwork parseMissionNetwork(const std::string& text,
                                   const BodyCatalog& catalog);
MissionNetwork readMissionNetwork(const std::string& path,
                                  const BodyCatalog& catalog);
}  // namespace cronicas
