
#include "policy/policy_chain.h"

#include <utility>

bool PolicyChain::add(std::unique_ptr<Policy> child) {
    if (!child) return false;
    children_.push_back(std::move(child));
    return true;
}

std::size_t PolicyChain::size() const noexcept { return children_.size(); }

//Stop at a denial and keep the first ask.
Decision PolicyChain::evaluate(const CommandRequest& req) {
    if (children_.empty()) return Decision::deny("empty policy chain denies by default", name());
    bool saw_ask = false;
    Decision first_ask;
    for (const auto& child : children_) {
        Decision decision = child->evaluate(req);
        if (decision.kind == Decision::Kind::Deny) return decision;
        if (decision.kind == Decision::Kind::Ask && !saw_ask) {
            first_ask = std::move(decision);
            saw_ask = true;
        }
    }
    if (saw_ask) return first_ask;
    return Decision::allow("all policies allowed the command", name());
}
