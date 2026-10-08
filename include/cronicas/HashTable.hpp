#pragma once
#include "CelestialBody.hpp"
#include <cstdint>
#include <vector>
namespace cronicas {
struct HashStatistics {
    size_t elements, capacity, collisions, rehashes;
    double loadFactor;
};
class HashTable {
    struct Node {
        CelestialBody body;
        std::uint64_t hash;
        Node* next;
    };
    std::vector<Node*> buckets_;
    size_t size_ = 0, collisions_ = 0, rehashes_ = 0;
    void clear() noexcept;
    void rehash(size_t capacity);

   public:
    explicit HashTable(size_t capacity = 16);
    ~HashTable();
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;
    HashTable(HashTable&& other) noexcept;
    HashTable& operator=(HashTable&& other) noexcept;
    static std::uint64_t hashKey(const std::string& key) noexcept;
    // true: nova chave; false: atualização da chave existente.
    bool insert(const CelestialBody& body);
    const CelestialBody* find(const std::string& id) const noexcept;
    std::vector<const CelestialBody*> elements() const;
    HashStatistics statistics() const noexcept {
        return {size_, capacity(), collisions_, rehashes_, loadFactor()};
    }
    size_t size() const noexcept { return size_; }
    size_t capacity() const noexcept { return buckets_.size(); }
    size_t collisions() const noexcept { return collisions_; }
    size_t rehashes() const noexcept { return rehashes_; }
    double loadFactor() const noexcept {
        return capacity() ? static_cast<double>(size_) / capacity() : 0;
    }
};
}  // namespace cronicas
