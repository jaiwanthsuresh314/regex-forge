#pragma once

#include "regexforge/ast.h"
#include "regexforge/engine.h"

#include <cstddef>
#include <memory>
#include <string_view>

namespace regexforge {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/// Resource limits for the lazy DFA engine.  All limits are soft by default:
/// when a state or memory limit is reached the cache is wiped and construction
/// restarts from the current input position (a "cache reset").  Only when
/// max_cache_resets itself is exhausted does the engine throw.
struct LazyDfaLimits {
    /// Maximum number of DFA states that may be materialized at once.
    std::size_t max_states = 10'000;

    /// Approximate upper bound on heap bytes used by the state cache.
    std::size_t max_memory_bytes = 32ULL * 1024ULL * 1024ULL;

    /// How many times the cache may be wiped before the engine gives up and
    /// throws std::runtime_error.
    std::size_t max_cache_resets = 3;
};

// ---------------------------------------------------------------------------
// Build-time metrics
// ---------------------------------------------------------------------------

/// Statistics accumulated while constructing DFA states lazily.
/// These are separate from ExecutionMetrics (which is per-match) because the
/// cache is shared across multiple full_match() calls.
struct LazyDfaMetrics {
    /// Total DFA states that have been materialised (ever, not just currently
    /// cached — surviving cache resets).
    std::size_t total_states_created = 0;

    /// Number of times a state-set lookup found an existing DFA state (fast
    /// path: no new DFA state was allocated).
    std::size_t cache_hits = 0;

    /// Number of times a state-set lookup missed and a new DFA state was
    /// allocated.
    std::size_t cache_misses = 0;

    /// Cumulative number of NFA-state transitions evaluated while building DFA
    /// transitions (a proxy for construction cost / work done).
    std::size_t construction_steps = 0;

    /// How many times the state cache has been wiped due to resource limits.
    std::size_t cache_resets = 0;
};

// ---------------------------------------------------------------------------
// Matcher
// ---------------------------------------------------------------------------

/// Lazy DFA matcher.
///
/// Constructs DFA states on demand from a private NFA representation built
/// from the AST.  The engine is independent of the ThompsonNFAMatcher
/// implementation in Workflow 1: it contains its own private NFA type and
/// uses the shared Matcher interface only for the fallback path.
///
/// Thread-safety: instances are NOT thread-safe.  Call full_match() from one
/// thread at a time; construct a separate matcher per thread if needed.
class LazyDFAMatcher : public Matcher {
public:
    explicit LazyDFAMatcher(Ast ast, LazyDfaLimits limits = {});
    ~LazyDFAMatcher() override;

    // Non-copyable, movable.
    LazyDFAMatcher(const LazyDFAMatcher&) = delete;
    LazyDFAMatcher& operator=(const LazyDFAMatcher&) = delete;
    LazyDFAMatcher(LazyDFAMatcher&&) noexcept;
    LazyDFAMatcher& operator=(LazyDFAMatcher&&) noexcept;

    // -----------------------------------------------------------------------
    // Core interface (Matcher)
    // -----------------------------------------------------------------------

    /// Match the entire input string against the compiled pattern.
    /// The returned ExecutionMetrics contain:
    ///   - steps                : number of input characters consumed
    ///   - generated_dfa_states : DFA states in the cache after this call
    ///   - peak_active_states   : peak NFA states (always 1 per DFA state)
    ///   - elapsed_ms           : wall-clock time for this call
    ///   - fell_back            : true if a cache reset occurred during the call
    MatchResult full_match(std::string_view input) override;

    // -----------------------------------------------------------------------
    // Introspection
    // -----------------------------------------------------------------------

    /// Number of DFA states currently in the cache.
    std::size_t generated_state_count() const noexcept;

    /// Number of cache resets triggered by resource limits so far.
    std::size_t cache_reset_count() const noexcept;

    /// Number of cache hits since construction (or last hard reset).
    std::size_t cache_hit_count() const noexcept;

    /// Number of cache misses since construction (or last hard reset).
    std::size_t cache_miss_count() const noexcept;

    /// Cumulative NFA-transition evaluations while building DFA edges.
    std::size_t construction_step_count() const noexcept;

    /// Snapshot of all build-time metrics.
    LazyDfaMetrics dfa_metrics() const noexcept;

private:
    struct Impl;

    Ast             ast_;
    LazyDfaLimits   limits_;
    std::unique_ptr<Impl> impl_;

    // Counters lifted out of Impl so that accessors don't need to touch the
    // full Impl definition in the header.
    std::size_t cache_resets_      = 0;
    std::size_t cache_hits_        = 0;
    std::size_t cache_misses_      = 0;
    std::size_t construction_steps_= 0;
    std::size_t total_created_     = 0;

    /// Wipe the DFA cache and rebuild the initial state from scratch.
    void reset_cache();
};

}  // namespace regexforge
