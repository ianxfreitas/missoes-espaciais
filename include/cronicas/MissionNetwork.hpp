#pragma once
#include <cstddef>
#include <cstdint>
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

MissionNetwork parseMissionNetwork(const std::string& text,
                                   const BodyCatalog& catalog);
MissionNetwork readMissionNetwork(const std::string& path,
                                  const BodyCatalog& catalog);
}  // namespace cronicas
