#include "cronicas/TrieIndex.hpp"
#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <stdexcept>

namespace cronicas {
namespace {
class Trie final : public TrieIndex {
    struct Node;
    using Nodes = std::list<Node>;

    struct Node {
        std::map<unsigned char, Nodes::iterator> children;
        std::set<std::string> ids;
    };

    // A lista possui os nós. As arestas referenciam posições estáveis, sem
    // exigir que a destruição de um ramo percorra a árvore recursivamente.
    Nodes nodes_{1};
    mutable size_t visited_ = 0;

    const Node* findNode(const std::string& text) const {
        const Node* node = &nodes_.front();
        visited_ = 1;
        for (unsigned char byte : text) {
            auto child = node->children.find(byte);
            if (child == node->children.end()) return nullptr;
            node = &*child->second;
            ++visited_;
        }
        return node;
    }

   public:
    Trie() = default;
    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;

    void insert(const std::string& name, const std::string& id) override {
        if (name.empty() || id.empty())
            throw std::invalid_argument("Nome e ID devem ser preenchidos");

        auto node = nodes_.begin();
        size_t offset = 0;
        for (; offset < name.size(); ++offset) {
            const auto byte = static_cast<unsigned char>(name[offset]);
            auto child = node->children.find(byte);
            if (child == node->children.end()) break;
            node = child->second;
        }
        if (offset == name.size()) {
            node->ids.insert(id);
            return;
        }

        // Prepara todo o ramo antes de ligá-lo à Trie. A transferência da
        // lista preserva os iteradores usados nas novas arestas.
        const auto firstByte = static_cast<unsigned char>(name[offset]);
        Nodes suffix(1);
        auto leaf = suffix.begin();
        for (++offset; offset < name.size(); ++offset) {
            const auto byte = static_cast<unsigned char>(name[offset]);
            const auto next = suffix.emplace(suffix.end());
            leaf->children.emplace(byte, next);
            leaf = next;
        }
        leaf->ids.insert(id);
        node->children.emplace(firstByte, suffix.begin());
        nodes_.splice(nodes_.end(), suffix);
    }

    bool erase(const std::string& name, const std::string& id) override {
        if (name.empty() || id.empty()) return false;

        auto node = nodes_.begin();
        std::vector<Nodes::iterator> parents;
        for (unsigned char byte : name) {
            auto child = node->children.find(byte);
            if (child == node->children.end()) return false;
            parents.push_back(node);
            node = child->second;
        }
        if (node->ids.erase(id) == 0) return false;

        // Remove de baixo para cima apenas os nós sem filhos nem IDs.
        size_t depth = name.size();
        while (depth > 0 && node->ids.empty() && node->children.empty()) {
            --depth;
            auto parent = parents[depth];
            parent->children.erase(static_cast<unsigned char>(name[depth]));
            nodes_.erase(node);
            node = parent;
        }
        return true;
    }

    std::vector<std::string> exact(const std::string& name) const override {
        const auto* node = findNode(name);
        if (!node) return {};
        return {node->ids.begin(), node->ids.end()};
    }

    std::vector<std::string> prefix(const std::string& text) const override {
        const auto* start = findNode(text);
        if (!start) return {};

        std::vector<std::string> ids;
        std::vector<const Node*> pending{start};
        while (!pending.empty()) {
            const auto* node = pending.back();
            pending.pop_back();
            ids.insert(ids.end(), node->ids.begin(), node->ids.end());
            for (const auto& child : node->children) {
                pending.push_back(&*child.second);
                ++visited_;
            }
        }
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        return ids;
    }

    size_t lastVisitedNodes() const override { return visited_; }
};
}  // namespace

std::unique_ptr<TrieIndex> makeTrieIndex() { return std::make_unique<Trie>(); }
}  // namespace cronicas
