#include "regexforge/backtracking.h"

#include <utility>
#include <chrono>

namespace regexforge {

ExecutionLimitExceeded::ExecutionLimitExceeded(std::size_t limit)
    : std::runtime_error("backtracking execution limit exceeded after " + std::to_string(limit) + " steps"), limit_(limit) {}

BacktrackingMatcher::BacktrackingMatcher(Ast ast, BacktrackingLimits limits)
    : ast_(std::move(ast)), limits_(limits) {}

MatchResult BacktrackingMatcher::full_match(std::string_view input) {
    const auto started = std::chrono::steady_clock::now();
    metrics_ = {};
    const bool matched = match_node(ast_, input, 0, [&](std::size_t position) { return position == input.size(); });
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started).count();
    ExecutionMetrics execution;
    execution.backtracking_steps = metrics_.steps;
    execution.steps = metrics_.steps;
    execution.elapsed_microseconds = static_cast<std::size_t>(elapsed);
    execution.elapsed_ms = static_cast<double>(elapsed) / 1000.0;
    return MatchResult{matched, execution};
}

bool BacktrackingMatcher::full_match(const std::string& input) {
    return full_match(std::string_view(input)).matched;
}

bool BacktrackingMatcher::full_match(const char* input) {
    return full_match(std::string_view(input)).matched;
}

void BacktrackingMatcher::count_step() {
    if (metrics_.steps >= limits_.max_steps) throw ExecutionLimitExceeded(limits_.max_steps);
    ++metrics_.steps;
}

bool BacktrackingMatcher::match_node(const Ast& node, std::string_view input, std::size_t position,
                                     const Continuation& continuation) {
    count_step();
    switch (node->kind) {
        case AstNode::Kind::Empty:
            return continuation(position);
        case AstNode::Kind::Literal:
            return position < input.size() && input[position] == node->literal && continuation(position + 1);
        case AstNode::Kind::Concatenation:
            return match_sequence(node->children, 0, input, position, continuation);
        case AstNode::Kind::Alternation:
            for (const auto& branch : node->children) {
                if (match_node(branch, input, position, continuation)) return true;
            }
            return false;
        case AstNode::Kind::Star:
            return match_star(node->children.front(), input, position, continuation);
    }
    return false;
}

bool BacktrackingMatcher::match_sequence(const std::vector<Ast>& nodes, std::size_t index,
                                         std::string_view input, std::size_t position,
                                         const Continuation& continuation) {
    if (index == nodes.size()) return continuation(position);
    return match_node(nodes[index], input, position,
                      [&](std::size_t next) { return match_sequence(nodes, index + 1, input, next, continuation); });
}

bool BacktrackingMatcher::match_star(const Ast& child, std::string_view input, std::size_t position,
                                     const Continuation& continuation) {
    // Try the greedy path first, but do not recurse when a child consumes nothing.
    if (match_node(child, input, position, [&](std::size_t next) {
            return next != position && match_star(child, input, next, continuation);
        })) {
        return true;
    }
    return continuation(position);
}

}  // namespace regexforge
