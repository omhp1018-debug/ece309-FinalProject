
#pragma once
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LruCache {
public:
    struct Stats {
        std::size_t hits = 0;
        std::size_t misses = 0;
        std::size_t evictions = 0;
    };

    explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

    ~LruCache() { clear(); }

    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    const Value* get(const Key& key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            ++stats_.misses;
            return nullptr;
        }
        ++stats_.hits;
        move_to_front(it->second);
        return &it->second->value;
    }

    void put(const Key& key, Value value) {
        if (capacity_ == 0) return;
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->value = std::move(value);
            move_to_front(it->second);
            return;
        }
        if (map_.size() == capacity_) {
            Node* victim = tail_;
            const Key victim_key = victim->key;
            unlink(victim);
            map_.erase(victim_key);
            delete victim;
            ++stats_.evictions;
        }
        Node* node = new Node{key, std::move(value), nullptr, nullptr};
        push_front(node);
        map_.emplace(node->key, node);
    }

    bool contains(const Key& key) const {
        return map_.find(key) != map_.end();
    }

    bool erase(const Key& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        Node* node = it->second;
        unlink(node);
        map_.erase(it);
        delete node;
        return true;
    }

    void clear() {
        Node* node = head_;
        while (node != nullptr) {
            Node* next = node->next;
            delete node;
            node = next;
        }
        map_.clear();
        head_ = nullptr;
        tail_ = nullptr;
    }

    std::size_t size() const noexcept { return map_.size(); }
    std::size_t capacity() const noexcept { return capacity_; }
    Stats       stats() const noexcept { return stats_; }

    std::vector<Key> keys_mru_to_lru() const {
        std::vector<Key> out;
        for (Node* n = head_; n != nullptr; n = n->next) out.push_back(n->key);
        return out;
    }

private:
    struct Node {
        Key   key;
        Value value;
        Node* prev;
        Node* next;
    };

    void unlink(Node* node) noexcept {
        if (node->prev != nullptr) node->prev->next = node->next;
        else head_ = node->next;
        if (node->next != nullptr) node->next->prev = node->prev;
        else tail_ = node->prev;
        node->prev = nullptr;
        node->next = nullptr;
    }

    void push_front(Node* node) noexcept {
        node->prev = nullptr;
        node->next = head_;
        if (head_ != nullptr) head_->prev = node;
        else tail_ = node;
        head_ = node;
    }

    //Move a used item to the front.
    void move_to_front(Node* node) noexcept {
        if (node != head_) {
            unlink(node);
            push_front(node);
        }
    }

    std::unordered_map<Key, Node*, Hash> map_;
    Node*       head_ = nullptr;
    Node*       tail_ = nullptr;
    std::size_t capacity_;
    Stats       stats_;
};
