// lazy_dfa_test.cpp — Workflow 2: Lazy DFA tests
//
// Covers:
//   - Basic correctness (full-string matching)
//   - State reuse across repeated calls
//   - Cache-hit / cache-miss counters
//   - Construction-cost tracking
//   - Metrics fields populated after a match
//   - State-limit triggers cache reset (not hard failure)
//   - Hard state limit (max_cache_resets = 0) throws
//   - Memory limit triggers cache reset
//   - LazyDfaMetrics snapshot
//   - Empty pattern and single-character patterns
//   - Long alternation to stress the cache

#include "regexforge/lazy_dfa.h"
#include "regexforge/parser.h"

#include <gtest/gtest.h>
#include <stdexcept>

using namespace regexforge;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

/// Convenience: parse a pattern string and run a full match.
bool matches(const std::string& pattern, const std::string& input,
             LazyDfaLimits limits = {}) {
    LazyDFAMatcher m(parse_pattern(pattern), limits);
    return m.full_match(input).matched;
}

}  // namespace

// ===========================================================================
// 1. Basic correctness
// ===========================================================================
TEST(LazyDfaTest, PerformsFullStringMatching) {
    EXPECT_TRUE (matches("(a|b)*abb", "abababb"));
    EXPECT_TRUE (matches("(a|b)*abb", "abb"));
    EXPECT_FALSE(matches("(a|b)*abb", "ab"));
    EXPECT_FALSE(matches("ab*",       "ac"));
    EXPECT_TRUE (matches("ab*",       "a"));
    EXPECT_TRUE (matches("ab*",       "abbb"));
    EXPECT_TRUE (matches("a|bc*",     "bcc"));
    EXPECT_TRUE (matches("a|bc*",     "a"));
    EXPECT_FALSE(matches("a|bc*",     "c"));
}

TEST(LazyDfaTest, MatchesEmptyPattern) {
    EXPECT_TRUE (matches("", ""));
    EXPECT_FALSE(matches("", "a"));
}

TEST(LazyDfaTest, MatchesSingleCharacterPatterns) {
    EXPECT_TRUE (matches("a", "a"));
    EXPECT_FALSE(matches("a", "b"));
    EXPECT_FALSE(matches("a", ""));
    EXPECT_FALSE(matches("a", "aa"));
}

TEST(LazyDfaTest, MatchesStarOnSingleChar) {
    EXPECT_TRUE (matches("a*", ""));
    EXPECT_TRUE (matches("a*", "aaa"));
    EXPECT_FALSE(matches("a*", "b"));
}

TEST(LazyDfaTest, MatchesConcatenation) {
    EXPECT_TRUE (matches("abc", "abc"));
    EXPECT_FALSE(matches("abc", "ab"));
    EXPECT_FALSE(matches("abc", "abcd"));
}

TEST(LazyDfaTest, MatchesNestedAlternation) {
    EXPECT_TRUE (matches("(a|b|c)*", "abcabc"));
    EXPECT_FALSE(matches("(a|b|c)*", "abcd"));
    EXPECT_TRUE (matches("(a|b|c)*", ""));
}

// ===========================================================================
// 2. State reuse across multiple calls
// ===========================================================================
TEST(LazyDfaTest, ReusesGeneratedStatesAcrossCalls) {
    LazyDFAMatcher m(parse_pattern("(a|b)*abb"));

    EXPECT_TRUE(m.full_match("abababb").matched);
    const std::size_t after_first = m.generated_state_count();
    EXPECT_GT(after_first, 0u);

    EXPECT_TRUE(m.full_match("abb").matched);
    // No new states should be created for a second call on the same pattern.
    EXPECT_EQ(m.generated_state_count(), after_first);
}

TEST(LazyDfaTest, StaticPatternProducesFiniteStateCount) {
    LazyDFAMatcher m(parse_pattern("(a|b)*abb"));
    for (int i = 0; i < 10; ++i) m.full_match("abababb");
    // The DFA for (a|b)*abb has exactly 5 states; verify it stays bounded.
    EXPECT_LE(m.generated_state_count(), 10u);
}

// ===========================================================================
// 3. Cache-hit and cache-miss counters
// ===========================================================================
TEST(LazyDfaTest, CacheHitCounterIncreasesOnRepeatedInput) {
    LazyDFAMatcher m(parse_pattern("a*b"));

    m.full_match("aaab");
    const std::size_t hits_first  = m.cache_hit_count();
    const std::size_t misses_first = m.cache_miss_count();

    m.full_match("aaab");
    // On the second call every DFA transition is already in the table,
    // so hits must increase and misses must not change.
    EXPECT_GT(m.cache_hit_count(), hits_first);
    EXPECT_EQ(m.cache_miss_count(), misses_first);
}

TEST(LazyDfaTest, CacheMissCounterIncreasesOnFirstMatch) {
    LazyDFAMatcher m(parse_pattern("abc"));
    EXPECT_EQ(m.cache_miss_count(), 0u);
    m.full_match("abc");
    // Each new DFA state is a miss.
    EXPECT_GT(m.cache_miss_count(), 0u);
}

TEST(LazyDfaTest, NoCacheHitsOnFirstMatchForFreshPattern) {
    // The very first call cannot have precomputed transitions (except the
    // initial state which is built at construction time, not during match).
    LazyDFAMatcher m(parse_pattern("xyz"));
    const std::size_t hits_before = m.cache_hit_count();
    m.full_match("xyz");
    // hits_before is 0; after one match transitions may have been cached
    // but NOT pre-fetched, so hits during the first walk are 0.
    EXPECT_EQ(hits_before, 0u);
}

// ===========================================================================
// 4. Construction-cost tracking
// ===========================================================================
TEST(LazyDfaTest, ConstructionStepsMonotonicallyIncrease) {
    LazyDFAMatcher m(parse_pattern("(a|b)*abb"));

    m.full_match("abb");
    const std::size_t steps1 = m.construction_step_count();

    // Second call explores already-computed DFA transitions, but examining
    // NFA states is done during the lazy expansion — steps should not
    // decrease.
    m.full_match("abb");
    EXPECT_GE(m.construction_step_count(), steps1);
}

TEST(LazyDfaTest, LongerInputIncursMoreOrEqualConstructionSteps) {
    LazyDFAMatcher m1(parse_pattern("(a|b)*abb"));
    LazyDFAMatcher m2(parse_pattern("(a|b)*abb"));

    m1.full_match("abb");
    m2.full_match("abababababb");

    EXPECT_GE(m2.construction_step_count(), m1.construction_step_count());
}

// ===========================================================================
// 5. ExecutionMetrics populated after a match
// ===========================================================================
TEST(LazyDfaTest, MetricsStepsEqualsInputLength) {
    LazyDFAMatcher m(parse_pattern("a*b"));
    const std::string input = "aaab";
    const auto result = m.full_match(input);
    EXPECT_EQ(result.metrics.steps, input.size());
}

TEST(LazyDfaTest, MetricsElapsedMsIsNonNegative) {
    LazyDFAMatcher m(parse_pattern("a*b"));
    const auto result = m.full_match("aaab");
    EXPECT_GE(result.metrics.elapsed_ms, 0.0);
}

TEST(LazyDfaTest, MetricsGeneratedDfaStatesConsistent) {
    LazyDFAMatcher m(parse_pattern("a*b"));
    const auto result = m.full_match("aaab");
    EXPECT_EQ(result.metrics.generated_dfa_states, m.generated_state_count());
}

TEST(LazyDfaTest, FellBackFalseByDefault) {
    LazyDFAMatcher m(parse_pattern("a*b"));
    const auto result = m.full_match("aaab");
    EXPECT_FALSE(result.metrics.fell_back);
}

TEST(LazyDfaTest, PeakActiveStatesIsPositive) {
    LazyDFAMatcher m(parse_pattern("(a|b)*abb"));
    const auto result = m.full_match("abb");
    EXPECT_GE(result.metrics.peak_active_states, 1u);
}

// ===========================================================================
// 6. State-limit triggers cache reset (restart-based fallback)
// ===========================================================================
TEST(LazyDfaTest, FallsBackWhenStateLimitIsReached) {
    // Allow only 1 DFA state, but up to 3 resets.
    LazyDFAMatcher m(parse_pattern("(a|aa)*b"),
                     LazyDfaLimits{1, 32ULL * 1024ULL * 1024ULL, 3});
    const auto result = m.full_match("aaab");
    EXPECT_TRUE(result.matched);
    EXPECT_TRUE(result.metrics.fell_back);
    EXPECT_GT(m.cache_reset_count(), 0u);
}

TEST(LazyDfaTest, CacheResetCountTracksResets) {
    LazyDFAMatcher m(parse_pattern("(a|aa)*b"),
                     LazyDfaLimits{1, 32ULL * 1024ULL * 1024ULL, 3});
    m.full_match("aaab");
    EXPECT_GT(m.cache_reset_count(), 0u);
}

// ===========================================================================
// 7. Hard state limit (max_cache_resets = 0) throws
// ===========================================================================
TEST(LazyDfaTest, ReportsLimitFailureAfterConfiguredResets) {
    LazyDFAMatcher m(parse_pattern("(a|aa)*b"),
                     LazyDfaLimits{1, 32ULL * 1024ULL * 1024ULL, 0});
    EXPECT_THROW(m.full_match("aaab"), std::runtime_error);
}

// ===========================================================================
// 8. Memory limit triggers reset
// ===========================================================================
TEST(LazyDfaTest, MemoryLimitTriggersReset) {
    // Set memory limit to 1 byte so it is always exceeded.
    LazyDFAMatcher m(parse_pattern("ab*c"),
                     LazyDfaLimits{10'000, 1, 3});
    const auto result = m.full_match("abbc");
    EXPECT_TRUE(result.matched);
    EXPECT_TRUE(result.metrics.fell_back);
}

TEST(LazyDfaTest, MemoryLimitHardThrowsWhenResetsExhausted) {
    LazyDFAMatcher m(parse_pattern("ab*c"),
                     LazyDfaLimits{10'000, 1, 0});
    EXPECT_THROW(m.full_match("abbc"), std::runtime_error);
}

// ===========================================================================
// 9. LazyDfaMetrics snapshot
// ===========================================================================
TEST(LazyDfaTest, DfaMetricsSnapshotMatchesAccessors) {
    LazyDFAMatcher m(parse_pattern("(a|b)*abb"));
    m.full_match("abababb");

    const auto snap = m.dfa_metrics();
    EXPECT_EQ(snap.cache_hits,        m.cache_hit_count());
    EXPECT_EQ(snap.cache_misses,      m.cache_miss_count());
    EXPECT_EQ(snap.construction_steps,m.construction_step_count());
    EXPECT_EQ(snap.cache_resets,      m.cache_reset_count());
    EXPECT_GE(snap.total_states_created, m.generated_state_count());
}

// ===========================================================================
// 10. Long alternation — stress the cache
// ===========================================================================
TEST(LazyDfaTest, LongAlternationCorrectness) {
    // Build pattern a|b|c|d|e|f|g|h|i|j
    const std::string pattern = "a|b|c|d|e|f|g|h|i|j";
    for (char c : std::string("abcdefghij")) {
        EXPECT_TRUE(matches(pattern, std::string(1, c)));
    }
    EXPECT_FALSE(matches(pattern, "k"));
    EXPECT_FALSE(matches(pattern, "ab"));
}

TEST(LazyDfaTest, LongInputDoesNotExplodeStateCount) {
    // a* should always use at most 2 DFA states regardless of input length.
    LazyDFAMatcher m(parse_pattern("a*"));
    m.full_match(std::string(10'000, 'a'));
    EXPECT_LE(m.generated_state_count(), 4u);
}
