#include "cronicas/BodyCatalog.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <utility>
namespace cronicas {
namespace {
constexpr std::array numericFields{
    std::pair{"gravity", &CelestialBody::gravity},
    std::pair{"meanRadius", &CelestialBody::meanRadius},
    std::pair{"avgTemp", &CelestialBody::avgTemp},
};

std::string lower(std::string text) {
    for (auto& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
}  // namespace
LoadSummary BodyCatalog::load(const ParseResult& parsed,
                              const std::string& source) {
    if (parsed.bodies.empty())
        throw std::runtime_error(
            "Carga sem corpos válidos; catálogo anterior preservado");
    HashTable replacement;
    LoadSummary summary{parsed.bodies.size() + parsed.rejected, 0, 0,
                        parsed.rejected, parsed.warnings};
    for (const auto& body : parsed.bodies) {
        if (replacement.insert(body))
            ++summary.inserted;
        else
            ++summary.updated;
    }
    auto nameIndex = makeTrieIndex();
    decltype(numericIndexes_) numericIndexes;
    for (auto& index : numericIndexes) index = makeBTreeIndex();
    for (const auto* body : replacement.elements()) {
        nameIndex->insert(lower(displayName(*body)), body->id);
        for (size_t i = 0; i < numericFields.size(); ++i) {
            const auto& value = body->*numericFields[i].second;
            if (value) numericIndexes[i]->insert(*value, body->id);
        }
    }

    std::string newSource = source;
    table_ = std::move(replacement);
    nameIndex_ = std::move(nameIndex);
    numericIndexes_ = std::move(numericIndexes);
    source_.swap(newSource);
    return summary;
}
std::vector<const CelestialBody*> BodyCatalog::select(

    const std::function<bool(const CelestialBody&)>& predicate) const {
    auto elements = table_.elements();
    elements.erase(std::remove_if(elements.begin(), elements.end(),
                                  [&](auto body) { return !predicate(*body); }),
                   elements.end());
    std::sort(elements.begin(), elements.end(),
              [](auto a, auto b) { return a->id < b->id; });
    return elements;
}
std::vector<const CelestialBody*> BodyCatalog::list() const {
    return select([](const auto&) { return true; });
}
std::vector<const CelestialBody*> BodyCatalog::searchName(
    const std::string& text) const {
    const auto needle = lower(text);
    return select([&](const auto& b) {
        return lower(displayName(b)).find(needle) != std::string::npos;
    });
}
std::vector<const CelestialBody*> BodyCatalog::resolveIds(
    const std::vector<std::string>& ids) const {
    std::vector<const CelestialBody*> bodies;
    bodies.reserve(ids.size());
    for (const auto& id : ids) {
        if (const auto* body = find(id)) bodies.push_back(body);
    }
    return bodies;
}
std::vector<const CelestialBody*> BodyCatalog::searchNameExact(
    const std::string& text) const {
    if (!nameIndex_) return {};
    return resolveIds(nameIndex_->exact(lower(text)));
}
std::vector<const CelestialBody*> BodyCatalog::searchNamePrefix(
    const std::string& text) const {
    if (!nameIndex_) return {};
    return resolveIds(nameIndex_->prefix(lower(text)));
}
std::vector<const CelestialBody*> BodyCatalog::filterType(
    const std::string& type) const {
    const auto target = lower(type);
    return select([&](const auto& b) { return lower(b.bodyType) == target; });
}
std::vector<const CelestialBody*> BodyCatalog::filterPlanet(bool planet) const {
    return select(
        [&](const auto& b) { return b.isPlanet && *b.isPlanet == planet; });
}
std::vector<const CelestialBody*> BodyCatalog::filterRange(
    const std::string& field, double minimum, double maximum) const {
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum < 0 ||
        minimum > maximum)
        throw std::invalid_argument("Intervalo inválido");

    const auto attribute = std::find_if(
        numericFields.begin(), numericFields.end(),
        [&](const auto& entry) { return field == entry.first; });
    if (attribute == numericFields.end())
        throw std::invalid_argument(
            "Atributo deve ser gravity, meanRadius ou avgTemp");

    const auto& index = numericIndexes_[attribute - numericFields.begin()];
    if (!index) return {};
    auto ids = minimum == maximum ? index->exact(minimum)
                                  : index->range(minimum, maximum);
    std::sort(ids.begin(), ids.end());
    return resolveIds(ids);
}
std::array<NumericIndexStatistics, 3> BodyCatalog::numericIndexStatistics() const {
    std::array<NumericIndexStatistics, 3> statistics{};
    for (size_t i = 0; i < statistics.size(); ++i) {
        statistics[i].field = numericFields[i].first;
        if (const auto& index = numericIndexes_[i]) {
            statistics[i].height = index->height();
            statistics[i].splits = index->splits();
        }
    }
    return statistics;
}
}  // namespace cronicas
