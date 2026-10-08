#include "cronicas/HashTable.hpp"
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace cronicas {
HashTable::HashTable(size_t capacity) : buckets_(capacity, nullptr) {
    if (!capacity) {
        throw std::invalid_argument("Capacidade deve ser positiva");
    }
}

HashTable::~HashTable() { clear(); }

void HashTable::clear() noexcept {
    for (auto& head : buckets_) {
        while (head) {
            // Guardar next antes de liberar: head deixa de ser válido após
            // delete.
            Node* next = head->next;
            delete head;
            head = next;
        }
    }
    size_ = collisions_ = rehashes_ = 0;
}

HashTable::HashTable(HashTable&& other) noexcept {
    buckets_.swap(other.buckets_);
    size_ = std::exchange(other.size_, 0);
    collisions_ = std::exchange(other.collisions_, 0);
    rehashes_ = std::exchange(other.rehashes_, 0);
}

HashTable& HashTable::operator=(HashTable&& other) noexcept {
    if (this != &other) {
        clear();
        buckets_.clear();
        buckets_.swap(other.buckets_);
        size_ = std::exchange(other.size_, 0);
        collisions_ = std::exchange(other.collisions_, 0);
        rehashes_ = std::exchange(other.rehashes_, 0);
    }
    return *this;
}

std::uint64_t HashTable::hashKey(const std::string& key) noexcept {
    std::uint64_t value = 14695981039346656037ULL;
    for (unsigned char byte : key) {
        value ^= byte;
        value *= 1099511628211ULL;
    }
    return value;
}

void HashTable::rehash(size_t capacity) {
    // A alocação ocorre antes de tocar nas cadeias existentes.
    std::vector<Node*> replacement(capacity, nullptr);
    for (auto head : buckets_) {
        while (head) {
            Node* next = head->next;
            const auto index = head->hash % capacity;
            head->next = replacement[index];
            replacement[index] = head;
            head = next;
        }
    }
    buckets_.swap(replacement);
    ++rehashes_;
    // replacement agora contém somente as antigas cabeças; não possui os nós.
}

bool HashTable::insert(const CelestialBody& body) {
    if (body.id.empty()) {
        throw std::invalid_argument("ID não pode ser vazio");
    }
    if (buckets_.empty()) {
        buckets_.resize(16, nullptr);  // Permite reutilizar uma tabela movida.
    }
    const auto hash = hashKey(body.id);
    auto index = hash % buckets_.size();
    for (Node* node = buckets_[index]; node; node = node->next) {
        if (node->body.id == body.id) {
            CelestialBody copy =
                body;  // Falha de cópia preserva o corpo anterior.
            std::swap(node->body, copy);
            return false;
        }
    }

    // RAII protege o novo nó se a expansão falhar antes de ligá-lo à lista.
    std::unique_ptr<Node> node(new Node{body, hash, nullptr});
    if (static_cast<double>(size_ + 1) / buckets_.size() > 0.75) {
        if (buckets_.size() > std::numeric_limits<size_t>::max() / 2) {
            throw std::length_error("Tabela excedeu capacidade máxima");
        }
        rehash(buckets_.size() * 2);
        index = hash % buckets_.size();
    }
    if (buckets_[index]) {
        ++collisions_;  // Uma chave nova em balde não vazio: um evento.
    }
    node->next = buckets_[index];
    buckets_[index] = node.release();  // A tabela passa a possuir o nó.
    ++size_;
    return true;
}

const CelestialBody* HashTable::find(const std::string& id) const noexcept {
    if (buckets_.empty()) {
        return nullptr;
    }
    for (Node* node = buckets_[hashKey(id) % buckets_.size()]; node;
         node = node->next) {
        if (node->body.id == id) {
            return &node->body;
        }
    }
    return nullptr;
}

std::vector<const CelestialBody*> HashTable::elements() const {
    std::vector<const CelestialBody*> result;
    result.reserve(size_);
    for (auto head : buckets_) {
        for (auto node = head; node; node = node->next) {
            result.push_back(&node->body);
        }
    }
    return result;
}
}  // namespace cronicas
