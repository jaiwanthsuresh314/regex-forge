#include "regexforge/nfa.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace regexforge {

namespace {

NfaFragment build_fragment(ThompsonNfa& nfa, const Ast& node) {
    if (!node) {
        StateId s = nfa.add_state();
        StateId t = nfa.add_state();
        nfa.add_epsilon_transition(s, t);
        return {s, t};
    }

    switch (node->kind) {
        case AstNode::Kind::Empty: {
            StateId s = nfa.add_state();
            StateId t = nfa.add_state();
            nfa.add_epsilon_transition(s, t);
            return {s, t};
        }
        case AstNode::Kind::Literal: {
            StateId s = nfa.add_state();
            StateId t = nfa.add_state();
            nfa.add_char_transition(s, node->literal, t);
            return {s, t};
        }
        case AstNode::Kind::Concatenation: {
            if (node->children.empty()) {
                StateId s = nfa.add_state();
                StateId t = nfa.add_state();
                nfa.add_epsilon_transition(s, t);
                return {s, t};
            }
            if (node->children.size() == 1) {
                return build_fragment(nfa, node->children.front());
            }

            std::vector<NfaFragment> frags;
            frags.reserve(node->children.size());
            for (const auto& child : node->children) {
                frags.push_back(build_fragment(nfa, child));
            }
            for (std::size_t i = 0; i + 1 < frags.size(); ++i) {
                nfa.add_epsilon_transition(frags[i].accept, frags[i + 1].start);
            }
            return {frags.front().start, frags.back().accept};
        }
        case AstNode::Kind::Alternation: {
            if (node->children.empty()) {
                StateId s = nfa.add_state();
                StateId t = nfa.add_state();
                nfa.add_epsilon_transition(s, t);
                return {s, t};
            }
            if (node->children.size() == 1) {
                return build_fragment(nfa, node->children.front());
            }

            StateId s = nfa.add_state();
            StateId t = nfa.add_state();
            for (const auto& child : node->children) {
                NfaFragment frag = build_fragment(nfa, child);
                nfa.add_epsilon_transition(s, frag.start);
                nfa.add_epsilon_transition(frag.accept, t);
            }
            return {s, t};
        }
        case AstNode::Kind::Star: {
            NfaFragment child = build_fragment(nfa, node->children.front());
            StateId s = nfa.add_state();
            StateId t = nfa.add_state();
            nfa.add_epsilon_transition(s, child.start);
            nfa.add_epsilon_transition(s, t);
            nfa.add_epsilon_transition(child.accept, child.start);
            nfa.add_epsilon_transition(child.accept, t);
            return {s, t};
        }
    }

    StateId s = nfa.add_state();
    StateId t = nfa.add_state();
    nfa.add_epsilon_transition(s, t);
    return {s, t};
}

}  // namespace

ThompsonNfa::ThompsonNfa(const Ast& ast) {
    if (!ast) {
        start_state_ = add_state();
        accept_state_ = add_state();
        add_epsilon_transition(start_state_, accept_state_);
        set_accept_state(accept_state_);
        return;
    }

    NfaFragment frag = build_fragment(*this, ast);
    start_state_ = frag.start;
    set_accept_state(frag.accept);
}

ThompsonNfa ThompsonNfa::from_ast(const Ast& ast) {
    return ThompsonNfa(ast);
}

ThompsonNfa build_thompson_nfa(const Ast& ast) {
    return ThompsonNfa(ast);
}

StateId ThompsonNfa::add_state() {
    StateId id = states_.size();
    states_.push_back(NfaState{id, {}, {}, false});
    return id;
}

void ThompsonNfa::add_epsilon_transition(StateId from, StateId to) {
    if (from < states_.size()) {
        states_[from].add_epsilon(to);
    }
}

void ThompsonNfa::add_char_transition(StateId from, char symbol, StateId to) {
    if (from < states_.size()) {
        states_[from].add_transition(symbol, to);
    }
}

void ThompsonNfa::set_accept_state(StateId id) noexcept {
    accept_state_ = id;
    if (id < states_.size()) {
        states_[id].is_accept = true;
    }
}

void ThompsonNfa::set_accept(StateId id, bool accept) {
    if (id < states_.size()) {
        states_[id].is_accept = accept;
    }
}

std::vector<StateId> ThompsonNfa::epsilon_closure(StateId state) const {
    return epsilon_closure(std::vector<StateId>{state});
}

std::vector<StateId> ThompsonNfa::epsilon_closure(const std::vector<StateId>& states) const {
    if (states_.empty()) {
        return {};
    }

    std::vector<bool> visited(states_.size(), false);
    std::vector<StateId> stack;
    stack.reserve(states_.size());

    for (StateId id : states) {
        if (id < states_.size() && !visited[id]) {
            visited[id] = true;
            stack.push_back(id);
        }
    }

    while (!stack.empty()) {
        StateId u = stack.back();
        stack.pop_back();

        for (const auto& eps : states_[u].epsilon_transitions) {
            StateId v = eps.to;
            if (v < states_.size() && !visited[v]) {
                visited[v] = true;
                stack.push_back(v);
            }
        }
    }

    std::vector<StateId> result;
    result.reserve(states_.size());
    for (StateId i = 0; i < states_.size(); ++i) {
        if (visited[i]) {
            result.push_back(i);
        }
    }
    return result;
}

std::vector<StateId> ThompsonNfa::step(const std::vector<StateId>& current_states, char symbol) const {
    std::vector<StateId> targets;
    for (StateId u : current_states) {
        if (u >= states_.size()) continue;
        for (const auto& trans : states_[u].char_transitions) {
            if (trans.symbol == symbol) {
                targets.push_back(trans.to);
            }
        }
    }
    return epsilon_closure(targets);
}

MatchResult ThompsonNfa::simulate(std::string_view input, NfaMetrics* detailed_metrics,
                                  bool early_termination) const {
    NfaMetrics local;
    NfaMetrics& m = detailed_metrics ? *detailed_metrics : local;
    m = {};

    if (states_.empty()) {
        return MatchResult{false, m};
    }

    std::vector<StateId> current = epsilon_closure(start_state_);
    m.active_state_counts.push_back(current.size());
    m.peak_active_states = current.size();

    for (char c : input) {
        if (current.empty() && early_termination) {
            break;
        }
        ++m.steps;
        if (current.empty()) {
            m.active_state_counts.push_back(0);
            continue;
        }
        current = step(current, c);
        m.active_state_counts.push_back(current.size());
        if (current.size() > m.peak_active_states) {
            m.peak_active_states = current.size();
        }
    }

    m.active_states = current.size();
    m.active_nfa_states = m.peak_active_states;

    bool matched = false;
    for (StateId id : current) {
        if (id == accept_state_ || (id < states_.size() && states_[id].is_accept)) {
            matched = true;
            break;
        }
    }

    return MatchResult{matched, m};
}

ThompsonNFAMatcher::ThompsonNFAMatcher(Ast ast, NfaLimits limits)
    : nfa_(ast), limits_(limits) {}

ThompsonNFAMatcher::ThompsonNFAMatcher(ThompsonNfa nfa, NfaLimits limits)
    : nfa_(std::move(nfa)), limits_(limits) {}

MatchResult ThompsonNFAMatcher::full_match(std::string_view input) {
    const auto started = std::chrono::steady_clock::now();
    MatchResult result = nfa_.simulate(input, &metrics_);
    metrics_.active_nfa_states = metrics_.peak_active_states;
    result.metrics.active_nfa_states = metrics_.peak_active_states;
    if (metrics_.peak_active_states > limits_.max_active_states) {
        throw std::runtime_error("NFA active-state limit exceeded");
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started).count();
    result.metrics.steps = metrics_.steps;
    result.metrics.peak_active_states = metrics_.peak_active_states;
    result.metrics.elapsed_microseconds = static_cast<std::size_t>(elapsed);
    result.metrics.elapsed_ms = static_cast<double>(elapsed) / 1000.0;
    return result;
}

}  // namespace regexforge
