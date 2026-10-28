#include "regexforge/regex.h"

#include <gtest/gtest.h>

using namespace regexforge;

TEST(RegexTest, CompilesPatternAndMatchesUsingSharedPolicy) {
    Regex regex("ab*");
    const auto result = regex.full_match("abbb");

    EXPECT_TRUE(result.match.matched);
    EXPECT_EQ(result.strategy, Strategy::Backtracking);
    EXPECT_EQ(result.selection.strategy, result.strategy);
    EXPECT_GT(result.match.metrics.steps, 0U);
}

TEST(RegexTest, UsesLazyDfaWhenWorkloadJustifiesIt) {
    RegexConfig config;
    config.expected_repetitions = 8;
    config.strategy_thresholds.nfa_risk = 0.0;
    config.strategy_thresholds.dfa_risk = 0.0;
    Regex regex("(a|b)*abb", config);
    const auto result = regex.full_match("abababb");

    EXPECT_TRUE(result.match.matched);
    EXPECT_EQ(result.strategy, Strategy::LazyDFA);
}

TEST(RegexTest, RejectsMalformedPatternsAtConstruction) {
    EXPECT_THROW(Regex("(ab"), ParseError);
}

TEST(RegexTest, ComparesAllEnginesOnTheSameInput) {
    Regex regex("(a|b)*abb");
    const auto comparison = regex.compare_all("abababb");

    ASSERT_EQ(comparison.size(), 3U);
    EXPECT_EQ(comparison[0].strategy, Strategy::Backtracking);
    EXPECT_EQ(comparison[1].strategy, Strategy::ThompsonNFA);
    EXPECT_EQ(comparison[2].strategy, Strategy::LazyDFA);
    EXPECT_TRUE(comparison[0].match.matched);
    EXPECT_TRUE(comparison[1].match.matched);
    EXPECT_TRUE(comparison[2].match.matched);
}
