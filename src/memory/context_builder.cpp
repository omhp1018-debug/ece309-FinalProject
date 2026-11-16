
#include "memory/context_builder.h"
#include "util/min_heap.h"
#include <utility>

//Drop lower-priority items until they fit.
ContextPlan select_context(const std::vector<ContextItem>& items,
                           std::size_t budget, std::size_t keep_recent) {
    ContextPlan plan;
    const std::size_t recent_start = keep_recent >= items.size() ? 0 : items.size() - keep_recent;
    std::vector<bool> keep(items.size(), true);
    MinHeap<std::pair<int, std::size_t>> candidates;
    for (std::size_t i = 0; i < items.size(); ++i) {
        plan.total_tokens += items[i].tokens;
        const bool protected_item = items[i].role == Role::System || items[i].pinned || i >= recent_start;
        if (!protected_item) candidates.push({drop_rank(items[i].role), i});
    }
    while (plan.total_tokens > budget && !candidates.empty()) {
        const std::size_t i = candidates.pop().second;
        keep[i] = false;
        plan.total_tokens -= items[i].tokens;
    }
    for (std::size_t i = 0; i < items.size(); ++i) if (keep[i]) plan.keep.push_back(i);
    plan.over_budget = plan.total_tokens > budget;
    return plan;
}
