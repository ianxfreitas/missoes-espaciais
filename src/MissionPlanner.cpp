#include "cronicas/MissionPlanner.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace cronicas {
namespace {
constexpr size_t unitsPerCredit = 100;
constexpr size_t maxTableBytes = 64 * 1024 * 1024;
constexpr size_t maxBudgetUnits = maxTableBytes / sizeof(double);
constexpr auto tableLimitMessage =
    "Cenário excede o limite de 64 MiB da programação dinâmica. "
    "Reduza o orçamento ou o limite de missões.";

double fromBudgetUnits(size_t units) {
    return static_cast<double>(units) / unitsPerCredit;
}

size_t floorBudgetUnits(double value) {
    auto units = static_cast<size_t>(value * unitsPerCredit);
    // Compara com as fronteiras de centésimo representáveis em double.
    // Assim, 0.29 vale 29 unidades mesmo se 0.29 * 100 arredondar para baixo.
    if (value < fromBudgetUnits(units))
        --units;
    else if (value >= fromBudgetUnits(units + 1))
        ++units;
    return units;
}

void validateAndSortCandidates(std::vector<MissionCandidate>& candidates,
                               double budget, bool needsRatio) {
    if (!std::isfinite(budget) || budget < 0)
        throw std::invalid_argument("Orçamento deve ser finito e não negativo");
    for (const auto& c : candidates)
        if (c.id.empty() || !std::isfinite(c.benefit) || c.benefit < 0 ||
            !std::isfinite(c.cost) || c.cost <= 0 ||
            (needsRatio && !std::isfinite(c.benefit / c.cost)))
            throw std::invalid_argument("Candidato inválido");
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });
    for (size_t i = 1; i < candidates.size(); ++i)
        if (candidates[i].id == candidates[i - 1].id)
            throw std::invalid_argument("Destino duplicado");
}
}  // namespace

CandidateSet estimateCandidates(
    const std::vector<const CelestialBody*>& bodies) {
    CandidateSet result;
    for (const auto* b : bodies) {
        if (!b) throw std::invalid_argument("Corpo nulo");
        std::string reason;
        double weight = 0;
        if (b->id == "terre")
            reason = "Terra é a base de referência";
        else if (b->bodyType == "Star")
            reason = "Estrelas fora do escopo de exploração";
        else {
            if (b->bodyType == "Planet")
                weight = 8;
            else if (b->bodyType == "Dwarf Planet")
                weight = 10;
            else if (b->bodyType == "Moon")
                weight = 7;
            else if (b->bodyType == "Asteroid")
                weight = 6;
            else if (b->bodyType == "Comet")
                weight = 9;
            else
                reason = "Categoria sem peso definido";
            if (reason.empty() && (!b->meanRadius || !b->gravity ||
                                   !std::isfinite(*b->meanRadius) ||
                                   !std::isfinite(*b->gravity) ||
                                   *b->meanRadius <= 0 || *b->gravity <= 0))
                reason = "Raio ou gravidade ausente, zero ou inválido";
        }
        if (!reason.empty()) {
            result.excluded.push_back({b->id, reason});
            continue;
        }
        const double radius = std::log10(1 + *b->meanRadius / 100.0);
        const double gravity = *b->gravity / 9.80665;
        const double benefit = weight + 2 * radius,
                     cost = 10 + 5 * radius + 2 * gravity;
        if (!std::isfinite(benefit) || !std::isfinite(cost)) {
            result.excluded.push_back(
                {b->id, "Estimativa fora da faixa numérica"});
            continue;
        }
        result.candidates.push_back({b->id, displayName(*b), benefit, cost});
    }
    return result;
}
MissionPlan planMissions(std::vector<MissionCandidate> candidates,
                         double budget, size_t limit) {
    validateAndSortCandidates(candidates, budget, true);
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) {
                  const double ra = a.benefit / a.cost, rb = b.benefit / b.cost;
                  if (ra != rb) return ra > rb;
                  if (a.cost != b.cost) return a.cost < b.cost;
                  return a.id < b.id;
              });
    MissionPlan result;
    result.budget = budget;
    result.missionLimit = limit;
    for (const auto& candidate : candidates) {
        if (result.selected.size() == limit) break;
        if (candidate.benefit == 0 ||
            candidate.cost > budget - result.totalCost)
            continue;
        const double benefit = result.totalBenefit + candidate.benefit;
        if (!std::isfinite(benefit))
            throw std::overflow_error(
                "Benefício acumulado excedeu faixa numérica");
        result.selected.push_back(candidate);
        result.totalCost += candidate.cost;
        result.totalBenefit = benefit;
    }
    return result;
}

MissionPlan optimizeMissions(std::vector<MissionCandidate> candidates,
                             double budget, size_t limit) {
    validateAndSortCandidates(candidates, budget, false);
    MissionPlan result;
    result.budget = budget;
    result.missionLimit = limit;

    const double maxBudget = fromBudgetUnits(maxBudgetUnits);
    const auto availableBudget = floorBudgetUnits(std::min(budget, maxBudget));
    if (limit == 0 || availableBudget == 0) return result;

    struct WeightedCandidate {
        const MissionCandidate* candidate;
        size_t cost;
    };
    std::vector<WeightedCandidate> feasible;
    size_t combinedCost = 0;
    size_t cheapestCost = availableBudget;
    for (const auto& candidate : candidates) {
        if (candidate.benefit == 0 || candidate.cost > budget) continue;
        if (candidate.cost > maxBudget) throw std::length_error(tableLimitMessage);
        auto cost = floorBudgetUnits(candidate.cost);
        if (candidate.cost > fromBudgetUnits(cost)) ++cost;
        if (cost > availableBudget) continue;
        feasible.push_back({&candidate, cost});
        combinedCost += std::min(cost, availableBudget - combinedCost);
        cheapestCost = std::min(cost, cheapestCost);
    }
    if (feasible.empty()) return result;

    const auto capacity = combinedCost;
    const auto slots = std::min({limit, feasible.size(), capacity / cheapestCost});
    const auto width = capacity + 1;
    // Testa as dimensões por divisão antes das multiplicações e alocações.
    if (slots + 1 > maxTableBytes / sizeof(double) / width)
        throw std::length_error(tableLimitMessage);
    const auto states = (slots + 1) * width;
    if (feasible.size() > (maxTableBytes - states * sizeof(double)) / states)
        throw std::length_error(tableLimitMessage);

    const double unreachable = -std::numeric_limits<double>::infinity();
    std::vector<double> best(states, unreachable);
    std::fill_n(best.begin(), width, 0.0);
    std::vector<unsigned char> took(feasible.size() * states, 0);

    for (size_t i = 0; i < feasible.size(); ++i) {
        const auto& item = feasible[i];
        // Cada linha representa uma quantidade exata. Descendo em quantidade,
        // a transição ainda lê a linha do conjunto anterior de candidatos.
        for (size_t count = std::min(slots, i + 1); count > 0; --count) {
            for (size_t b = item.cost; b <= capacity; ++b) {
                const auto state = count * width + b;
                const auto benefit = best[state - width - item.cost] +
                                     item.candidate->benefit;
                if (benefit > best[state]) {
                    if (!std::isfinite(benefit))
                        throw std::overflow_error(
                            "Benefício acumulado excedeu faixa numérica");
                    best[state] = benefit;
                    took[i * states + state] = 1;
                }
            }
        }
    }

    // Em empate, prefere menos missões e depois o menor custo discretizado.
    size_t count = 0;
    for (size_t k = 1; k <= slots; ++k) {
        const auto benefit = best[k * width + capacity];
        if (benefit > result.totalBenefit) {
            result.totalBenefit = benefit;
            count = k;
        }
    }
    const auto row = best.begin() + count * width;
    const auto usedUnits = static_cast<size_t>(
        std::lower_bound(row, row + width, result.totalBenefit) - row);

    size_t remaining = usedUnits;
    long double totalCost = 0;
    for (size_t i = feasible.size(); i > 0 && count > 0; --i) {
        if (!took[(i - 1) * states + count * width + remaining]) continue;
        const auto& item = feasible[i - 1];
        result.selected.push_back(*item.candidate);
        totalCost += item.candidate->cost;
        remaining -= item.cost;
        --count;
    }
    std::reverse(result.selected.begin(), result.selected.end());
    // O limite inteiro também corrige o erro de representação ao somar
    // decimais como 0.1 + 0.2. Os custos renderizados são os originais.
    result.totalCost = std::min(static_cast<double>(totalCost),
                                fromBudgetUnits(usedUnits));
    return result;
}
}  // namespace cronicas
