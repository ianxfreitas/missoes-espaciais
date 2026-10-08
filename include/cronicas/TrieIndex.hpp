#pragma once
#include <cstddef>
#include <string>
#include <vector>
namespace cronicas {
// Parte 2: associação de nomes a IDs, sem expor nós da Trie.
class TrieIndex {
   public:
    virtual ~TrieIndex() = default;
    virtual void insert(const std::string& name, const std::string& id) = 0;
    virtual bool erase(const std::string& name, const std::string& id) = 0;
    virtual std::vector<std::string> exact(const std::string& name) const = 0;
    virtual std::vector<std::string> prefix(
        const std::string& prefix) const = 0;
    virtual size_t lastVisitedNodes() const = 0;
};
}  // namespace cronicas
