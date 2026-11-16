
#include "memory/memory_index.h"

MemoryIndex::~MemoryIndex() {
    clear();
}

bool MemoryIndex::insert(MemoryRecord rec) {
    Node** link = &root_;
    while (*link != nullptr) {
        if (rec.id < (*link)->rec.id) link = &(*link)->left;
        else if (rec.id > (*link)->rec.id) link = &(*link)->right;
        else return false;
    }
    *link = new Node{std::move(rec), nullptr, nullptr};
    ++count_;
    return true;
}

const MemoryRecord* MemoryIndex::find(std::uint64_t id) const {
    const Node* node = root_;
    while (node != nullptr) {
        if (id < node->rec.id) node = node->left;
        else if (id > node->rec.id) node = node->right;
        else return &node->rec;
    }
    return nullptr;
}

//Keep the tree linked after removing a node.
bool MemoryIndex::remove(std::uint64_t id) {
    Node** link = &root_;
    while (*link != nullptr && (*link)->rec.id != id) {
        link = id < (*link)->rec.id ? &(*link)->left : &(*link)->right;
    }
    if (*link == nullptr) return false;
    Node* victim = *link;
    if (victim->left == nullptr || victim->right == nullptr) {
        *link = victim->left != nullptr ? victim->left : victim->right;
        delete victim;
    } else {
        Node** successor_link = &victim->right;
        while ((*successor_link)->left != nullptr) successor_link = &(*successor_link)->left;
        Node* successor = *successor_link;
        victim->rec = std::move(successor->rec);
        *successor_link = successor->right;
        delete successor;
    }
    --count_;
    return true;
}

std::size_t MemoryIndex::size() const noexcept { return count_; }

std::size_t MemoryIndex::height() const {
    const auto height_of = [&](const auto& self, const Node* node) -> std::size_t {
        if (node == nullptr) return 0;
        const std::size_t left = self(self, node->left);
        const std::size_t right = self(self, node->right);
        return 1 + (left > right ? left : right);
    };
    return height_of(height_of, root_);
}

void MemoryIndex::for_each_in_order(const std::function<void(const MemoryRecord&)>& fn) const {
    const auto walk = [&](const auto& self, const Node* node) -> void {
        if (node == nullptr) return;
        self(self, node->left);
        fn(node->rec);
        self(self, node->right);
    };
    walk(walk, root_);
}

void MemoryIndex::clear() {
    const auto destroy = [&](const auto& self, Node* node) -> void {
        if (node == nullptr) return;
        self(self, node->left);
        self(self, node->right);
        delete node;
    };
    destroy(destroy, root_);
    root_ = nullptr;
    count_ = 0;
}
