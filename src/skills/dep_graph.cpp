#include "skills/dep_graph.h"
#include <algorithm>
#include <sstream>
#include <utility>

bool DepGraph::add_node(const std::string& name) {
    return requires_.emplace(name, std::vector<std::string>{}).second;
}

bool DepGraph::add_edge(const std::string& from, const std::string& to) {
    auto it = requires_.find(from);
    if (it == requires_.end() || std::find(it->second.begin(), it->second.end(), to) != it->second.end()) return false;
    it->second.push_back(to);
    required_by_[to].push_back(from);
    return true;
}

bool DepGraph::has_node(const std::string& name) const {
    return requires_.find(name) != requires_.end();
}

std::size_t DepGraph::node_count() const noexcept { return requires_.size(); }

const std::vector<std::string>& DepGraph::requirements(const std::string& name) const {
    auto it = requires_.find(name);
    if (it != requires_.end()) return it->second;
    static const std::vector<std::string> none;
    return none;
}

//Visit requirements before the skill.
bool DepGraph::order_from(const std::string& root, std::vector<std::string>* out, std::string* error) const {
    if (!has_node(root)) { *error = "unknown: " + root; return false; }
    struct Frame { std::string name; std::size_t next = 0; };
    std::unordered_map<std::string, int> color;
    std::vector<std::string> path, order;
    std::vector<Frame> stack;
    color[root] = 1;
    path.push_back(root);
    stack.push_back({root, 0});
    while (!stack.empty()) {
        Frame& frame = stack.back();
        const auto edges = requires_.find(frame.name);
        if (edges != requires_.end() && frame.next < edges->second.size()) {
            const std::string dep = edges->second[frame.next++];
            if (!has_node(dep)) {
                *error = "missing: " + dep + " (required by " + frame.name + ")";
                return false;
            }
            if (color[dep] == 1) {
                const auto first = std::find(path.begin(), path.end(), dep);
                std::ostringstream msg;
                msg << "cycle: ";
                for (auto it = first; it != path.end(); ++it) {
                    if (it != first) msg << " -> ";
                    msg << *it;
                }
                msg << " -> " << dep;
                *error = msg.str();
                return false;
            }
            if (color[dep] == 0) {
                color[dep] = 1;
                path.push_back(dep);
                stack.push_back({dep, 0});
            }
            continue;
        }
        color[frame.name] = 2;
        order.push_back(frame.name);
        stack.pop_back();
        path.pop_back();
    }
    *out = std::move(order);
    return true;
}

//Find skills that depend on this one.
std::vector<std::string> DepGraph::dependents(const std::string& name) const {
    std::vector<std::string> result;
    std::vector<std::string> queue{name};
    std::unordered_map<std::string, std::size_t> distance;
    distance.emplace(name, 0);
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const std::string current = queue[head];
        const auto it = required_by_.find(current);
        if (it == required_by_.end()) continue;
        for (const std::string& next : it->second) {
            if (distance.find(next) != distance.end()) continue;
            distance.emplace(next, distance[current] + 1);
            queue.push_back(next);
            result.push_back(next);
        }
    }
    std::sort(result.begin(), result.end(), [&](const std::string& a, const std::string& b) {
        if (distance[a] != distance[b]) return distance[a] < distance[b];
        return a < b;
    });
    return result;
}
