#include "cronicas/MissionPlanner.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace cronicas {
CandidateSet estimateCandidates(const std::vector<const CelestialBody*>& bodies) {
    CandidateSet result;
    for(const auto* b:bodies) {
        if(!b) throw std::invalid_argument("Corpo nulo");
        std::string reason; double weight=0;
        if(b->id=="terre") reason="Terra é a base de referência";
        else if(b->bodyType=="Star") reason="Estrelas fora do escopo de exploração";
        else {
            if(b->bodyType=="Planet") weight=8;
            else if(b->bodyType=="Dwarf Planet") weight=10;
            else if(b->bodyType=="Moon") weight=7;
            else if(b->bodyType=="Asteroid") weight=6;
            else if(b->bodyType=="Comet") weight=9;
            else reason="Categoria sem peso definido";
            if(reason.empty() && (!b->meanRadius || !b->gravity || !std::isfinite(*b->meanRadius) || !std::isfinite(*b->gravity) || *b->meanRadius<=0 || *b->gravity<=0)) reason="Raio ou gravidade ausente, zero ou inválido";
        }
        if(!reason.empty()) { result.excluded.push_back({b->id,reason}); continue; }
        const double radius=std::log10(1+*b->meanRadius/100.0);
        const double gravity=*b->gravity/9.80665;
        const double benefit=weight+2*radius, cost=10+5*radius+2*gravity;
        if(!std::isfinite(benefit) || !std::isfinite(cost)) { result.excluded.push_back({b->id,"Estimativa fora da faixa numérica"}); continue; }
        result.candidates.push_back({b->id,displayName(*b),benefit,cost});
    }
    return result;
}
MissionPlan planMissions(std::vector<MissionCandidate> candidates,double budget,size_t limit) {
    if(!std::isfinite(budget) || budget<0) throw std::invalid_argument("Orçamento deve ser finito e não negativo");
    for(const auto& c:candidates) if(c.id.empty() || !std::isfinite(c.benefit) || c.benefit<0 || !std::isfinite(c.cost) || c.cost<=0 || !std::isfinite(c.benefit/c.cost)) throw std::invalid_argument("Candidato inválido");
    std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    for(size_t i=1;i<candidates.size();++i) if(candidates[i].id==candidates[i-1].id) throw std::invalid_argument("Destino duplicado");
    std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){
        const double ra=a.benefit/a.cost, rb=b.benefit/b.cost;
        if(ra!=rb) return ra>rb;
        if(a.cost!=b.cost) return a.cost<b.cost;
        return a.id<b.id;
    });
    MissionPlan result; result.budget=budget; result.missionLimit=limit;
    for(const auto& candidate:candidates) {
        if(result.selected.size()==limit) break;
        if(candidate.benefit==0 || candidate.cost>budget-result.totalCost) continue;
        const double benefit=result.totalBenefit+candidate.benefit;
        if(!std::isfinite(benefit)) throw std::overflow_error("Benefício acumulado excedeu faixa numérica");
        result.selected.push_back(candidate); result.totalCost+=candidate.cost; result.totalBenefit=benefit;
    }
    return result;
}
}
