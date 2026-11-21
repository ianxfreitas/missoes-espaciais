#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
namespace cronicas {
// Associação de nomes a IDs. As chaves são comparadas byte a byte.
class TrieIndex {
   public:
    virtual ~TrieIndex() = default;
    // Nome ou ID vazio é inválido. Reinserir o mesmo par é idempotente.
    virtual void insert(const std::string& name, const std::string& id) = 0;
    // Retorna false se a associação estiver ausente.
    virtual bool erase(const std::string& name, const std::string& id) = 0;
    // Consultas devolvem IDs únicos, ordenados por ID.
    virtual std::vector<std::string> exact(const std::string& name) const = 0;
    // O prefixo vazio corresponde a todos os nomes.
    virtual std::vector<std::string> prefix(
        const std::string& prefix) const = 0;
    // A raiz conta como um nó. Inserção e remoção preservam a métrica.
    virtual size_t lastVisitedNodes() const = 0;
};

std::unique_ptr<TrieIndex> makeTrieIndex();
}  // namespace cronicas
