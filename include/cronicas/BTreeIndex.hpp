#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
namespace cronicas {
// Índice ordenado por (valor, ID). As operações exigem valores finitos.
class BTreeIndex {
   public:
    virtual ~BTreeIndex() = default;
    // Exige ID preenchido. Reinserir o mesmo par é idempotente.
    virtual void insert(double value, const std::string& id) = 0;
    // Retorna false se o par estiver ausente.
    virtual bool erase(double value, const std::string& id) = 0;
    // Consultas retornam IDs na ordem das chaves. O intervalo é inclusivo.
    virtual std::vector<std::string> exact(double value) const = 0;
    virtual std::vector<std::string> range(double minimum,
                                           double maximum) const = 0;
    // Conta níveis. Uma árvore vazia tem altura zero.
    virtual size_t height() const = 0;
    // Conta divisões desde a construção, inclusive as da raiz.
    virtual size_t splits() const = 0;
};

std::unique_ptr<BTreeIndex> makeBTreeIndex();
}  // namespace cronicas
