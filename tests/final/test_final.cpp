 

#include "../mini_test.h"
#include "../test_helpers.h"

#include "memory/context_builder.h"
#include "memory/memory_index.h"
#include "memory/memory_store.h"
#include "policy/policies.h"
#include "policy/policy_chain.h"
#include "skills/dep_graph.h"
#include "skills/skill_catalog.h"
#include "skills/skill_index.h"
#include "util/lru_cache.h"
#include "util/min_heap.h"

#include <cstdint>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

TEST(heap_pops_in_order) {
    MinHeap<int> h;
    for (int x : {5, 1, 4, 1, 3}) h.push(x);
    CHECK_EQ(h.size(), std::size_t{5});
    CHECK_EQ(h.top(), 1);
    std::vector<int> got;
    while (!h.empty()) got.push_back(h.pop());
    CHECK_EQ(got, (std::vector<int>{1, 1, 3, 4, 5}));
}

TEST(lru_evicts_least_recently_used) {
    LruCache<std::string, int> c(2);
    c.put("a", 1);
    c.put("b", 2);
    REQUIRE(c.get("a") != nullptr);    
    c.put("c", 3);                     
    CHECK(c.contains("a"));
    CHECK(!c.contains("b"));
    CHECK_EQ(c.keys_mru_to_lru(), (std::vector<std::string>{"c", "a"}));
    CHECK_EQ(c.stats().evictions, std::size_t{1});
}

TEST(bst_remove_two_children) {
    MemoryIndex t;
    for (std::uint64_t id : {50, 30, 70, 60, 80}) REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));
    REQUIRE(t.remove(70));
    std::vector<std::uint64_t> ids;
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{30, 50, 60, 80}));
    CHECK(t.find(70) == nullptr);
    REQUIRE(t.find(80) != nullptr);
    CHECK_EQ(t.find(80)->text, std::string("m80"));
    CHECK_EQ(t.size(), std::size_t{4});
}

TEST(ctx_spec_example) {
    std::vector<ContextItem> items{
        {Role::System, 10, false},      
        {Role::User, 10, false},        
        {Role::Assistant, 10, false},   
        {Role::Tool, 10, false},        
        {Role::Assistant, 10, false},   
        {Role::Tool, 10, false},        
        {Role::User, 10, false},        
        {Role::Assistant, 10, false}};  
    ContextPlan p = select_context(items, 50, 2);
    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 1, 4, 6, 7}));
    CHECK_EQ(p.total_tokens, std::size_t{50});
    CHECK(!p.over_budget);
}

TEST(pol_rate_limit_window_boundary) {
    ManualClock clock(100);
    RateLimitPolicy p(2, 10.0, clock);    
    CommandRequest req;
    req.program = "ls";
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);    
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);    
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Deny);     
    clock.advance(8);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);    
    CHECK(p.tracked() <= 2u);
}

TEST(pol_chain_first_deny_stops_first_ask_wins) {
    auto ask1 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask1");
    auto ask2 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask2");
    PolicyChain chain;
    REQUIRE(chain.add(std::make_unique<FixedPolicy>(Decision::Kind::Allow, "ok")));
    REQUIRE(chain.add(std::move(ask1)));
    REQUIRE(chain.add(std::move(ask2)));
    CommandRequest req;
    req.program = "git";
    Decision d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Ask);
    CHECK_EQ(d.policy, std::string("ask1"));

    auto deny = std::make_unique<FixedPolicy>(Decision::Kind::Deny, "no");
    auto after = std::make_unique<FixedPolicy>(Decision::Kind::Allow, "after");
    FixedPolicy* after_ptr = after.get();    
    REQUIRE(chain.add(std::move(deny)));
    REQUIRE(chain.add(std::move(after)));
    d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Deny);
    CHECK_EQ(d.policy, std::string("no"));
    CHECK_EQ(after_ptr->calls, 0);      
}

TEST(graph_order_and_cycle_message) {
    DepGraph g;
    for (const char* n : {"a", "b", "c", "d"}) g.add_node(n);
    g.add_edge("a", "b");    
    g.add_edge("a", "c");
    g.add_edge("b", "d");
    g.add_edge("c", "d");    
    std::vector<std::string> order;
    std::string error;
    REQUIRE(g.order_from("a", &order, &error));
    CHECK_EQ(order, (std::vector<std::string>{"d", "b", "c", "a"}));

    g.add_edge("c", "a");    
    CHECK(!g.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> c -> a"));
}

TEST(index_ranking_and_ties) {
    SkillIndex ix;
    ix.add(0, "format numbers");
    ix.add(1, "csv report with table and numbers");
    ix.add(2, "table format");
    ix.add(3, "unrelated");
    CHECK_EQ(ix.search("csv table numbers", 10), (std::vector<std::size_t>{1, 0, 2}));
    CHECK_EQ(ix.search("format table", 1), (std::vector<std::size_t>{2}));
    REQUIRE(ix.postings("numbers") != nullptr);
    CHECK_EQ(*ix.postings("numbers"), (std::vector<std::size_t>{0, 1}));
}

TEST(heap_empty_throws_and_top_k_edges) {
    MinHeap<int> heap;
    bool top_threw = false, pop_threw = false;
    try { (void)heap.top(); } catch (const std::out_of_range&) { top_threw = true; }
    try { (void)heap.pop(); } catch (const std::out_of_range&) { pop_threw = true; }
    CHECK(top_threw);
    CHECK(pop_threw);
    const std::vector<int> values{4, 2, 9};
    CHECK(top_k(values, 0).empty());
    CHECK_EQ(top_k(values, 10), (std::vector<int>{9, 4, 2}));
}

TEST(heap_top_k_matches_sort_and_times) {
    std::mt19937 rng(309);
    std::vector<int> values(100000);
    for (int& value : values) value = static_cast<int>(rng());
    auto begin = std::chrono::steady_clock::now();
    const auto best = top_k(values, 10);
    const auto heap_end = std::chrono::steady_clock::now();
    auto sorted = values;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    const auto sort_end = std::chrono::steady_clock::now();
    CHECK_EQ(best, (std::vector<int>(sorted.begin(), sorted.begin() + 10)));
    const auto heap_us = std::chrono::duration_cast<std::chrono::microseconds>(heap_end - begin).count();
    const auto sort_us = std::chrono::duration_cast<std::chrono::microseconds>(sort_end - heap_end).count();
    std::cout << "heap top_k(100000,10): " << heap_us << " us; full sort: " << sort_us << " us\n";
}

TEST(lru_zero_capacity_and_stats) {
    LruCache<int, std::string> zero(0);
    zero.put(1, "x");
    CHECK_EQ(zero.size(), std::size_t{0});
    CHECK(zero.get(1) == nullptr);
    CHECK_EQ(zero.stats().misses, std::size_t{1});
    LruCache<int, std::string> one(1);
    one.put(1, "old");
    one.put(1, "new");
    CHECK_EQ(*one.get(1), std::string("new"));
    one.put(2, "next");
    CHECK_EQ(one.stats().evictions, std::size_t{1});
    CHECK_EQ(one.stats().hits, std::size_t{1});
}

TEST(lru_erase_head_middle_tail_and_clear) {
    LruCache<int, int> cache(5);
    for (int i = 1; i <= 4; ++i) cache.put(i, i);
    REQUIRE(cache.erase(4));  
    CHECK_EQ(cache.keys_mru_to_lru(), (std::vector<int>{3, 2, 1}));
    REQUIRE(cache.erase(2));  
    CHECK_EQ(cache.keys_mru_to_lru(), (std::vector<int>{3, 1}));
    REQUIRE(cache.erase(1));  
    CHECK_EQ(cache.keys_mru_to_lru(), (std::vector<int>{3}));
    cache.clear();
    CHECK_EQ(cache.size(), std::size_t{0});
}

TEST(bst_remove_leaf_one_child_root_and_height) {
    MemoryIndex tree;
    for (std::uint64_t id : {40, 20, 60, 10, 30, 25}) REQUIRE(tree.insert({id, std::to_string(id)}));
    CHECK_EQ(tree.height(), std::size_t{4});
    REQUIRE(tree.remove(10));  
    REQUIRE(tree.remove(20));  
    REQUIRE(tree.remove(40));  
    CHECK(tree.find(40) == nullptr);
    CHECK(!tree.remove(999));
    std::vector<std::uint64_t> ids;
    tree.for_each_in_order([&](const MemoryRecord& record) { ids.push_back(record.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{25, 30, 60}));
    CHECK_EQ(tree.size(), std::size_t{3});
}

TEST(bst_duplicate_clear_and_height_measurement) {
    MemoryIndex sequential;
    for (std::uint64_t i = 1; i <= 1000; ++i) REQUIRE(sequential.insert({i, "x"}));
    CHECK(!sequential.insert({500, "duplicate"}));
    const std::size_t seq_height = sequential.height();
    std::vector<std::uint64_t> ids(1000);
    std::iota(ids.begin(), ids.end(), 1);
    std::mt19937 rng(309);
    std::shuffle(ids.begin(), ids.end(), rng);
    MemoryIndex shuffled;
    for (std::uint64_t id : ids) REQUIRE(shuffled.insert({id, "x"}));
    const std::size_t shuffled_height = shuffled.height();
    CHECK_EQ(seq_height, std::size_t{1000});
    CHECK(shuffled_height < seq_height);
    std::cout << "BST heights (1000 ids): sequential=" << seq_height << ", shuffled=" << shuffled_height << "\n";
    sequential.clear();
    CHECK_EQ(sequential.size(), std::size_t{0});
}

TEST(ctx_protected_items_and_over_budget) {
    const std::vector<ContextItem> items{
        {Role::System, 20, false}, {Role::User, 10, false},
        {Role::Tool, 10, false}, {Role::Assistant, 10, true}, {Role::User, 20, false}};
    const ContextPlan plan = select_context(items, 5, 1);
    CHECK_EQ(plan.keep, (std::vector<std::size_t>{0, 3, 4}));
    CHECK_EQ(plan.total_tokens, std::size_t{50});
    CHECK(plan.over_budget);
}

TEST(pol_individual_allow_pattern_confirm_cases) {
    CommandRequest safe{"ls", {"-l"}, {}};
    CommandRequest denied{"rm", {"-rf", "/"}, {}};
    AllowlistPolicy allow({"ls", "echo"});
    CHECK(allow.evaluate(safe).kind == Decision::Kind::Allow);
    CHECK(allow.evaluate(denied).kind == Decision::Kind::Deny);
    ArgumentPatternPolicy patterns({";", "rm -rf", ""});
    CHECK(patterns.evaluate(safe).kind == Decision::Kind::Allow);
    CHECK(patterns.evaluate(denied).kind == Decision::Kind::Deny);
    ConfirmPolicy confirm({"git"});
    CHECK(confirm.evaluate(CommandRequest{"git", {"status"}, {}}).kind == Decision::Kind::Ask);
    CHECK(confirm.evaluate(safe).kind == Decision::Kind::Allow);
}

TEST(pol_empty_and_nested_chains) {
    CommandRequest req{"echo", {}, {}};
    PolicyChain empty;
    CHECK(empty.evaluate(req).kind == Decision::Kind::Deny);
    auto inner = std::make_unique<PolicyChain>();
    inner->add(std::make_unique<FixedPolicy>(Decision::Kind::Ask, "inner-ask"));
    PolicyChain outer;
    outer.add(std::move(inner));
    Decision result = outer.evaluate(req);
    CHECK(result.kind == Decision::Kind::Ask);
    CHECK_EQ(result.policy, std::string("inner-ask"));
}

TEST(graph_missing_self_loop_and_dependents_order) {
    DepGraph graph;
    for (const char* name : {"a", "b", "c", "d", "z"}) graph.add_node(name);
    graph.add_edge("b", "a");
    graph.add_edge("c", "a");
    graph.add_edge("d", "b");
    graph.add_edge("d", "c");
    CHECK_EQ(graph.dependents("a"), (std::vector<std::string>{"b", "c", "d"}));
    graph.add_edge("z", "missing");
    std::vector<std::string> order;
    std::string error;
    CHECK(!graph.order_from("z", &order, &error));
    CHECK_EQ(error, std::string("missing: missing (required by z)"));
    graph.add_edge("a", "a");
    CHECK(!graph.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> a"));
}

TEST(graph_three_node_cycle_exact_message) {
    DepGraph graph;
    for (const char* name : {"a", "b", "c"}) graph.add_node(name);
    graph.add_edge("a", "b"); graph.add_edge("b", "c"); graph.add_edge("c", "a");
    std::vector<std::string> order;
    std::string error;
    CHECK(!graph.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> b -> c -> a"));
}

TEST(graph_iterative_five_thousand_node_chain) {
    DepGraph graph;
    for (int i = 0; i < 5000; ++i) REQUIRE(graph.add_node("n" + std::to_string(i)));
    for (int i = 0; i < 4999; ++i) REQUIRE(graph.add_edge("n" + std::to_string(i), "n" + std::to_string(i + 1)));
    std::vector<std::string> order;
    std::string error;
    REQUIRE(graph.order_from("n0", &order, &error));
    CHECK_EQ(order.size(), std::size_t{5000});
    CHECK_EQ(order.front(), std::string("n4999"));
    CHECK_EQ(order.back(), std::string("n0"));
}

TEST(index_distinct_postings_and_empty_cases) {
    SkillIndex index;
    index.add(7, "alpha alpha beta");
    CHECK_EQ(*index.postings("alpha"), (std::vector<std::size_t>{7}));
    CHECK_EQ(index.word_count(), std::size_t{2});
    CHECK(index.postings("absent") == nullptr);
    CHECK(index.search("missing", 5).empty());
    CHECK(index.search("alpha", 0).empty());
}

TEST(index_timing_table_against_linear_scan) {
    std::mt19937 rng(309);
    for (std::size_t count : {100u, 1000u, 5000u}) {
        std::vector<std::string> docs;
        docs.reserve(count);
        SkillIndex index;
        for (std::size_t i = 0; i < count; ++i) {
            std::string doc = "common token" + std::to_string(i % 50) + " group" + std::to_string(rng() % 100);
            docs.push_back(doc);
            index.add(i, doc);
        }
        constexpr int repetitions = 100;
        auto begin = std::chrono::steady_clock::now();
        for (int i = 0; i < repetitions; ++i) (void)index.search("common group7", 10);
        const auto indexed_end = std::chrono::steady_clock::now();
        for (int i = 0; i < repetitions; ++i) (void)linear_search(docs, "common group7", 10);
        const auto linear_end = std::chrono::steady_clock::now();
        const double indexed_us = std::chrono::duration<double, std::micro>(indexed_end - begin).count() / repetitions;
        const double linear_us = std::chrono::duration<double, std::micro>(linear_end - indexed_end).count() / repetitions;
        std::cout << "index docs=" << count << ": " << indexed_us << " us/query; linear=" << linear_us << " us/query\n";
        CHECK_EQ(index.search("common group7", 10), linear_search(docs, "common group7", 10));
    }
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
