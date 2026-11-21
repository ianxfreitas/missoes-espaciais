#pragma once
#include "HashTable.hpp"
#include "BodyJsonParser.hpp"
#include "TrieIndex.hpp"
#include <functional>
#include <memory>
namespace cronicas {
struct LoadSummary {
    size_t received, inserted, updated, rejected;
    std::vector<std::string> warnings;
};
class BodyCatalog {
    HashTable table_;
    std::unique_ptr<TrieIndex> nameIndex_;
    std::string source_ = "nenhuma";

   public:
    LoadSummary load(const ParseResult& parsed, const std::string& source);
    const CelestialBody* find(const std::string& id) const {
        return table_.find(id);
    }
    std::vector<const CelestialBody*> list() const;
    std::vector<const CelestialBody*> searchName(const std::string& text) const;
    std::vector<const CelestialBody*> searchNameExact(
        const std::string& text) const;
    std::vector<const CelestialBody*> searchNamePrefix(
        const std::string& text) const;
    size_t lastNameSearchVisitedNodes() const {
        return nameIndex_ ? nameIndex_->lastVisitedNodes() : 0;
    }
    std::vector<const CelestialBody*> filterType(const std::string& type) const;
    std::vector<const CelestialBody*> filterPlanet(bool planet) const;
    std::vector<const CelestialBody*> filterRange(const std::string& field,
                                                  double minimum,
                                                  double maximum) const;
    HashStatistics statistics() const { return table_.statistics(); }
    const std::string& source() const { return source_; }

   private:
    std::vector<const CelestialBody*> resolveIds(
        const std::vector<std::string>& ids) const;
    std::vector<const CelestialBody*> select(
        const std::function<bool(const CelestialBody&)>& predicate) const;
};
}  // namespace cronicas
