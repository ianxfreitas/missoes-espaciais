#pragma once
#include "CelestialBody.hpp"
#include <vector>
namespace cronicas {
struct MissionCandidate { std::string id,name; double benefit,cost; };
struct ExcludedDestination { std::string id,reason; };
struct CandidateSet { std::vector<MissionCandidate> candidates; std::vector<ExcludedDestination> excluded; };
struct MissionPlan { std::vector<MissionCandidate> selected; double totalBenefit=0,totalCost=0; size_t missionLimit=0; double budget=0; };
CandidateSet estimateCandidates(const std::vector<const CelestialBody*>& bodies);
MissionPlan planMissions(std::vector<MissionCandidate> candidates,double budget,size_t limit);
}
