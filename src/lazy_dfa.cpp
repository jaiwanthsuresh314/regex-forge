// lazy_dfa.cpp — Workflow 2: Lazy DFA Construction
//
// This translation unit is intentionally self-contained: it has its own
// private NFA representation and does not #include "regexforge/nfa.h".
// The ThompsonNFAMatcher is referenced only through the shared Matcher
// interface (engine.h) on the fallback path.

#include "regexforge/lazy_dfa.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace regexforge {

// ===========================================================================
// Private NFA representation
// ===========================================================================
namespace {

/// A single NFA state with epsilon and labelled transitions.
struct NfaState {
    std::vector<std::size_t>                              epsilon;
    std::vector<std::pair<unsigned char, std::size_t>>    transitions;
    bool accepting = false;
};

struct Fragment { std::size_t start; std::size_t end; };

// ---------------------------------------------------------------------------
// Thompson NFA builder (AST → NFA, no dependency on nfa.h)
// ---------------------------------------------------------------------------
class Builder {
public:
    Fragment compile(const Ast& node) {
        switch (node->kind) {
            case AstNode::Kind::Empty: {
                const auto s = add(), e = add();
                states[s].epsilon.push_back(e);
                return {s, e};
            }
            case AstNode::Kind::Literal: {
                const auto s = add(), e = add();
                states[s].transitions.emplace_back(
                    static_cast<unsigned char>(node->literal), e);
                return {s, e};
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
                const auto s = add(), e = add();
                for (const auto& child : node->children) {
                    const auto branch = compile(child);
                    states[s].epsilon.push_back(branch.start);
                    states[branch.end].epsilon.push_back(e);
                }
                return {s, e};
            }
            case AstNode::Kind::Star: {
                const auto s = add(), e = add();
                const auto child = compile(node->children.front());
                states[s].epsilon  = {e, child.start};
                states[child.end].epsilon = {child.start, e};
                return {s, e};
            }
        }
        throw std::logic_error("unknown AST node kind in lazy DFA builder");
    }

    std::vector<NfaState> states;

private:
    std::size_t add() {
        states.emplace_back();
        return states.size() - 1;
    }
};

// ---------------------------------------------------------------------------
// Epsilon closure
// ---------------------------------------------------------------------------
using StateSet = std::vector<std::size_t>;

/// Expand `set` to include every NFA state reachable via epsilon edges.
/// The result is sorted and deduplicated.
void epsilon_closure(const std::vector<NfaState>& nfa, StateSet& set) {
    std::vector<bool>       seen(nfa.size(), false);
    std::vector<std::size_t> pending;
    pending.reserve(set.size());
    for (const auto s : set) {
        if (!seen[s]) { seen[s] = true; pending.push_back(s); }
    }
    for (std::size_t i = 0; i < pending.size(); ++i) {
        for (const auto next : nfa[pending[i]].epsilon) {
            if (!seen[next]) { seen[next] = true; pending.push_back(next); }
        }
    }
    std::sort(pending.begin(), pending.end());
    set = std::move(pending);
}

// ---------------------------------------------------------------------------
// Hash function for sorted state-sets
// ---------------------------------------------------------------------------
struct SetHash {
    std::size_t operator()(const StateSet& set) const noexcept {
        // FNV-1a mixing over state ids.
        std::size_t h = 14695981039346656037ULL;
        for (const auto s : set) {
            h ^= s;
            h *= 1099511628211ULL;
        }
        return h;
    }
};

/// Approximate heap footprint of one DFA state and its cache entry.
constexpr std::size_t kDfaStateSizeEstimate =
    256 * sizeof(std::size_t)   // transition table
    + 32 * sizeof(std::size_t)  // typical nfa_states vec
    + 2 * sizeof(std::size_t);  // cache key overhead (pointer + index)

}  // namespace

// ===========================================================================
// LazyDFAMatcher::Impl
// ===========================================================================

struct LazyDFAMatcher::Impl {
    // -----------------------------------------------------------------------
    // DFA state
    // -----------------------------------------------------------------------
    struct DfaState {
        StateSet                   nfa_states;
        std::array<std::size_t, 256> transitions{};
        bool                       accepting = false;

        static constexpr std::size_t kNoTransition =
            static_cast<std::size_t>(-1);

        DfaState() { transitions.fill(kNoTransition); }
    };

    // -----------------------------------------------------------------------
    // Data
    // -----------------------------------------------------------------------
    std::vector<NfaState>                                    nfa;
    std::vector<DfaState>                                    dfa;
    std::unordered_map<StateSet, std::size_t, SetHash>       cache;
    std::size_t                                              nfa_accept = 0;
    std::size_t                                              nfa_start  = 0;  ///< NFA start index, kept for cache resets.
    std::size_t                                              start      = 0;  ///< Current DFA start state index.

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /// Estimated bytes consumed by the current DFA + cache.
    std::size_t estimated_bytes() const noexcept {
        return dfa.size() * kDfaStateSizeEstimate
             + cache.size() * (sizeof(StateSet) + sizeof(std::size_t));
    }

    /// Build and register the initial DFA state from the NFA start state.
    void build_initial_state(std::size_t ns) {
        nfa_start = ns;
        StateSet initial{ns};
        epsilon_closure(nfa, initial);

        DfaState s;
        s.nfa_states = initial;
        s.accepting  = std::any_of(initial.begin(), initial.end(),
                                   [&](auto id) { return nfa[id].accepting; });
        cache.emplace(initial, 0);
        dfa.push_back(std::move(s));
        start = 0;
    }

    /// Wipe all DFA states and the cache, then rebuild the initial state.
    void reset() {
        dfa.clear();
        cache.clear();
        build_initial_state(nfa_start);
    }

    /// Run a plain NFA simulation over `input` using the private NFA.
    /// Used as the limit-breach fallback to avoid depending on nfa.h.
    bool nfa_simulate(std::string_view input) const {
        StateSet current{nfa_start};
        epsilon_closure(nfa, current);
        for (const unsigned char sym : input) {
            StateSet next;
            for (const auto s : current) {
                for (const auto& [label, target] : nfa[s].transitions) {
                    if (label == sym) next.push_back(target);
                }
            }
            epsilon_closure(nfa, next);
            current = std::move(next);
            if (current.empty()) return false;
        }
        return std::any_of(current.begin(), current.end(),
                           [&](auto s) { return nfa[s].accepting; });
    }
};

// ===========================================================================
// LazyDFAMatcher — construction / destruction
// ===========================================================================

LazyDFAMatcher::LazyDFAMatcher(Ast ast, LazyDfaLimits limits)
    : ast_(std::move(ast)),
      limits_(limits),
      impl_(std::make_unique<Impl>()) {

    // Build the private NFA from the AST.
    Builder builder;
    const auto fragment = builder.compile(ast_);
    builder.states[fragment.end].accepting = true;
    impl_->nfa        = std::move(builder.states);
    impl_->nfa_accept = fragment.end;

    // Materialise the initial DFA state (= epsilon-closure of NFA start).
    impl_->build_initial_state(fragment.start);
    total_created_ = 1;
}

LazyDFAMatcher::~LazyDFAMatcher() = default;

LazyDFAMatcher::LazyDFAMatcher(LazyDFAMatcher&&) noexcept            = default;
LazyDFAMatcher& LazyDFAMatcher::operator=(LazyDFAMatcher&&) noexcept = default;

// ===========================================================================
// Introspection
// ===========================================================================

std::size_t LazyDFAMatcher::generated_state_count() const noexcept {
    return impl_->dfa.size();
}
std::size_t LazyDFAMatcher::cache_reset_count()     const noexcept { return cache_resets_; }
std::size_t LazyDFAMatcher::cache_hit_count()       const noexcept { return cache_hits_; }
std::size_t LazyDFAMatcher::cache_miss_count()      const noexcept { return cache_misses_; }
std::size_t LazyDFAMatcher::construction_step_count() const noexcept { return construction_steps_; }

LazyDfaMetrics LazyDFAMatcher::dfa_metrics() const noexcept {
    return LazyDfaMetrics{
        total_created_,
        cache_hits_,
        cache_misses_,
        construction_steps_,
        cache_resets_
    };
}

// ===========================================================================
// Cache reset helper — drops the DFA cache and wipes to initial state.
// ===========================================================================

void LazyDFAMatcher::reset_cache() {
    ++cache_resets_;
    impl_->reset();
    ++total_created_;  // count the newly created initial DFA state
}

// ===========================================================================
// full_match
// ===========================================================================
//
// When the DFA cache hits a resource limit (state count or memory), the engine
// falls back to an inline NFA simulation (impl_->nfa_simulate) over the full
// input.  This is O(n·|NFA|) but always terminates without growing the cache.
// If limits_.max_cache_resets == 0 the engine throws immediately instead.
//

MatchResult LazyDFAMatcher::full_match(std::string_view input) {
    const auto t0 = std::chrono::steady_clock::now();

    bool        fell_back     = false;
    std::size_t current       = impl_->start;
    std::size_t peak_set_size = impl_->dfa[current].nfa_states.size();

    auto elapsed_us_now = [&]() -> std::size_t {
        return static_cast<std::size_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - t0).count());
    };

    auto build_metrics = [&]() -> ExecutionMetrics {
        const auto us = elapsed_us_now();
        ExecutionMetrics m;
        m.steps                = input.size();
        m.generated_dfa_states = impl_->dfa.size();
        m.peak_active_states   = peak_set_size;
        m.elapsed_microseconds = us;
        m.elapsed_ms           = static_cast<double>(us) / 1000.0;
        m.fell_back            = fell_back;
        return m;
    };

    for (std::size_t pos = 0; pos < input.size(); /* advanced below */) {
        const unsigned char sym = static_cast<unsigned char>(input[pos]);

        // -----------------------------------------------------------------
        // Fast path: transition already in the table.
        // -----------------------------------------------------------------
        const std::size_t cached_dest = impl_->dfa[current].transitions[sym];

        if (cached_dest != Impl::DfaState::kNoTransition) {
            ++cache_hits_;
            current = cached_dest;
            peak_set_size = std::max(peak_set_size,
                                     impl_->dfa[current].nfa_states.size());
            ++pos;
            continue;
        }

        // -----------------------------------------------------------------
        // Slow path: compute the NFA successor set for this symbol.
        // Copy out the NFA state set BEFORE any vector push_back that could
        // invalidate references into impl_->dfa.
        // -----------------------------------------------------------------
        StateSet next_set;
        {
            const StateSet& cur_nfa = impl_->dfa[current].nfa_states;
            for (const auto nfa_id : cur_nfa) {
                for (const auto& [label, target] : impl_->nfa[nfa_id].transitions) {
                    if (label == sym) next_set.push_back(target);
                }
                ++construction_steps_;
            }
        }
        epsilon_closure(impl_->nfa, next_set);

        if (next_set.empty()) {
            return MatchResult{false, build_metrics()};
        }

        // Check cache.
        const auto found = impl_->cache.find(next_set);
        if (found != impl_->cache.end()) {
            ++cache_hits_;
            impl_->dfa[current].transitions[sym] = found->second;
            current = found->second;
        } else {
            ++cache_misses_;

            const bool state_limit_hit  = impl_->dfa.size() >= limits_.max_states;
            const bool memory_limit_hit = impl_->estimated_bytes() >= limits_.max_memory_bytes;

            if (state_limit_hit || memory_limit_hit) {
                if (cache_resets_ >= limits_.max_cache_resets) {
                    throw std::runtime_error(
                        "lazy DFA resource limits exceeded: too many cache resets");
                }
                // Limit-breach fallback: perform one inline NFA simulation
                // over the complete input string.  This avoids both growing
                // the cache unboundedly and depending on Workflow 1's NFA.
                ++cache_resets_;
                fell_back = true;
                const bool matched = impl_->nfa_simulate(input);
                return MatchResult{matched, build_metrics()};
            }

            // Allocate new DFA state.
            // After push_back ALL existing DFA pointers/refs are invalid.
            const std::size_t new_id = impl_->dfa.size();
            ++total_created_;

            Impl::DfaState new_state;
            new_state.nfa_states = next_set;
            new_state.accepting  = std::any_of(
                next_set.begin(), next_set.end(),
                [&](auto id) { return impl_->nfa[id].accepting; });

            impl_->cache.emplace(next_set, new_id);
            impl_->dfa.push_back(std::move(new_state));  // may reallocate!

            // Re-access current state by index (safe after reallocation).
            impl_->dfa[current].transitions[sym] = new_id;
            current = new_id;
        }

        peak_set_size = std::max(peak_set_size,
                                  impl_->dfa[current].nfa_states.size());
        ++pos;
    }

    return MatchResult{impl_->dfa[current].accepting, build_metrics()};
}

}  // namespace regexforge
