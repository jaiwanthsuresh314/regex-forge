#include "regexforge/nfa.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace regexforge {

ThompsonNFAMatcher::ThompsonNFAMatcher(Ast ast, NfaLimits limits)
    : ast_(std::move(ast)), limits_(limits) {
    const Fragment fragment = compile(ast_);
    start_ = fragment.start;
    accept_ = fragment.end;
    states_[accept_].accepting = true;
}

std::size_t ThompsonNFAMatcher::add_state() {
    states_.push_back(State{});
    return states_.size() - 1;
}

ThompsonNFAMatcher::Fragment ThompsonNFAMatcher::compile(const Ast& node) {
    switch (node->kind) {
        case AstNode::Kind::Empty: {
            const auto start = add_state(); const auto end = add_state();
            states_[start].epsilon.push_back(end); return {start, end};
        }
        case AstNode::Kind::Literal: {
            const auto start = add_state(); const auto end = add_state();
            states_[start].transitions.emplace_back(node->literal, end); return {start, end};
        }
        case AstNode::Kind::Concatenation: {
            Fragment result = compile(node->children.front());
            for (std::size_t i = 1; i < node->children.size(); ++i) {
                const Fragment next = compile(node->children[i]);
                states_[result.end].epsilon.push_back(next.start); result.end = next.end;
            }
            return result;
        }
        case AstNode::Kind::Alternation: {
            const auto start = add_state(); const auto end = add_state();
            for (const auto& branch : node->children) {
                const Fragment child = compile(branch);
                states_[start].epsilon.push_back(child.start);
                states_[child.end].epsilon.push_back(end);
            }
            return {start, end};
        }
        case AstNode::Kind::Star: {
            const auto start = add_state(); const auto end = add_state();
            const Fragment child = compile(node->children.front());
            states_[start].epsilon.push_back(end); states_[start].epsilon.push_back(child.start);
            states_[child.end].epsilon.push_back(child.start); states_[child.end].epsilon.push_back(end);
            return {start, end};
        }
    }
    throw std::logic_error("unknown AST node kind");
}

void ThompsonNFAMatcher::epsilon_closure(std::vector<std::size_t>& states) const {
    std::vector<bool> seen(states_.size(), false);
    std::vector<std::size_t> pending;
    for (const auto state : states) {
        if (!seen[state]) { seen[state] = true; pending.push_back(state); }
    }
    for (std::size_t i = 0; i < pending.size(); ++i) {
        for (const auto next : states_[pending[i]].epsilon) {
            if (!seen[next]) { seen[next] = true; pending.push_back(next); }
        }
    }
    states = std::move(pending);
}

void ThompsonNFAMatcher::record_active(std::size_t count) {
    max_active_states_ = std::max(max_active_states_, count);
    if (count > limits_.max_active_states) throw std::runtime_error("NFA active-state limit exceeded");
}

MatchResult ThompsonNFAMatcher::full_match(std::string_view input) {
    max_active_states_ = 0;
    std::vector<std::size_t> current{start_};
    epsilon_closure(current); record_active(current.size());
    for (const char symbol : input) {
        std::vector<std::size_t> next;
        for (const auto state : current) {
            for (const auto& [value, destination] : states_[state].transitions) {
                if (value == symbol) next.push_back(destination);
            }
        }
        epsilon_closure(next); record_active(next.size()); current = std::move(next);
    }
    const bool matched = std::any_of(current.begin(), current.end(), [&](const auto state) {
        return states_[state].accepting;
    });
    return MatchResult{matched, ExecutionMetrics{0, max_active_states_, 0, 0}};
}

}  // namespace regexforge
