#include "regexforge/lazy_dfa.h"

#include "regexforge/nfa.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace regexforge {
namespace {

struct NfaState {
    std::vector<std::size_t> epsilon;
    std::vector<std::pair<unsigned char, std::size_t>> transitions;
    bool accepting = false;
};

struct Fragment { std::size_t start; std::size_t end; };

class Builder {
public:
    Fragment compile(const Ast& node) {
        switch (node->kind) {
            case AstNode::Kind::Empty: {
                const auto start = add(); const auto end = add();
                states[start].epsilon.push_back(end); return {start, end};
            }
            case AstNode::Kind::Literal: {
                const auto start = add(); const auto end = add();
                states[start].transitions.emplace_back(static_cast<unsigned char>(node->literal), end);
                return {start, end};
            }
            case AstNode::Kind::Concatenation: {
                if (node->children.empty()) return compile(AstNode::empty());
                auto result = compile(node->children.front());
                for (std::size_t i = 1; i < node->children.size(); ++i) {
                    const auto next = compile(node->children[i]);
                    states[result.end].epsilon.push_back(next.start);
                    result.end = next.end;
                }
                return result;
            }
            case AstNode::Kind::Alternation: {
                const auto start = add(); const auto end = add();
                for (const auto& child : node->children) {
                    const auto branch = compile(child);
                    states[start].epsilon.push_back(branch.start);
                    states[branch.end].epsilon.push_back(end);
                }
                return {start, end};
            }
            case AstNode::Kind::Star: {
                const auto start = add(); const auto end = add();
                const auto child = compile(node->children.front());
                states[start].epsilon = {end, child.start};
                states[child.end].epsilon = {child.start, end};
                return {start, end};
            }
        }
        throw std::logic_error("unknown AST node kind");
    }

    std::vector<NfaState> states;

private:
    std::size_t add() { states.emplace_back(); return states.size() - 1; }
};

using StateSet = std::vector<std::size_t>;

void closure(const std::vector<NfaState>& nfa, StateSet& set) {
    std::vector<bool> seen(nfa.size(), false);
    std::vector<std::size_t> pending;
    for (const auto state : set) {
        if (!seen[state]) { seen[state] = true; pending.push_back(state); }
    }
    for (std::size_t i = 0; i < pending.size(); ++i) {
        for (const auto next : nfa[pending[i]].epsilon) {
            if (!seen[next]) { seen[next] = true; pending.push_back(next); }
        }
    }
    std::sort(pending.begin(), pending.end());
    set = std::move(pending);
}

std::size_t hash_set(const StateSet& set) {
    std::size_t hash = 1469598103934665603ULL;
    for (const auto state : set) {
        hash ^= state + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    }
    return hash;
}

struct SetHash { std::size_t operator()(const StateSet& set) const { return hash_set(set); } };

}  // namespace

struct LazyDFAMatcher::Impl {
    struct DfaState {
        StateSet nfa_states;
        std::array<std::size_t, 256> transitions{};
        bool accepting = false;
        DfaState() { transitions.fill(no_transition); }
        static constexpr std::size_t no_transition = static_cast<std::size_t>(-1);
    };

    std::vector<NfaState> nfa;
    std::vector<DfaState> dfa;
    std::unordered_map<StateSet, std::size_t, SetHash> cache;
    std::size_t start = 0;
};

LazyDFAMatcher::LazyDFAMatcher(Ast ast, LazyDfaLimits limits)
    : ast_(std::move(ast)), limits_(limits), impl_(std::make_unique<Impl>()) {
    Builder builder;
    const auto fragment = builder.compile(ast_);
    builder.states[fragment.end].accepting = true;
    impl_->nfa = std::move(builder.states);

    StateSet initial{fragment.start};
    closure(impl_->nfa, initial);
    Impl::DfaState state;
    state.nfa_states = initial;
    state.accepting = std::any_of(initial.begin(), initial.end(), [&](auto s) { return impl_->nfa[s].accepting; });
    impl_->cache.emplace(initial, 0);
    impl_->dfa.push_back(std::move(state));
}

LazyDFAMatcher::~LazyDFAMatcher() = default;

std::size_t LazyDFAMatcher::generated_state_count() const noexcept { return impl_->dfa.size(); }

MatchResult LazyDFAMatcher::full_match(std::string_view input) {
    const auto started = std::chrono::steady_clock::now();
    bool fell_back = false;
    std::size_t current = impl_->start;

    auto finish = [&](bool matched) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count();
        ExecutionMetrics metrics;
        metrics.generated_dfa_states = impl_->dfa.size();
        metrics.steps = input.size();
        metrics.elapsed_microseconds = static_cast<std::size_t>(elapsed);
        metrics.elapsed_ms = static_cast<double>(elapsed) / 1000.0;
        metrics.fell_back = fell_back;
        return MatchResult{matched, metrics};
    };

    for (const unsigned char symbol : input) {
        auto& state = impl_->dfa[current];
        auto& destination = state.transitions[symbol];
        if (destination == Impl::DfaState::no_transition) {
            StateSet next;
            for (const auto nfa_state : state.nfa_states) {
                for (const auto& [value, target] : impl_->nfa[nfa_state].transitions) {
                    if (value == symbol) next.push_back(target);
                }
            }
            closure(impl_->nfa, next);

            if (next.empty()) {
                destination = current;
                // The self-loop is only an internal dead transition; result is
                // forced false below by switching to the empty state behavior.
                destination = Impl::DfaState::no_transition;
                return finish(false);
            }

            const auto found = impl_->cache.find(next);
            if (found != impl_->cache.end()) {
                destination = found->second;
            } else {
                const std::size_t estimated_bytes = (impl_->dfa.size() + 1) * sizeof(Impl::DfaState)
                    + (impl_->cache.size() + 1) * (sizeof(StateSet) + sizeof(std::size_t));
                if (impl_->dfa.size() >= limits_.max_states || estimated_bytes > limits_.max_memory_bytes) {
                    if (cache_resets_ >= limits_.max_cache_resets) {
                        throw std::runtime_error("lazy DFA resource limits exceeded");
                    }
                    ++cache_resets_;
                    fell_back = true;
                    ThompsonNFAMatcher fallback(ast_);
                    auto result = fallback.full_match(input);
                    result.metrics.fell_back = true;
                    result.metrics.generated_dfa_states = impl_->dfa.size();
                    result.metrics.steps = input.size();
                    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - started).count();
                    result.metrics.elapsed_microseconds = static_cast<std::size_t>(elapsed);
                    result.metrics.elapsed_ms = static_cast<double>(elapsed) / 1000.0;
                    return result;
                }
                Impl::DfaState created;
                created.nfa_states = next;
                created.accepting = std::any_of(next.begin(), next.end(), [&](auto s) { return impl_->nfa[s].accepting; });
                destination = impl_->dfa.size();
                impl_->cache.emplace(next, destination);
                impl_->dfa.push_back(std::move(created));
            }
        }
        current = destination;
    }
    return finish(impl_->dfa[current].accepting);
}

}  // namespace regexforge
