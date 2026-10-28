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
