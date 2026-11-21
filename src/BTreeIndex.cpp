#include "cronicas/BTreeIndex.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace cronicas {
namespace {
class BTree final : public BTreeIndex {
    using Key = std::pair<double, std::string>;
    static constexpr size_t minDegree = 2;
    static constexpr size_t minKeys = minDegree - 1;
    static constexpr size_t maxKeys = 2 * minDegree - 1;

    struct Node {
        std::vector<Key> keys;
        std::vector<std::unique_ptr<Node>> children;

        explicit Node(bool leaf) {
            keys.reserve(maxKeys);
            if (!leaf) children.reserve(maxKeys + 1);
        }

        bool isLeaf() const { return children.empty(); }
    };

    std::unique_ptr<Node> root_;
    size_t splits_ = 0;

    static size_t lowerBound(const Node& node, const Key& key) {
        return std::lower_bound(node.keys.begin(), node.keys.end(), key) -
               node.keys.begin();
    }

    bool contains(const Key& key) const {
        const Node* node = root_.get();
        while (node) {
            const auto index = lowerBound(*node, key);
            if (index < node->keys.size() && node->keys[index] == key)
                return true;
            if (node->isLeaf()) return false;
            node = node->children[index].get();
        }
        return false;
    }

    // O filho está cheio e o pai tem espaço para a mediana.
    void splitChild(Node& parent, size_t index) {
        auto& left = *parent.children[index];
        auto right = std::make_unique<Node>(left.isLeaf());
        auto median = std::move(left.keys[minKeys]);
        for (size_t i = minDegree; i < left.keys.size(); ++i)
            right->keys.push_back(std::move(left.keys[i]));
        left.keys.resize(minKeys);
        if (!left.isLeaf()) {
            for (size_t i = minDegree; i < left.children.size(); ++i)
                right->children.push_back(std::move(left.children[i]));
            left.children.resize(minDegree);
        }
        parent.keys.insert(parent.keys.begin() + index, std::move(median));
        parent.children.insert(parent.children.begin() + index + 1,
                               std::move(right));
        ++splits_;
    }

    static void borrowFromLeft(Node& parent, size_t index) {
        auto& child = *parent.children[index];
        auto& sibling = *parent.children[index - 1];
        child.keys.insert(child.keys.begin(), std::move(parent.keys[index - 1]));
        parent.keys[index - 1] = std::move(sibling.keys.back());
        sibling.keys.pop_back();
        if (!sibling.isLeaf()) {
            child.children.insert(child.children.begin(),
                                  std::move(sibling.children.back()));
            sibling.children.pop_back();
        }
    }

    static void borrowFromRight(Node& parent, size_t index) {
        auto& child = *parent.children[index];
        auto& sibling = *parent.children[index + 1];
        child.keys.push_back(std::move(parent.keys[index]));
        parent.keys[index] = std::move(sibling.keys.front());
        sibling.keys.erase(sibling.keys.begin());
        if (!sibling.isLeaf()) {
            child.children.push_back(std::move(sibling.children.front()));
            sibling.children.erase(sibling.children.begin());
        }
    }

    static Node& mergeChildren(Node& parent, size_t index) {
        auto& left = *parent.children[index];
        auto& right = *parent.children[index + 1];
        left.keys.push_back(std::move(parent.keys[index]));
        for (auto& key : right.keys) left.keys.push_back(std::move(key));
        for (auto& child : right.children)
            left.children.push_back(std::move(child));
        parent.keys.erase(parent.keys.begin() + index);
        parent.children.erase(parent.children.begin() + index + 1);
        return left;
    }

    // Garante que o filho possa perder uma chave durante a remoção.
    static Node& prepareChild(Node& parent, size_t index) {
        auto& child = *parent.children[index];
        if (child.keys.size() > minKeys) return child;

        if (index > 0 && parent.children[index - 1]->keys.size() > minKeys) {
            borrowFromLeft(parent, index);
        } else if (index < parent.keys.size() &&
                   parent.children[index + 1]->keys.size() > minKeys) {
            borrowFromRight(parent, index);
        } else {
            const auto separator = index < parent.keys.size() ? index : index - 1;
            return mergeChildren(parent, separator);
        }
        return child;
    }

    static Key takeExtreme(Node& start, bool maximum) {
        auto* node = &start;
        while (!node->isLeaf())
            node = &prepareChild(*node, maximum ? node->keys.size() : 0);
        auto position = maximum ? node->keys.end() - 1 : node->keys.begin();
        auto key = std::move(*position);
        node->keys.erase(position);
        return key;
    }

    static void eraseFrom(Node& node, const Key& key) {
        const auto index = lowerBound(node, key);
        if (node.isLeaf()) {
            node.keys.erase(node.keys.begin() + index);
            return;
        }
        if (index == node.keys.size() || node.keys[index] != key) {
            eraseFrom(prepareChild(node, index), key);
        } else if (node.children[index]->keys.size() > minKeys) {
            node.keys[index] = takeExtreme(*node.children[index], true);
        } else if (node.children[index + 1]->keys.size() > minKeys) {
            node.keys[index] = takeExtreme(*node.children[index + 1], false);
        } else {
            eraseFrom(mergeChildren(node, index), key);
        }
    }

    static void collectRange(const Node& node, double minimum, double maximum,
                             std::vector<std::string>& ids) {
        const auto first = std::lower_bound(
            node.keys.begin(), node.keys.end(), minimum,
            [](const Key& key, double bound) { return key.first < bound; });
        const bool leaf = node.isLeaf();
        auto index = static_cast<size_t>(first - node.keys.begin());
        for (; index < node.keys.size() && node.keys[index].first <= maximum;
             ++index) {
            if (!leaf) collectRange(*node.children[index], minimum, maximum, ids);
            ids.push_back(node.keys[index].second);
        }
        if (!leaf) collectRange(*node.children[index], minimum, maximum, ids);
    }

   public:
    void insert(double value, const std::string& id) override {
        if (!std::isfinite(value) || id.empty())
            throw std::invalid_argument("Valor deve ser finito e ID preenchido");

        Key key{value, id};
        if (contains(key)) return;
        if (!root_) root_ = std::make_unique<Node>(true);
        if (root_->keys.size() == maxKeys) {
            auto newRoot = std::make_unique<Node>(false);
            newRoot->children.push_back(std::move(root_));
            try {
                splitChild(*newRoot, 0);
            } catch (...) {
                root_ = std::move(newRoot->children.front());
                throw;
            }
            root_ = std::move(newRoot);
        }

        auto* node = root_.get();
        while (!node->isLeaf()) {
            auto index = lowerBound(*node, key);
            if (node->children[index]->keys.size() == maxKeys) {
                splitChild(*node, index);
                if (node->keys[index] < key) ++index;
            }
            node = node->children[index].get();
        }
        node->keys.insert(node->keys.begin() + lowerBound(*node, key),
                          std::move(key));
    }

    bool erase(double value, const std::string& id) override {
        if (!std::isfinite(value))
            throw std::invalid_argument("Valor deve ser finito");
        const Key key{value, id};
        if (!contains(key)) return false;

        eraseFrom(*root_, key);
        if (root_->keys.empty()) {
            auto oldRoot = std::move(root_);
            if (!oldRoot->isLeaf()) root_ = std::move(oldRoot->children.front());
        }
        return true;
    }

    std::vector<std::string> exact(double value) const override {
        return range(value, value);
    }

    std::vector<std::string> range(double minimum, double maximum) const override {
        if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum)
            throw std::invalid_argument("Intervalo inválido");

        std::vector<std::string> ids;
        if (root_) collectRange(*root_, minimum, maximum, ids);
        return ids;
    }

    size_t height() const override {
        size_t levels = 0;
        for (auto* node = root_.get(); node; ++levels)
            node = node->isLeaf() ? nullptr : node->children.front().get();
        return levels;
    }

    size_t splits() const override { return splits_; }
};
}  // namespace

std::unique_ptr<BTreeIndex> makeBTreeIndex() { return std::make_unique<BTree>(); }
}  // namespace cronicas
