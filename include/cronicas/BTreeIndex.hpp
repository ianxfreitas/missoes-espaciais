#pragma once
#include <cstddef>
#include <string>
#include <vector>
namespace cronicas {
// Parte 2: índice físico ordenado por (valor, ID); valores iguais são permitidos.
class BTreeIndex {
public:
    virtual ~BTreeIndex() = default;
    virtual void insert(double value,const std::string& id)=0;
    virtual bool erase(double value,const std::string& id)=0;
    virtual std::vector<std::string> exact(double value) const=0;
    virtual std::vector<std::string> range(double minimum,double maximum) const=0;
    virtual size_t height() const=0;
    virtual size_t splits() const=0;
};
}
