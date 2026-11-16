#include "skills/skill_index.h"
#include "util/min_heap.h"
#include "util/text.h"
#include <utility>

void SkillIndex::add(std::size_t doc_id, std::string_view text) {
    for (const std::string& word : unique_tokens(text)) postings_[word].push_back(doc_id);
}

const std::vector<std::size_t>* SkillIndex::postings(std::string_view word) const {
    const auto it = postings_.find(std::string(word));
    return it == postings_.end() ? nullptr : &it->second;
}

std::size_t SkillIndex::word_count() const noexcept { return postings_.size(); }

//Rank documents by matching query words.
std::vector<std::size_t> SkillIndex::search(std::string_view query, std::size_t k) const {
    if (k == 0) return {};
    std::unordered_map<std::size_t, std::size_t> scores;
    for (const std::string& word : unique_tokens(query)) {
        const auto it = postings_.find(word);
        if (it != postings_.end()) for (std::size_t id : it->second) ++scores[id];
    }
    using Scored = std::pair<std::size_t, std::size_t>;
    const auto worse = [](const Scored& a, const Scored& b) {
        if (a.first != b.first) return a.first < b.first;
        return a.second > b.second;
    };
    std::vector<Scored> ranked;
    ranked.reserve(scores.size());
    for (const auto& item : scores) ranked.emplace_back(item.second, item.first);
    const auto best = top_k(ranked, k, worse);
    std::vector<std::size_t> result;
    result.reserve(best.size());
    for (const auto& item : best) result.push_back(item.second);
    return result;
}
