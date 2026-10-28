#include "regexforge/backtracking.h"
#include "regexforge/nfa.h"
#include "regexforge/parser.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace regexforge;

namespace {

MatchResult nfa_match(const std::string& pattern, std::string_view input) {
    ThompsonNFAMatcher matcher(parse_pattern(pattern));
    return matcher.full_match(input);
}

bool matches(const std::string& pattern, std::string_view input) {
    return nfa_match(pattern, input).matched;
}

}  // namespace

TEST(NfaTest, PerformsFullStringMatching) {
    EXPECT_TRUE(matches("(a|b)*abb", "abababb"));
    EXPECT_FALSE(matches("ab*", "ac"));
    EXPECT_TRUE(matches("a|bc*", "bcc"));
}

TEST(NfaTest, HandlesEmptyExpressionsAndStars) {
    EXPECT_TRUE(matches("", ""));
    EXPECT_FALSE(matches("", "a"));
    EXPECT_TRUE(matches("()*", ""));
    EXPECT_FALSE(matches("()*", "a"));
}

TEST(NfaTest, ReportsActiveStateMetric) {
    ThompsonNFAMatcher matcher(parse_pattern("(a|aa)*b"));
    EXPECT_FALSE(matcher.full_match("aaaa").matched);
    EXPECT_GT(matcher.full_match("aaaa").metrics.active_nfa_states, 0U);
    EXPECT_GT(matcher.state_count(), 0U);
}

TEST(NfaTest, EnforcesActiveStateLimit) {
    ThompsonNFAMatcher matcher(parse_pattern("(a|aa)*b"), NfaLimits{1});
    EXPECT_THROW(matcher.full_match("aaaa"), std::runtime_error);
}

TEST(NfaStructureTest, StateAndTransitions) {
    NfaState state;
    state.id = 42;
    state.add_epsilon(43);
    state.add_transition('x', 44);

    EXPECT_EQ(state.id, 42U);
    ASSERT_EQ(state.epsilon_transitions.size(), 1U);
    EXPECT_EQ(state.epsilon_transitions[0].to, 43U);
    ASSERT_EQ(state.char_transitions.size(), 1U);
    EXPECT_EQ(state.char_transitions[0].symbol, 'x');
    EXPECT_EQ(state.char_transitions[0].to, 44U);
    EXPECT_FALSE(state.is_accept);

    EpsilonTransition eps1{10};
    EpsilonTransition eps2{10};
    EpsilonTransition eps3{20};
    EXPECT_EQ(eps1, eps2);
    EXPECT_NE(eps1, eps3);

    CharTransition ct1{'a', 5};
    CharTransition ct2{'a', 5};
    CharTransition ct3{'b', 5};
    EXPECT_EQ(ct1, ct2);
    EXPECT_NE(ct1, ct3);
}

TEST(NfaStructureTest, ManualGraphConstruction) {
    ThompsonNfa nfa;
    StateId s0 = nfa.add_state();
    StateId s1 = nfa.add_state();
    StateId s2 = nfa.add_state();

    nfa.set_start_state(s0);
    nfa.set_accept_state(s2);
    nfa.add_char_transition(s0, 'a', s1);
    nfa.add_epsilon_transition(s1, s2);

    EXPECT_EQ(nfa.num_states(), 3U);
    EXPECT_EQ(nfa.start_state(), s0);
    EXPECT_EQ(nfa.accept_state(), s2);
    EXPECT_TRUE(nfa.state(s2).is_accept);

    EXPECT_TRUE(nfa.simulate("a").matched);
    EXPECT_FALSE(nfa.simulate("b").matched);
    EXPECT_FALSE(nfa.simulate("").matched);
}

TEST(NfaClosureTest, ComputesEpsilonClosure) {
    ThompsonNfa nfa;
    StateId s0 = nfa.add_state();
    StateId s1 = nfa.add_state();
    StateId s2 = nfa.add_state();
    StateId s3 = nfa.add_state();

    // 0 --eps--> 1 --eps--> 2
    // 0 --eps--> 3
    nfa.add_epsilon_transition(s0, s1);
    nfa.add_epsilon_transition(s1, s2);
    nfa.add_epsilon_transition(s0, s3);

    std::vector<StateId> closure0 = nfa.epsilon_closure(s0);
    std::vector<StateId> expected0 = {s0, s1, s2, s3};
    EXPECT_EQ(closure0, expected0);

    std::vector<StateId> closure1 = nfa.epsilon_closure(s1);
    std::vector<StateId> expected1 = {s1, s2};
    EXPECT_EQ(closure1, expected1);

    std::vector<StateId> closure2 = nfa.epsilon_closure(s2);
    std::vector<StateId> expected2 = {s2};
    EXPECT_EQ(closure2, expected2);
}

TEST(NfaClosureTest, HandlesCyclesWithoutInfiniteLoop) {
    ThompsonNfa nfa;
    StateId s0 = nfa.add_state();
    StateId s1 = nfa.add_state();

    // 0 <-> 1 cycle
    nfa.add_epsilon_transition(s0, s1);
    nfa.add_epsilon_transition(s1, s0);

    std::vector<StateId> closure = nfa.epsilon_closure(s0);
    std::vector<StateId> expected = {s0, s1};
    EXPECT_EQ(closure, expected);
}

TEST(NfaConstructionTest, BuildsFromAstNodes) {
    ThompsonNfa empty_nfa = build_thompson_nfa(parse_pattern(""));
    EXPECT_EQ(empty_nfa.num_states(), 2U);

    ThompsonNfa lit_nfa = build_thompson_nfa(parse_pattern("a"));
    EXPECT_EQ(lit_nfa.num_states(), 2U);

    ThompsonNfa concat_nfa = build_thompson_nfa(parse_pattern("ab"));
    EXPECT_EQ(concat_nfa.num_states(), 4U);

    ThompsonNfa alt_nfa = build_thompson_nfa(parse_pattern("a|b"));
    EXPECT_EQ(alt_nfa.num_states(), 6U);

    ThompsonNfa star_nfa = build_thompson_nfa(parse_pattern("a*"));
    EXPECT_EQ(star_nfa.num_states(), 4U);
}

TEST(NfaMatchingTest, MatchesBasicPatterns) {
    EXPECT_TRUE(matches("", ""));
    EXPECT_FALSE(matches("", "a"));

    EXPECT_TRUE(matches("a", "a"));
    EXPECT_FALSE(matches("a", ""));
    EXPECT_FALSE(matches("a", "b"));
    EXPECT_FALSE(matches("a", "aa"));

    EXPECT_TRUE(matches("ab", "ab"));
    EXPECT_FALSE(matches("ab", "a"));
    EXPECT_FALSE(matches("ab", "b"));
    EXPECT_FALSE(matches("ab", "aba"));

    EXPECT_TRUE(matches("a|b", "a"));
    EXPECT_TRUE(matches("a|b", "b"));
    EXPECT_FALSE(matches("a|b", "c"));
    EXPECT_FALSE(matches("a|b", "ab"));
    EXPECT_FALSE(matches("a|b", ""));

    EXPECT_TRUE(matches("a*", ""));
    EXPECT_TRUE(matches("a*", "a"));
    EXPECT_TRUE(matches("a*", "aa"));
    EXPECT_TRUE(matches("a*", "aaaaa"));
    EXPECT_FALSE(matches("a*", "b"));
    EXPECT_FALSE(matches("a*", "ab"));
}

TEST(NfaMatchingTest, MatchesEmptyAlternativesAndGroups) {
    EXPECT_TRUE(matches("()", ""));
    EXPECT_FALSE(matches("()", "a"));

    EXPECT_TRUE(matches("()*", ""));
    EXPECT_FALSE(matches("()*", "a"));

    EXPECT_TRUE(matches("|a", ""));
    EXPECT_TRUE(matches("|a", "a"));
    EXPECT_FALSE(matches("|a", "b"));

    EXPECT_TRUE(matches("a|", ""));
    EXPECT_TRUE(matches("a|", "a"));
    EXPECT_FALSE(matches("a|", "b"));

    EXPECT_TRUE(matches("|", ""));
    EXPECT_FALSE(matches("|", "a"));
}

TEST(NfaMatchingTest, MatchesComplexPatterns) {
    EXPECT_TRUE(matches("(a|b)*abb", "abababb"));
    EXPECT_TRUE(matches("(a|b)*abb", "abb"));
    EXPECT_TRUE(matches("(a|b)*abb", "aabb"));
    EXPECT_TRUE(matches("(a|b)*abb", "babb"));
    EXPECT_FALSE(matches("(a|b)*abb", "abab"));
    EXPECT_FALSE(matches("(a|b)*abb", ""));

    EXPECT_TRUE(matches("a|bc*", "a"));
    EXPECT_TRUE(matches("a|bc*", "b"));
    EXPECT_TRUE(matches("a|bc*", "bc"));
    EXPECT_TRUE(matches("a|bc*", "bcc"));
    EXPECT_FALSE(matches("a|bc*", "ac"));
    EXPECT_FALSE(matches("a|bc*", "c"));
}

TEST(NfaMetricsTest, TracksSimulationStepsAndActiveStates) {
    NfaMatcher matcher(parse_pattern("(a|b)*abb"));
    MatchResult result = matcher.full_match("ababb");

    EXPECT_TRUE(result.matched);
    EXPECT_EQ(result.metrics.steps, 5U);
    EXPECT_GT(result.metrics.peak_active_states, 0U);
    EXPECT_GT(matcher.metrics().active_states, 0U);
    EXPECT_EQ(result.metrics.active_nfa_states, result.metrics.peak_active_states);

    // Active state counts should record the count before input and after each step (5 steps -> 6 counts)
    const auto& counts = matcher.metrics().active_state_counts;
    EXPECT_EQ(counts.size(), 6U);
    EXPECT_GT(counts[0], 0U);
    for (std::size_t c : counts) {
        EXPECT_LE(c, matcher.metrics().peak_active_states);
    }
    EXPECT_EQ(counts.back(), matcher.metrics().active_states);
}

TEST(NfaMetricsTest, EarlyTerminationStopsWhenActiveStatesEmpty) {
    ThompsonNfa nfa(parse_pattern("ab"));
    NfaMetrics metrics_early;
    MatchResult res_early = nfa.simulate("axxxxx", &metrics_early, /*early_termination=*/true);
    EXPECT_FALSE(res_early.matched);
    // After 'a', active state is at 'b'. On 'x', active states becomes empty (step 2).
    // Early termination stops immediately at step 2 rather than reading all 6 chars.
    EXPECT_EQ(metrics_early.steps, 2U);
    EXPECT_EQ(metrics_early.active_states, 0U);

    NfaMetrics metrics_full;
    MatchResult res_full = nfa.simulate("axxxxx", &metrics_full, /*early_termination=*/false);
    EXPECT_FALSE(res_full.matched);
    EXPECT_EQ(metrics_full.steps, 6U);
}

TEST(NfaDifferentialTest, AgreesWithBacktrackingOnDiverseWorkloads) {
    const std::vector<std::string> patterns = {
        "", "a", "ab", "abc", "a|b", "a|b|c", "a*", "ab*", "a|bc*",
        "(a|b)*abb", "()", "()*", "|a", "a|", "|", "(a(b|c))*",
        "((a|b)*c)*", "a*b*c*"
    };

    const std::vector<std::string> inputs = {
        "", "a", "b", "c", "ab", "ba", "abc", "bcc", "abababb", "abb", "aabb", "ac"
    };

    for (const auto& pat : patterns) {
        Ast ast = parse_pattern(pat);
        BacktrackingMatcher b_matcher(ast);
        NfaMatcher n_matcher(ast);

        for (const auto& input : inputs) {
            bool b_res = b_matcher.full_match(input);
            MatchResult n_res = n_matcher.full_match(input);
            EXPECT_EQ(n_res.matched, b_res)
                << "Mismatch on pattern '" << pat << "' with input '" << input << "'";
        }
    }
}

TEST(NfaPerformanceTest, AvoidsCatastrophicBacktracking) {
    // (a|aa)*b on a sequence of 'a's causes exponential backtracking steps.
    std::string pattern = "(a|aa)*b";
    std::string pathological_input(24, 'a');

    // Backtracking matcher with step limit will throw
    BacktrackingMatcher b_matcher(parse_pattern(pattern), BacktrackingLimits{1000});
    EXPECT_THROW(b_matcher.full_match(pathological_input), ExecutionLimitExceeded);

    // Thompson NFA matches in linear time without any exponential explosion
    NfaMatcher n_matcher(parse_pattern(pattern));
    MatchResult n_res = n_matcher.full_match(pathological_input);

    EXPECT_FALSE(n_res.matched);
    EXPECT_EQ(n_res.metrics.steps, 24U);
    EXPECT_LE(n_res.metrics.peak_active_states, n_matcher.nfa().num_states());
}

TEST(NfaContractTest, ConformsToSharedMatcherInterface) {
    Ast ast = parse_pattern("a*b");
    std::unique_ptr<Matcher> matcher = std::make_unique<NfaMatcher>(ast);

    MatchResult match = matcher->full_match("aaab");
    EXPECT_TRUE(match.matched);
    EXPECT_EQ(match.metrics.steps, 4U);

    MatchResult reject = matcher->full_match("aaaa");
    EXPECT_FALSE(reject.matched);
}
